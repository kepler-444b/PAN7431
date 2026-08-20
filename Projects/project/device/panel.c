#include "panel.h"
#include "../app/adc.h"
#include "../app/base.h"
#include "../app/config.h"
#include "../app/eventbus.h"
#include "../app/protocol.h"
#include "../app/pwm_hw.h"
#include "../bsp/bsp_timer.h"
#include "../bsp/bsp_tm5020a.h"
#include "../bsp/bsp_uart.h"
#include "../bsp/bsp_zero.h"
#include "device_manager.h"
#include "pan211.h"

#if defined PANEL

#define W_R     0b00010110 // 控制白灯和继电器
#define WS_RS   0b00101010 // 控制白灯短亮,继电器短开
#define R_RS    0b00110010 // 控制继电器,继电器短开
#define WS_R    0b00011010 // 控制白灯短亮,继电器
#define R       0b00010000 // 控制继电器
#define R_K     0b00010010 // 控制继电器更新按键状态
#define WS_RS_R 0b00111010 // 控制白灯短亮,继电器短开,继电器

// BIT0:data->data[2]
// BIT1:k_status
// BIT2:w_cur
// BIT3:w_short
// BIT4:r_cur
// BIT5:r_short
// BIT6:reserve
// BIT7:reserve

#define FUNC_ARGS        data, temp_cfg, temp_status, data_src

#define FUNC_PARAMS      frame_t *data,               \
                         const panel_cfg_t *temp_cfg, \
                         panel_status_t *temp_status, \
                         data_source_e data_src

#define TIMER_PARAMS     const panel_cfg_t *temp_cfg, \
                         panel_status_t *temp_status, \
                         common_panel_t *temp_common

#define ADC_PARAMS       panel_status_t *temp_status, \
                         common_panel_t *temp_common, \
                         adc_value_t *adc_value

#define LONG_PRESS       60    // 长按时间
#define SHORT_TIME_LED   100   // 短亮时间
#define SHORT_TIME_RELAY 60000 // 继电器导通时间

#define PROCESS_OUTER(cfg, status, ...)                  \
    do {                                                 \
        for (uint8_t _i = 0; _i < CONFIG_NUMBER; _i++) { \
            const panel_cfg_t *p_cfg = &(cfg)[_i];       \
            panel_status_t *p_status = &(status)[_i];    \
            __VA_ARGS__                                  \
        }                                                \
    } while (0)

#define PROCESS_INNER(cfg_ex, status_ex, ...)               \
    do {                                                    \
        for (uint8_t _j = 0; _j < CONFIG_NUMBER; _j++) {    \
            const panel_cfg_t *p_cfg_ex = &(cfg_ex)[_j];    \
            panel_status_t *p_status_ex = &(status_ex)[_j]; \
            __VA_ARGS__                                     \
        }                                                   \
    } while (0)

#define VOL_BUF_SIZE 10  // 电压值缓冲区数量
#define MIN_VOL      329 // 无按键按下时的最小电压值
#define MAX_VOL      330 // 无按键按下时的最大电压值

#define LOCK_COUNT   40 // 锁键时间(200ms)

typedef struct
{
    uint16_t min;
    uint16_t max;
} key_vol_t;

typedef struct
{ // 用于每个按键的状态

    bool k_press;  // 按键按下
    bool k_status; // 按键状态

    bool w_cur;   // 白灯状态
    bool w_short; // 启用短亮(窗帘开关)
    bool w_last;  // 白灯上次状态

    bool y_cur;  // 黄灯状态
    bool y_last; // 黄灯上次状态

    bool r_cur;   // 继电状态
    bool r_short; // 继电器短开
    bool r_last;  // 继电器上次状态

    uint16_t w_short_count;    // 短亮计数器
    uint32_t r_short_count;    // 继电器短开计数器
    const key_vol_t vol_range; // 按键电压范围
} panel_status_t;

typedef struct
{
    bool bl_close; // 关闭背光
    bool bl_open;  // 开启背光
    bool bl_status;
    uint16_t bl_delay_count; // 关闭背光

    bool bl_close_no;           // 关闭背光
    bool bl_open_no;            // 开启背光
    uint16_t bl_no_delay_count; // 关闭背光

    bool all_close_no; // 总关状态(无需唤醒)
    bool all_close_no_last;

    bool all_close;      // 总关状态(需要唤醒)
    bool all_close_last; // 上次总关状态

    bool all_off_black; // 总关,且关闭所有背光灯
    bool all_dim_black; // 总关,且调暗所有背光灯

    bool check_w_led;           // 检测 LED
    uint16_t check_w_led_count; // 延时检测 LED

    bool curtain_open;                       // 窗帘开(迎宾)
    uint8_t curtain_open_idx[CONFIG_NUMBER]; // 用于存放窗帘开(迎宾)的按键索引
    uint16_t curtain_open_count;

    bool led_filck;           // 闪烁
    bool key_long_press;      // 长按状态
    bool enter_config;        // 进入配置状态
    uint16_t key_long_count;  // 长按计数
    uint16_t led_filck_count; // 闪烁计数
    bool remove_card;
    uint16_t remove_card_count;

    uint8_t key_lock_count; // 锁键时间
    bool key_locked;        // 锁键
} common_panel_t;

typedef struct
{
    uint8_t buf_idx; // 缓冲区下标
    uint16_t vol;    // 电压值
    uint16_t vol_buf[VOL_BUF_SIZE];
} adc_value_t;

static panel_status_t my_panel_status[CONFIG_NUMBER] = {
    PANEL_VOL_RANGE_DEF,
};

typedef enum {
    OTHER,
    THIS
} data_source_e;

typedef enum {
    CLOSE,
    SAVE,
    LOAD,
} led_statue_e;

typedef enum {
    BACKLIGHT_DIM,     // 调暗背光
    BACKLIGHT_OFF,     // 总关关闭背光
    BACKLIGHT_OFF_DIM, // 总关调暗背光
    BACKLIGHT_ON       // 开启背光

} backlight_mode_e;

typedef enum {
    PANEL_DO_KEY_LIGHT  = 0, // 控制灯,不控制继电器(普通映射)
    PANEL_DO_KEY_RELAY  = 1, // 控制灯并控制继电器(主按键)
    PANEL_DO_RELAY_ONLY = 2  // 只控制继电器，不控制灯
} panel_action_e;

static void panel_read_adc(void *arg);
static void panel_proce_cmd(void *arg);
static void panel_event_handler(event_type_e event, void *params);
static void process_led_flicker(common_panel_t *common_panel);
static void panel_fast_exe(uint8_t flag, uint8_t idx);
static void process_panel_simulate(uint8_t key_number, uint8_t status);
static void panel_backlight_status(backlight_mode_e mode);
static void panel_ctrl_led_all(bool led_state);
static void panel_power_status(void);
static void panel_insert_card(void);
static void panel_remove_card(void);
static void panel_bl_close(void);
static void panel_exe_led_status(led_statue_e type);
static void panel_data_cb(frame_t *data, data_source_e data_src);
static void set_panel_status(uint8_t idx, uint8_t flag, panel_action_e action);
static void panel_key_map(panel_status_t *temp_status, uint8_t cmd_idx, uint8_t main_key, uint8_t slave_key, bool is_toggle);
static bool panel_find_sim_key(frame_t *data);
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
static void panel_dimming4_mode(FUNC_PARAMS);
static void panel_night_light(FUNC_PARAMS);

static common_panel_t my_common_panel;
static adc_value_t my_adc_value;
static bool save_led_status[CONFIG_NUMBER];
static bool seve_key_status[CONFIG_NUMBER];

void panel_device_init(void)
{
    bsp_panel_init();
    app_adc_init();
    bsp_start_timer(2, 5, panel_read_adc, NULL, TMR_AUTO_MODE);
    bsp_start_timer(3, 1, panel_proce_cmd, NULL, TMR_AUTO_MODE);
    app_eventbus_subscribe(panel_event_handler);
    pwm_hw_init();

#if defined ZERO_ENABLE
    bsp_zero_init();
#endif

#if defined PANEL_A20
    app_pwm_hw_add_pin(PWM_PB3);
    app_pwm_hw_add_pin(PWM_PA8);
    app_pwm_hw_add_pin(PWM_PA10);
    app_pwm_hw_add_pin(PWM_PA11);
    app_pwm_hw_add_pin(PWM_PB4);
    app_pwm_hw_add_pin(PWM_PB5);

    const panel_cfg_t *p_cfg = app_get_panel_cfg();
    for (uint8_t i = 0; i < CONFIG_NUMBER; i++) {
        app_set_pwm_hw_fade(p_cfg[i].led_y_pin, 2400, 1000, 5, 1);
    }
#else
    app_pwm_hw_add_pin(PWM_PA8);
    app_set_pwm_hw_fade(PWM_PA8, 2400, 1000, 5, 1);
#endif
    if (DEVICE_PANEL == app_get_panel_type()) { // 只有受控电面板上电执行"迎宾"
        panel_power_status();
    } else {
        my_common_panel.check_w_led = true; // 检查开启的白灯
    }
    APP_PRINTF("panel_device_init\n");
}
static void panel_read_adc(void *arg)
{
#if 0 // 庞洛洛让我测试定时开关
    static uint16_t count;
    static bool status;
    count++;
    if (count > 100) {
        const panel_cfg_t *p_cfg = app_get_panel_cfg();

        status = !status;
        count  = 0;

        APP_SET_GPIO(p_cfg->relay_pin[0], status);
    }
#endif
    uint16_t adc_value = app_get_adc_value();
    if (adc_value != ERR_VOL) { // Invalid Voltage
        my_adc_value.vol = adc_value;
        process_panel_adc(my_panel_status, &my_common_panel, &my_adc_value);
    }
    if (my_common_panel.led_filck) {
        process_led_flicker(&my_common_panel);
    } else {
        return;
    }
}

static void process_panel_adc(ADC_PARAMS)
{
    // 锁键
    if (temp_common->key_locked) {
        temp_common->key_lock_count++;
        if (temp_common->key_lock_count >= LOCK_COUNT) {
            temp_common->key_lock_count = 0;
            temp_common->key_locked     = false;
        }
        return;
    }

    // APP_PRINTF("vol:%d\n", adc_value->vol);
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
            APP_PRINTF("i:%d\n", i);
            temp_common->key_locked = true; // 锁键

            const panel_cfg_t *temp_cfg = app_get_panel_cfg();
            // 外层根据条件设置是否允许翻转
            bool is_toggle = true;

            bool special_key = (BIT2(temp_cfg[i].perm) && !BIT4(temp_cfg[i].perm) && (temp_cfg[i].func != ALL_CLOSE && temp_cfg[i].func != ALL_ON_OFF));
            if ((temp_common->all_close) && !special_key) {
                // 在"总关"状态下且不是特殊按键,则不反转状态
                is_toggle = false;
            }
            const uint8_t sim_key_number = app_get_sim_key_number();

            switch (sim_key_number) {
                case SIM_1KEY: {
                    const uint8_t key_pairs[][2] = {{0, 1}, {0, 2}, {0, 3}, {0, 4}, {0, 5}}; // 映射关系
                    for (int pair_idx = 0; pair_idx < CONFIG_NUMBER; pair_idx++) {
                        uint8_t main_key   = key_pairs[pair_idx][0];
                        uint8_t mapped_key = key_pairs[pair_idx][1];
                        if (i == main_key || i == mapped_key) {
                            panel_key_map(temp_status, 0, main_key, mapped_key, is_toggle);
                            break;
                        }
                    }
                    break;
                }
                case SIM_2KEY: {
#if defined PANEL_TD
                    const uint8_t key_pairs[][2] = {{0, 3}, {1, 2}}; // 映射关系
#else
                    const uint8_t key_pairs[][2] = {{0, 1}, {2, 3}}; // 映射关系
#endif
                    for (int pair_idx = 0; pair_idx < CONFIG_NUMBER; pair_idx++) {
                        uint8_t main_key   = key_pairs[pair_idx][0]; // 主按键
                        uint8_t mapped_key = key_pairs[pair_idx][1]; // 从键
                        if (i == main_key || i == mapped_key) {
                            panel_key_map(temp_status, pair_idx, main_key, mapped_key, is_toggle);
                            break;
                        }
                    }
                    break;
                }
                case SIM_3KEY: {
#if defined PANEL_TD
                    const uint8_t key_pairs[][2] = {{0, 3}, {4, 5}, {2, 1}}; // 映射关系
#else
                    const uint8_t key_pairs[][2] = {{0, 1}, {4, 5}, {2, 3}}; // 映射关系
#endif
                    for (int pair_idx = 0; pair_idx < CONFIG_NUMBER; pair_idx++) {
                        uint8_t main_key   = key_pairs[pair_idx][0]; // 主按键
                        uint8_t mapped_key = key_pairs[pair_idx][1]; // 从键
                        if (i == main_key || i == mapped_key) {
                            panel_key_map(temp_status, pair_idx, main_key, mapped_key, is_toggle);
                            break;
                        }
                    }
                    break;
                }
                case SIM_4KEY:
                case SIM_6KEY:
                    panel_key_map(temp_status, i, i, 0xFF, is_toggle);
                    break;
            }

            temp_common->key_long_press = true;
            temp_common->key_long_count = 0;
            continue;
        }
        // 处理长按
        if (temp_common->key_long_press && ++temp_common->key_long_count >= LONG_PRESS) {
            app_eventbus_publish(EVENT_REQUEST_NETWORK, NULL);
            temp_common->key_long_press = false;
        }
    }
}

// 处理面板按键映射关系
static void panel_key_map(panel_status_t *temp_status, uint8_t cmd_idx, uint8_t main_key, uint8_t slave_key, bool is_toggle)
{
    // APP_PRINTF("cmd_idx:%d main_key:%d slave_key:%d\n", cmd_idx, main_key, slave_key);

    bool is_special = temp_status[cmd_idx].r_short; // 如果当前按键的继电器处于"短开"状态,则标记为"特殊命令"
    APP_PRINTF("is_toggle:%d\n", is_toggle);
    if (is_toggle) { // 切换按键状态
        temp_status[main_key].k_status ^= 1;
        if (slave_key != 0xFF) { // 0xFF 表示没有从键
            temp_status[slave_key].k_status ^= 1;
        }
    } else {
        temp_status[main_key].k_status = false;
        if (slave_key != 0xFF) { // 0xFF 表示没有从键
            temp_status[slave_key].k_status = false;
        }
    }

    // 发送命令
    app_send_cmd(cmd_idx, (is_special ? 0x00 : temp_status[main_key].k_status), PANEL_HEAD, (is_special ? SPECIAL_CMD : COMMON_CMD));
    APP_PRINTF(" temp_status[main_key].k_status:%d\n", temp_status[main_key].k_status);
    temp_status[main_key].k_press = true;
    if (slave_key != 0xFF) {
        temp_status[slave_key].k_press = true;
    }
}

static void process_panel_simulate(uint8_t key_number, uint8_t status)
{
    APP_PRINTF("key_number:%d status:%d\n", key_number, status);
    my_panel_status[key_number].k_status = status;
    APP_PRINTF("k_status:%d\n", my_panel_status[key_number].k_status);

    const bool is_special = my_panel_status[key_number].r_short;
    app_send_cmd(key_number, (is_special ? 0x00 : my_panel_status[key_number].k_status), PANEL_HEAD, (is_special ? SPECIAL_CMD : COMMON_CMD));
}

static void panel_proce_cmd(void *arg)
{
    const panel_cfg_t *temp_cfg = app_get_panel_cfg();
    process_exe_status(temp_cfg, my_panel_status, &my_common_panel);
}

static void process_exe_status(TIMER_PARAMS)
{
    // 进入总关模式(需要唤醒) =======================================================================================
    if (temp_common->all_close != temp_common->all_close_last) {
        if (temp_common->all_close) {
            temp_common->bl_close = true;

        } else {
            if (!temp_common->bl_status) { // 此时,LED状态可能已经被"备用"键恢复
                panel_exe_led_status(LOAD);
            }
            panel_bl_close(); // 执行背光总关
            temp_common->bl_open = true;
        }
        temp_common->all_close_last = temp_common->all_close;
    }
    // 关闭背光灯(需要唤醒)
    if (temp_common->bl_close) {
        temp_common->bl_delay_count++;
        if (temp_common->bl_delay_count == 1000) { // 开始关闭背光灯

            // 需要唤醒
            if (temp_common->all_close) { // 注:在计时期间,可能会取消总关,此时需要结束计时cxR
                if (temp_common->all_off_black) {
                    panel_backlight_status(BACKLIGHT_OFF);
                } else if (temp_common->all_dim_black) {
                    panel_backlight_status(BACKLIGHT_OFF_DIM);
                }

            } else if (!temp_common->all_close) {
                temp_common->bl_close       = false;
                temp_common->bl_delay_count = 0;
            }
        }
        if (temp_common->bl_delay_count == 4000) { // 关闭指示灯

            // 需要唤醒
            if (temp_common->all_close) {
                panel_exe_led_status(SAVE);
                panel_exe_led_status(CLOSE);
                APP_PRINTF("temp_common->all_close_no\n");
            }
            temp_common->bl_close       = false;
            temp_common->bl_delay_count = 0;
            temp_common->bl_status      = false;
        }
    }
    // 开启背光灯(无需唤醒)
    if (temp_common->bl_open) {
        panel_backlight_status(BACKLIGHT_ON);
        temp_common->bl_open        = false;
        temp_common->bl_delay_count = 0;
        temp_common->bl_status      = true;
    }

    // 进入总关模式(无需唤醒) =======================================================================================
    if (temp_common->all_close_no != temp_common->all_close_no_last) { // 进入总关模式(无需唤醒)
        APP_PRINTF("all_close_no:%d\n", temp_common->all_close_no);
        if (temp_common->all_close_no) {
            temp_common->bl_close_no = true;
        } else {
            panel_exe_led_status(LOAD);
        }
        temp_common->bl_open_no = true;

        temp_common->all_close_no_last = temp_common->all_close_no;
    }
    // 关闭背光灯(无需唤醒)
    if (temp_common->bl_close_no) {
        temp_common->bl_no_delay_count++;
        // 开始关闭背光灯
        if (temp_common->bl_no_delay_count == 1000) {
            if (temp_common->all_close_no) {
                if (temp_common->all_off_black) {
                    panel_backlight_status(BACKLIGHT_OFF);
                } else if (temp_common->all_dim_black) {
                    panel_backlight_status(BACKLIGHT_OFF_DIM);
                }

            } else if (!temp_common->all_close_no) { // 打断计数
                temp_common->bl_close_no       = false;
                temp_common->bl_no_delay_count = 0;
            }
        }
        // 关闭指示灯
        if (temp_common->bl_no_delay_count == 4000) {

            if (temp_common->bl_close_no) {
                panel_exe_led_status(SAVE);
                panel_exe_led_status(CLOSE);
            }
            temp_common->bl_close_no       = false;
            temp_common->bl_no_delay_count = 0;
        }
    }
    // 开启背光灯(无需唤醒)
    if (temp_common->bl_open_no) {
        panel_backlight_status(BACKLIGHT_ON);
        temp_common->bl_open_no        = false;
        temp_common->bl_no_delay_count = 0;
    }

    if (temp_common->check_w_led) { // 检查白灯个数
        if (++temp_common->check_w_led_count >= 1000) {
            bool has_w_led_on = false;
            // const uint8_t sim_key_num = app_get_sim_key_number();
            for (uint8_t i = 0; i < CONFIG_NUMBER; i++) {
                if (temp_status[i].w_cur) {
                    has_w_led_on = true;
                    break;
                }
            }
            if (!has_w_led_on) {
                if (!my_common_panel.all_close && !my_common_panel.all_close_no) {
                    panel_backlight_status(BACKLIGHT_DIM);
                } else if (my_common_panel.all_close) {
                    if (my_common_panel.all_off_black) {
                        panel_backlight_status(BACKLIGHT_OFF);
                    } else if (my_common_panel.all_dim_black) {
                        panel_backlight_status(BACKLIGHT_OFF_DIM);
                    }
                }
            }
            temp_common->check_w_led       = false;
            temp_common->check_w_led_count = 0;
        }
    }

    if (temp_common->curtain_open) { // 延时执行"窗帘开"的迎宾状态
        if (++temp_common->curtain_open_count >= 3000) {
            for (uint8_t i = 0; i < CONFIG_NUMBER; i++) {
                if (temp_common->curtain_open_idx[i] == true) {
                    panel_fast_exe(WS_RS | 0x01, i);
                }
            }
            temp_common->curtain_open = false;
            temp_common->check_w_led  = true; // 检查开启的白灯
        }
    }

    if (temp_common->remove_card) {
        temp_common->remove_card_count++;
        if (temp_common->remove_card_count == 27000) { // 执行拔卡动作
            panel_remove_card();
        }
        if (temp_common->remove_card_count == 28000) { // 多延时1ms后,关闭背光灯
            panel_backlight_status(BACKLIGHT_OFF);
            temp_common->remove_card       = false;
            temp_common->remove_card_count = 0;
        }
    }

    PROCESS_OUTER(temp_cfg, temp_status, {
        if (p_status->w_short) { // 更新白灯短亮
            if (p_status->w_short_count++ == 0) {
                p_status->w_cur = true;
            }
            if (p_status->w_short_count >= SHORT_TIME_LED) {
                p_status->w_cur         = false;
                p_status->w_short       = false;
                p_status->w_short_count = 0;
            }
        }
        if (p_status->r_short) { // 更新继电器短开
            if (p_status->r_short_count++ == 0) {
                p_status->r_cur = true;
            }
            if (p_status->r_short_count >= SHORT_TIME_RELAY) {
                p_status->r_cur         = false;
                p_status->r_short       = false;
                p_status->w_cur         = false;
                p_status->k_status      = false;
                p_status->r_short_count = 0;
            }
        }
        if (p_status->w_cur != p_status->w_last) { // 更新白灯状态
#if defined PANEL_A20
            APP_SET_GPIO(p_cfg->led_w_pin, p_status->w_cur);
            if (!my_common_panel.all_close && !my_common_panel.all_close_no) { // 如果是在"总关状态"下,则不再点亮背光灯
                app_set_pwm_hw_fade(p_cfg->led_y_pin, p_status->w_cur ? 0 : 2400, 500, 5, 1);
            }
#else
            APP_SET_GPIO(p_cfg->led_w_pin, !p_status->w_cur);
#endif
            p_status->w_last = p_status->w_cur;
        }

        if (p_status->r_cur != p_status->r_last) { // 更新继电器状态
            for (uint8_t i = 0; i < RELAY_NUMBER; i++) {
                if (!app_gpio_equal(p_cfg->relay_pin[i], DEF)) { // 只执行勾选了的继电器

#if defined ZERO_ENABLE
                    zero_set_gpio(p_cfg->relay_pin[i], p_status->r_cur);
#else
                    APP_SET_GPIO(p_cfg->relay_pin[i], p_status->r_cur);
#endif
                }
            }
            p_status->r_last = p_status->r_cur;
        }
    });
}

// 保存 LED 状态
static void panel_exe_led_status(led_statue_e type)
{
    for (uint8_t i = 0; i < CONFIG_NUMBER; i++) {
        switch (type) {
            case CLOSE:
                my_panel_status[i].w_cur    = false;
                my_panel_status[i].k_status = false;
                break;
            case LOAD:
                my_panel_status[i].w_cur    = save_led_status[i];
                my_panel_status[i].k_status = seve_key_status[i];
                break;
            case SAVE:
                save_led_status[i] = my_panel_status[i].w_cur;
                seve_key_status[i] = my_panel_status[i].k_status;
                break;
        }
    }
}

static void panel_backlight_status(backlight_mode_e mode)
{
    const panel_cfg_t *p_cfg = app_get_panel_cfg();

    switch (mode) {
        case BACKLIGHT_DIM: { // 没有白灯亮的时候,将整体背光调暗
            APP_PRINTF("BACKLIGHT_DIM\n");
#if defined PANEL_A20

            const panel_cfg_t *p_cfg = app_get_panel_cfg();
            for (uint8_t i = 0; i < CONFIG_NUMBER; i++) {
                if (!my_panel_status[i].w_cur) {
                    app_set_pwm_hw_fade(p_cfg[i].led_y_pin, 500, 3000, 5, 1);
                }
            }
#else
            app_set_pwm_hw_fade(PWM_PA8, 500, 3000, 5, 1);
#endif
        } break;
        case BACKLIGHT_ON: // 有白灯亮的时候,将整体背光调亮
            APP_PRINTF("BACKLIGHT_ON\n");
#if defined PANEL_A20
            for (uint8_t i = 0; i < CONFIG_NUMBER; i++) {
                if (!my_panel_status[i].w_cur) {
                    app_set_pwm_hw_fade(p_cfg[i].led_y_pin, 2400, 3000, 5, 1);
                }
            }
#else
            app_set_pwm_hw_fade(PWM_PA8, 2400, 3000, 5, 1);
#endif
            break;
        case BACKLIGHT_OFF: // "总关"下,关闭所有背光
            APP_PRINTF("BACKLIGHT_OFF\n");
#if defined PANEL_A20
            for (uint8_t i = 0; i < CONFIG_NUMBER; i++) {
                app_set_pwm_hw_fade(p_cfg[i].led_y_pin, 0, 3000, 5, 1);
            }
#else
            app_set_pwm_hw_fade(PWM_PA8, 0, 3000, 5, 1);
#endif
            break;

        case BACKLIGHT_OFF_DIM: // 总关下,调暗所有背光
#if defined PANEL_A20
            for (uint8_t i = 0; i < CONFIG_NUMBER; i++) {
                app_set_pwm_hw_fade(p_cfg[i].led_y_pin, 500, 3000, 5, 1);
            }
#else
            app_set_pwm_hw_fade(PWM_PA8, 500, 3000, 5, 1);

#endif
            break;
        default:
            return;
    }
}

static void panel_data_cb(frame_t *data, data_source_e data_src)
{
    // 收到插卡取电数据
    if (data->data[0] == CARD_HEAD && data->data[1] == CARD_CMD) {
        uint8_t device_type = app_get_panel_type();
        // 拔卡
        if (data->data[3] == 0x00) {
            APP_PRINTF("remove_card\n");
            if (device_type == DEVICE_PANEL_AP) { // 常供电设备,收到拔卡信息,延时30s执行
                my_common_panel.remove_card       = true;
                my_common_panel.remove_card_count = 0;
            } else if (device_type == DEVICE_PANEL) { // 受控电设备,立即执行
                panel_remove_card();
            }
        }
        // 插卡
        else {
            if (device_type == DEVICE_PANEL_AP) {  // 长供电面板,接收"插卡"指令
                if (my_common_panel.remove_card) { // 如果正在延时执行"拔卡"动作,则取消,执行插卡动作
                    APP_PRINTF("interrupt remove_card\n");
                    my_common_panel.remove_card       = false;
                    my_common_panel.remove_card_count = 0;
                }
                APP_PRINTF("insert_card\n");
                panel_insert_card();
            }
        }
    }
    // 收到面板数据
    if (data->data[0] == PANEL_HEAD) {

        // APP_PRINTF_BUF("panel_rx", data->data, data->length);
        if ((data->data[2] == 0x80) || (data->data[2] == 0x81) || (data->data[2] == 0x82) ||
            (data->data[2] == 0x83) || (data->data[2] == 0x84)) {
            // 过滤温控器的命令
            return;
        }
        const panel_cfg_t *temp_cfg = app_get_panel_cfg();
        process_cmd_check(data, temp_cfg, my_panel_status, data_src);
    }
    // 产测面板,接收过零测试信号
    if (data->data[0] == PANEL_TEST_CMD) {
#if defined PANEL_DIS
        return;
#endif
        static bool status = false;

        status = !status;
        zero_set_gpio_check(PB1, status);
        zero_set_gpio_check(PA5, status);
        zero_set_gpio_check(PA6, status);
        zero_set_gpio_check(PB0, status);
    }
}

// 查找本面板中是否有相同的按键
static bool panel_find_sim_key(frame_t *data)
{
    bool ret = false;

    const panel_cfg_t *temp_cfg  = app_get_panel_cfg();
    const uint8_t sim_key_number = app_get_sim_key_number();
    for (uint8_t i = 0; i < sim_key_number; i++) {

        bool special     = (BIT2(temp_cfg[i].perm) && !BIT4(temp_cfg[i].perm));
        bool group_match = (data->data[3] == temp_cfg[i].group);

        if (!special) { // 如果不是特殊按键,跳过
            continue;
        }
        if (!group_match) { // 不是同一分组跳过;
            continue;
        }
        if ((data->data[1] == CURTAIN_STOP) && (temp_cfg[i].func == CURTAIN_OPEN || temp_cfg[i].func == CURTAIN_CLOSE)) {
            ret = true;
        }
        if (data->data[1] == temp_cfg[i].func) {
            ret = true;
        }
    }
    return ret;
}

static void process_cmd_check(FUNC_PARAMS)
{
    bool skip_outer = false;

    if (my_common_panel.all_close_no == true) { // 当前处于"总关"模式,且无需唤醒,任意键退出该模式
        APP_PRINTF("all_close_no\n");
        my_common_panel.all_close_no      = false;
        my_common_panel.all_close_no_last = false;
        my_common_panel.bl_open_no        = true; // 开启背光(无需唤醒)

        panel_exe_led_status(LOAD); // 恢复被总关(无需唤醒)关闭的指示灯
    }
    if (my_common_panel.all_close == true) { // 当前是总关模式
        APP_PRINTF("all_close\n");

        // 如果是勾选了"备用"且没有勾选"只开",且该按键不是"总关"或"总开关",这种按键为特殊按键(立即执行动作,而不唤醒任何面板)
        bool src_special_key = (BIT2(data->data[6]) && !BIT4(data->data[6]) && (data->data[1] != ALL_CLOSE && data->data[1] != ALL_ON_OFF));
        bool my_special_key  = panel_find_sim_key(data);

        if (src_special_key && !my_special_key) { // 其他面板的特殊按键,且自身没有相同的按键
            return;
        }

        if (src_special_key && my_special_key) { // 自身触发的是特殊按键,或其他面板的特殊按键但与自身双控
            if (!my_common_panel.bl_status) {
                panel_exe_led_status(LOAD);
            }
            if (my_common_panel.bl_close) { // 若此时正在执行"总关",则取消关闭背光灯
                my_common_panel.bl_close       = false;
                my_common_panel.bl_delay_count = 0;
            }

            my_common_panel.check_w_led = true;
            skip_outer                  = true; // 标记跳过执行"背光总关"
        }
        // 来自任意键(唤醒),关闭已经打开的夜灯
        if (!skip_outer) {

            APP_PRINTF("wake up\n");
            my_common_panel.all_close   = false; // 唤醒(执行"背光总关")
            my_common_panel.check_w_led = true;  // 检查白灯

            return;
        }
    }
    APP_PRINTF("no wake up\n");
    // my_common_panel.bl_open_no  = true; // 开启背光(无需唤醒)
    my_common_panel.bl_open     = true; // 开启背光(需要唤醒)
    my_common_panel.check_w_led = true; // 检查白灯

    switch (data->data[1]) {
        case ALL_CLOSE:
            panel_all_close(FUNC_ARGS);
            break;
        case ALL_ON_OFF:
#if defined PANEL_DIS
            return;
#endif
            panel_all_on_off(FUNC_ARGS);
            break;
        case CLEAN_ROOM:
            panel_clean_room(FUNC_ARGS);
            break;
        case DND_MODE:
            panel_dnd_mode(FUNC_ARGS);
            break;
        case LATER_MODE:
            panel_later_mode(FUNC_ARGS);
            break;
        case CHECK_OUT:
            panel_chect_out(FUNC_ARGS);
            break;
        case SOS_MODE:
            panel_sos_mode(FUNC_ARGS);
            break;
        case SERVICE:
            panel_service(FUNC_ARGS);
            break;
        case CURTAIN_OPEN:
            panel_curtain_open(FUNC_ARGS);
            break;
        case CURTAIN_CLOSE:
            panel_curtain_close(FUNC_ARGS);
            break;
        case SCENE_MODE:
            panel_scene_mode(FUNC_ARGS);
            break;
        case LIGHT_MODE:
            panel_light_mode(FUNC_ARGS);
            break;
        case NIGHT_LIGHT:
            panel_night_light(FUNC_ARGS);
            break;
        case LIGHT_UP:
            break;
        case LIGHT_DOWN:
            break;
        case DIMMING_3:
            break;
        case DIMMING_4:
            panel_dimming4_mode(FUNC_ARGS);
            break;
        case UNLOCKING:
            break;
        case BLUETOOTH:
            break;
        case CURTAIN_STOP:
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
    if (common_panel->led_filck_count <= 50) {
        panel_ctrl_led_all(true);
    } else if (common_panel->led_filck_count <= 100) {
        panel_ctrl_led_all(false);
    } else if (common_panel->led_filck_count <= 150) {
        panel_ctrl_led_all(true);
    } else {
        common_panel->led_filck_count = 0;
        common_panel->led_filck       = false;
        panel_ctrl_led_all(false);
    }
}

static void panel_ctrl_led_all(bool led_state)
{
    for (uint8_t i = 0; i < CONFIG_NUMBER; i++) {
        my_panel_status[i].w_cur = led_state;
    }
}

static void panel_event_handler(event_type_e event, void *params)
{
    switch (event) {
        case EVENT_PANEL_RX: {
            frame_t *my_panel_frame = (frame_t *)params;
            panel_data_cb(my_panel_frame, OTHER);
        } break;
        case EVENT_PANEL_RX_MY: {
            frame_t *my_panel_frame = (frame_t *)params;
            panel_data_cb(my_panel_frame, THIS);
        } break;
        case EVENT_SIMULATE_CTRL: {
            frame_t *my_panel_frame = (frame_t *)params;
            process_panel_simulate(my_panel_frame->data[0] - 1, my_panel_frame->data[1]);
        } break;
        case EVENT_LED_BLINK: {
            my_common_panel.led_filck = true;
        } break;
        default:
            break;
    }
}
static void panel_power_status(void)
{
    my_common_panel.bl_open_no  = true; // 开启背光(无需唤醒)
    my_common_panel.bl_open     = true; // 开启背光(需要唤醒)
    my_common_panel.check_w_led = true; // 检查开启的白灯
    PROCESS_OUTER(app_get_panel_cfg(), my_panel_status, {
        if (!BIT3(p_cfg->perm)) {
            continue; // 权限是否勾选"迎宾"
        }
        switch (p_cfg->func) {
            case LIGHT_MODE:
            case DIMMING_4:
            case ALL_ON_OFF:
                panel_fast_exe(W_R | 0x01, _i);
                break;
            case SCENE_MODE: // 场景模式,不开白灯
                panel_fast_exe(R_K | 0x01, _i);
                break;
            case CURTAIN_OPEN: { // 窗帘开,需要延时3s执行

                if (!BIT6(p_cfg->perm)) { // 窗帘开,只勾选"迎宾",插卡打开
                    my_common_panel.curtain_open         = true;
                    my_common_panel.curtain_open_idx[_i] = true;
                }
            } break;

            case CURTAIN_CLOSE: { // 窗帘关,需要延时3s执行

                // 窗帘关,勾选"迎宾"+"取反",插卡关闭
                if (BIT6(p_cfg->perm)) {
                    my_common_panel.curtain_open         = true;
                    my_common_panel.curtain_open_idx[_i] = true;
                }

            } break;
            default:
                break;
        }
    });
    my_common_panel.check_w_led = true; // 检查开启的白灯
}

// 执行插卡"迎宾"
static void panel_insert_card(void)
{
    panel_power_status();
}

// 执行拔卡
static void panel_remove_card(void)
{
    APP_PRINTF("remove_card\n");
    PROCESS_OUTER(app_get_panel_cfg(), my_panel_status, {
        switch (p_cfg->func) {
            case CLEAN_ROOM: //(迎宾?关闭:保持)
                if (BIT3(p_cfg->perm)) {
                    p_status->k_status = false;
                    p_status->r_cur    = false;
                    p_status->w_cur    = false;
                }
                break;
            case DND_MODE: //(迎宾?保持:关闭)
                if (BIT3(!p_cfg->perm)) {
                    p_status->k_status = false;
                    p_status->r_cur    = false;
                    p_status->w_cur    = false;
                }
                break;
            case CURTAIN_OPEN:
                if (BIT3(p_cfg->perm) && BIT6(p_cfg->perm)) { // "窗帘开"勾选"迎宾"+"取反",拔卡开窗帘

                    PROCESS_INNER(app_get_panel_cfg(), my_panel_status, { // 先关闭同分组的"窗帘关"
                        bool func_match_ex  = (p_cfg_ex->func == CURTAIN_CLOSE);
                        bool group_match_ex = (p_cfg->group == p_cfg_ex->group);

                        if (!func_match_ex || !group_match_ex)
                            continue;
                        panel_fast_exe(R_RS | 0x00, _j); // 关闭同分组的窗帘关
                    });
                    panel_fast_exe(WS_RS | 0x01, _i);
                }
                break;
            case CURTAIN_CLOSE: {
                if (BIT3(p_cfg->perm) && !BIT6(p_cfg->perm)) { // "窗帘关"只勾选"迎宾",拔卡关

                    PROCESS_INNER(app_get_panel_cfg(), my_panel_status, { // 先关闭同分组的"窗帘开"
                        bool func_match_ex  = (p_cfg_ex->func == CURTAIN_OPEN);
                        bool group_match_ex = (p_cfg->group == p_cfg_ex->group);

                        if (!func_match_ex || !group_match_ex)
                            continue;
                        panel_fast_exe(R_RS | 0x00, _j); // 关闭同分组的窗帘关
                    });
                    panel_fast_exe(WS_RS | 0x01, _i);
                }
            } break;
            default:
                // p_status->k_status = false;
                p_status->r_cur = false;
                p_status->w_cur = false;
                break;
        }
    });
    my_common_panel.check_w_led = true; // 检查开启的白灯
}

static void panel_fast_exe(uint8_t flag, uint8_t idx)
{
    // BIT7:data->data[2]
    // BIT6:k_status
    // BIT5:w_cur
    // BIT4:w_short
    // BIT3:r_cur
    // BIT2:r_short
    // BIT1:reserve
    // BIT0:reserve
    const uint8_t sim_key_number = app_get_sim_key_number();
    switch (sim_key_number) {
        case SIM_1KEY: {
            if (idx == 0) {
                if (idx == 0) {
                    set_panel_status(0, flag, PANEL_DO_KEY_RELAY);
                    set_panel_status(1, flag, PANEL_DO_KEY_LIGHT);
                    set_panel_status(2, flag, PANEL_DO_KEY_LIGHT);
                    set_panel_status(3, flag, PANEL_DO_KEY_LIGHT);
                    set_panel_status(4, flag, PANEL_DO_KEY_LIGHT);
                    set_panel_status(5, flag, PANEL_DO_KEY_LIGHT);
                }
            } else {
                set_panel_status(idx, flag, PANEL_DO_RELAY_ONLY); // 借用继电器
            }
        } break;
        case SIM_2KEY: { // 2 键映射
            if (idx == 0 || idx == 1) {
#if defined PANEL_TD
                if (idx == 0) {
                    set_panel_status(0, flag, PANEL_DO_KEY_LIGHT);
                    set_panel_status(3, flag, PANEL_DO_KEY_LIGHT);

                } else if (idx == 1) {
                    set_panel_status(1, flag, PANEL_DO_KEY_LIGHT);
                    set_panel_status(2, flag, PANEL_DO_KEY_LIGHT);
                }
                set_panel_status(idx, flag, PANEL_DO_RELAY_ONLY); // 控制主按键的继电器
#else
                if (idx == 0) {
                    set_panel_status(0, flag, PANEL_DO_KEY_LIGHT);
                    set_panel_status(1, flag, PANEL_DO_KEY_LIGHT);

                } else if (idx == 1) {
                    set_panel_status(2, flag, PANEL_DO_KEY_LIGHT);
                    set_panel_status(3, flag, PANEL_DO_KEY_LIGHT);
                }
                set_panel_status(idx, flag, PANEL_DO_RELAY_ONLY); // 控制主按键的继电器
#endif
            } else {
                set_panel_status(idx, flag, PANEL_DO_RELAY_ONLY); // 借用继电器
            }
        } break;
        case SIM_3KEY: {

#if defined PANEL_TD
            if (idx == 0 || idx == 1 || idx == 2) {
                if (idx == 0) {
                    set_panel_status(0, flag, PANEL_DO_KEY_LIGHT);
                    set_panel_status(3, flag, PANEL_DO_KEY_LIGHT);
                } else if (idx == 1) {
                    set_panel_status(4, flag, PANEL_DO_KEY_LIGHT);
                    set_panel_status(5, flag, PANEL_DO_KEY_LIGHT);
                } else if (idx == 2) {
                    set_panel_status(2, flag, PANEL_DO_KEY_LIGHT);
                    set_panel_status(1, flag, PANEL_DO_KEY_LIGHT);
                }
                set_panel_status(idx, flag, PANEL_DO_RELAY_ONLY); // 控制主按键的继电器
            }
#else
            if (idx == 0 || idx == 1 || idx == 2) {
                if (idx == 0) {
                    set_panel_status(0, flag, PANEL_DO_KEY_LIGHT);
                    set_panel_status(1, flag, PANEL_DO_KEY_LIGHT);
                } else if (idx == 1) {
                    set_panel_status(4, flag, PANEL_DO_KEY_LIGHT);
                    set_panel_status(5, flag, PANEL_DO_KEY_LIGHT);
                } else if (idx == 2) {
                    set_panel_status(2, flag, PANEL_DO_KEY_LIGHT);
                    set_panel_status(3, flag, PANEL_DO_KEY_LIGHT);
                }
                set_panel_status(idx, flag, PANEL_DO_RELAY_ONLY); // 控制主按键的继电器
            }
#endif
            else {

                set_panel_status(idx, flag, PANEL_DO_RELAY_ONLY); // 借用继电器
            }
            break;
        }
        case SIM_4KEY:
        case SIM_6KEY:
            set_panel_status(idx, flag, PANEL_DO_KEY_RELAY);
            break;
        default:
            break;
    }
}

static void set_panel_status(uint8_t idx, uint8_t flag, panel_action_e action)
{
    const panel_cfg_t *temp_cfg = app_get_panel_cfg();

    // 特殊用法:只是输出继电器,如果当前的按键已经是打开的状态, R 会只输出继电器,而切掉其指示灯,按键的状态设置为"关闭"
    if ((flag & 0xFE) == R) { // (清掉 bit0 以后,其它位等于 R),只是继电器输出,要先切掉其白灯
        my_panel_status[idx].w_cur    = 0x00;
        my_panel_status[idx].k_status = 0x00;
    }

    // 灯逻辑
    if (action == PANEL_DO_KEY_LIGHT || action == PANEL_DO_KEY_RELAY) {

        if (BIT1(flag)) // 勾选了"k_status",按键的状态根据"data->data[2]"改变
            my_panel_status[idx].k_status = BIT0(flag);
        if (BIT2(flag)) // 勾选了"w_cur",白灯根据"data->data[2]"点亮
            my_panel_status[idx].w_cur = BIT0(flag);
        if (BIT3(flag)) // 勾选了"w_short",白灯短亮
            my_panel_status[idx].w_short = true;
    }

    // 继电器逻辑
    if (action == PANEL_DO_KEY_RELAY) {
        if (BIT1(flag))
            my_panel_status[idx].k_status = BIT0(flag);
        if (BIT4(flag))
            my_panel_status[idx].r_cur = BIT0(flag);
        if (BIT5(flag)) {
            my_panel_status[idx].r_short       = BIT0(flag);
            my_panel_status[idx].r_short_count = 0;
        }
    }

    // 借用继电器
    if (action == PANEL_DO_RELAY_ONLY) {
        if (BIT4(flag))
            my_panel_status[idx].r_cur = BIT0(flag);
        if (BIT5(flag)) {
            my_panel_status[idx].r_short       = BIT0(flag);
            my_panel_status[idx].r_short_count = 0;
        }
    }
}

static void panel_bl_close(void) // 执行背光总关(即起夜模式 或 唤醒模式)
{
    const panel_cfg_t *temp_cfg = app_get_panel_cfg();
    for (uint8_t i = 0; i < CONFIG_NUMBER; i++) {
        switch (temp_cfg[i].func) {
                // case NIGHT_LIGHT: { // 唤醒,关闭已经打开的夜灯
                //     panel_fast_exe(W_R | 0x00, i);
                // } break;

            case ALL_CLOSE: {
                if (BIT6(temp_cfg[i].perm)) {       // 勾选了"取反",即任意键开启"总关"的继电器
                    panel_fast_exe(WS_R | 0x01, i); // 任意键打开"总关"的继电器
                } else {                            // 未勾选"取反",即任意键开启,关闭"总关"的继电器
                    panel_fast_exe(WS_R | 0x00, i); // 关闭"总关"的继电器
                }
            } break;
            case ALL_ON_OFF: {
                if (BIT6(temp_cfg[i].perm)) {      // 勾选了"取反",即任意键开启"总开关"的继电器
                    panel_fast_exe(W_R | 0x01, i); // 任意键打开"总开关"的继电器
                } else {                           // 未勾选"取反",即任意键开启,关闭"总开关"的继电器
                    panel_fast_exe(W_R | 0x00, i); // 关闭"总开关"的继电器
                }
            } break;

            default: {
                if ((BIT1(temp_cfg[i].perm))) { // 其他按键勾选"总关背光"

                    if (BIT6(temp_cfg[i].perm)) {      // 勾选了"取反"
                        panel_fast_exe(W_R | 0x00, i); // 打开指示灯与继电器
                    } else {                           // 未勾选"取反"
                        panel_fast_exe(W_R | 0x01, i);
                    }
                }
            } break;
        }
#if 0
        if (temp_cfg[i].func == ALL_CLOSE) {
            if (BIT6(temp_cfg[i].perm)) {       // 勾选了"取反",即任意键开启"总关"的继电器
                panel_fast_exe(WS_R | 0x01, i); // 任意键打开"总关"的继电器
            } else {                            // 未勾选"取反",即任意键开启,关闭"总关"的继电器
                panel_fast_exe(WS_R | 0x00, i); // 关闭"总关"的继电器
            }
        } else if (temp_cfg[i].func == ALL_ON_OFF) {
            if (BIT6(temp_cfg[i].perm)) {      // 勾选了"取反",即任意键开启"总开关"的继电器
                panel_fast_exe(W_R | 0x01, i); // 任意键打开"总开关"的继电器
            } else {                           // 未勾选"取反",即任意键开启,关闭"总开关"的继电器
                panel_fast_exe(W_R | 0x00, i); // 关闭"总开关"的继电器
            }
        } else {
            if ((BIT1(temp_cfg[i].perm))) { // 其他按键勾选"总关背光"

                if (BIT6(temp_cfg[i].perm)) {      // 勾选了"取反"
                    panel_fast_exe(W_R | 0x00, i); // 打开指示灯与继电器
                } else {                           // 未勾选"取反"
                    panel_fast_exe(W_R | 0x01, i);
                }
            }
        }
#endif
    }
}

/* ***************************************** 处理按键类型 ***************************************** */

static void panel_all_close(FUNC_PARAMS) // 总关
{
    if (BIT1(data->data[6]) && !BIT2(data->data[6])) { // 如果勾选了"总关背光",且没有勾选"备用"
        // 关闭背光灯,进入总关状态(需要唤醒)
        my_common_panel.all_off_black = true;
        my_common_panel.all_close     = true;
    }
    if (!BIT1(data->data[6]) && BIT2(data->data[6])) { // 如果没有勾选"总关背光",且勾选了"备用"
        // 调暗背光灯,进入总关状态(需要唤醒)
        my_common_panel.all_dim_black = true;
        my_common_panel.all_close     = true;
    }
    if (!BIT1(data->data[6]) && !BIT2(data->data[6])) { // 如果没有勾选"总关背光",且没有勾选"备用"
        // 调暗背光灯,无需唤醒
        if (my_common_panel.all_close_no == false) {
            my_common_panel.all_close_no  = true;
            my_common_panel.all_dim_black = true;
        }
    }
    if (BIT1(data->data[6]) && BIT2(data->data[6])) { // 如果勾选了"总关背光",且勾选了"备用"
        // 关闭背光灯,无需唤醒
        if (my_common_panel.all_close_no == false) {
            my_common_panel.all_close_no  = true;
            my_common_panel.all_off_black = true;
        }
    }

    PROCESS_OUTER(temp_cfg, temp_status, {
        if (!BIT5(p_cfg->perm))
            continue; // 跳过没有勾选"总关"的按键

        // 匹配总关区域(总关区域相同,或源总关区域是0xF)
        bool area_match = (H_BIT(data->data[4]) == 0xF || H_BIT(data->data[4]) == H_BIT(p_cfg->area));
        if (!area_match)
            continue;

        switch (p_cfg->func) {
            case ALL_CLOSE: {
                bool group_match = (data->data[3] == p_cfg->group);
                if (!group_match)
                    break;
                panel_fast_exe(WS_R | (data->data[2] & 0x01), _i);
                if (!BIT6(p_cfg->perm)) { // 如果没有勾选"取反",则第一次按下"开继电器"
                    panel_fast_exe(WS_R | (data->data[2] & 0x01), _i);
                } else if (BIT6(p_cfg->perm)) { // 如果勾选了"取反",则第一次按下"关继电器"
                    panel_fast_exe(WS_R | (data->data[2] & 0x00), _i);
                }
            } break;
            case NIGHT_LIGHT: { // "夜灯"与"睡眠"模式状态一致,与其他模式状态相反
                panel_fast_exe(W_R | (data->data[2] & 0x01), _i);
            } break;
            case DND_MODE:
                panel_fast_exe(W_R | 0x01, _i); // 如果"勿扰"勾选了"总关",则开"勿扰"
                break;
            case ALL_ON_OFF:
                panel_fast_exe(W_R | 0x00, _i); // 如果"清理"勾选了"总关",则关"清理"
                break;
            case SCENE_MODE:
            case LIGHT_MODE:
            case DIMMING_4:
            case CLEAN_ROOM:
                panel_fast_exe(W_R | 0x00, _i);
                break;
            case CURTAIN_CLOSE: { // 勾选了"总关"的"窗帘关"
                PROCESS_INNER(temp_cfg, temp_status, {
                    bool func_match  = (p_cfg_ex->func == CURTAIN_OPEN);
                    bool group_match = (p_cfg_ex->group == p_cfg->group);
                    if (!func_match || !group_match)
                        continue;
                    panel_fast_exe(R_RS | 0x00, _j);
                });
                panel_fast_exe(WS_RS | 0x01, _i);
            } break;

            case CURTAIN_OPEN: { // 勾选了"总关"的"窗帘开"
                PROCESS_INNER(temp_cfg, temp_status, {
                    bool func_match  = (p_cfg_ex->func == CURTAIN_CLOSE);
                    bool group_match = (p_cfg_ex->group == p_cfg->group);
                    if (!func_match || !group_match)
                        continue;
                    panel_fast_exe(R_RS | 0x00, _j);
                });
                panel_fast_exe(WS_RS | 0x01, _i);
            } break;
            default:
                break;
        }
    });
}

static void panel_all_on_off(FUNC_PARAMS) // all_on_off
{
    if (!data->data[2]) {                                  // 为"总开关"的"关"操作
        if (BIT1(data->data[6]) && !BIT2(data->data[6])) { // 如果勾选了"总关背光",且没有勾选"备用"
            // 关闭背光灯,进入总关状态(需要唤醒)
            my_common_panel.all_off_black = true;
            my_common_panel.all_close     = true;
        }
        if (!BIT1(data->data[6]) && BIT2(data->data[6])) { // 如果没有勾选"总关背光",且勾选了"备用"
            // 调暗背光灯,进入总关状态(需要唤醒)
            my_common_panel.all_dim_black = true;
            my_common_panel.all_close     = true;
        }
        if (!BIT1(data->data[6]) && !BIT2(data->data[6])) { // 如果没有勾选"总关背光",且没有勾选"备用"
            // 正常操作,无需唤醒
            if (my_common_panel.all_close_no == false) {
                my_common_panel.all_close_no  = true;
                my_common_panel.all_dim_black = true;
            }
        }
        if (BIT1(data->data[6]) && BIT2(data->data[6])) {
            // 正常操作,无需唤醒,但需要全灭背光
            if (my_common_panel.all_close_no == false) {
                my_common_panel.all_close_no  = true;
                my_common_panel.all_off_black = true;
            }
        }
    }
    PROCESS_OUTER(temp_cfg, temp_status, {
        // 是否勾选"总开关"
        if (!BIT0(p_cfg->perm))
            continue;

        // 匹配"总关区域"
        bool area_match = (H_BIT(data->data[4]) == 0xF || H_BIT(data->data[4]) == H_BIT(p_cfg->area));
        if (!area_match)
            continue;

        switch (p_cfg->func) {
            case ALL_ON_OFF:

                if (data->data[3] == p_cfg->group) {
                    panel_fast_exe(W_R | 0x00, _i); // 关闭同分组的"总开关"

                    if ((BIT4(p_cfg->perm) && BIT6(p_cfg->perm)) && (data->data[2] == false)) { // 如果勾选了"只开"和"取反",且收到的状态是"关闭",则代表"总开关"的关
                        panel_fast_exe(WS_R | 0x01, _i);
                    }
                    if ((BIT4(p_cfg->perm) && !BIT6(p_cfg->perm)) && (data->data[2] == true)) { // 如果只勾选了"只开",且收到的状态是"打开",则代表"总开关"的开
                        panel_fast_exe(W_R | 0x01, _i);
                    }
                    if (!BIT4(p_cfg->perm) && !BIT6(p_cfg->perm)) { // 都未勾选
                        panel_fast_exe(W_R | (data->data[2] & 0x01), _i);
                    }
                }
                break;

            case DND_MODE: // 如果"勿扰"勾选了"总开关",则受"总开关"的"关"控制,其控制形式为:开启"勿扰"
                if (data->data[2] == false) {
                    panel_fast_exe(W_R | 0x01, _i);
                }
                break;
            case CLEAN_ROOM: // 如果"清理"勾选了"总开关",则受"总开关"的"关"控制,其控制形式为:关闭"清理"
                if (data->data[2] == false) {
                    panel_fast_exe(W_R | 0x00, _i);
                }
                break;
            case NIGHT_LIGHT: {
                if (BIT6(p_cfg->perm)) { // 勾选取反后,夜灯随总开关的总开而打开,随总开关的总关而关闭,该功能只对总开关有影响
                    panel_fast_exe(W_R | (data->data[2] & 0x01), _i);
                } else {
                    panel_fast_exe(W_R | (~data->data[2] & 0x01), _i);
                }

            } break;

            case LIGHT_MODE:
            case DIMMING_4:
                if (BIT7(p_cfg->perm) && !data->data[2]) { // 如果勾选了"不总开",则不受"总开关"的"总开"控制
                    panel_fast_exe(W_R | 0x00, _i);
                } else if (!BIT7(p_cfg->perm)) { //  如果没有勾选"不总开"
                    panel_fast_exe(W_R | (data->data[2] & 0x01), _i);
                }
                break;

            case SCENE_MODE: {
                if (data->data[2]) {
                    panel_fast_exe(R | 0x01, _i); // "总开"时,场景按键只输出继电器
                } else {
                    panel_fast_exe(W_R | 0x00, _i); // "总关"时,场景按键指示灯和继电器都关闭
                }

            } break;

            case ALL_CLOSE: // 如果"总关"勾选了"总开关",则不闪烁
                panel_fast_exe(R_K | (data->data[2] & 0x01), _i);
                break;

            case CURTAIN_CLOSE:
                if (data->data[2] == 0x00) { // 如果是"总开关"的"总关"
                    PROCESS_INNER(temp_cfg, temp_status, {
                        bool func_match  = (p_cfg_ex->func == CURTAIN_OPEN);
                        bool group_match = (p_cfg_ex->group == p_cfg->group);
                        if (!func_match || !group_match)
                            continue;
                        panel_fast_exe(R_RS | 0x00, _j);
                    });
                    panel_fast_exe(WS_RS | 0x01, _i);
                }
                break;
            case CURTAIN_OPEN:
                APP_PRINTF("CURTAIN_OPEN\n");
                if (data->data[2] == 0x01) { // 如果是"总开关"的"总开"
                    PROCESS_INNER(temp_cfg, temp_status, {
                        bool func_match  = (p_cfg_ex->func == CURTAIN_CLOSE);
                        bool group_match = (p_cfg_ex->group == p_cfg->group);
                        if (!func_match || !group_match)
                            continue;
                        panel_fast_exe(R_RS | 0x00, _j);
                    });
                    panel_fast_exe(WS_RS | 0x01, _i);
                }
                break;
            default:
                break;
        }
    });
}

static void panel_clean_room(FUNC_PARAMS)
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        if (data->data[2]) { // 如果状态是"开",则关闭自己的"勿扰"按键
            PROCESS_INNER(temp_cfg, temp_status, {
                bool func_match_ex = (p_cfg_ex->func == DND_MODE);
                if (!func_match_ex)
                    continue;
                panel_fast_exe(W_R | (~data->data[2] & 0x01), _j);
            });
        }
        bool func_match = (p_cfg->func == CLEAN_ROOM); // 查找自己的"清理"按键
        if (!func_match)
            continue;
        panel_fast_exe(W_R | (data->data[2] & 0x01), _i);
    });
}

static void panel_dnd_mode(FUNC_PARAMS)
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        if (data->data[2]) { // 如果状态是"开",则关闭自己的"清理"按键
            PROCESS_INNER(temp_cfg, temp_status, {
                bool func_match_ex = (p_cfg_ex->func == CLEAN_ROOM);

                if (!func_match_ex)
                    continue;
                panel_fast_exe(W_R | (~data->data[2] & 0x01), _j);
            });
        }

        bool func_match = (p_cfg->func == DND_MODE); // 查找自己的"勿扰"按键
        if (!func_match)
            continue;
        panel_fast_exe(W_R | (data->data[2] & 0x01), _i);
    });
}

static void panel_later_mode(FUNC_PARAMS)
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        bool func_match  = (p_cfg->func == LATER_MODE);
        bool group_match = (data->data[3] == p_cfg->group || data->data[3] == 0xFF) &&
                           !(data_src != THIS && data->data[3] == 0x00);
        if (!func_match || !group_match)
            continue;
        panel_fast_exe(0b00100110 | (data->data[2] & 0x01), _i);
    });
}

static void panel_chect_out(FUNC_PARAMS)
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        bool func_match  = (p_cfg->func == CHECK_OUT);
        bool group_match = (data->data[3] == p_cfg->group || data->data[3] == 0xFF) &&
                           !(data_src != THIS && data->data[3] == 0x00);
        if (!func_match || !group_match)
            continue;
        panel_fast_exe(W_R | (data->data[2] & 0x01), _i);
    });
}

static void panel_sos_mode(FUNC_PARAMS)
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        bool func_match  = (p_cfg->func == SOS_MODE);
        bool group_match = (data->data[3] == p_cfg->group || data->data[3] == 0xFF) &&
                           !(data_src != THIS && data->data[3] == 0x00);
        if (!func_match || !group_match)
            continue;
        panel_fast_exe(W_R | (data->data[2] & 0x01), _i);
    });
}

static void panel_service(FUNC_PARAMS)
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        bool func_match  = (p_cfg->func == SERVICE);
        bool group_match = (data->data[3] == p_cfg->group || data->data[3] == 0xFF) &&
                           !(data_src != THIS && data->data[3] == 0x00);
        if (!func_match || !group_match)
            continue;
        panel_fast_exe(W_R | (data->data[2] & 0x01), _i);
    });
}

static void panel_curtain_open(FUNC_PARAMS)
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        bool func_match  = (p_cfg->func == CURTAIN_OPEN);
        bool group_match = (data->data[3] == p_cfg->group || data->data[3] == 0xF) &&
                           !(data_src != THIS && data->data[3] == 0x00);
        // Match group or broadcast, skip group 0 from other devices
        if (!func_match || !group_match)
            continue;
        PROCESS_INNER(temp_cfg, temp_status, {
            bool func_match_ex  = (p_cfg_ex->func == CURTAIN_CLOSE);
            bool group_match_ex = (data->data[3] == p_cfg_ex->group || data->data[3] == 0xF);
            if (!func_match_ex || !group_match_ex)
                continue;
            panel_fast_exe(R_RS | 0x00, _j); // 关闭同分组的窗帘关
        });
        panel_fast_exe(WS_RS | 0x01, _i);
    });
}

static void panel_curtain_close(FUNC_PARAMS)
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        bool func_match  = (p_cfg->func == CURTAIN_CLOSE);
        bool group_match = (data->data[3] == p_cfg->group || data->data[3] == 0xFF) &&
                           !(data_src != THIS && data->data[3] == 0x00);
        // Match group or broadcast, skip group 0 from other devices
        if (!func_match || !group_match)
            continue;

        PROCESS_INNER(temp_cfg, temp_status, {
            bool func_match_ex  = (p_cfg_ex->func == CURTAIN_OPEN);
            bool group_match_ex = (data->data[3] == p_cfg_ex->group || data->data[3] == 0xFF);
            if (!func_match_ex || !group_match_ex)
                continue;
            panel_fast_exe(R_RS | 0x00, _j);
        });
        panel_fast_exe(WS_RS | 0x01, _i);
    });
}

static void panel_curtain_stop(FUNC_PARAMS)
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        bool func_match  = ((p_cfg->func == CURTAIN_OPEN) || (p_cfg->func == CURTAIN_CLOSE));
        bool group_match = (data->data[3] == p_cfg->group || data->data[3] == 0xFF) &&
                           !(data_src != THIS && data->data[3] == 0x00);
        // Match group or broadcast, skip group 0 from other devices

        bool relay_short = p_status->r_short;
        if (!func_match || !group_match || !relay_short)
            continue;
        panel_fast_exe(WS_RS_R | 0x00, _i);
    });
}

static void panel_scene_mode(FUNC_PARAMS)
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        bool area_match = ((L_BIT(data->data[4]) == 0xF) ||               // 源场景分组是15
                           (L_BIT(p_cfg->area) == 0xF) ||                 // 自身场景分组是15(受勾选了的场景按键控制,未勾选的场景按键保持)
                           (L_BIT(data->data[4]) == L_BIT(p_cfg->area))); // 源场景分组和自身场景分组相同

        if (!area_match) {
            continue;
        }

        // special 特殊场景需满足以下条件:
        // 1.自身是场景按键 2.当源场景按键的双控分组为15,可以控制双控分组是0-14的不同场景,当双控分组为14时,可以控制双控分组是0-13的不同场景(只是继电器输出)
        bool special = ((p_cfg->func == SCENE_MODE) && ((data->data[3] == 0x0F) && p_cfg->group < 0x0F || ((data->data[3] == 0x0E) && p_cfg->group < 0x0E)));

        if (special) {
            panel_fast_exe(W_R | 0x00, _i); // 切掉同场景区域的其他场景

            panel_fast_exe(R | (data->data[2] & 0x01), _i); // 只是继电器输出
            APP_PRINTF("special\n");
            continue;
        }

        bool mask = data->data[7] & p_cfg->scene_group; // 是否勾选了此场景
        // APP_PRINTF("idx:%d mask%d\n", _i, mask);
        if (!mask) { // 未勾选
            switch (p_cfg->func) {

                case SCENE_MODE: { // 切掉同场景区域的其他场景
                    panel_fast_exe(W_R | 0x00, _i);
                } break;

                case DIMMING_4:
                case LIGHT_MODE:
                case ALL_CLOSE: // 未勾选的是"总关"则切掉其继电器
                case ALL_ON_OFF:
                case NIGHT_LIGHT:
                    if ((L_BIT(p_cfg->area) != 0xF)) { // 该灯的场景分组不是15,切掉
                        panel_fast_exe(W_R | 0x00, _i);
                    }

                    break;
                default:
                    break;
            }
            continue;

        } else { // 勾选
            switch (p_cfg->func) {
                case CURTAIN_OPEN: // 勾选此场景的"窗帘开"
                    APP_PRINTF("CURTAIN_OPEN\n");
                    if (data->data[2]) {
                        PROCESS_INNER(temp_cfg, temp_status, {
                            bool func_match_ex  = (p_cfg_ex->func == CURTAIN_CLOSE);
                            bool group_match_ex = (data->data[3] == p_cfg->group || data->data[3] == 0xFF);
                            if (!func_match_ex || !group_match_ex)
                                continue;
                            panel_fast_exe(R_RS | 0x00, _j);
                        });
                        panel_fast_exe(WS_RS | 0x01, _i);
                    }
                    break;
                case CURTAIN_CLOSE: // 勾选此场景的"窗帘关"
                    if (data->data[2]) {
                        PROCESS_INNER(temp_cfg, temp_status, {
                            bool func_match_ex  = (p_cfg_ex->func == CURTAIN_OPEN);
                            bool group_match_ex = (data->data[3] == p_cfg->group || data->data[3] == 0xFF);
                            if (!func_match_ex || !group_match_ex)
                                continue;
                            panel_fast_exe(R_RS | 0x00, _j);
                        });
                        panel_fast_exe(WS_RS | 0x01, _i);
                    }
                    break;
                case ALL_ON_OFF:
                case LIGHT_MODE:
                case DIMMING_4:
                case SCENE_MODE:
                case ALL_CLOSE:
                    panel_fast_exe(W_R | (data->data[2] & 0x01), _i);
                    break;
                case DND_MODE: { // 勾选此场景的"勿扰"
                    panel_fast_exe(W_R | 0x00, _i);
                } break;
                default:
                    break;
            }
        }
    });
}

static void panel_light_mode(FUNC_PARAMS)
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        bool func_match  = (p_cfg->func == LIGHT_MODE);
        bool group_match = (data->data[3] == p_cfg->group || data->data[3] == 0xFF);

        bool skip_group = false;
#ifndef PANEL_DIS // 产生测固件,测试距离,即便是双控分组为 0x00,也不跳过,执行双控

        skip_group = (data_src != THIS && data->data[3] == 0x00); // 如果是其他设备且分组为00,跳过(不双控)
#endif
        // 如果主控设备勾选了"只开"+"备用",被控设备勾选了"只开"且没有勾选"备用"且"总关区域相同",则无条件双控
        bool special = ((BIT2(data->data[6]) && BIT4(data->data[6])) &&
                        BIT4(p_cfg->perm) && (!BIT2(p_cfg->perm)) &&
                        (H_BIT(data->data[4]) == 0xF || H_BIT(data->data[4]) == H_BIT(p_cfg->area)));

        if (special && func_match) {
            panel_fast_exe(W_R | (data->data[2] & 0x01), _i);
        }
        if (!func_match || !group_match || skip_group) {
            continue;
        }
        if (data_src == THIS && data->data[3] == 0x00) { // 如果是本设备,且分组为00,则根据data[8],来选择控制某个按键
            if (_i == data->data[8]) {
                panel_fast_exe(W_R | (data->data[2] & 0x01), _i);
            }
        } else {
            panel_fast_exe(W_R | (data->data[2] & 0x01), _i);
        }
    });
}

static void panel_dimming4_mode(FUNC_PARAMS)
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        bool func_match  = (p_cfg->func == DIMMING_4);
        bool group_match = (data->data[3] == p_cfg->group || data->data[3] == 0xFF);
        bool skip_group  = (data_src != THIS && data->data[3] == 0x00); // 如果是其他设备且分组为00,跳过(不双控)

        // 如果主控设备勾选了"只开"+"备用",被控设备勾选了"只开",则无条件双控
        bool special = ((BIT2(data->data[6]) && BIT4(data->data[6])) && BIT4(p_cfg->perm));

        if (special && func_match) {
            panel_fast_exe(W_R | (data->data[2] & 0x01), _i);
        }
        if (!func_match || !group_match || skip_group) {
            continue;
        }
        if (data_src == THIS && data->data[3] == 0x00) {
            if (_i == data->data[8]) { // 根据data[8],来选择控制某个按键
                panel_fast_exe(W_R | (data->data[2] & 0x01), _i);
            }
        } else {
            panel_fast_exe(W_R | (data->data[2] & 0x01), _i);
        }
    });
}

static void panel_night_light(FUNC_PARAMS) // 夜灯
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        bool func_match  = (p_cfg->func == NIGHT_LIGHT);
        bool group_match = (data->data[3] == p_cfg->group || data->data[3] == 0xFF) &&
                           !(data_src != THIS && data->data[3] == 0x00);
        if (!func_match || !group_match)
            continue;
        panel_fast_exe(W_R | (data->data[2] & 0x01), _i);
    });
}
#endif
