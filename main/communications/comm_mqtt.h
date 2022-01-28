#ifndef COMM_MQTT_H_ 
#define COMM_MQTT_H_

#include "esp_system.h"

/* Starts a MQTT client */
esp_err_t mqtt_app_start(void);

/* Publish a new MQTT message */
void mqtt_publish (char *data);

#endif