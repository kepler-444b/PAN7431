#ifndef _PWM_H_
#define _PWM_H_
#include <stdbool.h>
#include "gpio.h"

#if 0
#define PWM_MAX_CHANNELS 8

void app_pwm_init(void);

bool app_pwm_add_pin(gpio_pin_t pin);

void app_set_pwm_duty(gpio_pin_t pin, uint16_t duty);

void app_set_pwm_fade(gpio_pin_t pin, uint16_t duty, uint16_t fade_time_ms);

uint16_t app_get_pwm_duty(gpio_pin_t pin);

bool app_is_pwm_fading(gpio_pin_t pin);

#endif
#endif
