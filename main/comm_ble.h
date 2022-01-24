#ifndef COMM_BLE_H_ 
#define COMM_BLE_H_

#include <stdint.h>

typedef struct MAC_BLE_ELEM{
	uint8_t addr[6];
	struct MAC_BLE_ELEM* prev;
} MAC_BLE_ELEM_t;

void esp_ble_init(void);
void scan_BLE_devices(int duration);
uint8_t get_people_estimation (void);

#endif