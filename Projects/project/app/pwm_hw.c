#include "pwm_hw.h"
#include "py32f0xx.h"
#include <string.h>
#include "py32f0xx_hal_tim.h"
#include "../bsp/bsp_uart.h"
#include "../bsp/bsp_timer.h"
#include "../app/curve_table.h"

#define PWM_FREQ_HZ     1000       // 期望 PWM 频率 (Hz)
#define TIM_CLK_HZ      48000000UL // 系统时钟 48 MHz
#define PWM_FADE_CH_MAX 4          // 最大4通道
#define FADE_TIMER_MS   1          // 渐变更新间隔 (ms)

#define ABS(x)          ((x) < 0 ? -(x) : (x))

TIM_HandleTypeDef Timer1;
TIM_OC_InitTypeDef pwmConfig;

// 使用整数固定点 16.16 管理渐变索引
typedef struct {
    pwm_hw_pins pin;
    uint32_t start_idx_fp;  // 固定点 16.16
    uint32_t target_idx_fp; // 固定点 16.16
    uint32_t current_idx_fp;
    uint32_t step_inc_fp; // 每步增量
    uint16_t steps_total;
    uint16_t steps_done;
    bool active;
} pwm_fade_ctrl_t;

static pwm_fade_ctrl_t fade_channels[PWM_FADE_CH_MAX];

static bool Timer1_initialized = false;

static void timer1_init(uint32_t channel);

void pwm_hw_init(void)
{
    memset(fade_channels, 0, sizeof(fade_channels));
}

bool app_pwm_hw_add_pin(pwm_hw_pins pin)
{
    switch (pin) {
        case PWM_PB3:
            timer1_init(TIM_CHANNEL_2);
            break;
        case PWM_PA8:
            timer1_init(TIM_CHANNEL_1);
            break;
        default:
            return false;
    }
    return true;
}

void app_set_pwm_hw_fade(pwm_hw_pins pin, uint16_t target_duty, uint16_t duration_ms)
{
    if (target_duty >= FADE_TABLE_SIZE) target_duty = FADE_TABLE_SIZE - 1;
    if (duration_ms > 5000) duration_ms = 5000;
    if (duration_ms == 0) duration_ms = 1;

    pwm_fade_ctrl_t *ch = NULL;
    for (int i = 0; i < PWM_FADE_CH_MAX; i++) {
        if (!fade_channels[i].active) {
            ch = &fade_channels[i];
            break;
        }
    }
    if (!ch) return;

    // 获取当前占空比索引
    uint16_t current_idx = 0;
    switch (pin) {
        case PWM_PA8:
            current_idx = __HAL_TIM_GET_COMPARE(&Timer1, TIM_CHANNEL_1) * (FADE_TABLE_SIZE - 1) / Timer1.Init.Period;
            break;
        case PWM_PB3:
            current_idx = __HAL_TIM_GET_COMPARE(&Timer1, TIM_CHANNEL_2) * (FADE_TABLE_SIZE - 1) / Timer1.Init.Period;
            break;
        default:
            return;
    }
    ch->pin            = pin;
    ch->start_idx_fp   = current_idx << 16;
    ch->target_idx_fp  = target_duty << 16;
    ch->current_idx_fp = ch->start_idx_fp;
    ch->steps_total    = (duration_ms + FADE_TIMER_MS - 1) / FADE_TIMER_MS;
    ch->steps_done     = 0;
    ch->active         = (ch->steps_total > 0 && ch->start_idx_fp != ch->target_idx_fp);
    ch->step_inc_fp    = (ch->target_idx_fp - ch->start_idx_fp) / ch->steps_total;
}

void pwm_hw_set_duty(pwm_hw_pins pins, uint16_t duty_val)
{
    if (duty_val > Timer1.Init.Period) duty_val = Timer1.Init.Period;
    switch (pins) {
        case PWM_PA8:
            __HAL_TIM_SET_COMPARE(&Timer1, TIM_CHANNEL_1, duty_val);
            break;
        case PWM_PB3:
            __HAL_TIM_SET_COMPARE(&Timer1, TIM_CHANNEL_2, duty_val);
            break;
        default:
            break;
    }
}

static void timer1_init(uint32_t channel)
{
    if (!Timer1_initialized) {
        __HAL_RCC_TIM1_CLK_ENABLE();
        Timer1.Instance               = TIM1;
        Timer1.Init.Prescaler         = (TIM_CLK_HZ / 1000000UL) - 1; // 1MHz
        Timer1.Init.Period            = (1000000UL / PWM_FREQ_HZ) - 1;
        Timer1.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
        Timer1.Init.CounterMode       = TIM_COUNTERMODE_UP;
        Timer1.Init.RepetitionCounter = 0;
        Timer1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
        HAL_TIM_PWM_Init(&Timer1);

        pwmConfig.OCMode       = TIM_OCMODE_PWM1;
        pwmConfig.OCPolarity   = TIM_OCPOLARITY_HIGH;
        pwmConfig.OCFastMode   = TIM_OCFAST_DISABLE;
        pwmConfig.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
        pwmConfig.OCNIdleState = TIM_OCNIDLESTATE_RESET;
        pwmConfig.OCIdleState  = TIM_OCIDLESTATE_RESET;
        pwmConfig.Pulse        = 0;
        Timer1_initialized     = true;
        HAL_TIM_Base_Start_IT(&Timer1); // 启动 TIM Base 并使能 Update 中断
        HAL_NVIC_SetPriority(TIM1_BRK_UP_TRG_COM_IRQn, 2, 0);
        HAL_NVIC_EnableIRQ(TIM1_BRK_UP_TRG_COM_IRQn);
    }

    HAL_TIM_PWM_ConfigChannel(&Timer1, &pwmConfig, channel);
    HAL_TIM_PWM_Start(&Timer1, channel);
    APP_PRINTF("timer1_init\n");
}

static void pwm_hw_fade_update(void)
{
    for (int i = 0; i < PWM_FADE_CH_MAX; i++) {
        pwm_fade_ctrl_t *ch = &fade_channels[i];
        if (!ch->active) continue;

        if (ch->steps_done >= ch->steps_total) {
            uint16_t idx = ch->target_idx_fp >> 16;
            if (idx >= FADE_TABLE_SIZE) idx = FADE_TABLE_SIZE - 1;
            uint32_t duty = fade_table[idx] * Timer1.Init.Period / 1000;
            pwm_hw_set_duty(ch->pin, duty);
            ch->active = false;
            continue;
        }

        ch->current_idx_fp += ch->step_inc_fp;
        uint16_t idx = ch->current_idx_fp >> 16;
        if (idx >= FADE_TABLE_SIZE) idx = FADE_TABLE_SIZE - 1;

        uint32_t duty = fade_table[idx] * Timer1.Init.Period / 1000;
        pwm_hw_set_duty(ch->pin, duty);
        ch->steps_done++;
    }
}
void TIM1_BRK_UP_TRG_COM_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&Timer1);
    pwm_hw_fade_update();
}
