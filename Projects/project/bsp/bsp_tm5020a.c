#include "bsp_tm5020a.h"
#include "../app/gpio.h"

#define PIN_SDI PB4
#define PIN_CLK PB3
#define PIN_LE  PB5

static uint16_t out_status = 0;

void bsp_tm5020a_update(void);

void bsp_tm5020a_init(void)
{
    APP_SET_GPIO(PIN_LE, true);
    APP_SET_GPIO(PIN_SDI, false);
    APP_SET_GPIO(PIN_CLK, false);
    out_status = 0;
    bsp_tm5020a_update();
}

// Set out_pins
void bsp_tm5020a_set(uint8_t pin, bool status)
{
    if (pin > 15) return;

    if (status)
        out_status |= (1U << pin); // Open this out_pins
    else
        out_status &= ~(1U << pin); // Close this out_pins

    // Write data
    APP_SET_GPIO(PIN_LE, false);
    // delay_us(1);
    for (int8_t i = 15; i >= 0; i--) {
        APP_SET_GPIO(PIN_SDI, (out_status >> i) & 0x01);
        // delay_us(1);
        APP_SET_GPIO(PIN_CLK, true);
        // delay_us(1);
        APP_SET_GPIO(PIN_CLK, false);
        // delay_us(1);
    }
    APP_SET_GPIO(PIN_LE, true);
    // delay_us(1);
}

// Update - Close all out_pins
void bsp_tm5020a_update(void)
{
    APP_SET_GPIO(PIN_LE, false);
    // delay_us(1);

    for (int8_t i = 15; i >= 0; i--) {
        APP_SET_GPIO(PIN_SDI, (out_status >> i) & 0x01);
        // delay_us(1);
        APP_SET_GPIO(PIN_CLK, true);
        // delay_us(1);
        APP_SET_GPIO(PIN_CLK, false);
        // delay_us(1);
    }
    APP_SET_GPIO(PIN_LE, true);
    // delay_us(1);
}
