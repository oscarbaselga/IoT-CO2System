#ifndef SENSOR_SGP30_H_ 
#define SENSOR_SGP30_H_

#include "stdint.h"

void sgp30_config (void);
void sgp30_init(void);
uint16_t sgp30_co2_reading();

#endif