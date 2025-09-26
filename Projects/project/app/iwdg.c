#include "py32f0xx.h"
#include "iwdg.h"
#include "../bsp/bsp_timer.h"
#include "../bsp/bsp_uart.h"
#include "py32f0xx_hal_iwdg.h"

static void iwdg_feed_dog(void *arg);

static IWDG_HandleTypeDef IwdgHandle = {0};
void app_iwdg_init(void)
{
    IwdgHandle.Instance       = IWDG;
    IwdgHandle.Init.Prescaler = IWDG_PRESCALER_32;
    IwdgHandle.Init.Reload    = (1024);
    if (HAL_IWDG_Init(&IwdgHandle) != HAL_OK) {
        APP_ERROR("");
    }

    bsp_start_timer(0, 500, iwdg_feed_dog, NULL, TMR_AUTO_MODE);
}

static void iwdg_feed_dog(void *arg)
{
    HAL_IWDG_Refresh(&IwdgHandle);
}
