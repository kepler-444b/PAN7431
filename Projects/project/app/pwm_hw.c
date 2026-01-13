#include "pwm_hw.h"
#include "py32f0xx.h"
#include <string.h>
#include <math.h>
#include "py32f0xx_hal_tim.h"
#include "../bsp/bsp_uart.h"
// #include "../app/curve_table.h"
#include "../bsp/bsp_timer.h"

#define PWM_FREQ_HZ        20000UL    // PWM 频率 (Hz)
#define TIM_CLK_HZ         48000000UL // 系统时钟 48 MHz
#define PWM_FADE_CH_MAX    4          // 最大4通道
#define FADE_TIMER_MS      1          // 渐变更新间隔 (ms)
#define PWM_PERIOD         2400       // PWM 分辨率

#define PWM_DEAD_ZONE_DUTY 5 // 死区占空比

TIM_HandleTypeDef Timer1;
TIM_OC_InitTypeDef pwmConfig;

// 使用整数管理渐变索引
typedef struct {
    pwm_hw_pins pin;
    uint16_t start_idx;    // 起始索引
    uint16_t target_idx;   // 目标索引
    uint16_t current_idx;  // 当前索引
    uint16_t fade_counter; // 当前步数
    uint16_t fade_steps;   // 总步数
    uint8_t dither_acc;    // 抖动累加器
    bool active;
} pwm_fade_ctrl_t;

static pwm_fade_ctrl_t fade_channels[PWM_FADE_CH_MAX];
static bool Timer1_initialized = false;

static void timer1_init(uint32_t channel);
static void pwm_hw_fade_update(void *arg);
static uint32_t pwm_hw_gamma(uint16_t idx, uint32_t period);

void pwm_hw_init(void)
{
    memset(fade_channels, 0, sizeof(fade_channels));
    bsp_start_timer(9, 1, pwm_hw_fade_update, NULL, TMR_AUTO_MODE);
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
    APP_PRINTF("1\n");
    if (duration_ms > 5000) duration_ms = 5000;
    if (duration_ms == 0) duration_ms = 1;

    pwm_fade_ctrl_t *ch = NULL;
    for (int i = 0; i < PWM_FADE_CH_MAX; i++) { // 找到这个 pin 对应的通道
        if (fade_channels[i].pin == pin) {
            ch = &fade_channels[i];
            break;
        }
        // 若找不到,则使用一个空闲通道
        if (fade_channels[i].pin == 0 && !ch) ch = &fade_channels[i];
    }

    if (!ch) return;

    ch->pin          = pin;
    ch->start_idx    = ch->current_idx;
    ch->target_idx   = (target_duty >= PWM_PERIOD) ? PWM_PERIOD - 1 : target_duty;
    ch->fade_steps   = duration_ms / FADE_TIMER_MS;
    ch->fade_counter = 0;
    ch->active       = (ch->start_idx != ch->target_idx);

    if (!ch->active) { // 保底逻辑 如果目标亮度和当前亮度一样(不需要渐变),也要立即更新一次硬件寄存器
        pwm_hw_set_duty(pin, (uint16_t)(pwm_hw_gamma(ch->target_idx, Timer1.Init.Period) >> 4));
    }
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
        Timer1.Init.Prescaler         = 0;
        Timer1.Init.Period            = (TIM_CLK_HZ / PWM_FREQ_HZ) - 1;
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

        HAL_NVIC_SetPriority(TIM1_BRK_UP_TRG_COM_IRQn, 0, 1);
        HAL_NVIC_EnableIRQ(TIM1_BRK_UP_TRG_COM_IRQn);
    }

    HAL_TIM_PWM_ConfigChannel(&Timer1, &pwmConfig, channel);
    HAL_TIM_PWM_Start(&Timer1, channel);
    APP_PRINTF("timer1_init\n");
}

static void pwm_hw_fade_update(void *arg)
{
    for (int i = 0; i < PWM_FADE_CH_MAX; i++) {
        pwm_fade_ctrl_t *ch = &fade_channels[i];

        if (!ch->active) continue; // 跳过未激活的 PWM 通道

        ch->fade_counter++; // 增加步数
        if (ch->fade_counter >= ch->fade_steps) {

            ch->current_idx = ch->target_idx;
            ch->active      = false; // 渐变完成
            APP_PRINTF("2\n");
        } else {
            int32_t delta   = (int32_t)ch->target_idx - (int32_t)ch->start_idx;
            ch->current_idx = ch->start_idx + (delta * ch->fade_counter) / ch->fade_steps;
        }

        uint32_t duty_fixed = pwm_hw_gamma(ch->current_idx, Timer1.Init.Period);

        uint16_t duty_int = (uint16_t)(duty_fixed >> 4);  // 整数
        uint8_t duty_frac = (uint8_t)(duty_fixed & 0x0F); // 小数

        ch->dither_acc += duty_frac;
        if (ch->dither_acc >= 16) {
            ch->dither_acc -= 16;
            duty_int += 1;
        }
        pwm_hw_set_duty(ch->pin, duty_int);
    }
}

static uint32_t pwm_hw_gamma(uint16_t idx, uint32_t period)
{
    if (idx == 0) return 0;

    uint32_t max_idx = PWM_PERIOD - 1;
    uint32_t range   = period - PWM_DEAD_ZONE_DUTY;

    uint64_t curve_val  = (uint64_t)idx * idx * range;
    uint32_t duty_fixed = (uint32_t)((curve_val << 4) / ((uint32_t)max_idx * max_idx));

    return duty_fixed + (PWM_DEAD_ZONE_DUTY << 4);
}
