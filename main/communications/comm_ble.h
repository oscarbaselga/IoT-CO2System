#ifndef COMM_BLE_H_ 
#define COMM_BLE_H_

#include <stdint.h>
#include "esp_err.h"


/* Type definition for storing a BLE device address */
typedef struct MAC_BLE_ELEM{
	uint8_t addr[6];
	struct MAC_BLE_ELEM* prev;
} MAC_BLE_ELEM_t;

/* Initializes BLE */
esp_err_t esp_ble_init(void);

/* Starts scanning devices for a while */
void scan_BLE_devices(int duration);

/* Get the estimated number of people */
uint8_t get_people_estimation (void);


#endif