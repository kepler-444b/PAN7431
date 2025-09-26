#ifndef _TRAN_DEVICE_H_
#define _TRAN_DEVICE_H_
#include <stdbool.h>

void app_tran_device_init(void);
void rssi_update(void);
bool rssi_is_enabled(void);

#endif