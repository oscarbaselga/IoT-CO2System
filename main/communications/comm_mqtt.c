#include "comm_mqtt.h"

#include <string.h>

#include "esp_log.h"
#include "mqtt_client.h"

#include "globals.h"


#if CONFIG_BROKER_CERTIFICATE_OVERRIDDEN == 1
static const uint8_t mqtt_test_broker_pem_start[]  = "-----BEGIN CERTIFICATE-----\n" CONFIG_BROKER_CERTIFICATE_OVERRIDE "\n-----END CERTIFICATE-----";
#else
extern const uint8_t mqtt_test_broker_pem_start[]   asm("_binary_mqtt_test_broker_pem_start");
#endif
extern const uint8_t mqtt_test_broker_pem_end[]   asm("_binary_mqtt_test_broker_pem_end");


esp_mqtt_client_handle_t global_client;


static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event) {

    switch (event->event_id) {

        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG_MQTT, "Node %s connected", ESP_ID);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG_MQTT, "Node %s disconnected", ESP_ID);
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG_MQTT, "Subscription confirmation");
            break;

        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG_MQTT, "Unsuscribed");
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG_MQTT, "ACK, only for qos=1 or 2");
            break;

        case MQTT_EVENT_DATA: {
            
            ESP_LOGI(TAG_MQTT, "Received from %s -> %s", event->topic, event->data);
            break;
            
        }
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG_MQTT, "Error event");
            break;

        default:
            ESP_LOGI(TAG_MQTT, "Unknown event id(%d)", event->event_id);
            break;

    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG_MQTT, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

esp_err_t mqtt_app_start(void) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = CONFIG_MQTT_BROKER_URI,
        .cert_pem = (const char *)mqtt_test_broker_pem_start,
    };

    global_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(global_client, ESP_EVENT_ANY_ID, mqtt_event_handler, global_client);
    esp_mqtt_client_start(global_client);

    return ESP_OK;
}

void mqtt_publish (char *data) {
    char topic[100];
    strcpy(topic, ESP_LOCATION);
    strcat(topic, ESP_ID);

    esp_mqtt_client_publish(global_client, topic, data, 0, CONFIG_MQTT_QOS, 0);
    ESP_LOGI(TAG_MQTT, "Message published in %s", topic);
}
