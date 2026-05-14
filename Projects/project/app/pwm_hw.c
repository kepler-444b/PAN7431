#include "pwm_hw.h"
#include "py32f0xx.h"
#include <string.h>
#include <math.h>
#include "py32f0xx_hal_tim.h"
#include "../bsp/bsp_uart.h"
#include "../bsp/bsp_timer.h"
#include "../device/device_manager.h"

// #define PWM_FREQ_HZ     20000UL // PWM 频率 (Hz)
#define PWM_FREQ_HZ     1000UL // PWM 频率 (Hz)

#define TIM_CLK_HZ      48000000UL // 系统时钟 48 MHz
#define PWM_FADE_CH_MAX 6          // 最大6通道
#define FADE_TIMER_MS   1          // 渐变更新间隔 (ms)
#define PWM_PERIOD      2400       // PWM 分辨率

TIM_HandleTypeDef Timer1;
TIM_HandleTypeDef Timer3;
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
    uint16_t dead_zone;    // PWM 死区
    uint8_t lum_curve;     // 调光曲线

    bool active;
} pwm_fade_ctrl_t;

// 函数声明
static void timer1_init(uint32_t channel);
static void timer3_init(uint32_t channel);

static void pwm_hw_fade_update(void *arg);
static uint32_t pwm_hw_gamma(uint16_t idx, uint32_t period, uint16_t dead_zone, uint8_t lum_curve);
static uint32_t pwm_hw_get_dead_duty(uint16_t dead_zone_lum, uint32_t period, uint8_t lum_curve);

// 全局变量
static pwm_fade_ctrl_t fade_channels[PWM_FADE_CH_MAX];
static bool Timer1_initialized = false;
static bool Timer3_initialized = false;

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
        case PWM_PA10:
            timer1_init(TIM_CHANNEL_3);
            break;
        case PWM_PA11:
            timer1_init(TIM_CHANNEL_4);
            break;
        case PWM_PB4:
            timer3_init(TIM_CHANNEL_1);
            break;
        case PWM_PB5:
            timer3_init(TIM_CHANNEL_2);
            break;
        default:
            return false;
    }
    return true;
}

void app_set_pwm_hw_fade(pwm_hw_pins pin, uint16_t target_duty, uint16_t duration_ms, uint16_t dead_zone, uint8_t lum_curve)
{
#if 1
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
    ch->lum_curve    = lum_curve;
    // ch->dead_zone    = dead_zone;

    // ch->dead_zone = pwm_hw_gamma(ch->target_idx, Timer1.Init.Period, dead_zone, lum_curve); // 根据死区亮度,设置死区占空比

    ch->dead_zone = pwm_hw_get_dead_duty(dead_zone, Timer1.Init.Period, lum_curve); // 根据死区亮度,设置死区占空比
    // APP_PRINTF("dead_zone:%d\n", ch->dead_zone);

    if (!ch->active) { // 保底逻辑 如果目标亮度和当前亮度一样(不需要渐变),也要立即更新一次硬件寄存器
        pwm_hw_set_duty(pin, (uint16_t)(pwm_hw_gamma(ch->target_idx, Timer1.Init.Period, ch->dead_zone, ch->lum_curve) >> 4));
    }
#endif
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
        case PWM_PA10:
            __HAL_TIM_SET_COMPARE(&Timer1, TIM_CHANNEL_3, duty_val);
            break;
        case PWM_PA11:
            __HAL_TIM_SET_COMPARE(&Timer1, TIM_CHANNEL_4, duty_val);
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
        APP_PRINTF("timer1_init\n");
    }

    HAL_TIM_PWM_ConfigChannel(&Timer1, &pwmConfig, channel);
    HAL_TIM_PWM_Start(&Timer1, channel);
}

static void timer3_init(uint32_t channel)
{
    if (!Timer3_initialized) {
        __HAL_RCC_TIM3_CLK_ENABLE();
        Timer3.Instance               = TIM3;
        Timer3.Init.Prescaler         = 0;
        Timer3.Init.Period            = (TIM_CLK_HZ / PWM_FREQ_HZ) - 1;
        Timer3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
        Timer3.Init.CounterMode       = TIM_COUNTERMODE_UP;
        Timer3.Init.RepetitionCounter = 0;
        Timer3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
        HAL_TIM_PWM_Init(&Timer3);

        pwmConfig.OCMode       = TIM_OCMODE_PWM1;
        pwmConfig.OCPolarity   = TIM_OCPOLARITY_HIGH;
        pwmConfig.OCFastMode   = TIM_OCFAST_DISABLE;
        pwmConfig.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
        pwmConfig.OCNIdleState = TIM_OCNIDLESTATE_RESET;
        pwmConfig.OCIdleState  = TIM_OCIDLESTATE_RESET;
        pwmConfig.Pulse        = 0;
        Timer3_initialized     = true;
        APP_PRINTF("timer3_init\n");
    }
    HAL_TIM_PWM_ConfigChannel(&Timer3, &pwmConfig, channel);
    HAL_TIM_PWM_Start(&Timer3, channel);
}

static void pwm_hw_fade_update(void *arg)
{
    for (int i = 0; i < PWM_FADE_CH_MAX; i++) {
        pwm_fade_ctrl_t *ch = &fade_channels[i];

        if (!ch->active) {
            continue; // 跳过未激活的 PWM 通道
        }

        ch->fade_counter++; // 增加步数
        if (ch->fade_counter >= ch->fade_steps) {

            ch->current_idx = ch->target_idx;
            ch->active      = false; // 渐变完成
        } else {
            int32_t delta   = (int32_t)ch->target_idx - (int32_t)ch->start_idx;
            ch->current_idx = ch->start_idx + (delta * ch->fade_counter) / ch->fade_steps;
        }

        uint32_t duty_fixed = pwm_hw_gamma(ch->current_idx, Timer1.Init.Period, ch->dead_zone, ch->lum_curve);

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

static uint32_t pwm_hw_gamma(uint16_t idx, uint32_t period, uint16_t dead_zone, uint8_t lum_curve)
{
    if (idx == 0) return 0;

    uint32_t max_idx = PWM_PERIOD - 1;
    uint32_t range   = period - dead_zone;
    uint64_t curve_val;

    switch (lum_curve) {
        case 1: // gamma 2.0
            curve_val = (uint64_t)idx * idx * range;
            break;
        default:
            curve_val = (uint64_t)idx * idx * range;
            break;
    }
    uint32_t duty_fixed = (uint32_t)((curve_val << 4) / ((uint32_t)max_idx * max_idx));

    return duty_fixed + (dead_zone << 4);
}

// 根据死区亮度值,获取死区占空比
static uint32_t pwm_hw_get_dead_duty(uint16_t dead_zone_lum, uint32_t period, uint8_t lum_curve)
{
    if (dead_zone_lum == 0) {
        return 0;
    }

    uint32_t dead_zone;
    switch (lum_curve) {
        case 1: { // gamma 2.0
            dead_zone = dead_zone_lum * dead_zone_lum * period / (100 * 100);
        } break;
        default:
            dead_zone = dead_zone_lum * dead_zone_lum * period / (100 * 100);
            break;
    }

    return dead_zone;
}
