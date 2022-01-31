#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_timer.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_sleep.h"
#include "esp_pm.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "esp_netif.h"
//#include "cJSON.h"
#include "cbor.h"

#include "sensors/sensor_sgp30.h"
#include "communications/comm_mqtt.h"
#include "communications/comm_http.h"
#include "communications/comm_sntp.h"
#include "communications/comm_ble.h"
#include "provisioning/prov.h"
#include "globals.h"


/**
 *  By default, the period used by the SGP30 sensor to take a reading
 *  is only 1 second
 */
#define SGP30_READING_PERIOD_SEC    1

/**
 *  Timers and semaphores used by the implemented tasks
 */
esp_timer_handle_t timer_sensor_sgp30, timer_deep_sleep, timer_ble;
SemaphoreHandle_t sgp30_semphr, deep_sleep_semphr, ble_semphr;


/* ----------------- AUXILIAR FUNCTIONS ---------------- */
/**
 * @brief   Calculates the number of usecs between two times
 *
 * @param[in] node_time   A referece time represented as "struct tm"
 *                        It is assigned the current time in this code
 * @param[in] limit_hr    The time up to which the interval is calculated
 *                        It is assigned an exact hour of the day
 *
 * @return  The number of usecs that have elapsed between these timestamps
 */
static uint64_t calculate_range_us (struct tm node_time, int limit_hr) {
    int aux, hour = node_time.tm_hour, min = node_time.tm_min;
    uint64_t result;

    if (hour == limit_hr) {
        result = 60 - min;
        result = result * 60 * 1000000;
        return result;
    } 
    else if (hour < limit_hr) aux = limit_hr;
    else aux = limit_hr + 24;

    result = (aux - hour - 1) * 60; // Full hours completed converted to mins
    result = result + (60 - min);   // Mins left to reach "limit_hr"
    result = result * 60 * 1000000; // Total mins converted to us

    return result;

}


/* ------------------ TIMER CALLBACKS ------------------ */
/** 
 *  SGP30 timer callback
 */
static void timer_sensor_sgp30_callback(void* arg) {
    xSemaphoreGive(sgp30_semphr);
}

/**
 *  Deep sleep timer callback
 */
static void timer_deep_sleep_callback(void* arg) {
    xSemaphoreGive(deep_sleep_semphr);
}

/**
 *  BLE timer callback
 */
static void timer_ble_callback(void* arg) {
    xSemaphoreGive(ble_semphr);
}


/* ----------------------- TASKS ----------------------- */
/**
 *  This task reads the SGP30 sensor and sends data, in CBOR representation, over MQTT
 */
void sgp30_task(void *pvParameter) {
    uint16_t num_readings = 0;
    uint32_t sum_co2 = 0;

    while (1) {
        xSemaphoreTake(sgp30_semphr, portMAX_DELAY);

        if(num_readings < CONFIG_MQTT_SENDING_PERIOD_SEC) {
            sum_co2 += (uint32_t)sgp30_co2_reading();
            num_readings++;
        } else { 
            /* Send result via MQTT */
            uint16_t co2_ppm = sum_co2 / num_readings;
            //ESP_LOGI(TAG_SGP30, "CO2(ppm) -> %d", co2_ppm);

            // cJSON *data_json = cJSON_CreateObject();
            // cJSON_AddNumberToObject(data_json, "CO2", co2_ppm);
            // const char *data = cJSON_Print(data_json);
            // mqtt_publish(data);

            /* CBOR creation */
            CborEncoder root_encoder;
            uint8_t data_cbor[100];
            cbor_encoder_init(&root_encoder, data_cbor, sizeof(data_cbor), 0);

            CborEncoder array_encoder;
            CborEncoder map_encoder;
            cbor_encoder_create_array(&root_encoder, &array_encoder, 1); // 1-item length array -> [
            cbor_encoder_create_map(&array_encoder, &map_encoder, 2);    // 2-item length map -> {

            cbor_encode_text_stringz(&map_encoder, "ESP_ID");
            cbor_encode_text_stringz(&map_encoder, ESP_ID);

            cbor_encode_text_stringz(&map_encoder, "CO2");
            cbor_encode_uint(&map_encoder, co2_ppm);

            cbor_encoder_close_container(&array_encoder, &map_encoder);  // }
            cbor_encoder_close_container(&root_encoder, &array_encoder); // ]

            mqtt_publish((char*)data_cbor);
            //ESP_LOGI(TAG_SGP30, "CBOR -> %s", (char*)data_cbor);

            num_readings = 0;
            sum_co2 = 0;
        }

    }

    vTaskDelete(NULL);
}

/**
 *  This task checks the current time and sets a timer to enter/exit from deep sleep mode
 *  based on the period established by the user
 */
void deep_sleep_task(void *pvParameter) {
    struct tm node_time;
    uint8_t sleep_now = 0;
    uint64_t next_checking_us;

    while (1) {

        node_time = get_sys_time();
        int node_time_hr = node_time.tm_hour;

        if (CONFIG_DEEP_SLEEP_START_TIME_HR <= CONFIG_DEEP_SLEEP_FINISH_TIME_HR) {
            /* Both times belong to the same day */

            if (node_time_hr >= CONFIG_DEEP_SLEEP_START_TIME_HR && 
                node_time_hr < CONFIG_DEEP_SLEEP_FINISH_TIME_HR) {
                /* Should already be asleep */
                sleep_now  = 1;
                next_checking_us = calculate_range_us(node_time, CONFIG_DEEP_SLEEP_FINISH_TIME_HR);
            } else {
                next_checking_us = calculate_range_us(node_time, CONFIG_DEEP_SLEEP_START_TIME_HR);
            }

        } else {
            /* Each time belongs to consecutives days */

            if (node_time_hr >= CONFIG_DEEP_SLEEP_START_TIME_HR || 
                node_time_hr < CONFIG_DEEP_SLEEP_FINISH_TIME_HR) {
                /* Should already be asleep */
                sleep_now  = 1;
                next_checking_us = calculate_range_us(node_time, CONFIG_DEEP_SLEEP_FINISH_TIME_HR);
            } else {
                next_checking_us = calculate_range_us(node_time, CONFIG_DEEP_SLEEP_START_TIME_HR);
            }

        }

        if (sleep_now) { 
            ESP_LOGI(TAG_SLEEP, "Exiting deep sleep mode in %d mins (now %d:%.2d)", 
                                        (int)(next_checking_us / (1000000 * 60)), 
                                        node_time_hr, 
                                        node_time.tm_min);
            sleep_now = 0;
            esp_sleep_enable_timer_wakeup(next_checking_us);
            esp_deep_sleep_start();
        } else {
            ESP_LOGI(TAG_SLEEP, "Entering deep sleep mode in %d mins (now %d:%.2d)", 
                                        (int)(next_checking_us / (1000000 * 60)), 
                                        node_time_hr, 
                                        node_time.tm_min);
            esp_timer_start_once(timer_deep_sleep, next_checking_us);
        }

        xSemaphoreTake(deep_sleep_semphr, portMAX_DELAY);

    }
}

/**
 *  This task scans BLE devices and estimates the number of people
 *  in an specific area
 *  This data is also sent over MQTT as CBOR
 */
void ble_task(void *pvParameter) {

    while (1) {

        /* Start scan */
        scan_BLE_devices(CONFIG_BLE_SCANNING_DURATION_SEC);

        /* Wait for the scan to finish plus one sec to ensure BLE scanning has ended */
        vTaskDelay(pdMS_TO_TICKS((CONFIG_BLE_SCANNING_DURATION_SEC + 1) * 1000));

        /* CBOR creation */
        CborEncoder root_encoder;
        uint8_t data_cbor[100];
        cbor_encoder_init(&root_encoder, data_cbor, sizeof(data_cbor), 0);

        CborEncoder array_encoder;
        CborEncoder map_encoder;
        cbor_encoder_create_array(&root_encoder, &array_encoder, 1); // 1-item length array -> [
        cbor_encoder_create_map(&array_encoder, &map_encoder, 2);    // 2-item length map -> {

        cbor_encode_text_stringz(&map_encoder, "ESP_ID");
        cbor_encode_text_stringz(&map_encoder, ESP_ID);

        uint8_t ble_last_estimation = get_people_estimation();
        cbor_encode_text_stringz(&map_encoder, "BLE_people");
        cbor_encode_uint(&map_encoder, ble_last_estimation);

        cbor_encoder_close_container(&array_encoder, &map_encoder);  // }
        cbor_encoder_close_container(&root_encoder, &array_encoder); // ]

        mqtt_publish((char*)data_cbor);
        //ESP_LOGI(TAG_SGP30, "CBOR -> %s", (char*)data_cbor);
        
        xSemaphoreTake(ble_semphr, portMAX_DELAY);

    }
}


void app_main(void)
{
    /* Update the node info */
    strcpy(ESP_LOCATION, CONFIG_ESP_LOCATION);
    strcpy(ESP_ID, CONFIG_ESP_ID);
    
    /**
     *  Initialize the  Non-Volatile Storage, a layer over the TCP/IP stack 
     *  and a loop where to register the events of the components
     */
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());


    /* -------------------- SGP30 SENSOR -------------------- */
    ESP_ERROR_CHECK(sgp30_config());
    ESP_ERROR_CHECK(sgp30_init());

    const esp_timer_create_args_t timer_sensor_sgp30_args = {
        .callback = &timer_sensor_sgp30_callback,
        .name = "sensor sgp30 timer"
    };
    esp_timer_create(&timer_sensor_sgp30_args, &timer_sensor_sgp30);

    sgp30_semphr = xSemaphoreCreateBinary();


    /* ------------------ POWER MANAGEMENT ------------------ */
    esp_pm_config_esp32_t conf_sleep_mode = {
        .max_freq_mhz = CONFIG_PM_SLEEP_MAX_FREQ_MHZ,
        .min_freq_mhz = CONFIG_PM_SLEEP_MIN_FREQ_MHZ,
        .light_sleep_enable = 1,
    };
    esp_pm_configure(&conf_sleep_mode);

    const esp_timer_create_args_t timer_deep_sleep_args = {
        .callback = &timer_deep_sleep_callback,
        .name = "deep sleep timer"
    };
    esp_timer_create(&timer_deep_sleep_args, &timer_deep_sleep);

    deep_sleep_semphr = xSemaphoreCreateBinary();


    /* ---------------------- BLUETOOTH --------------------- */
    ESP_ERROR_CHECK(esp_ble_init());

    const esp_timer_create_args_t timer_ble_args = {
        .callback = &timer_ble_callback,
        .name = "ble timer"
    };
    esp_timer_create(&timer_ble_args, &timer_ble);

    ble_semphr = xSemaphoreCreateBinary();


    /* --------------------- DEPLOYMENT --------------------- */
    /* Provisioning process */
    ESP_ERROR_CHECK(start_esp_provisioning());

    /* Set the current time in the system */
    ESP_ERROR_CHECK(set_sys_time());
    
    /* Start MQTT client */
    ESP_ERROR_CHECK(mqtt_app_start());

    /* Start REST server */
    ESP_ERROR_CHECK(start_rest_server());

    /* Start timers */
    esp_timer_start_periodic(timer_sensor_sgp30, SGP30_READING_PERIOD_SEC * 1000000);
    esp_timer_start_periodic(timer_ble, CONFIG_BLE_ESTIMATION_PERIOD_SEC * 1000000);
    
    /* Run tasks */
    xTaskCreate(&sgp30_task, "SGP30 task", 4096, NULL, 5, NULL);
    xTaskCreate(&deep_sleep_task, "Deep sleep task", 4096, NULL, 5, NULL);
    xTaskCreate(&ble_task, "BLE task", 4096, NULL, 5, NULL);


    vTaskDelete(NULL);

}
