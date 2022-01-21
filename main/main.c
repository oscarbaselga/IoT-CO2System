#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_timer.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
//#include "cJSON.h"
#include "cbor.h"

#include "sensor_sgp30.h"
#include "comm_mqtt.h"
#include "comm_http.h"
#include "globals.h"


#define SGP30_READING_PERIOD_US     1000000
#define MQTT_SENDING_PERIOD_SEC     3 // menuconfig


esp_timer_handle_t timer_sensor_sgp30;


// TIMER CALLBACKS
static void timer_sensor_sgp30_callback(void* arg) {
    static uint16_t num_readings = 0;
    static uint32_t sum_co2 = 0;

    if(num_readings < MQTT_SENDING_PERIOD_SEC) {
        sum_co2 += (uint32_t)sgp30_co2_reading();
        num_readings++;
    } else {
        uint16_t co2_ppm = sum_co2 / num_readings;
        ESP_LOGI(TAG_SGP30, "CO2(ppm) -> %d", co2_ppm);

        // cJSON *data_json = cJSON_CreateObject();
        // cJSON_AddNumberToObject(data_json, "CO2", co2_ppm);
        // const char *data = cJSON_Print(data_json);
        // mqtt_publish(data);

        // CBOR creation
        CborEncoder root_encoder;
        uint8_t data_cbor[100];
        cbor_encoder_init(&root_encoder, data_cbor, sizeof(data_cbor), 0);

        CborEncoder array_encoder;
        CborEncoder map_encoder;
        cbor_encoder_create_array(&root_encoder, &array_encoder, 1);    // 1-item length array -> [
        cbor_encoder_create_map(&array_encoder, &map_encoder, 2);       // 2-item length map -> {

        cbor_encode_text_stringz(&map_encoder, "ESP_ID");
        cbor_encode_text_stringz(&map_encoder, ESP_ID);

        cbor_encode_text_stringz(&map_encoder, "CO2");
        cbor_encode_uint(&map_encoder, co2_ppm);

        cbor_encoder_close_container(&array_encoder, &map_encoder);     // }
        cbor_encoder_close_container(&root_encoder, &array_encoder);    // ]

        mqtt_publish((char*)data_cbor);
        //ESP_LOGI(TAG_SGP30, "CBOR -> %s", (char*)data_cbor);
        num_readings = 0;
        sum_co2 = 0;
    }

}

void app_main(void)
{
    strcpy(ESP_LOCATION, CONFIG_ESP_LOCATION);
    strcpy(ESP_ID, CONFIG_ESP_ID);
    
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* -------------------- SENSOR SGP30 -------------------- */
    // Configuration and initialization
    sgp30_config();
    sgp30_init();

    // Periodic timer for a CO2 reading
    const esp_timer_create_args_t timer_sensor_sgp30_args = {
        .callback = &timer_sensor_sgp30_callback,
        .name = "timer sensor sgp30"
    };
    esp_timer_create(&timer_sensor_sgp30_args, &timer_sensor_sgp30);


    /* --------------------- DEPLOYMENT --------------------- */
    ESP_ERROR_CHECK(example_connect());
    
    ESP_ERROR_CHECK(mqtt_app_start());
    ESP_ERROR_CHECK(start_rest_server());

    esp_timer_start_periodic(timer_sensor_sgp30, SGP30_READING_PERIOD_US);    

}
