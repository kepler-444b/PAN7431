#include "py32f0xx.h"
#include "py32f0xx_hal_tim.h"
#include "time.h"
#include "bsp_timer.h"
#include "../bsp/bsp_uart.h"

#define MAX_TMR_COUNT      32
#define TMR_BASE_HZ        1000

#define SET_TMR_ACTIVE(id) (s_tmr_active_bitmap |= (1U << (id)))
#define CLR_TMR_ACTIVE(id) (s_tmr_active_bitmap &= ~(1U << (id)))
#define IS_TMR_ACTIVE(id)  ((s_tmr_active_bitmap >> (id)) & 1U)

static SOFT_TMR s_t_tmr[MAX_TMR_COUNT] = {0}; // 定于软件定时器结构体变量

static __IO int32_t g_i_run_time = 0; // 全局运行时间,单位1ms 最长可以表示 24.85天,如果你的产品连续运行时间超过这个数,则必须考虑溢出问题

static __IO uint32_t s_tmr_active_bitmap = 0;
TIM_HandleTypeDef sTimxHandle            = {0};

static void __bsp_timer_init(void)
{
    uint32_t uwPrescalerValue;
    uwPrescalerValue = (uint32_t)(HAL_RCC_GetPCLK1Freq() / TMR_BASE_HZ) - 1;
    __HAL_RCC_TIM14_CLK_ENABLE();

    sTimxHandle.Instance               = TIM14;
    sTimxHandle.Init.Period            = 1;
    sTimxHandle.Init.Prescaler         = uwPrescalerValue;
    sTimxHandle.Init.ClockDivision     = 0;
    sTimxHandle.Init.CounterMode       = TIM_COUNTERMODE_UP;
    sTimxHandle.Init.RepetitionCounter = 0;
    sTimxHandle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(&sTimxHandle) != HAL_OK) {
        APP_ERROR("");
    }
    HAL_NVIC_EnableIRQ(TIM14_IRQn);
    if (HAL_TIM_Base_Start_IT(&sTimxHandle) != HAL_OK) {
        APP_ERROR("");
    }
}

void bsp_timer_init(void)
{
    for (uint8_t i = 0; i < MAX_TMR_COUNT; i++) { // 清零所有的软件定时器
        s_t_tmr[i].Count   = 0;
        s_t_tmr[i].PreLoad = 0;
        s_t_tmr[i].Flag    = 0;
        s_t_tmr[i].Mode    = TMR_ONCE_MODE; // 缺省是1次性工作模式
    }
    __bsp_timer_init();
}

void bsp_start_timer(uint8_t id, uint32_t period, void (*cb)(void *), void *arg, TMR_MODE_E auto_reload)
{
    if (id >= MAX_TMR_COUNT) return;

    __disable_irq();
    s_t_tmr[id].Count    = period;
    s_t_tmr[id].PreLoad  = period;
    s_t_tmr[id].Flag     = 0;
    s_t_tmr[id].Mode     = auto_reload ? TMR_AUTO_MODE : TMR_ONCE_MODE;
    s_t_tmr[id].Callback = cb;
    s_t_tmr[id].arg      = arg;
    SET_TMR_ACTIVE(id);
    __enable_irq();
}

void bsp_stop_timer(uint8_t _id)
{
    if (_id >= MAX_TMR_COUNT) return;
    __disable_irq();
    s_t_tmr[_id].Count    = 0;
    s_t_tmr[_id].PreLoad  = 0;
    s_t_tmr[_id].Flag     = 0;
    s_t_tmr[_id].Mode     = TMR_ONCE_MODE;
    s_t_tmr[_id].Callback = NULL;
    s_t_tmr[_id].arg      = NULL;
    CLR_TMR_ACTIVE(_id);
    __enable_irq();
}

void bsp_timer_poll(void)
{
    if (s_tmr_active_bitmap == 0) return;

    for (uint8_t i = 0; i < MAX_TMR_COUNT; i++) {
        if (IS_TMR_ACTIVE(i) && s_t_tmr[i].Flag) {
            s_t_tmr[i].Flag = 0;
            if (s_t_tmr[i].Callback) {
                s_t_tmr[i].Callback(s_t_tmr[i].arg);
            }
        }
    }
}

int32_t bsp_get_run_time(void)
{
    int32_t runtime;
    __disable_irq();
    runtime = g_i_run_time; // 这个变量在Systick中断中被改写,因此需要关中断进行保护
    __enable_irq();
    return runtime;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    SOFT_TMR *tmr;
    for (uint8_t i = 0; i < MAX_TMR_COUNT; i++) {
        if (!IS_TMR_ACTIVE(i)) continue;

        tmr = &s_t_tmr[i];
        if (tmr->Count == 0) continue;

        if (--tmr->Count > 0) continue;

        tmr->Flag = 1;
        if (tmr->Mode == TMR_AUTO_MODE) {
            tmr->Count = tmr->PreLoad;
        }
    }
    g_i_run_time++;
    if (g_i_run_time == 0x7FFFFFFF) {
        g_i_run_time = 0;
    }
}
