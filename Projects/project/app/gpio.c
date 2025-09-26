#include "gpio.h"
#include <stdint.h>
#include <stdbool.h>

const char *app_get_gpio_name(gpio_pin_t gpio)
{
    // 检查GPIO是否有效
    if (!GPIO_IS_VALID(gpio)) {
        return "DEF";
    }
    // 检查端口
    if (gpio.port == GPIOA) {

        switch (gpio.pin) {
            case GPIO_PIN_2:
                return "PA2";
            case GPIO_PIN_3:
                return "PA3";
            case GPIO_PIN_4:
                return "PA4";
            case GPIO_PIN_5:
                return "PA5";
            case GPIO_PIN_6:
                return "PA6";
            case GPIO_PIN_7:
                return "PA7";
            case GPIO_PIN_8:
                return "PA8";
            case GPIO_PIN_9:
                return "PA9";
            case GPIO_PIN_10:
                return "PA10";
            case GPIO_PIN_11:
                return "PA11";
            case GPIO_PIN_12:
                return "PA12";
            case GPIO_PIN_13:
                return "PA13";
            case GPIO_PIN_14:
                return "PA14";
            case GPIO_PIN_15:
                return "PA15";
            default:
                return "PA?";
        }
    } else if (gpio.port == GPIOB) {
        switch (gpio.pin) {
            case GPIO_PIN_0:
                return "PB0";
            case GPIO_PIN_1:
                return "PB1";
            case GPIO_PIN_2:
                return "PB2";
            case GPIO_PIN_3:
                return "PB3";
            case GPIO_PIN_4:
                return "PB4";
            case GPIO_PIN_5:
                return "PB5";
            case GPIO_PIN_6:
                return "PB6";
            case GPIO_PIN_7:
                return "PB7";
            case GPIO_PIN_8:
                return "PB8";
            default:
                return "PB?";
        }
    }
    return "UNKNOWN";
}

bool app_gpio_equal(gpio_pin_t a, gpio_pin_t b)
{
    return (a.port == b.port && a.pin == b.pin);
}
