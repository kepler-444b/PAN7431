#include "light_driver_ct.h"
#include "../bsp/bsp_pcb.h"
#include "../bsp/bsp_uart.h"
#include "../app/pwm_hw.h"
#include "../app/pwm.h"
#if defined LIGHT_DRIVER_CT

void light_driver_ct_init(void)
{
    APP_PRINTF("light_driver_ct_init");
    bsp_light_driver_ct_init();
    pwm_hw_init();
    // app_pwm_init();

    // app_pwm_add_pin(PA8);
    // app_pwm_add_pin(PB3);

    // app_set_pwm_fade(PA8, 1000, 1000);
    // app_set_pwm_fade(PB3, 1000, 1000);

    app_pwm_hw_add_pin(PWM_PA8);
    app_pwm_hw_add_pin(PWM_PB3);

    // pwm_hw_set_duty(PWM_PB3, 1000);
    // pwm_hw_set_duty(PWM_PA8, 1000);

    app_set_pwm_hw_fade(PWM_PA8, 1000, 5000);
    app_set_pwm_hw_fade(PWM_PB3, 1000, 5000);
    //         APP_SET_GPIO(PA8, true);
    //     APP_SET_GPIO(PB3, true);
}

#endif