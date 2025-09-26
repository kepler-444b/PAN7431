#ifndef _ADC_H_
#define _ADC_H_
#include <stdint.h>

#define ERR_VOL 5000

void app_adc_init(void);
uint16_t app_get_adc_value(void);

#endif
