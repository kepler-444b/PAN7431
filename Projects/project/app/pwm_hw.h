#ifndef _PWM_HW_H_
#define _PWM_HW_H_
#include <stdint.h>
#include <stdbool.h>
#include "gpio.h"

typedef enum {
    PWM_PA8,
    PWM_PB3,
} pwm_hw_pins;

void pwm_hw_init(void);
void pwm_hw_set_duty(pwm_hw_pins pins, uint16_t duty_val);
bool app_pwm_hw_add_pin(pwm_hw_pins pin);
void app_set_pwm_hw_fade(pwm_hw_pins pin, uint16_t target_duty, uint16_t duration_ms);

#endif