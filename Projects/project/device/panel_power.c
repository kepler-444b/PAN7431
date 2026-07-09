#include "panel_power.h"
#include "../bsp/bsp_timer.h"
#include "../bsp/bsp_uart.h"
#include "../bsp/bsp_pcb.h"
#include "../app/adc.h"
#include "../app/base.h"
#include "../app/eventbus.h"
#include "../app/base.h"
#include "../app/protocol.h"
#include "../bsp/bsp_zero.h"
#include <stdint.h>
#include <stdbool.h>
#include "../app/gpio.h"
#include "../app/pwm_hw.h"

#if defined PANEL_POWER

// 宏定义

#define VOL_BUF_SIZE      10  // 电压值缓冲区数量
#define MIN_VOL           329 // 无按键按下时的最小电压值
#define MAX_VOL           330 // 无按键按下时的最大电压值
#define LONG_PRESS        60  // 长按时间
#define LOCK_COUNT        40  // 锁键时间(200ms)

#define PANEL_INSERT_CARD {0xFA, 0x01, 0x01, 0x06, 0x00, 0xFE, 0x00, 0x00}
#define PANEL_REMOVE_CARD {0xFA, 0x01, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00}
#define PANLE_ALL_CLOSE   {0xF1, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x23, 0x00}

typedef struct
{
    uint8_t buf_idx; // 缓冲区下标
    uint16_t vol;    // 电压值
    uint16_t vol_buf[VOL_BUF_SIZE];
} adc_value_t;

typedef struct
{
    uint16_t min;
    uint16_t max;
} key_vol_t;

// 用于每个按键的状态
typedef struct
{
    bool k_press;  // 按键按下
    bool k_status; // 按键状态

    const key_vol_t vol_range; // 按键电压范围
} panel_status_t;

typedef struct
{
    bool key_status;      // 按键状态
    bool key_last_status; // 按键上次状态

    bool led_filck;           // 闪烁
    bool key_long_press;      // 长按状态
    bool enter_config;        // 进入配置状态
    uint16_t key_long_count;  // 长按计数
    uint16_t led_filck_count; // 闪烁计数

    uint8_t key_lock_count; // 锁键时间
    bool key_locked;        // 锁键

    bool remove_card;           // 拔卡
    uint16_t remove_card_count; // 拔卡后延时
    bool led_b_filck;           // 背光灯闪烁

    bool insert_card; // 插卡
    uint16_t insert_card_count;

    bool all_close;      // 总关状态
    bool all_close_last; // 上次总关状态

    bool bl_close;
    uint16_t bl_delay_count;

} common_panel_t;

typedef struct {
    gpio_pin_t led_w_pin;  // 按键所控白灯
    pwm_hw_pins led_y_pin; // 按键所控黄灯
} panel_power_pin_t;

// 函数声明
static void panel_read_adc(void *arg);
static void panel_ctrl_status_by_kay(bool led_state);
static void panel_ctrl_led_b_all(uint16_t lum);
static void process_led_flicker(common_panel_t *common_panel);
static void process_panel_adc(panel_status_t *temp_status, common_panel_t *temp_common, adc_value_t *adc_value);
static void panel_event_handler(event_type_e event, void *params);

// 全局变量
static adc_value_t my_adc_value;
static common_panel_t my_common_panel;
static panel_power_pin_t my_panel_power_pin[6] = {
    {PA2, PWM_PB3},
    {PA3, PWM_PA8},
    {PA7, PWM_PA10},
    {PA12, PWM_PA11},
    {PA15, PWM_PB4},
    {PB2, PWM_PB5},
};
static panel_status_t my_panel_status[CONFIG_NUMBER] = {
    PANEL_VOL_RANGE_DEF,
};
void panel_power_init(void)
{
    app_adc_init();
    pwm_hw_init();
    bsp_panel_power_init();

    bsp_zero_init();

    app_pwm_hw_add_pin(PWM_PB3);
    app_pwm_hw_add_pin(PWM_PA8);
    app_pwm_hw_add_pin(PWM_PA10);
    app_pwm_hw_add_pin(PWM_PA11);
    app_pwm_hw_add_pin(PWM_PB4);
    app_pwm_hw_add_pin(PWM_PB5);

    for (uint8_t i = 0; i < 6; i++) {
        app_set_pwm_hw_fade(my_panel_power_pin[i].led_y_pin, 2400, 1000, 5, 1);
    }
    app_eventbus_subscribe(panel_event_handler);
    bsp_start_timer(2, 5, panel_read_adc, NULL, TMR_AUTO_MODE);
    APP_PRINTF("panel_power_init\n");

    panel_ctrl_status_by_kay(false); // 上电默认为"电源关"
}

static void panel_read_adc(void *arg)
{
    uint16_t adc_value = app_get_adc_value();
    if (adc_value != ERR_VOL) { // Invalid Voltage
        my_adc_value.vol = adc_value;
        process_panel_adc(my_panel_status, &my_common_panel, &my_adc_value);
    }
    if (my_common_panel.led_filck) {
        process_led_flicker(&my_common_panel);
    }

    // 延迟 20s 后发送拔卡命令
    if (my_common_panel.remove_card) {

        if (my_common_panel.remove_card_count == 400 || my_common_panel.remove_card_count == 500 || my_common_panel.remove_card_count == 600) {
            uint8_t cmd[8] = PANEL_REMOVE_CARD; // 发送拔卡命令
            app_send_data(cmd, 8);
        }
        if (my_common_panel.remove_card_count >= 4000) { // 20s 后关闭继电器

            zero_set_gpio(PB1, false); // 关闭继电器

            app_set_pwm_hw_fade(my_panel_power_pin[0].led_y_pin, 2400, 1000, 5, 1);
            app_set_pwm_hw_fade(my_panel_power_pin[3].led_y_pin, 2400, 1000, 5, 1);
            app_set_pwm_hw_fade(my_panel_power_pin[4].led_y_pin, 2400, 1000, 5, 1);

            my_common_panel.remove_card       = false;
            my_common_panel.remove_card_count = 0;

        } else if ((my_common_panel.remove_card_count % 250) == 0) { // 背光等呼吸

            my_common_panel.led_b_filck = !my_common_panel.led_b_filck;
            bool state                  = my_common_panel.led_b_filck;

            app_set_pwm_hw_fade(my_panel_power_pin[0].led_y_pin, state ? 0 : 2400, 1000, 5, 1);
            app_set_pwm_hw_fade(my_panel_power_pin[3].led_y_pin, state ? 0 : 2400, 1000, 5, 1);
            app_set_pwm_hw_fade(my_panel_power_pin[4].led_y_pin, state ? 0 : 2400, 1000, 5, 1);
        }
        my_common_panel.remove_card_count++;
    }

    // 延时 2s 后发送插卡命令
    if (my_common_panel.insert_card) {

        my_common_panel.insert_card_count++;
        if (my_common_panel.insert_card_count == 400 || my_common_panel.insert_card_count == 500) {
            uint8_t cmd[8] = PANEL_INSERT_CARD;
            app_send_data(cmd, 8);
        }
        if (my_common_panel.insert_card_count >= 600) {
            uint8_t cmd[8] = PANEL_INSERT_CARD;
            app_send_data(cmd, 8);
            my_common_panel.insert_card       = false;
            my_common_panel.insert_card_count = 0;
        }
    }

    // 收到了"总关背光"的命令
    if (my_common_panel.all_close != my_common_panel.all_close_last) {
        if (my_common_panel.all_close) {
            my_common_panel.bl_close = true;
        }
        my_common_panel.all_close_last = my_common_panel.all_close;
    }

    // 延时执行"总关背光"
    if (my_common_panel.bl_close) {
        my_common_panel.bl_delay_count++;

        if (my_common_panel.bl_delay_count >= 300) { // 关闭背光灯

            for (uint8_t i = 0; i < 6; i++) {
                app_set_pwm_hw_fade(my_panel_power_pin[i].led_y_pin, 0, 1000, 5, 1);
            }
        }
        if (my_common_panel.bl_delay_count >= 800) { // 关闭指示灯

            for (uint8_t i = 0; i < 6; i++) {
                APP_SET_GPIO(my_panel_power_pin[i].led_w_pin, false);
            }
            my_common_panel.bl_close       = false;
            my_common_panel.bl_delay_count = 0;
        }
    }
}

static void process_panel_adc(panel_status_t *temp_status, common_panel_t *temp_common, adc_value_t *adc_value)
{
    if (temp_common->key_locked) {
        temp_common->key_lock_count++;
        if (temp_common->key_lock_count >= LOCK_COUNT) {
            temp_common->key_lock_count = 0;
            temp_common->key_locked     = false;
        }
        return;
    }
    // printf("vol:%d\n", adc_value->vol);
    for (uint8_t i = 0; i < CONFIG_NUMBER; i++) {
        if (adc_value->vol < temp_status[i].vol_range.min || adc_value->vol > temp_status[i].vol_range.max) {
            if (adc_value->vol >= MIN_VOL && adc_value->vol <= MAX_VOL) {
                temp_status[i].k_press      = false;
                temp_common->key_long_press = false;
                temp_common->key_long_count = 0;
            }
            continue;
        }
        // Fill the vol_buf
        adc_value->vol_buf[adc_value->buf_idx++] = adc_value->vol;
        if (adc_value->buf_idx < VOL_BUF_SIZE) {
            continue;
        }
        adc_value->buf_idx = 0; // vol_buf is fill

        uint16_t new_value = app_calculate_average(adc_value->vol_buf, VOL_BUF_SIZE);
        if (new_value < temp_status[i].vol_range.min || new_value > temp_status[i].vol_range.max) {
            continue; // 检查平均值是否在有效范围
        }
        if (!temp_status[i].k_press && !temp_common->enter_config) { // 处理按键按下
            temp_common->key_locked = true;                          // 锁键
            temp_status[i].k_press  = true;

            if (my_common_panel.all_close) { // 当前已经在"总关背光"状态下
                // 唤醒
                if (my_common_panel.key_status) {
                    panel_ctrl_status_by_kay(true);
                } else {
                    panel_ctrl_status_by_kay(false);
                }

                uint8_t cmd[8] = PANLE_ALL_CLOSE; // 总关命令
                app_send_data(cmd, 8);
                my_common_panel.all_close = false;

                if (my_common_panel.bl_close) { // 若此时正在执行"总关",则取消关闭背光灯
                    my_common_panel.bl_close       = false;
                    my_common_panel.bl_delay_count = 0;
                }
                return;
            }
            if ((i == 1 || i == 2 || i == 5)) {
                my_common_panel.key_status = false;
            } else if ((i == 0 || i == 3 || i == 4)) {
                my_common_panel.key_status = true;
            }

            bool status     = my_common_panel.key_status;
            bool new_status = false;
            if (my_common_panel.key_last_status != my_common_panel.key_status) {
                my_common_panel.key_last_status = my_common_panel.key_status;

                new_status = true;
            }
            if (!new_status) { // 为了避免重复发码,若按下重复的按键,直接退出
                return;
            }
            panel_ctrl_status_by_kay(status);

            if (status) { // 插卡命令

                zero_set_gpio(PB1, true); // 继电器立即吸合
                my_common_panel.insert_card       = true;
                my_common_panel.insert_card_count = 0;

                // 取消拔卡延时(如果此时拔卡正在执行的话)
                my_common_panel.remove_card       = false;
                my_common_panel.remove_card_count = 0;
            } else { // 拔卡命令

                // 取消插卡延时(如果此时插卡正在执行的话)
                my_common_panel.insert_card       = false;
                my_common_panel.insert_card_count = 0;

                my_common_panel.remove_card = true;
                my_common_panel.led_b_filck = true;
            }

            temp_common->key_long_press = true;
            temp_common->key_long_count = 0;
            continue;
        }
        // 处理长按
        if (temp_common->key_long_press && ++temp_common->key_long_count >= LONG_PRESS) {
            app_eventbus_publish(EVENT_REQUEST_NETWORK, NULL);
            temp_common->key_long_press = false;
            APP_PRINTF("long ste\n");
        }
    }
}

static void panel_event_handler(event_type_e event, void *params)
{
    APP_PRINTF("panel_event_handler\n");
    switch (event) {
        case EVENT_PANEL_RX: {
            if (my_common_panel.remove_card) { // 处于拔卡倒计时
                return;
            }

            frame_t *my_panel_frame = (frame_t *)params;
            uint8_t cmd             = my_panel_frame->data[1];
            bool all_close          = BIT1(my_panel_frame->data[6]); // 是否勾选了"总关背光"

            // 如果是勾选了"备用"且没有勾选"只开",这种按键为特殊按键(立即执行动作,而不唤醒任何面板)
            bool src_special_key = (BIT2(my_panel_frame->data[6]) && !BIT4(my_panel_frame->data[6]));
            if (src_special_key) {
                return;
            }
            if (my_common_panel.bl_close) {
                my_common_panel.bl_close       = false;
                my_common_panel.bl_delay_count = 0;
            }

            if (my_common_panel.all_close) { // 进入"总关背光"

                APP_PRINTF("wake up\n");
                if (my_common_panel.key_status) {
                    panel_ctrl_status_by_kay(true);
                } else {
                    panel_ctrl_status_by_kay(false);
                }

                my_common_panel.all_close = false;
            } else {
                if (cmd == ALL_CLOSE || cmd == ALL_ON_OFF) { // 收到"总关"或"总开关"
                    if (cmd == ALL_CLOSE) {                  // 收到"总关"的"总关背光"
                        if (all_close) {
                            my_common_panel.all_close = true;
                            APP_PRINTF("all close\n");
                        }
                    } else if (cmd == ALL_ON_OFF) {
                        if (all_close && !my_panel_frame->data[2]) { // 收到"总开关"的"总关背光"
                            my_common_panel.all_close = true;
                            APP_PRINTF("all close open");
                        }
                    }
                }
            }

        } break;
        case EVENT_LED_BLINK: {
            my_common_panel.led_filck = true;
        } break;
        default:
            break;
    }
}

// 指示灯闪烁
static void process_led_flicker(common_panel_t *common_panel)
{
    common_panel->led_filck_count++;
    if (common_panel->led_filck_count <= 50) {
        panel_ctrl_status_by_kay(true);
    } else if (common_panel->led_filck_count <= 100) {
        panel_ctrl_status_by_kay(false);
    } else if (common_panel->led_filck_count <= 150) {
        panel_ctrl_status_by_kay(true);
    } else {
        common_panel->led_filck_count = 0;
        common_panel->led_filck       = false;
        panel_ctrl_status_by_kay(false);
    }
}

// 根据按键状态,控制白/黄灯状态
static void panel_ctrl_status_by_kay(bool led_state)
{
    if (led_state) { // 电源开
        APP_SET_GPIO(my_panel_power_pin[1].led_w_pin, false);
        APP_SET_GPIO(my_panel_power_pin[2].led_w_pin, false);
        APP_SET_GPIO(my_panel_power_pin[5].led_w_pin, false);

        app_set_pwm_hw_fade(my_panel_power_pin[1].led_y_pin, 2400, 100, 5, 1);
        app_set_pwm_hw_fade(my_panel_power_pin[2].led_y_pin, 2400, 100, 5, 1);
        app_set_pwm_hw_fade(my_panel_power_pin[5].led_y_pin, 2400, 100, 5, 1);

        APP_SET_GPIO(my_panel_power_pin[0].led_w_pin, true);
        APP_SET_GPIO(my_panel_power_pin[3].led_w_pin, true);
        APP_SET_GPIO(my_panel_power_pin[4].led_w_pin, true);

        app_set_pwm_hw_fade(my_panel_power_pin[0].led_y_pin, 0, 100, 5, 1);
        app_set_pwm_hw_fade(my_panel_power_pin[3].led_y_pin, 0, 100, 5, 1);
        app_set_pwm_hw_fade(my_panel_power_pin[4].led_y_pin, 0, 100, 5, 1);

    } else { // 电源关
        APP_SET_GPIO(my_panel_power_pin[1].led_w_pin, true);
        APP_SET_GPIO(my_panel_power_pin[2].led_w_pin, true);
        APP_SET_GPIO(my_panel_power_pin[5].led_w_pin, true);

        app_set_pwm_hw_fade(my_panel_power_pin[1].led_y_pin, 0, 100, 5, 1);
        app_set_pwm_hw_fade(my_panel_power_pin[2].led_y_pin, 0, 100, 5, 1);
        app_set_pwm_hw_fade(my_panel_power_pin[5].led_y_pin, 0, 100, 5, 1);

        APP_SET_GPIO(my_panel_power_pin[0].led_w_pin, false);
        APP_SET_GPIO(my_panel_power_pin[3].led_w_pin, false);
        APP_SET_GPIO(my_panel_power_pin[4].led_w_pin, false);

        app_set_pwm_hw_fade(my_panel_power_pin[0].led_y_pin, 2400, 100, 5, 1);
        app_set_pwm_hw_fade(my_panel_power_pin[3].led_y_pin, 2400, 100, 5, 1);
        app_set_pwm_hw_fade(my_panel_power_pin[4].led_y_pin, 2400, 100, 5, 1);
    }
}

// 控制所有背光灯状态
static void panel_ctrl_led_b_all(uint16_t lum)
{
    for (uint8_t i = 0; i < 6; i++) {
        app_set_pwm_hw_fade(my_panel_power_pin[i].led_y_pin, lum, 1000, 5, 1);
    }
}

#endif
