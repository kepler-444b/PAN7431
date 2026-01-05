#ifndef _BSP_PCB_H_
#define _BSP_PCB_H_
#include "../app/gpio.h"
#include "../device/device_manager.h"

#define PANEL_VOL_RANGE_DEF          \
    [0] = {.vol_range = {0, 10}},    \
    [1] = {.vol_range = {30, 40}},   \
    [2] = {.vol_range = {85, 95}},   \
    [3] = {.vol_range = {145, 155}}, \
    [4] = {.vol_range = {170, 180}}, \
    [5] = {.vol_range = {195, 205}}

#define RELAY_GPIO_MAP_DEF {PB1, PA5, PA6, PB0}             // 继电器 GPIO 映射
#define LED_W_GPIO_MAP_DEF {PA3, PA2, PB5, PB4, PA10, PA11} // white led
#define LED_Y_GPIO_MAP_DEF PA8                              // yellow led

void bsp_panel_init(void);
void bsp_setter_init(void);
void bsp_repeater_init(void);
void bsp_light_driver_ct_init(void);

#endif
