#include "panel.h"
#include "pan211.h"
#include "device_manager.h"
#include "../app/eventbus.h"
#include "../app/protocol.h"
#include "../app/config.h"
#include "../app/adc.h"
#include "../bsp/bsp_timer.h"
#include "../app/pwm.h"
#include "../app/base.h"
#include "../bsp/bsp_uart.h"
#include "../bsp/bsp_tm5020a.h"
#include "../bsp/bsp_zero.h"

#if defined PANEL_KEY

// 函数参数
#define FUNC_PARAMS      panel_frame_t *data, const panel_cfg_t *temp_cfg, panel_status_t *temp_status, data_source src
#define FUNC_ARGS        data, temp_cfg, temp_status, src
#define TIMER_PARAMS     const panel_cfg_t *temp_cfg, panel_status_t *temp_status, common_panel_t *temp_common

#define SHORT_TIME_LED   100   // 短亮时间
#define SHORT_TIME_RELAY 30000 // 继电器导通时间
#define LONG_PRESS       60    // 长按时间

#define FADE_TIME        500

// 外层遍历
#define PROCESS_OUTER(cfg, status, ...)               \
    do {                                              \
        for (uint8_t _i = 0; _i < KEY_NUMBER; _i++) { \
            const panel_cfg_t *p_cfg = &(cfg)[_i];    \
            panel_status_t *p_status = &(status)[_i]; \
            __VA_ARGS__                               \
        }                                             \
    } while (0)

// 内层遍历
#define PROCESS_INNER(cfg_ex, status_ex, ...)               \
    do {                                                    \
        for (uint8_t _j = 0; _j < KEY_NUMBER; _j++) {       \
            const panel_cfg_t *p_cfg_ex = &(cfg_ex)[_j];    \
            panel_status_t *p_status_ex = &(status_ex)[_j]; \
            __VA_ARGS__                                     \
        }                                                   \
    } while (0)

#define ADC_VOL_NUMBER 10  // 电压值缓冲区数量
#define MIN_VOL        329 // 无按键按下时的最小电压值
#define MAX_VOL        330 // 无按键按下时的最大电压值

#define ADC_PARAMS     panel_status_t *temp_status, common_panel_t *temp_common, adc_value_t *adc_value

typedef struct {
    uint16_t min;
    uint16_t max;
} key_vol_t;

typedef struct { // 用于每个按键的状态

    bool key_press;  // 按键按下
    bool key_status; // 按键状态

    bool led_w_open;  // 白灯状态
    bool led_w_short; // 启用短亮(窗帘开关)
    bool led_w_last;  // 白灯上次状态

    bool led_y_open; // 黄灯状态
    bool led_y_last; // 黄灯上次状态

    bool relay_open;  // 继电状态
    bool relay_short; // 继电器短开
    bool relay_last;  // 继电器上次状态

    uint16_t led_w_short_count; // 短亮计数器
    uint32_t relay_open_count;  // 继电器短开计数器
    const key_vol_t vol_range;  // 按键电压范围
} panel_status_t;

typedef struct
{
    bool bl_close;            // 总关背光
    uint16_t bl_delay_count;  // 总关背光延时执行
    bool led_filck;           // 闪烁
    bool key_long_press;      // 长按状态
    bool enter_config;        // 进入配置状态
    uint16_t back_lum;        // 所有背光灯亮度
    uint16_t key_long_count;  // 长按计数
    uint16_t led_filck_count; // 闪烁计数
} common_panel_t;

typedef struct {

    uint8_t buf_idx; // 缓冲区下标
    uint16_t vol;    // 电压值
    uint16_t vol_buf[ADC_VOL_NUMBER];
} adc_value_t;

static panel_status_t my_panel_status[KEY_NUMBER] = {
    PANEL_VOL_RANGE_DEF,
};

static void panel_read_adc(void *arg);
static void panel_proce_cmd(void *arg);
static void panel_event_handler(event_type_e event, void *params);
static void process_led_flicker(common_panel_t *common_panel);
static void panel_fast_exe(panel_status_t *temp_fast, uint8_t flag);
static void process_panel_simulate(uint8_t key_number, uint8_t status);
static void panel_backlight_status(const panel_cfg_t *temp_cfg, panel_status_t *temp_status, bool status);
static void panel_ctrl_led_all(bool led_state);
static void panel_data_cb(panel_frame_t *data, data_source src);
static void process_panel_adc(ADC_PARAMS);
static void process_exe_status(TIMER_PARAMS);
static void process_cmd_check(FUNC_PARAMS);
static void panel_all_close(FUNC_PARAMS);
static void panel_all_on_off(FUNC_PARAMS);
static void panel_curtain_open(FUNC_PARAMS);
static void panel_curtain_close(FUNC_PARAMS);
static void panel_curtain_stop(FUNC_PARAMS);
static void panel_clean_room(FUNC_PARAMS);
static void panel_dnd_mode(FUNC_PARAMS);
static void panel_later_mode(FUNC_PARAMS);
static void panel_chect_out(FUNC_PARAMS);
static void panel_sos_mode(FUNC_PARAMS);
static void panel_service(FUNC_PARAMS);
static void panel_scene_mode(FUNC_PARAMS);
static void panel_light_mode(FUNC_PARAMS);
static void panel_night_light(FUNC_PARAMS);

static common_panel_t my_common_panel;
static adc_value_t my_adc_value;
static uint16_t key_static = 0;

void panel_device_init(void)
{
    bsp_panel_init();
    app_adc_init();
    bsp_start_timer(2, 5, panel_read_adc, NULL, TMR_AUTO_MODE);
    bsp_start_timer(3, 1, panel_proce_cmd, NULL, TMR_AUTO_MODE);
    app_eventbus_subscribe(panel_event_handler);
    app_pwm_init();

#if defined ZERO_ENABLE
    bsp_zero_init();
#endif
    app_pwm_add_pin(PA8);
    app_set_pwm_fade(PA8, 500, 1000);
}
static void panel_read_adc(void *arg)
{
    uint16_t adc_value = app_get_adc_value();
    if (adc_value != ERR_VOL) { // 无效电压
        my_adc_value.vol = adc_value;
        process_panel_adc(my_panel_status, &my_common_panel, &my_adc_value);
    }
    if (my_common_panel.led_filck) {
        process_led_flicker(&my_common_panel);
    }
}

static void process_panel_adc(ADC_PARAMS)
{
    // APP_PRINTF("vol:%d\n", adc_value->vol);
    for (uint8_t i = 0; i < KEY_NUMBER; i++) {
        panel_status_t *p_status = &temp_status[i];
        if (adc_value->vol < p_status->vol_range.min || adc_value->vol > p_status->vol_range.max) {
            if (adc_value->vol >= MIN_VOL && adc_value->vol <= MAX_VOL) {
                p_status->key_press         = false;
                temp_common->key_long_press = false;
                temp_common->key_long_count = 0;
            }
            continue;
        }
        // 填充 vol_buf
        adc_value->vol_buf[adc_value->buf_idx++] = adc_value->vol;
        if (adc_value->buf_idx < ADC_VOL_NUMBER) {
            continue;
        }
        adc_value->buf_idx = 0; // vol_buf 被填充满(下标置0)
        uint16_t new_value = app_calculate_average(adc_value->vol_buf, ADC_VOL_NUMBER);
        if (new_value < p_status->vol_range.min || new_value > p_status->vol_range.max) {
            continue; // 检查平均值是否在有效范围
        }
        if (!p_status->key_press && !temp_common->enter_config) { // 处理按键按下
            p_status->key_status ^= 1;
            const bool is_special = p_status->relay_short;
            // APP_PRINTF("key:%d\n", i);
            app_send_cmd(i, (is_special ? 0x00 : p_status->key_status), PANEL_HEAD, (is_special ? SPECIAL_CMD : COMMON_CMD));

            p_status->key_press         = true;
            temp_common->key_long_press = true;
            temp_common->key_long_count = 0;
            continue;
        }
        // 处理长按
        if (temp_common->key_long_press && ++temp_common->key_long_count >= LONG_PRESS) {
            // app_send_cmd(0, 0, APPLY_CONFIG, 0x00, is_ex);
            app_eventbus_publish(EVENT_SWITCH_CHANNEL0, NULL);
            temp_common->key_long_press = false;
        }
    }
}

static void process_panel_simulate(uint8_t key_number, uint8_t status)
{
    APP_PRINTF("key_number:%d status:%d\n", key_number, status);
    my_panel_status[key_number].key_status = status;
    APP_PRINTF("key_status:%d\n", my_panel_status[key_number].key_status);

    const bool is_special = my_panel_status[key_number].relay_short;
    app_send_cmd(key_number, (is_special ? 0x00 : my_panel_status[key_number].key_status), PANEL_HEAD, (is_special ? SPECIAL_CMD : COMMON_CMD));
}

static void panel_proce_cmd(void *arg)
{
    const panel_cfg_t *temp_cfg = app_get_panel_cfg();
    process_exe_status(temp_cfg, my_panel_status, &my_common_panel);
}
static void process_exe_status(TIMER_PARAMS)
{
    if (temp_common->bl_close) { // 处理背光总关
        if (++temp_common->bl_delay_count >= 1000) {
            panel_backlight_status(temp_cfg, temp_status, false);
            temp_common->bl_close       = false;
            temp_common->bl_delay_count = 0;
        }
    }

    PROCESS_OUTER(temp_cfg, temp_status, {
        if (p_status->led_w_short) { // White LED short-term open
            if (p_status->led_w_short_count++ == 0) {
                p_status->led_w_open = true;
            }
            if (p_status->led_w_short_count >= SHORT_TIME_LED) {
                p_status->led_w_open        = false;
                p_status->led_w_short       = false;
                p_status->led_w_short_count = 0;
            }
        }
        if (p_status->relay_short) { // Relay short-term open
            if (p_status->relay_open_count++ == 0) {
                p_status->relay_open = true;
            }
            if (p_status->relay_open_count >= SHORT_TIME_RELAY) {
                p_status->relay_open       = false;
                p_status->relay_short      = false;
                p_status->led_w_open       = false;
                p_status->key_status       = false;
                p_status->relay_open_count = 0;
            }
        }
        if (p_status->led_w_open != p_status->led_w_last) { // Update the status of the white LED

            APP_SET_GPIO(p_cfg->led_w_pin, !p_status->led_w_open); // The white LED is driven by a low leval
            // APP_PRINTF("%s\n", app_get_gpio_name(p_cfg->led_w_pin));
            p_status->led_w_last = p_status->led_w_open;
            // panel_backlight_status(temp_cfg, temp_status, true);
        }

        if (p_status->relay_open != p_status->relay_last) { // Update the status of the relay
            for (uint8_t i = 0; i < RELAY_NUMBER; i++) {
                if (!app_gpio_equal(p_cfg->relay_pin[i], DEF)) { // Only execute the selected relays

#if defined ZERO_ENABLE
                    zero_set_gpio(p_cfg->relay_pin[i], p_status->relay_open);
#else
                    // APP_PRINTF("%s\n", app_get_gpio_name(p_cfg->relay_pin[i]));
                    APP_SET_GPIO(p_cfg->relay_pin[i], p_status->relay_open);
#endif
                }
            }
            p_status->relay_last = p_status->relay_open;
            // APP_PRINTF("%d %d %d %d\n", APP_GET_GPIO(PB12), APP_GET_GPIO(PB13), APP_GET_GPIO(PB14), APP_GET_GPIO(PB15));
        }
    });
}

static void panel_backlight_status(const panel_cfg_t *temp_cfg, panel_status_t *temp_status, bool status)
{

#if defined PANEL_6KEY_A11
    uint8_t no_w_led = 0;
    for (uint8_t i = 0; i < KEY_NUMBER; i++) {
        no_w_led += temp_status[i].led_w_open;
    }
    if (status) {
        app_set_pwm_fade(PA8, no_w_led ? 500 : 50, 3000);
    } else {
        app_set_pwm_fade(PA8, 0, 3000);
    }
#endif
}

static void panel_data_cb(panel_frame_t *data, data_source src)
{
    APP_PRINTF_BUF("panel_rx", data->data, data->length);
    const panel_cfg_t *temp_cfg = app_get_panel_cfg();
    process_cmd_check(data, temp_cfg, my_panel_status, src);
}

static void process_cmd_check(FUNC_PARAMS)
{
    switch (data->data[1]) {
        case ALL_CLOSE: // 总关
            panel_all_close(FUNC_ARGS);
            break;

        case ALL_ON_OFF: // 总开关
            panel_all_on_off(FUNC_ARGS);
            break;
        case CLEAN_ROOM: // 清理房间
            panel_clean_room(FUNC_ARGS);
            break;
        case DND_MODE: // 勿扰模式
            panel_dnd_mode(FUNC_ARGS);
            break;
        case LATER_MODE: // 请稍后
            panel_later_mode(FUNC_ARGS);
            break;
        case CHECK_OUT: // 退房
            panel_chect_out(FUNC_ARGS);
            break;
        case SOS_MODE: // 紧急呼叫
            panel_sos_mode(FUNC_ARGS);
            break;
        case SERVICE: // 请求服务
            panel_service(FUNC_ARGS);
            break;
        case CURTAIN_OPEN: // 窗帘开
            panel_curtain_open(FUNC_ARGS);
            break;
        case CURTAIN_CLOSE: // 窗帘关
            panel_curtain_close(FUNC_ARGS);
            break;
        case SCENE_MODE: // 场景模式
            panel_scene_mode(FUNC_ARGS);
            break;
        case LIGHT_MODE: // 灯控模式
            panel_light_mode(FUNC_ARGS);
            break;
        case NIGHT_LIGHT: // 夜灯
            panel_night_light(FUNC_ARGS);
            break;
        case LIGHT_UP:
            break;
        case LIGHT_DOWN:
            break;
        case DIMMING_3:
            break;
        case DIMMING_4:
            break;
        case UNLOCKING:
            break;
        case BLUETOOTH:
            break;
        case CURTAIN_STOP: // 窗帘停
            panel_curtain_stop(FUNC_ARGS);
            break;
        case VOLUME_ADD:
            break;
        case VOLUME_SUB:
            break;
        case PLAY_PAUSE:
            break;
        case NEXT_SONG:
            break;
        case LAST_SONG:
            break;
        default:
            return;
    }
}

static void process_led_flicker(common_panel_t *common_panel)
{
    common_panel->led_filck_count++;
    if (common_panel->led_filck_count <= 500) {
        panel_ctrl_led_all(false);
    } else if (common_panel->led_filck_count <= 1000) {
        panel_ctrl_led_all(true);
    } else if (common_panel->led_filck_count <= 1500) {
        panel_ctrl_led_all(false);
    } else {
        common_panel->led_filck_count = 0;
        common_panel->led_filck       = false;
        panel_ctrl_led_all(true);
    }
}

static void panel_ctrl_led_all(bool led_state)
{
    for (uint8_t i = 0; i < KEY_NUMBER; i++) {
        my_panel_status[i].led_w_open = led_state;
    }
}

static void panel_event_handler(event_type_e event, void *params)
{
    switch (event) {
        case EVENT_PANEL_RX: {
            panel_frame_t *my_panel_frame = (panel_frame_t *)params;
            // APP_PRINTF_BUF("EVENT_PANEL_RX", my_panel_frame->data, my_panel_frame->length);
            panel_data_cb(my_panel_frame, other_deivce);

        } break;
        case EVENT_PANEL_RX_MY: {
            panel_frame_t *my_panel_frame                = (panel_frame_t *)params;
            my_panel_frame->data[my_panel_frame->length] = 0x00;
            my_panel_frame->length += 1;

            // APP_PRINTF_BUF("EVENT_PANEL_RX_MY", my_panel_frame->data, my_panel_frame->length);
            panel_data_cb(my_panel_frame, this_device);

        } break;
        case EVENT_SIMULATE_KEY: {
            panel_frame_t *my_panel_frame = (panel_frame_t *)params;
            process_panel_simulate(my_panel_frame->data[0] - 1, my_panel_frame->data[1]);
        } break;
        case EVENT_LED_BLINK: {
            for (uint8_t i = 0; i < KEY_NUMBER; i++) {
                my_panel_status[i].led_w_short = true;
            }
        } break;
        default:
            break;
    }
}

static void panel_fast_exe(panel_status_t *temp_fast, uint8_t flag)
{
    // 0:data->data[2];1:key_status;2:led_w_open,3:led_w_short,4:relay_open;5:relay_short;6:reserve;7:reserve

    if (BIT1(flag)) {
        temp_fast->key_status = BIT0(flag);
    }
    if (BIT2(flag)) {
        temp_fast->led_w_open = BIT0(flag);
    }
    if (BIT3(flag)) {
        temp_fast->led_w_short = true; // 短亮会快速熄灭,无需持续状态,故而不受data->data[2]控制
    }
    if (BIT4(flag)) {
        temp_fast->relay_open = BIT0(flag);
    }
    if (BIT5(flag)) {
        temp_fast->relay_short      = BIT0(flag);
        temp_fast->relay_open_count = 0; // 继电器短开计数器提前置0
    }
}

/* ***************************************** 处理按键类型 ***************************************** */
static void panel_all_close(FUNC_PARAMS) // 总关
{
    if (BIT1(data->data[6])) {
        my_common_panel.bl_close = true;
#if defined PANEL_8KEY_A13
        my_common_panel_ex.bl_close = true;
#endif
    }
    PROCESS_OUTER(temp_cfg, temp_status, {
        if (!BIT5(p_cfg->perm)) {
            continue; // 无"睡眠"权限,跳过
        }
        if (H_BIT(data->data[4]) != 0xF &&                // 非全局指令
            H_BIT(data->data[4]) != H_BIT(p_cfg->area)) { // 分区不匹配
            continue;
        }

        switch (p_cfg->func) {
            case ALL_CLOSE:
                if (data->data[3] == p_cfg->group) {
                    panel_fast_exe(p_status, 0b00011010 | (data->data[2] & 0x01));
                }
                break;
            case NIGHT_LIGHT:
            case DND_MODE:
            case ALL_ON_OFF:
                panel_fast_exe(p_status, 0b00010110 | (data->data[2] & 0x01));
                break;
            case SCENE_MODE:
            case LIGHT_MODE:

            case CLEAN_ROOM:
                // panel_fast_exe(p_status, 0b00010110 | 0x00);
                break;
            case CURTAIN_CLOSE: // 勾选"总关"的窗帘关
                PROCESS_INNER(temp_cfg, temp_status, {
                    if (p_cfg_ex->func != CURTAIN_OPEN ||
                        p_cfg_ex->group != p_cfg->group) {
                        continue;
                    }
                    panel_fast_exe(p_status_ex, 0b00110000 | 0x00);
                });
                panel_fast_exe(p_status, 0b00101010 | 0x01);
                break;
            default:
                break;
        }
    });
}

static void panel_all_on_off(FUNC_PARAMS) // 总开关
{
    if (BIT1(data->data[6])) {
        my_common_panel.bl_close = true;
#if defined PANEL_8KEY_A13
        my_common_panel_ex.bl_close = true;
#endif
    }
    PROCESS_OUTER(temp_cfg, temp_status, {
        if (!BIT0(p_cfg->perm)) {
            continue; // 无"总开关"权限跳过
        }
        if (H_BIT(data->data[4]) != 0xF &&                // 分区不匹配
            H_BIT(data->data[4]) != H_BIT(p_cfg->area)) { // 非全局指令
            continue;
        }
        switch (p_cfg->func) {
            case ALL_ON_OFF:
                if (data->data[3] == p_cfg->group) {
                    if (BIT4(p_cfg->perm) && BIT6(p_cfg->perm)) { //"只开" + "取反"
                        panel_fast_exe(p_status, 0b00011010 | 0x00);
                    } else if (BIT4(p_cfg->perm)) { // "只开"
                        panel_fast_exe(p_status, 0b00010110 | 0x01);
                    } else if (BIT6(p_cfg->perm)) { // "取反"
                        panel_fast_exe(p_status, 0b00010110 | (~data->data[2] & 0x01));
                    } else { // 默认
                        panel_fast_exe(p_status, 0b00010110 | (data->data[2] & 0x01));
                    }
                }
                break;
            case DND_MODE:
                panel_fast_exe(p_status, 0b00010110 | (~data->data[2] & 0x01));
                break;
            case LIGHT_MODE:
            case SCENE_MODE:
            case CLEAN_ROOM:
            case NIGHT_LIGHT:
                if (BIT7(p_cfg->perm) && !data->data[2]) { // 勾选了"不总开",则只受"总开关"的关控制
                    panel_fast_exe(p_status, 0b00010110 | 0x00);
                } else if (!BIT7(p_cfg->perm)) { // 勾选"不总开",则正常动作
                    panel_fast_exe(p_status, 0b00010110 | (data->data[2] & 0x01));
                }
                break;
            case ALL_CLOSE: // 勾选"总开关"的"总关"不执行闪烁
                panel_fast_exe(p_status, 0b00010010 | (data->data[2] & 0x01));
                break;
            case CURTAIN_CLOSE: // 勾选"总开关"的窗帘关
                if (!data->data[2]) {
                    PROCESS_INNER(temp_cfg, temp_status, {
                        if (p_cfg_ex->func != CURTAIN_OPEN ||
                            p_cfg_ex->group != p_cfg->group) {
                            continue;
                        }
                        panel_fast_exe(p_status_ex, 0b00110000 | 0x00);
                    });
                    panel_fast_exe(p_status, 0b00101010 | 0x01);
                }
                break;
            case CURTAIN_OPEN: // 勾选"总开关"的窗帘开
                if (data->data[2]) {
                    PROCESS_INNER(temp_cfg, temp_status, {
                        if (p_cfg_ex->func != CURTAIN_CLOSE ||
                            p_cfg_ex->group != p_cfg->group) {
                            continue;
                        }
                        panel_fast_exe(p_status_ex, 0b00110000 | 0x00);
                    });
                    panel_fast_exe(p_status, 0b00101010 | 0x01);
                }
                break;
            default:
                break;
        }
    });
}

static void panel_clean_room(FUNC_PARAMS) // 清理房间
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        if (p_cfg->func != CLEAN_ROOM ||
            (data->data[3] != p_cfg->group && data->data[3] != 0xFF)) {
            continue;
        }
        if (data->data[2]) {
            PROCESS_INNER(temp_cfg, temp_status, {
                if (p_cfg_ex->func != DND_MODE ||
                    (data->data[3] != p_cfg_ex->group && data->data[3] != 0xFF)) {
                    continue;
                }
                panel_fast_exe(p_status_ex, 0b00010110 | (~data->data[2] & 0x01));
            });
        }
        panel_fast_exe(p_status, 0b00010110 | (data->data[2] & 0x01));
    });
}

static void panel_dnd_mode(FUNC_PARAMS) // 勿扰模式
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        if (p_cfg->func != DND_MODE ||
            (data->data[3] != p_cfg->group && data->data[3] != 0xFF)) {
            continue;
        }
        if (data->data[2]) {
            PROCESS_INNER(temp_cfg, temp_status, {
                if (p_cfg_ex->func != CLEAN_ROOM ||
                    (data->data[3] != p_cfg_ex->group && data->data[3] != 0xFF)) {
                    continue;
                }
                panel_fast_exe(p_status_ex, 0b00010110 | (~data->data[2] & 0x01));
            });
        }
        panel_fast_exe(p_status, 0b00010110 | (data->data[2] & 0x01));
    });
}

static void panel_later_mode(FUNC_PARAMS) // 请稍后
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        if (p_cfg->func != LATER_MODE ||
            (data->data[3] != p_cfg->group && data->data[3] != 0xFF)) {
            continue;
        }
        panel_fast_exe(p_status, 0b00100110 | (data->data[2] & 0x01));
    });
}

static void panel_chect_out(FUNC_PARAMS) // 退房
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        if (p_cfg->func != CHECK_OUT ||
            (data->data[3] != p_cfg->group && data->data[3] != 0xFF)) {
            continue;
        }
        panel_fast_exe(p_status, 0b00010110 | (data->data[2] & 0x01));
    });
}

static void panel_sos_mode(FUNC_PARAMS) // 紧急呼叫
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        if (p_cfg->func != SOS_MODE ||
            (data->data[3] != p_cfg->group && data->data[3] != 0xFF)) {
            continue;
        }
        panel_fast_exe(p_status, 0b00010110 | (data->data[2] & 0x01));
    });
}

static void panel_service(FUNC_PARAMS) // 请求服务
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        if (p_cfg->func != SERVICE ||
            (data->data[3] != p_cfg->group && data->data[3] != 0xFF)) {
            continue;
        }
        panel_fast_exe(p_status, 0b00010110 | (data->data[2] & 0x01));
    });
}

static void panel_curtain_open(FUNC_PARAMS) // 窗帘开
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        if (p_cfg->func != CURTAIN_OPEN ||
            (data->data[3] != p_cfg->group && data->data[3] != 0xFF)) {
            continue;
        }
        PROCESS_INNER(temp_cfg, temp_status, {
            if (p_cfg_ex->func != CURTAIN_CLOSE ||
                (data->data[3] != p_cfg_ex->group && data->data[3] != 0xFF)) {
                continue;
            }
            panel_fast_exe(p_status_ex, 0b00110000 | 0x00);
        });
        panel_fast_exe(p_status, 0b00101010 | 0x01);
    });
}

static void panel_curtain_close(FUNC_PARAMS) // 窗帘关
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        if (p_cfg->func != CURTAIN_CLOSE ||
            (data->data[3] != p_cfg->group && data->data[3] != 0xFF)) {
            continue;
        }
        PROCESS_INNER(temp_cfg, temp_status, {
            if (p_cfg_ex->func != CURTAIN_OPEN ||
                (data->data[3] != p_cfg_ex->group && data->data[3] != 0xFF)) {
                continue;
            }
            panel_fast_exe(p_status_ex, 0b00110000 | 0x00);
        });
        panel_fast_exe(p_status, 0b00101010 | 0x01);
    });
}

static void panel_curtain_stop(FUNC_PARAMS) // 窗帘停
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        if (((p_cfg->func != CURTAIN_OPEN) && (p_cfg->func != CURTAIN_CLOSE)) ||
            (data->data[3] != p_cfg->group && data->data[3] != 0xFF) ||
            !p_status->relay_short) {
            continue;
        }
        panel_fast_exe(p_status, 0b00111010 | 0x00);
    });
}

static void panel_scene_mode(FUNC_PARAMS) // 场景模式
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        if (L_BIT(data->data[4]) != 0xF &&
            L_BIT(data->data[4]) != L_BIT(p_cfg->area)) { // 匹配场景区域
            continue;
        }
        // 关闭此面板的该"场景区域"的所有按键
        panel_fast_exe(p_status, 0b00010110 | 0x00);

        uint8_t mask = data->data[7] & p_cfg->scene_group;
        if (!mask) { // 若未勾选该场景分组,跳过
            continue;
        }
        switch (p_cfg->func) {
            case CURTAIN_OPEN: // 勾选此场景的"窗帘开"
                if (data->data[2]) {
                    PROCESS_INNER(temp_cfg, temp_status, {
                        if (p_cfg_ex->func == CURTAIN_CLOSE &&
                            p_cfg_ex->group == p_cfg->group) {
                            panel_fast_exe(p_status_ex, 0b00110000 | 0x00);
                        }
                    });
                    panel_fast_exe(p_status, 0b00101010 | 0x01);
                }
                break;
            case CURTAIN_CLOSE: // 勾选此场景的"窗帘关"
                if (!data->data[2]) {
                    PROCESS_INNER(temp_cfg, temp_status, {
                        if (p_cfg_ex->func == CURTAIN_OPEN &&
                            (data->data[3] == p_cfg_ex->group || data->data[3] == 0xFF)) {
                            panel_fast_exe(p_status_ex, 0b00110000 | 0x00);
                        }
                    });
                    panel_fast_exe(p_status, 0b00101010 | 0x01);
                }
                break;
            case LIGHT_MODE:
            case SCENE_MODE:
            case ALL_ON_OFF:
                panel_fast_exe(p_status, 0b00010110 | (data->data[2] & 0x01));
                break;
            default:
                break;
        }
    });
}

static void panel_light_mode(FUNC_PARAMS) // 灯控模式
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        if (p_cfg->func != LIGHT_MODE)
            continue;

        if (data->data[3] != p_cfg->group && data->data[3] != 0xFF)
            continue;

        if (src != this_device && data->data[3] == 0x00)
            continue;

        panel_fast_exe(p_status, 0x16 | (data->data[2] & 0x01));
    });
}

static void panel_night_light(FUNC_PARAMS) // 夜灯
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        if (p_cfg->func != NIGHT_LIGHT ||
            (data->data[3] != p_cfg->group && data->data[3] != 0xFF)) {
            continue;
        }
        panel_fast_exe(p_status, 0b00010110 | (data->data[2] & 0x01));
    });
}
#endif
