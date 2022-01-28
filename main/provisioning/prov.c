#include "prov.h"

#include <string.h>
#include <protocomm.h>
#include <protocomm_httpd.h>
#include <protocomm_security0.h>
#include <protocomm_security1.h>
#include <wifi_provisioning/wifi_config.h>
#include <lwip/err.h>
#include <lwip/sys.h>

#include <esp_log.h>
#include <esp_err.h>
#include <esp_wifi.h>
#include <esp_event.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "globals.h"


/* Handler for catching WiFi events */
static void app_prov_event_handler(void* handler_arg, esp_event_base_t base, int id, void* data);

/* Handlers for wifi_config provisioning endpoint */
extern wifi_prov_config_handlers_t wifi_prov_handlers;

/**
 * @brief   Data relevant to provisioning application
 */
struct app_prov_data {
    protocomm_t *pc;                      /*!< Protocomm handler */
    int security;                         /*!< Type of security to use with protocomm */
    const protocomm_security_pop_t *pop;  /*!< Pointer to proof of possession */
    esp_timer_handle_t timer;             /*!< Handle to timer */

    /* State of WiFi Station */
    wifi_prov_sta_state_t wifi_state;

    /* Code for WiFi station disconnection (if disconnected) */
    wifi_prov_sta_fail_reason_t wifi_disconnect_reason;
};

/* Pointer to provisioning application data */
static struct app_prov_data *g_prov;

static esp_err_t app_prov_start_service(void)
{
    /* Create new protocomm instance */
    g_prov->pc = protocomm_new();
    if (g_prov->pc == NULL) {
        ESP_LOGE(TAG_PROV, "Failed to create new protocomm instance");
        return ESP_FAIL;
    }

    /* Config for protocomm_httpd_start() */
    protocomm_httpd_config_t pc_config = {
        .data = {
            .config = PROTOCOMM_HTTPD_DEFAULT_CONFIG()
        }
    };
    /* Start protocomm server on top of HTTP */
    if (protocomm_httpd_start(g_prov->pc, &pc_config) != ESP_OK) {
        ESP_LOGE(TAG_PROV, "Failed to start protocomm HTTP server");
        return ESP_FAIL;
    }

    /* Set protocomm version verification endpoint for protocol */
    protocomm_set_version(g_prov->pc, "proto-ver", "V0.1");

    /* Set protocomm security type for endpoint */
    if (g_prov->security == 0) {
        protocomm_set_security(g_prov->pc, "prov-session", &protocomm_security0, NULL);
    } else if (g_prov->security == 1) {
        protocomm_set_security(g_prov->pc, "prov-session", &protocomm_security1, g_prov->pop);
    }

    /* Add endpoint for provisioning to set wifi station config */
    if (protocomm_add_endpoint(g_prov->pc, "prov-config",
                               wifi_prov_config_data_handler,
                               (void *) &wifi_prov_handlers) != ESP_OK) {
        ESP_LOGE(TAG_PROV, "Failed to set provisioning endpoint");
        protocomm_httpd_stop(g_prov->pc);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static void app_prov_stop_service(void)
{
    /* Remove provisioning endpoint */
    protocomm_remove_endpoint(g_prov->pc, "prov-config");
    /* Unset provisioning security */
    protocomm_unset_security(g_prov->pc, "prov-session");
    /* Unset provisioning version endpoint */
    protocomm_unset_version(g_prov->pc, "proto-ver");
    /* Stop protocomm server */
    protocomm_httpd_stop(g_prov->pc);
    /* Delete protocomm instance */
    protocomm_delete(g_prov->pc);

    /* Remove event handler */
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, app_prov_event_handler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, app_prov_event_handler);
}

/* Task spawned by timer callback */
static void stop_prov_task(void * arg)
{
    ESP_LOGI(TAG_PROV, "Stopping provisioning");
    app_prov_stop_service();
    esp_wifi_set_mode(WIFI_MODE_STA);

    /* Timer not needed anymore */
    esp_timer_handle_t timer = g_prov->timer;
    esp_timer_delete(timer);
    g_prov->timer = NULL;

    /* Free provisioning process data */
    free(g_prov);
    g_prov = NULL;
    ESP_LOGI(TAG_PROV, "Provisioning stopped");

    vTaskDelete(NULL);
}

/* Callback to be invoked by timer */
static void _stop_prov_cb(void * arg)
{
    xTaskCreate(&stop_prov_task, "stop_prov", 2048, NULL, tskIDLE_PRIORITY, NULL);
}

/* Event handler for starting/stopping provisioning */
static void app_prov_event_handler(void* handler_arg, esp_event_base_t event_base,
                                   int event_id, void* event_data)
{
    /* If pointer to provisioning application data is NULL
     * then provisioning is not running */
    if (!g_prov) {
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG_PROV, "STA Start");
        /* Once configuration is received through protocomm,
         * device is started as station. Once station starts,
         * wait for connection to establish with configured
         * host SSID and password */
        g_prov->wifi_state = WIFI_PROV_STA_CONNECTING;
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG_PROV, "STA Got IP");
        /* Station got IP. That means configuration is successful.
         * Schedule timer to stop provisioning app after 30 seconds. */
        g_prov->wifi_state = WIFI_PROV_STA_CONNECTED;
        if (g_prov && g_prov->timer) {
            /* Note that, after restarting the WiFi in Station + AP mode, the
             * user gets disconnected from the AP for a while. But at the same
             * time, the user app requests for status update from the device
             * to verify that the provisioning was successful. Therefore, the
             * turning off of the AP must be delayed long enough for the user
             * to reconnect and get STA connection status from the device.
             * Otherwise, the AP will be turned off before the user can
             * reconnect and thus the user app will see connection timed out,
             * signaling a failure in provisioning. */
            esp_timer_start_once(g_prov->timer, WAITING_TIME_BW_PROV_HTTP*1000000);
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGE(TAG_PROV, "STA Disconnected");
        /* Station couldn't connect to configured host SSID */
        g_prov->wifi_state = WIFI_PROV_STA_DISCONNECTED;

        wifi_event_sta_disconnected_t* disconnected = (wifi_event_sta_disconnected_t*) event_data;
        ESP_LOGE(TAG_PROV, "Disconnect reason : %d", disconnected->reason);

        /* Set code corresponding to the reason for disconnection */
        switch (disconnected->reason) {
        case WIFI_REASON_AUTH_EXPIRE:
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_BEACON_TIMEOUT:
        case WIFI_REASON_AUTH_FAIL:
        case WIFI_REASON_ASSOC_FAIL:
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
            ESP_LOGI(TAG_PROV, "STA Auth Error");
            g_prov->wifi_disconnect_reason = WIFI_PROV_STA_AUTH_ERROR;
            break;
        case WIFI_REASON_NO_AP_FOUND:
            ESP_LOGI(TAG_PROV, "STA AP Not found");
            g_prov->wifi_disconnect_reason = WIFI_PROV_STA_AP_NOT_FOUND;
            break;
        default:
            /* If none of the expected reasons,
             * retry connecting to host SSID */
            g_prov->wifi_state = WIFI_PROV_STA_CONNECTING;
        }
        esp_wifi_connect();
    }
}

esp_err_t app_prov_get_wifi_state(wifi_prov_sta_state_t* state)
{
    if (g_prov == NULL || state == NULL) {
        return ESP_FAIL;
    }

    *state = g_prov->wifi_state;
    return ESP_OK;
}

esp_err_t app_prov_get_wifi_disconnect_reason(wifi_prov_sta_fail_reason_t* reason)
{
    if (g_prov == NULL || reason == NULL) {
        return ESP_FAIL;
    }

    if (g_prov->wifi_state != WIFI_PROV_STA_DISCONNECTED) {
        return ESP_FAIL;
    }

    *reason = g_prov->wifi_disconnect_reason;
    return ESP_OK;
}

esp_err_t app_prov_is_provisioned(bool *provisioned)
{
    *provisioned = false;

#ifdef CONFIG_EXAMPLE_RESET_PROVISIONED
    esp_wifi_restore();
    return ESP_OK;
#endif

    /* Get WiFi Station configuration */
    wifi_config_t wifi_cfg;
    if (esp_wifi_get_config(WIFI_IF_STA, &wifi_cfg) != ESP_OK) {
        return ESP_FAIL;
    }

    if (strlen((const char*) wifi_cfg.sta.ssid)) {
        *provisioned = true;
        ESP_LOGI(TAG_PROV, "Found ssid %s",     (const char*) wifi_cfg.sta.ssid);
        ESP_LOGI(TAG_PROV, "Found password %s", (const char*) wifi_cfg.sta.password);
    }
    return ESP_OK;
}

esp_err_t app_prov_configure_sta(wifi_config_t *wifi_cfg)
{
    /* Configure WiFi as both AP and Station */
    if (esp_wifi_set_mode(WIFI_MODE_APSTA) != ESP_OK) {
        ESP_LOGE(TAG_PROV, "Failed to set WiFi mode");
        return ESP_FAIL;
    }
    /* Configure WiFi station with host credentials
     * provided during provisioning */
    if (esp_wifi_set_config(WIFI_IF_STA, wifi_cfg) != ESP_OK) {
        ESP_LOGE(TAG_PROV, "Failed to set WiFi configuration");
        return ESP_FAIL;
    }
    /* Restart WiFi */
    if (esp_wifi_start() != ESP_OK) {
        ESP_LOGE(TAG_PROV, "Failed to restart WiFi");
        return ESP_FAIL;
    }
    /* Connect to AP */
    if (esp_wifi_connect() != ESP_OK) {
        ESP_LOGE(TAG_PROV, "Failed to connect WiFi");
        return ESP_FAIL;
    }

    if (g_prov) {
        /* Reset wifi station state for provisioning app */
        g_prov->wifi_state = WIFI_PROV_STA_CONNECTING;
    }
    return ESP_OK;
}

static esp_err_t start_wifi_ap(const char *ssid, const char *pass)
{
    /* Build WiFi configuration for AP mode */
    wifi_config_t wifi_config = {
        .ap = {
            .max_connection = 5,
        },
    };

    strlcpy((char *) wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid));

    if (strlen(pass) == 0) {
        memset(wifi_config.ap.password, 0, sizeof(wifi_config.ap.password));
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    } else {
        strlcpy((char *) wifi_config.ap.password, pass, sizeof(wifi_config.ap.password));
        wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    }

    /* Start WiFi in AP mode with configuration built above */
    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_PROV, "Failed to set WiFi mode : %d", err);
        return err;
    }
    err = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_PROV, "Failed to set WiFi config : %d", err);
        return err;
    }
    err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG_PROV, "Failed to start WiFi : %d", err);
        return err;
    }

    return ESP_OK;
}

esp_err_t app_prov_start_softap_provisioning(const char *ssid, const char *pass,
                                             int security, const protocomm_security_pop_t *pop)
{
    /* If provisioning app data present,
     * means provisioning app is already running */
    if (g_prov) {
        ESP_LOGI(TAG_PROV, "Invalid provisioning state");
        return ESP_FAIL;
    }

    /* Allocate memory for provisioning app data */
    g_prov = (struct app_prov_data *) calloc(1, sizeof(struct app_prov_data));
    if (!g_prov) {
        ESP_LOGI(TAG_PROV, "Unable to allocate prov data");
        return ESP_ERR_NO_MEM;
    }

    /* Initialize app data */
    g_prov->pop = pop;
    g_prov->security = security;

    /* Create timer object as a member of app data */
    esp_timer_create_args_t timer_conf = {
        .callback = _stop_prov_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "stop_softap_tm"
    };
    esp_err_t err = esp_timer_create(&timer_conf, &g_prov->timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_PROV, "Failed to create timer");
        return err;
    }

    err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, app_prov_event_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_PROV, "Failed to register WiFi event handler");
        return err;
    }

    err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, app_prov_event_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_PROV, "Failed to register IP event handler");
        return err;
    }

    /* Start WiFi softAP with specified ssid and password */
    err = start_wifi_ap(ssid, pass);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_PROV, "Failed to start WiFi AP");
        return err;
    }

    /* Start provisioning service through HTTP */
    err = app_prov_start_service();
    if (err != ESP_OK) {
        ESP_LOGE(TAG_PROV, "Failed to start provisioning app");
        return err;
    }

    ESP_LOGI(TAG_PROV, "SoftAP Provisioning started with SSID '%s', Password '%s'", ssid, pass);
    return ESP_OK;
}

static void event_handler(void* arg, esp_event_base_t event_base,
                          int event_id, void* event_data)
{
    static int s_retry_num = 0;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < CONFIG_EXAMPLE_AP_RECONN_ATTEMPTS) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG_PROV, "retry to connect to the AP");
        }
        ESP_LOGI(TAG_PROV,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG_PROV, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
    }
}

static void wifi_init_sta(void)
{
    /* Set our event handling */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event_handler, NULL));

    /* Start Wi-Fi in station mode with credentials set during provisioning */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void start_softap_provisioning(void)
{
    /* Security version */
    int security = 0;
    /* Proof of possession */
    const protocomm_security_pop_t *pop = NULL;

#ifdef CONFIG_EXAMPLE_USE_SEC_1
    security = 1;
#endif

    /* Having proof of possession is optional */
#ifdef CONFIG_EXAMPLE_USE_POP
    const static protocomm_security_pop_t app_pop = {
        .data = (uint8_t *) CONFIG_EXAMPLE_POP,
        .len = (sizeof(CONFIG_EXAMPLE_POP)-1)
    };
    pop = &app_pop;
#endif

        const char *ssid = NULL;

#ifdef CONFIG_EXAMPLE_SSID
        ssid = CONFIG_EXAMPLE_SSID;
#else
        uint8_t eth_mac[6];
        esp_wifi_get_mac(WIFI_IF_STA, eth_mac);

        char ssid_with_mac[33];
        snprintf(ssid_with_mac, sizeof(ssid_with_mac), "PROV_%02X%02X%02X",
                 eth_mac[3], eth_mac[4], eth_mac[5]);

        ssid = ssid_with_mac;
#endif

    ESP_ERROR_CHECK(app_prov_start_softap_provisioning(
        ssid, CONFIG_EXAMPLE_PASS, security, pop));
}

esp_err_t start_esp_provisioning(void){

    /* Initialize Wi-Fi including netif with default config */
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Check if device is provisioned */
    bool provisioned;
    if (app_prov_is_provisioned(&provisioned) != ESP_OK) {
        ESP_LOGE(TAG_PROV, "Error getting device provisioning state");
        return 0;
    }

    if (provisioned == false) {
        /* If not provisioned, start provisioning via soft AP */
        ESP_LOGI(TAG_PROV, "Starting WiFi SoftAP provisioning");
        start_softap_provisioning();

        while(provisioned == false) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            app_prov_is_provisioned(&provisioned);
        }

    } else {
        /* Start WiFi station with credentials set during provisioning */
        ESP_LOGI(TAG_PROV, "Starting WiFi station");
        wifi_init_sta();
    }

    return ESP_OK;
}
