#include "py32f0xx.h"
#include "py32f0xx_hal_tim.h"
#include "pwm.h"
#include "curve_table.h"
#include "../bsp/bsp_uart.h"
#include "../device/device_manager.h"

#define SYSTEM_CLOCK_FREQ 48000000 // 系统时钟频率(48MHz)
#define TIMER_PERIOD      49       // 50 us 触发一次中断
#define PWM_RESOLUTION    500      // PWM分辨率(500)
#define MAX_FADE_TIME_MS  5000     // 最大渐变时间(5秒)
#define FADE_UPDATE_MS    10       // 渐变更新间隔(10ms)

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

    if (duty > PWM_RESOLUTION) {
        duty = PWM_RESOLUTION;
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
    duty = (duty > PWM_RESOLUTION) ? PWM_RESOLUTION : duty;

    // 渐变时间为0则立即切换
    if (fade_time_ms == 0) {
        app_set_pwm_duty(pin, duty);
        return;
    }

    // 限制最大渐变时间
    fade_time_ms = (fade_time_ms > MAX_FADE_TIME_MS) ? MAX_FADE_TIME_MS : fade_time_ms;

    // 计算渐变步数(带四舍五入)
    uint16_t steps = (fade_time_ms + FADE_UPDATE_MS / 2) / FADE_UPDATE_MS;
    steps          = (steps == 0) ? 1 : steps; // 确保至少1步

    // 更新渐变参数
    pwm_channels[index].target_duty  = duty;
    pwm_channels[index].start_duty   = pwm_channels[index].current_duty;
    pwm_channels[index].fade_steps   = steps;
    pwm_channels[index].fade_counter = 0;
    pwm_channels[index].is_fading    = true;

    // 如果当前已在目标值,直接完成渐变
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
#if 0
    if (!__HAL_TIM_GET_FLAG(&Timer3, TIM_FLAG_UPDATE)) return;
    if (!__HAL_TIM_GET_IT_SOURCE(&Timer3, TIM_IT_UPDATE)) return;
    __HAL_TIM_CLEAR_IT(&Timer3, TIM_IT_UPDATE);
    static uint16_t pwm_counter = 0;

    pwm_counter = (pwm_counter + 1) % PWM_RESOLUTION;

    for (uint8_t i = 0; i < PWM_MAX_CHANNELS; i++) {
        if (!pwm_channels[i].is_active) continue;

        bool pwm_output = (pwm_counter < pwm_channels[i].current_duty);
        APP_SET_GPIO(pwm_channels[i].gpio,
#if defined PWM_DIR
                     !pwm_output
#else
                     pwm_output
#endif
        );
    }
#endif
#if 0
    static volatile uint16_t pwm_counter = 0;
    static volatile uint16_t fade_timer  = 0;

    if (!__HAL_TIM_GET_FLAG(&Timer3, TIM_FLAG_UPDATE)) { // 判断更新事件标志
        return;
    }
    if (!__HAL_TIM_GET_IT_SOURCE(&Timer3, TIM_IT_UPDATE)) { // 判断中断源
        return;
    }
    __HAL_TIM_CLEAR_IT(&Timer3, TIM_IT_UPDATE); // 清除中断标志
    // 这里直接写你的 1us 任务

    // PWM生成逻辑
    pwm_counter = (pwm_counter + 1) % PWM_RESOLUTION;

    // 更新所有通道的输出状态
    for (uint8_t i = 0; i < PWM_MAX_CHANNELS; i++) {
        if (!pwm_channels[i].is_active) { // 跳过未激活的通道
            continue;
        }

        bool pwm_output = (pwm_counter < pwm_channels[i].current_duty);
        APP_SET_GPIO(pwm_channels[i].gpio,
#if defined PWM_DIR
                     !pwm_output
#else
                     pwm_output
#endif
        );
    }

    // 渐变处理逻辑(每10ms调整一次占空比)
    fade_timer = (fade_timer + 1) % (FADE_UPDATE_MS * 1000 / 50);
    if (fade_timer != 0) {
        return;
    }
    for (int i = 0; i < PWM_MAX_CHANNELS; i++) {
        if (!pwm_channels[i].is_active || !pwm_channels[i].is_fading) {
            continue;
        }

        pwm_channels[i].fade_counter++;

        // 计算进度并限制范围
        uint16_t progress = (uint16_t)pwm_channels[i].fade_counter * FADE_TABLE_SIZE / pwm_channels[i].fade_steps;
        progress          = MIN(progress, FADE_TABLE_SIZE - 1);
        // 获取缓入缓出曲线值
        uint16_t curve_value = fade_table[progress];
        // 计算新占空比
        uint16_t start = pwm_channels[i].start_duty;
        uint16_t end   = pwm_channels[i].target_duty;
        int16_t range  = end - start;

        uint16_t new_duty = start + (range * curve_value) / 500;
        new_duty          = CLAMP(new_duty, 0, PWM_RESOLUTION);

        pwm_channels[i].current_duty = (uint16_t)new_duty;

        // 检查渐变是否完成
        if (pwm_channels[i].fade_counter >= pwm_channels[i].fade_steps) {
            pwm_channels[i].current_duty = end;
            pwm_channels[i].is_fading    = false;
        }
    }

#endif
    static volatile uint16_t pwm_counter = 0;
    static volatile uint16_t fade_timer  = 0;
    static int pwm_acc[PWM_MAX_CHANNELS] = {0}; // 累积误差寄存器

    if (!__HAL_TIM_GET_FLAG(&Timer3, TIM_FLAG_UPDATE)) return;
    __HAL_TIM_CLEAR_IT(&Timer3, TIM_IT_UPDATE);

    // PWM计数
    pwm_counter = (pwm_counter + 1) % PWM_RESOLUTION;

    for (uint8_t i = 0; i < PWM_MAX_CHANNELS; i++) {
        if (!pwm_channels[i].is_active) continue;

        // 累积误差算法:低占空比闪烁优化
        pwm_acc[i] += pwm_channels[i].current_duty; // 累计占空比

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
    // 渐变处理(每 FADE_UPDATE_MS 调整一次占空比)
    fade_timer = (fade_timer + 1) % (FADE_UPDATE_MS * 1000 / 50);
    if (fade_timer != 0) return;

    for (int i = 0; i < PWM_MAX_CHANNELS; i++) {
        if (!pwm_channels[i].is_active || !pwm_channels[i].is_fading) continue;
        pwm_channels[i].fade_counter++;
        // 计算进度
        uint16_t progress    = pwm_channels[i].fade_counter * FADE_TABLE_SIZE / pwm_channels[i].fade_steps;
        progress             = MIN(progress, FADE_TABLE_SIZE - 1);
        uint16_t curve_value = fade_table[progress];
        uint16_t start       = pwm_channels[i].start_duty;
        uint16_t end         = pwm_channels[i].target_duty;
        int16_t range        = end - start;
        uint16_t new_duty    = start + (range * curve_value) / 500;
        new_duty             = CLAMP(new_duty, 0, PWM_RESOLUTION);

        pwm_channels[i].current_duty = new_duty;

        // 渐变完成
        if (pwm_channels[i].fade_counter >= pwm_channels[i].fade_steps) {
            pwm_channels[i].current_duty = end;
            pwm_channels[i].is_fading    = false;
        }
    }
}

void app_pwm_fade_poll(void)
{
    static uint16_t fade_timer = 0;

    fade_timer++;
    if (fade_timer < (FADE_UPDATE_MS)) return;
    fade_timer = 0;

    for (int i = 0; i < PWM_MAX_CHANNELS; i++) {
        if (!pwm_channels[i].is_active || !pwm_channels[i].is_fading) continue;

        pwm_channels[i].fade_counter++;

        uint16_t progress = pwm_channels[i].fade_counter * FADE_TABLE_SIZE / pwm_channels[i].fade_steps;
        progress          = MIN(progress, FADE_TABLE_SIZE - 1);

        uint16_t curve_value = fade_table[progress];
        uint16_t start       = pwm_channels[i].start_duty;
        uint16_t end         = pwm_channels[i].target_duty;
        int16_t range        = end - start;

        uint16_t new_duty = start + (range * curve_value) / 500;
        new_duty          = CLAMP(new_duty, 0, PWM_RESOLUTION);

        pwm_channels[i].current_duty = new_duty;

        if (pwm_channels[i].fade_counter >= pwm_channels[i].fade_steps) {
            pwm_channels[i].current_duty = end;
            pwm_channels[i].is_fading    = false;
        }
    }
}
