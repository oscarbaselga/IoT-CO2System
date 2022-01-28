#ifndef COMM_SNTP_H_ 
#define COMM_SNTP_H_

#include <time.h>
#include "esp_err.h"

/* Set the current time to the node system time */
esp_err_t set_sys_time (void);

/* Get the system time of the node */
struct tm get_sys_time (void);

#endif