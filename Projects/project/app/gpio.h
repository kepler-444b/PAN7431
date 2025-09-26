#ifndef _GPIO_H_
#define _GPIO_H_
#include <stdbool.h>
#include <stdint.h>
#include "py32f0xx_hal.h"

// 检查GPIO是否有效
#define GPIO_IS_VALID(obj) ((obj).pin || (obj).port)

// 设置GPIO
#define APP_SET_GPIO(obj, status)                   \
    do {                                            \
        if (status) {                               \
            (obj.port)->BSRR = (uint32_t)(obj.pin); \
        } else {                                    \
            (obj.port)->BRR = (uint32_t)(obj.pin);  \
        }                                           \
    } while (0)

typedef struct {
    GPIO_TypeDef *port;
    uint32_t pin;
} gpio_pin_t;

const char *app_get_gpio_name(gpio_pin_t gpio);

// 判断两个 gpio 引脚是否相同
bool app_gpio_equal(gpio_pin_t a, gpio_pin_t b);

// 通用GPIO宏定义
#define DEFINE_GPIO(port, pin) ((gpio_pin_t){port, pin})

// 默认无效引脚
#define DEF  DEFINE_GPIO(0, 0)
#define PA2  DEFINE_GPIO(GPIOA, GPIO_PIN_2)
#define PA3  DEFINE_GPIO(GPIOA, GPIO_PIN_3)
#define PA4  DEFINE_GPIO(GPIOA, GPIO_PIN_4)
#define PA5  DEFINE_GPIO(GPIOA, GPIO_PIN_5)
#define PA6  DEFINE_GPIO(GPIOA, GPIO_PIN_6)
#define PA7  DEFINE_GPIO(GPIOA, GPIO_PIN_7)
#define PA8  DEFINE_GPIO(GPIOA, GPIO_PIN_8)
#define PA9  DEFINE_GPIO(GPIOA, GPIO_PIN_9)
#define PA10 DEFINE_GPIO(GPIOA, GPIO_PIN_10)
#define PA11 DEFINE_GPIO(GPIOA, GPIO_PIN_11)
#define PA12 DEFINE_GPIO(GPIOA, GPIO_PIN_12)
#define PA13 DEFINE_GPIO(GPIOA, GPIO_PIN_13)
#define PA14 DEFINE_GPIO(GPIOA, GPIO_PIN_14)
#define PA15 DEFINE_GPIO(GPIOA, GPIO_PIN_15)

#define PB0  DEFINE_GPIO(GPIOB, GPIO_PIN_0)
#define PB1  DEFINE_GPIO(GPIOB, GPIO_PIN_1)
#define PB2  DEFINE_GPIO(GPIOB, GPIO_PIN_2)
#define PB3  DEFINE_GPIO(GPIOB, GPIO_PIN_3)
#define PB4  DEFINE_GPIO(GPIOB, GPIO_PIN_4)
#define PB5  DEFINE_GPIO(GPIOB, GPIO_PIN_5)
#define PB6  DEFINE_GPIO(GPIOB, GPIO_PIN_6)
#define PB7  DEFINE_GPIO(GPIOB, GPIO_PIN_7)
#define PB8  DEFINE_GPIO(GPIOB, GPIO_PIN_8)

#endif
