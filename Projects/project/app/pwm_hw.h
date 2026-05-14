#ifndef _PWM_HW_H_
#define _PWM_HW_H_
#include <stdint.h>
#include <stdbool.h>
#include "gpio.h"

typedef enum {

    PWM_PB3 = 1, // Y1(双色温灯驱)
    PWM_PA8,     // Y2(A11背光灯,双色温灯驱)
    PWM_PA10,    // Y3
    PWM_PA11,    // Y4
    PWM_PB4,     // Y5
    PWM_PB5,     // Y6

} pwm_hw_pins;

void pwm_hw_init(void);
void pwm_hw_set_duty(pwm_hw_pins pins, uint16_t duty_val);
bool app_pwm_hw_add_pin(pwm_hw_pins pin);
void app_set_pwm_hw_fade(pwm_hw_pins pin, uint16_t target_duty, uint16_t duration_ms, uint16_t dead_zone, uint8_t lum_curve);

/*

占空比计算公式
gamma2.0 = idx^2 * P / (PWM_PERIOD - 1)^2

// 考虑死区偏移
gamma2.0 = DZ + idx^2 * (P - DZ) / (PWM_PERIOD - 1)^2

DZ:        死区占空比
idx:       当前亮度索引值
PWM_PERIOD:亮度最大索引值,即满占空比2400
P:         硬件PWM周期,2400

*/
#endif