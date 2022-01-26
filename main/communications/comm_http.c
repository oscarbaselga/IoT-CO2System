#include "comm_http.h"

#include <string.h>

#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "globals.h"
#include "../sensors/sensor_sgp30.h"
#include "comm_ble.h"


#define CONFIG_HTTP_MAX_CONTENT_LEN     100 // menuconfig

#define REST_CHECK(a, str, ...)                                                        \
    do                                                                                 \
    {                                                                                  \
        if (!(a))                                                                      \
        {                                                                              \
            ESP_LOGE(TAG_HTTP, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            exit(0);                                                                   \
        }                                                                              \
    } while (0)


// Handler for getting system info
static esp_err_t system_info_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");

    cJSON *root = cJSON_CreateObject();
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    cJSON_AddStringToObject(root, "version", IDF_VER);
    cJSON_AddNumberToObject(root, "cores", chip_info.cores);
    
    const char *sys_info = cJSON_Print(root);
    httpd_resp_sendstr(req, sys_info);

    free((void *)sys_info);
    cJSON_Delete(root);

    return ESP_OK;
}

// Handler for getting node info
static esp_err_t node_info_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "ESP_LOCATION", ESP_LOCATION);
    cJSON_AddStringToObject(root, "ESP_ID", ESP_ID);
    
    const char *node_info = cJSON_Print(root);
    httpd_resp_sendstr(req, node_info);

    free((void *)node_info);
    cJSON_Delete(root);

    return ESP_OK;
}

// Handler for modifying ESP_LOCATION
static esp_err_t esp_location_post_handler(httpd_req_t *req) {
    int req_len = req->content_len;
    if (req_len >= CONFIG_HTTP_MAX_CONTENT_LEN) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }

    char buf[req_len];
    if (httpd_req_recv(req, buf, req_len) <= 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "unknown error");
        return ESP_FAIL;
    }

    strcpy(ESP_LOCATION, buf);
    
    httpd_resp_sendstr(req, "ESP_LOCATION modified successfully");

    return ESP_OK;
}

// Handler for modifying ESP_ID
static esp_err_t esp_id_post_handler(httpd_req_t *req) {
    int req_len = req->content_len;
    if (req_len >= CONFIG_HTTP_MAX_CONTENT_LEN) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }

    char buf[req_len];
    if (httpd_req_recv(req, buf, req_len) <= 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "unknown error");
        return ESP_FAIL;
    }

    strcpy(ESP_ID, buf);
    
    httpd_resp_sendstr(req, "ESP_ID modified successfully");

    return ESP_OK;
}

// Handler for capturing sensor data
static esp_err_t capture_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "CO2", (uint32_t)sgp30_co2_reading());
    cJSON_AddNumberToObject(root, "BLE_people", (uint8_t)get_people_estimation());
    
    const char *captured_data = cJSON_Print(root);
    httpd_resp_sendstr(req, captured_data);

    free((void *)captured_data);
    cJSON_Delete(root);

    return ESP_OK;
}


esp_err_t start_rest_server(void) {

    vTaskDelay(pdMS_TO_TICKS((WAITING_TIME_BW_PROV_HTTP + 1) * 1000));

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG_HTTP, "Starting HTTP Server");
    REST_CHECK(httpd_start(&server, &config) == ESP_OK, "Start server failed");

    // URI handler for fetching system info
    httpd_uri_t system_info_get_uri = {
        .uri = "/system/info",
        .method = HTTP_GET,
        .handler = system_info_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &system_info_get_uri);

    // URI handler for fetching node info
    httpd_uri_t node_info_get_uri = {
        .uri = "/node/info",
        .method = HTTP_GET,
        .handler = node_info_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &node_info_get_uri);

    // URI handler for modifying ESP_LOCATION
    httpd_uri_t esp_location_post_uri = {
        .uri = "/node/esp_location",
        .method = HTTP_POST,
        .handler = esp_location_post_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &esp_location_post_uri);

    // URI handler for modifying ESP_ID
    // Example: curl -d "{NEW_ESP_ID}" -X POST http://{IP}/node/esp_id
    httpd_uri_t esp_id_post_uri = {
        .uri = "/node/esp_id",
        .method = HTTP_POST,
        .handler = esp_id_post_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &esp_id_post_uri);

    // URI handler for acquiring data (SGP30 and people estimation)
    httpd_uri_t capture_get_uri = {
        .uri = "/node/capture",
        .method = HTTP_GET,
        .handler = capture_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &capture_get_uri);


    return ESP_OK;
    
}