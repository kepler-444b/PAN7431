#ifndef _BSP_ZERO_H_
#define _BSP_ZERO_H_

#include "../app/gpio.h"

void bsp_zero_init(void);
void zero_set_gpio(const gpio_pin_t pin, bool status);

#endif