#ifndef GLOBALS_H_ 
#define GLOBALS_H_

#define TAG_SGP30           "SENSOR_SGP30"
#define TAG_MQTT            "COMM_MQTTS"
#define TAG_HTTP            "COMM_HTTPS"
#define TAG_SNTP            "COMM_SNTP"
#define TAG_SLEEP           "PWR_SLEEP"
#define TAG_BLE             "COMM_BLE"
#define TAG_PROV            "ESP_PROV"

#define CONFIG_ESP_LOCATION        "/INFORMATICA/2/9/" // menuconfig
#define CONFIG_ESP_ID              "1" // menuconfig

#define WAITING_TIME_BW_PROV_HTTP   10 // menuconfig

char ESP_LOCATION [95];
char ESP_ID [5];

#endif