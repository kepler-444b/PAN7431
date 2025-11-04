#include "light_driver_ct.h"
#include "../bsp/bsp_pcb.h"
#include "../bsp/bsp_uart.h"
#include "../app/pwm_hw.h"
#include "../app/eventbus.h"
#include "../app/pwm.h"
#include "../bsp/bsp_timer.h"
#if defined LIGHT_DRIVER_CT

static bool breath_up = true;

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

    // app_set_pwm_hw_fade(PWM_PA8, 500, 5000);
    // app_set_pwm_hw_fade(PWM_PB3, 500, 5000);
    bsp_start_timer(10, 5000, test1, NULL, TMR_AUTO_MODE);
}

static void light_devier_ct_event_handler(event_type_e event, void *params)
{
    switch (event) {
        case EVENT_LIGHT_RX:
            /* code */
            break;

        default:
            break;
    }
}

#endif