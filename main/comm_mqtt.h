#ifndef COMM_MQTT_H_ 
#define COMM_MQTT_H_

#include "esp_system.h"

esp_err_t mqtt_app_start(void);
void mqtt_publish (char *data);

#endif