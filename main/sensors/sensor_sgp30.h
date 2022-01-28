#ifndef SENSOR_SGP30_H_ 
#define SENSOR_SGP30_H_

#include "esp_err.h"
#include "stdint.h"

/* Configures the SGP30 sensor */
esp_err_t  sgp30_config (void);

/* Initializes the SGP30 sensor */
esp_err_t sgp30_init(void);

/* Returns a CO2 reading */
uint16_t sgp30_co2_reading(void);

#endif