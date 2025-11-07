#include "light_driver_ct.h"
#include "../bsp/bsp_pcb.h"
#include "../bsp/bsp_uart.h"
#include "../app/pwm_hw.h"
#include "../app/eventbus.h"
#include "../app/protocol.h"
#include "../app/pwm.h"
#include "../bsp/bsp_timer.h"
#include "../app/base.h"

#if defined LIGHT_DRIVER_CT
// #define LED_LINE

static bool breath_up = true;
static bool led_status;

static bool p1 = false;
static bool p2 = false;

void test1(void *arg)
{
    APP_PRINTF("test1\n");
    breath_up = !breath_up;
    if (breath_up) {
        app_set_pwm_hw_fade(PWM_PA8, 0, 5000);
        app_set_pwm_hw_fade(PWM_PB3, 0, 5000);
    } else {
        app_set_pwm_hw_fade(PWM_PA8, 1000, 5000);
        app_set_pwm_hw_fade(PWM_PB3, 1000, 5000);
    }
}

static void set_lum(uint8_t lum, uint8_t number);
static void data_cb(panel_frame_t *data);
static void light_devier_ct_event_handler(event_type_e event, void *params);
void light_driver_ct_init(void)
{
    APP_PRINTF("light_driver_ct_init");
    bsp_light_driver_ct_init();
    pwm_hw_init();
    app_eventbus_init();
    app_eventbus_subscribe(light_devier_ct_event_handler);

    app_pwm_hw_add_pin(PWM_PA8);
    app_pwm_hw_add_pin(PWM_PB3);
#if 0
    bsp_start_timer(10, 5000, test1, NULL, TMR_AUTO_MODE);
#endif
}

static void light_devier_ct_event_handler(event_type_e event, void *params)
{
    switch (event) {
        case EVENT_LIGHT_RX: {
            panel_frame_t *my_panel_frame = (panel_frame_t *)params;
            data_cb(my_panel_frame);

        } break;
        case EVENT_LED_BLINK: {
            led_status ^= 1;
            APP_SET_GPIO(PA4, led_status);
        } break;
        default:
            break;
    }
}

static void data_cb(panel_frame_t *data)
{
    uint8_t frame_head  = data->data[0];
    uint8_t cmd         = data->data[1];
    uint8_t value       = data->data[2];
    // uint8_t status      = data->data[3];
    uint8_t scene_group = data->data[7];
    APP_PRINTF_BUF("data", data->data, data->length);
    if (frame_head == PANEL_HEAD && cmd == DIMMING_3) { // 调光
        set_lum(value, 3);
    } else if (frame_head == PANEL_HEAD && cmd == SCENE_MODE) { // 场景模式
        switch (scene_group) {
            case 0x01: { // 明亮模式
                p1 = true;
                p2 = true;
                if (value == true) {
                    set_lum(10, 3);
                } else if (value == false) {
                    set_lum(0, 3);
                }
            } break;
            case 0x04: { // 阅读模式
#if defined LED_LINE
                p1 = true;
                p2 = true;
                set_lum(0, 3);
                p1 = false;
                p2 = false;
#else
                p1 = true;
                p2 = true;
                if (value == true) {
                    set_lum(3, 1);
                } else if (value == false) {
                    set_lum(0, 1);
                }
                set_lum(0, 2);
                p2 = false;
#endif
            } break;
            case 0x08: { // 温馨模式
#if defined LED_LINE
                p1 = true;
                p2 = true;
                if (value == true) {
                    set_lum(5, 3);
                } else if (value == false) {
                    set_lum(0, 3);
                }
#else
                p1 = true;
                p2 = true;
                set_lum(0, 3);
                p1 = false;
                p2 = false;
#endif
            } break;
            default:
                break;
        }
    } else if (frame_head == PANEL_HEAD && cmd == ALL_CLOSE) {

        p1 = true;
        p2 = true;
        set_lum(0, 3);
        // p1 = false;
        // p2 = false;
    }
}

static void set_lum(uint8_t lum, uint8_t number)
{
    lum > 10 ? lum = 10 : lum;
    switch (number) {
        case 1: {
            if (p1 == true) {
                app_set_pwm_hw_fade(PWM_PA8, lum * 100, 2000);
            }
        } break;
        case 2: {
            if (p2 == true) {
                app_set_pwm_hw_fade(PWM_PB3, lum * 100, 2000);
            }
        } break;
        case 3: {
            if (p1 == true) {
                app_set_pwm_hw_fade(PWM_PA8, lum * 100, 2000);
            }
            if (p2 == true) {
                app_set_pwm_hw_fade(PWM_PB3, lum * 100, 2000);
            }
        } break;
        default:
            break;
    }
}
#endif