#ifndef COMM_SNTP_H_ 
#define COMM_SNTP_H_

#include <time.h>

void set_sys_time (void);
struct tm get_sys_time (void);

#endif