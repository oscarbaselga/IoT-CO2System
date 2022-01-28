#ifndef GLOBALS_H_ 
#define GLOBALS_H_

/* Tags of each module */
#define TAG_SGP30   "SENSOR_SGP30"
#define TAG_MQTT    "COMM_MQTTS"
#define TAG_HTTP    "COMM_HTTPS"
#define TAG_SNTP    "COMM_SNTP"
#define TAG_SLEEP   "PWR_SLEEP"
#define TAG_BLE     "COMM_BLE"
#define TAG_PROV    "ESP_PROV"

/**
 *  Time to wait between the provisioning process
 *  and the start of the HTTP server
 */
#define WAITING_TIME_BW_PROV_HTTP   10

/* Node information */ 
char ESP_LOCATION [95];
char ESP_ID [5];

#endif