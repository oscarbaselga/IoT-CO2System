#ifndef GLOBALS_H_ 
#define GLOBALS_H_

#define TAG_SGP30           "SENSOR_SGP30"
#define TAG_MQTT            "COMM_MQTT"
#define TAG_HTTP            "COMM_HTTP"
#define TAG_SNTP            "COMM_SNTP"
#define TAG_SLEEP           "PWR_SLEEP"
#define TAG_BLE             "COMM_BLE"

#define CONFIG_ESP_LOCATION        "/INFORMATICA/2/9/" // menuconfig
#define CONFIG_ESP_ID              "1" // menuconfig

char ESP_LOCATION [95];
char ESP_ID [5];

#endif