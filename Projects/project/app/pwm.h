#ifndef _PWM_H_
#define _PWM_H_
#include <stdbool.h>
#include "gpio.h"

// PWM最大通道数
#define PWM_MAX_CHANNELS 8

// 初始化PWM模块
void app_pwm_init(void);

// 添加PWM引脚(必须在初始化后调用)
bool app_pwm_add_pin(gpio_pin_t pin);

// 设置指定引脚的PWM占空比(0-500)，立即切换
void app_set_pwm_duty(gpio_pin_t pin, uint16_t duty);

// 设置指定引脚的PWM渐变到指定占空比(0-500)，并指定渐变时间(ms)
void app_set_pwm_fade(gpio_pin_t pin, uint16_t duty, uint16_t fade_time_ms);

// 获取指定引脚的当前PWM占空比
uint16_t app_get_pwm_duty(gpio_pin_t pin);

// 检查指定引脚是否正在渐变中
bool app_is_pwm_fading(gpio_pin_t pin);

void app_pwm_fade_poll(void);


#endif
