#include "py32f0xx.h"
#include "py32f0xx_hal_tim.h"
#include "pwm.h"
#include "curve_table.h"
#include "../bsp/bsp_uart.h"
#include "../bsp/bsp_timer.h"
#include "../device/device_manager.h"

#define SYSTEM_CLOCK_FREQ 48000000 // 系统时钟频率(48MHz)
#define TIMER_PERIOD      24       // 25 us 触发一次中断
#define PWM_RESOLUTION    1000     // PWM分辨率(1000)
#define MAX_FADE_TIME_MS  5000     // 最大渐变时间(5秒)
#define FADE_UPDATE_MS    1        // 渐变更新间隔(10ms)
#define PWM_MIN_DUTY      50       // 最低占空比

// 定义 MIN 宏(取较小值)
#define MIN(a, b) ((a) < (b) ? (a) : (b))

// 定义 CLAMP 宏(限制值在 [min_val, max_val] 范围内)
#define CLAMP(x, min, max) ((x) <= (min) ? (min) : ((x) >= (max) ? (max) : (x)))

// PWM控制结构体
typedef struct {
    volatile uint16_t current_duty; // 当前PWM占空比(0-1000)
    volatile uint16_t target_duty;  // 目标PWM占空比(0-1000)
    volatile bool is_fading;        // 是否正在渐变中
    volatile uint16_t fade_counter; // 渐变计数器
    volatile uint16_t fade_steps;   // 渐变总步数
    bool is_active;                 // 是否已激活
    gpio_pin_t gpio;                // 对应的GPIO引脚
    uint16_t start_duty;            // 渐变起始占空比
} pwm_control_t;

static pwm_control_t pwm_channels[PWM_MAX_CHANNELS];  // PWM通道控制数组
volatile static uint8_t active_channel_count = 0;     // 已激活的pwm通道数量
volatile static bool pwm_initialized         = false; // pwm模块是否已经初始化

void app_pwm_fade_timer(void *arg);

// 根据引脚查找对应的PWM控制结构体索引
static int find_pwm_index(gpio_pin_t pin)
{
    for (int i = 0; i < PWM_MAX_CHANNELS; i++) {
        if (pwm_channels[i].is_active &&
            pwm_channels[i].gpio.port == pin.port &&
            pwm_channels[i].gpio.pin == pin.pin) {
            return i;
        }
    }
    return -1;
}
TIM_HandleTypeDef Timer3;

void app_pwm_init(void)
{
    pwm_initialized = true;
    __HAL_RCC_TIM3_CLK_ENABLE();

    Timer3.Instance               = TIM3;
    Timer3.Init.Prescaler         = (SYSTEM_CLOCK_FREQ / 1000000) - 1;
    Timer3.Init.CounterMode       = TIM_COUNTERMODE_UP;
    Timer3.Init.Period            = TIMER_PERIOD;
    Timer3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    Timer3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_Base_Init(&Timer3);

    HAL_NVIC_SetPriority(TIM3_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(TIM3_IRQn);

    HAL_TIM_Base_Start_IT(&Timer3);
    bsp_start_timer(8, 1, app_pwm_fade_timer, NULL, true);
}

void app_pwm_fade_timer(void *arg)
{
    for (int i = 0; i < PWM_MAX_CHANNELS; i++) {
        pwm_control_t *ch = &pwm_channels[i];
        if (!ch->is_active || !ch->is_fading) continue;

        ch->fade_counter++;

        uint16_t progress = ch->fade_counter * FADE_TABLE_SIZE / ch->fade_steps;
        if (progress >= FADE_TABLE_SIZE) progress = FADE_TABLE_SIZE - 1;

        uint16_t curve_value = fade_table[progress];
        int32_t range        = (int32_t)ch->target_duty - ch->start_duty;

        int32_t new_duty = ch->start_duty + (range * curve_value) / FADE_TABLE_SIZE;
        ch->current_duty = CLAMP(new_duty, 0, PWM_RESOLUTION);

        if (ch->fade_counter >= ch->fade_steps) {
            ch->current_duty = ch->target_duty;
            ch->is_fading    = false;
        }
    }
}
// 添加PWM引脚
bool app_pwm_add_pin(gpio_pin_t pin)
{
    if (!pwm_initialized || active_channel_count >= PWM_MAX_CHANNELS) {
        return false;
    }
    // 检查是否已存在
    if (find_pwm_index(pin) >= 0) {
        return false;
    }
    // 找到空闲位置
    for (int i = 0; i < PWM_MAX_CHANNELS; i++) {
        if (!pwm_channels[i].is_active) {
            pwm_channels[i].current_duty = 0;
            pwm_channels[i].target_duty  = 0;
            pwm_channels[i].fade_counter = 0;
            pwm_channels[i].fade_steps   = 0;
            pwm_channels[i].is_fading    = false;
            pwm_channels[i].is_active    = true;
            pwm_channels[i].gpio         = pin;

            APP_SET_GPIO(pin, true); // 初始状态关闭
            active_channel_count++;
            return true;
        }
    }
    return false;
}

void app_set_pwm_duty(gpio_pin_t pin, uint16_t duty)
{
    int index = find_pwm_index(pin);
    if (index < 0) return;

    if (duty > PWM_RESOLUTION) duty = PWM_RESOLUTION;

    // 限制最低占空比
    if (duty > 0 && duty < PWM_MIN_DUTY) {
        duty = PWM_MIN_DUTY;
    }

    pwm_channels[index].current_duty = duty;
    pwm_channels[index].target_duty  = duty;
    pwm_channels[index].is_fading    = false;
}

void app_set_pwm_fade(gpio_pin_t pin, uint16_t duty, uint16_t fade_time_ms)
{
    int index = find_pwm_index(pin);
    if (index < 0) return;

    // 限制占空比范围
    if (duty > PWM_RESOLUTION) duty = PWM_RESOLUTION;
    if (duty > 0 && duty < PWM_MIN_DUTY) duty = PWM_MIN_DUTY;

    if (fade_time_ms == 0) {
        app_set_pwm_duty(pin, duty);
        return;
    }

    fade_time_ms   = (fade_time_ms > MAX_FADE_TIME_MS) ? MAX_FADE_TIME_MS : fade_time_ms;
    uint16_t steps = (fade_time_ms + FADE_UPDATE_MS / 2) / FADE_UPDATE_MS;
    steps          = (steps == 0) ? 1 : steps;

    pwm_channels[index].target_duty  = duty;
    pwm_channels[index].start_duty   = pwm_channels[index].current_duty;
    pwm_channels[index].fade_steps   = steps;
    pwm_channels[index].fade_counter = 0;
    pwm_channels[index].is_fading    = true;

    if (pwm_channels[index].current_duty == duty) {
        pwm_channels[index].is_fading = false;
    }
}

uint16_t app_get_pwm_duty(gpio_pin_t pin)
{
    int index = find_pwm_index(pin);
    if (index < 0) return 0;
    return pwm_channels[index].current_duty;
}

bool app_is_pwm_fading(gpio_pin_t pin)
{
    int index = find_pwm_index(pin);
    if (index < 0) return false;
    return pwm_channels[index].is_fading;
}

void TIM3_IRQHandler(void)
{
    if (!__HAL_TIM_GET_FLAG(&Timer3, TIM_FLAG_UPDATE)) return;
    if (!__HAL_TIM_GET_IT_SOURCE(&Timer3, TIM_IT_UPDATE)) return;
    __HAL_TIM_CLEAR_IT(&Timer3, TIM_IT_UPDATE);

    static uint16_t pwm_counter          = 0;
    static int pwm_acc[PWM_MAX_CHANNELS] = {0}; // 抖动累积寄存器

    pwm_counter = (pwm_counter + 1) % PWM_RESOLUTION;

    for (uint8_t i = 0; i < PWM_MAX_CHANNELS; i++) {
        if (!pwm_channels[i].is_active) continue;

        // 累积误差算法
        pwm_acc[i] += pwm_channels[i].current_duty;
        bool pwm_output = false;
        if (pwm_acc[i] >= PWM_RESOLUTION) {
            pwm_output = true;
            pwm_acc[i] -= PWM_RESOLUTION;
        }
        APP_SET_GPIO(pwm_channels[i].gpio,
#if defined PWM_DIR
                     !pwm_output
#else
                     pwm_output
#endif
        );
    }
}
