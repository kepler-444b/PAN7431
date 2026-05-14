#include "light_driver_ct.h"
#include <string.h>
#include "../bsp/bsp_pcb.h"
#include "../bsp/bsp_uart.h"
#include "../app/base.h"
#include "../app/pwm_hw.h"
#include "../app/eventbus.h"
#include "../app/protocol.h"
#include "../app/config.h"
#include "../app/pwm.h"
#include "../bsp/bsp_timer.h"
#include "../app/base.h"
#include "pan211.h"

#if defined LIGHT_DRIVER_CT

typedef struct
{
    bool c_status;     // 霍尔元件当前状态
    bool l_status;     // 霍尔元件上次状态
    uint8_t count;     // 霍尔元件触发计数
    uint16_t timerout; // 计数超时

    bool led_status;      // LED 状态
    bool led_blink;       // LED 闪烁
    uint16_t blink_count; // LED 闪烁计数

    uint8_t channel[LED_CHANNEL]; // 灯的当前亮度

    bool remove_card;           // 拔卡
    uint16_t remove_card_count; // 拔卡计数

    bool all_close; // 总关背光

    bool proch_light;           // 门磁开廊灯
    uint16_t proch_light_count; // 打开计数

} common_light_t;

static bool save_channel_lum[LED_CHANNEL];

// 函数声明

static void light_all_close(frame_t *data);
static void light_all_on_off(frame_t *data);
static void light_scene_mode(frame_t *data);
static void light_light_mode(frame_t *data);
static void light_dimming3_mode(frame_t *data);
static void light_dimming4_mode(frame_t *data);

static void light_bl_close(void);
static void light_power_up(void);
static void light_devier_ct_remove_card(void);

static void light_proce_cmd(void *arg);
static void light_data_check(frame_t *data);
static void ligth_set_lum(uint8_t channel, uint16_t lum);
static void light_devier_ct_event_handler(event_type_e event, void *params);
static void light_simulate_lum(uint8_t lum_1, uint16_t lum_2);

// 全局变量
static common_light_t my_common_light;

void light_driver_ct_init(void)
{
#ifndef LIGHT_DRIVER_RELAY
    bsp_light_driver_ct_init();
    pwm_hw_init();
    app_pwm_hw_add_pin(PWM_PB3);
    app_pwm_hw_add_pin(PWM_PA8);
#else
    bsp_light_driver_relay_init();
#endif

    app_eventbus_subscribe(light_devier_ct_event_handler);
    bsp_start_timer(4, 10, light_proce_cmd, NULL, TMR_AUTO_MODE);

    if (app_get_light_driver_type() == DEVICE_LIGHT_DRIVE) { // 只有受控电灯驱,上电执行"迎宾"
        light_power_up();
    }

    APP_PRINTF("light_driver_ct_init\n");
}

static void light_proce_cmd(void *arg)
{
    my_common_light.c_status = !APP_GET_GPIO(PA15);
    if (my_common_light.c_status && !my_common_light.l_status) { // 触发霍尔元件
        my_common_light.count++;
        my_common_light.timerout = 0;

        // 切换led
        my_common_light.led_status ^= 1;
        APP_SET_GPIO(PA4, my_common_light.led_status);
        APP_PRINTF("count:%d\n", my_common_light.count);
    }
    my_common_light.l_status = my_common_light.c_status; // 更新状态

    if (my_common_light.count > 0) { // 连续触发
        my_common_light.timerout++;

        if (my_common_light.count >= 3) {

            app_eventbus_publish(EVENT_REQUEST_NETWORK, NULL);

            my_common_light.count    = 0;
            my_common_light.timerout = 0;
        } else if (my_common_light.timerout >= 300) {
            APP_PRINTF("clear\n");
            my_common_light.count    = 0;
            my_common_light.timerout = 0;
        }
    }

    if (my_common_light.led_blink) { // LED 闪烁逻辑
        my_common_light.blink_count++;

        if (my_common_light.blink_count % 10 == 0) {
            my_common_light.led_status ^= 1;
            APP_SET_GPIO(PA4, my_common_light.led_status);
        }
        if (my_common_light.blink_count >= 100) {
            my_common_light.led_blink   = false;
            my_common_light.blink_count = 0;
        }
    }

    // 拔卡
    if (my_common_light.remove_card) {
        my_common_light.remove_card_count++;
        if (my_common_light.remove_card_count == 1300) { // 执行拔卡动作
            my_common_light.remove_card       = false;
            my_common_light.remove_card_count = 0;
            light_devier_ct_remove_card();
        }
    }

    // 门磁开廊灯
    if (my_common_light.proch_light) {
        my_common_light.proch_light_count++;
        if (my_common_light.proch_light_count == 1) { // 打开廊灯

            const light_cfg_t *p_cfg = app_get_light_cfg();
            for (uint8_t i = 0; i < LED_CHANNEL; i++) {
                if (!(p_cfg[i].func == LIGHT_MODE) || !(p_cfg[i].group == 0x01)) {
                    continue;
                }
                ligth_set_lum(i, p_cfg[i].led_lum);
            }
        }

        if (my_common_light.proch_light_count == 1000) { // 10s 后关廊灯

            const light_cfg_t *p_cfg = app_get_light_cfg();
            for (uint8_t i = 0; i < LED_CHANNEL; i++) {
                if (!(p_cfg[i].func == LIGHT_MODE) || !(p_cfg[i].group == 0x01)) {
                    continue;
                }
                ligth_set_lum(i, 0);
            }

            my_common_light.proch_light       = false;
            my_common_light.proch_light_count = 0;
        }
    }
}

static void light_devier_ct_event_handler(event_type_e event, void *params)
{
    switch (event) {
        case EVENT_LIGHT_RX: {
            frame_t *my_panel_frame = (frame_t *)params;
            light_data_check(my_panel_frame);
        } break;
        case EVENT_LED_TRIGGER: {
            my_common_light.led_status ^= 1;
            APP_SET_GPIO(PA4, my_common_light.led_status);
        } break;
        case EVENT_LED_BLINK: {
            my_common_light.led_blink = true;
        } break;
        case EVENT_SIMULATE_CTRL: {
            frame_t *my_frame = (frame_t *)params;
            light_simulate_lum(my_frame->data[0], my_frame->data[1]);
        } break;
        default:
            break;
    }
}

#if 0
// 保存各路状态
static void light_save_channel_lum(channel_statue_e type)
{
    switch (type) {
        case SAVE:
            break;
        case LOAD:
            break;
        case CLOSE:
            break;
        default:
            return;
    }
}
#endif

static void light_data_check(frame_t *data)
{
    APP_PRINTF_BUF("data", data->data, data->length);

    if (data->data[0] == CARD_HEAD) { // 插拔卡或门磁

        if (app_get_light_driver_type() != DEVICE_LIGHT_DRIVE_AP) { // 若不是"常供电灯驱",则提前返回
            APP_PRINTF("is not DEVICE_LIGHT_DRIVE_AP\n");
            return;
        }

        if (data->data[1] == CARD_CMD) { // 插拔卡指令
            if (data->data[3] == 0x00) { // 拔卡

                if (my_common_light.remove_card == false) {
                    APP_PRINTF("remove card\n");
                    my_common_light.remove_card = true;
                }

            } else { // 插卡

                if (my_common_light.remove_card) { // 如果正在拔卡计数中,则打断
                    APP_PRINTF("interrupt remove_card\n");
                    my_common_light.remove_card       = false;
                    my_common_light.remove_card_count = 0;
                }

                if (my_common_light.proch_light) { // 如果正在廊灯短亮中,则打断
                    my_common_light.proch_light       = false;
                    my_common_light.proch_light_count = 0;
                }

                APP_PRINTF("insert card\n");
                light_power_up();
            }
        }

        if (data->data[1] == DOOR_SENSOR) { // 门磁指令

            if (data->data[2] == 0x00) { // 推门,廊灯打开10s
                APP_PRINTF("open the door\n");

                const light_cfg_t *p_cfg = app_get_light_cfg();
                for (uint8_t i = 0; i < LED_CHANNEL; i++) {
                    if ((p_cfg[i].func == LIGHT_MODE) && (p_cfg[i].group == 0x01)) { // 廊灯:双控分组为1的灯控模式

                        my_common_light.proch_light = true;
                    }
                }
            }

            if (data->data[2] == 0x04) { // 关门,暂无动作
                APP_PRINTF("close the door\n");
            }
        }
    }

    if (data->data[0] == PANEL_HEAD) { // 来自灯控面板的数据
        const light_cfg_t *p_cfg = app_get_light_cfg();

        if (my_common_light.all_close == true) { // 当前在"总关背光"状态

            // 如果是勾选了"备用"且没有勾选"只开",这种按键为特殊按键(执行对应路的调光,而不唤醒灯驱)
            bool src_special_key = ((BIT2(data->data[6]) && !BIT4(data->data[6])) && (data->data[1] == LIGHT_MODE));

            if (src_special_key) {
                light_light_mode(data);
                return;
            }

            light_bl_close(); // 执行"总关背光"
            return;
        }

        switch (data->data[1]) {
            case ALL_CLOSE:
                light_all_close(data);
                break;
            case ALL_ON_OFF:
                light_all_on_off(data);
                break;
            case LIGHT_MODE:
            case NIGHT_LIGHT:
                light_light_mode(data);
                break;
            case SCENE_MODE:
                light_scene_mode(data);
                break;
            case DIMMING_3:
                light_dimming3_mode(data);
                break;
            case DIMMING_4:
                light_dimming4_mode(data);
                break;
            default:
                break;
        }
    }
}

static void light_simulate_lum(uint8_t lum_1, uint16_t lum_2)
{
    ligth_set_lum(0, lum_1);
    ligth_set_lum(1, lum_2);
}

static void light_all_close(frame_t *data)
{
    const light_cfg_t *p_cfg = app_get_light_cfg();

    for (uint8_t i = 0; i < LED_CHANNEL; i++) {

        if (!BIT5(p_cfg[i].perm))
            continue; // 跳过没有勾选"总关"的路

        bool area_match = (H_BIT(data->data[4]) == 0xF || H_BIT(data->data[4]) == H_BIT(p_cfg->area)); // 匹配"总关区域"
        if (!area_match)
            continue;

        if (BIT1(data->data[6])) { // 如果"总关"勾选了"总关背光"

            my_common_light.all_close = true;
        }
        ligth_set_lum(i, 0);
    }
}

static void light_all_on_off(frame_t *data)
{
    const light_cfg_t *p_cfg = app_get_light_cfg();

    for (uint8_t i = 0; i < LED_CHANNEL; i++) {

        if (!BIT0(p_cfg[i].perm))
            continue; // 跳过没有勾选"总开关"的路

        bool area_match = (H_BIT(data->data[4]) == 0xF || H_BIT(data->data[4]) == H_BIT(p_cfg->area));
        if (!area_match)
            continue;
        if (BIT7(p_cfg->perm) && !data->data[2]) { // 如果勾选了"不总开",则不受"总开关"的"总开"控制
            ligth_set_lum(i, 0);
        } else if (!BIT7(p_cfg->perm)) { // 如果未勾选"不总开"
            ligth_set_lum(i, data->data[2] ? p_cfg[i].led_lum : 0);
        }
    }
}

static void light_light_mode(frame_t *data)
{
    APP_PRINTF("light_light_mode\n");
    uint8_t src_func   = data->data[1];
    uint8_t src_status = data->data[2];

    uint8_t src_group       = data->data[3];
    uint8_t src_area        = data->data[4];
    uint8_t src_perm        = data->data[6];
    uint8_t src_scene_group = data->data[7];

    const light_cfg_t *p_cfg = app_get_light_cfg();

    for (uint8_t i = 0; i < LED_CHANNEL; i++) {

        bool func_match = (p_cfg[i].func == LIGHT_MODE || p_cfg[i].func == DIMMING_4 || NIGHT_LIGHT); // 匹配按键功能

        bool group_match = (p_cfg[i].group == src_group || src_group == 0xFF);
        // 如果主控设备勾选了"只开"+"备用",被控设备勾选了"只开",则无条件双控
        bool special = ((BIT2(src_perm) && BIT4(src_perm)) && BIT4(p_cfg[i].perm));

        if (special && func_match) {
        }

        if (!func_match || !group_match) {
            APP_PRINTF("src_group :%d  my_group :%d\n", src_group, p_cfg[i].group);
            continue;
        }
        ligth_set_lum(i, src_status ? p_cfg[i].led_lum : 0);
    }
}

static void light_dimming3_mode(frame_t *data)
{
    APP_PRINTF("light_dimming3_mode\n");

    uint8_t src_status = data->data[2];

    uint8_t src_group = data->data[3];
    uint8_t src_area  = data->data[4];

    const light_cfg_t *p_cfg = app_get_light_cfg();

    for (uint8_t i = 0; i < LED_CHANNEL; i++) {

        bool func_match = (p_cfg[i].func == DIMMING_4); // 匹配按键功能

        bool group_match = (p_cfg[i].group == src_group || src_group == 0x00); // 匹配双控分组(双控分组为0x00,则控制所有)

        bool is_open = (my_common_light.channel[i] != 0x00); // 当前此路已经是开启状态
        if (!func_match || !group_match || !is_open) {
            continue;
        }
        ligth_set_lum(i, src_status * 10);
    }
}

static void light_dimming4_mode(frame_t *data)
{
    APP_PRINTF("light_dimming4_mode\n");
    uint8_t src_func   = data->data[1];
    uint8_t src_status = data->data[2];

    uint8_t src_group       = data->data[3];
    uint8_t src_area        = data->data[4];
    uint8_t src_perm        = data->data[6];
    uint8_t src_scene_group = data->data[7];

    const light_cfg_t *p_cfg = app_get_light_cfg();

    for (uint8_t i = 0; i < LED_CHANNEL; i++) {

        bool func_match  = (p_cfg[i].func == DIMMING_4); // 匹配按键功能
        bool group_match = (p_cfg[i].group == src_group || src_group == 0xFF);
        // 如果主控设备勾选了"只开"+"备用",被控设备勾选了"只开",则无条件双控
        bool special = ((BIT2(src_perm) && BIT4(src_perm)) && BIT4(p_cfg[i].perm));

        if (special && func_match) {
        }

        if (!func_match || !group_match) {
            continue;
        }
        ligth_set_lum(i, src_status ? p_cfg[i].led_lum : 0);
    }
}

static void light_scene_mode(frame_t *data)
{
    APP_PRINTF("light_scene_mode\n");
    uint8_t src_status      = data->data[2];
    uint8_t src_scene_group = data->data[7];

    const light_cfg_t *p_cfg = app_get_light_cfg();

    for (uint8_t i = 0; i < LED_CHANNEL; i++) {

        uint8_t scene_group = src_scene_group & p_cfg[i].scene_group; // 源场景组与灯配置场景组的交集(勾选的场景位)

        bool area_match = ((L_BIT(data->data[4]) == 0xF) ||                 // 源场景分组是15
                           (L_BIT(p_cfg[i].area) == 0xF) ||                 // 自身场景分组是15(受勾选了的场景按键控制,未勾选的场景按键保持)
                           (L_BIT(data->data[4]) == L_BIT(p_cfg[i].area))); // 源场景分组和自身场景分组相同

        if (!area_match) {
            continue;
        }

        if (scene_group == 0 && (L_BIT(p_cfg[i].area) != 0xF)) { // 都未勾选 且 自身场景分组不为15,切掉
            ligth_set_lum(i, 0);
        }

        for (uint8_t bit = 0; bit < 8; bit++) { // 哪一位勾选了

            bool is_bit_set = (scene_group & (1 << bit)) != 0; // 判断这一位是否置1
            if (!is_bit_set) {
                continue;
            }

            ligth_set_lum(i, src_status ? p_cfg[i].scene_lum[bit] : 0); // 设置灯光
        }
    }
}

// 执行总关背光
static void light_bl_close(void)
{
    const light_cfg_t *p_cfg  = app_get_light_cfg();
    my_common_light.all_close = false; // 唤醒

    for (uint8_t i = 0; i < LED_CHANNEL; i++) {
        switch (p_cfg[i].fade_time) {
            case NIGHT_LIGHT: { // 唤醒,关闭已经打开的夜灯
                ligth_set_lum(i, 0);
            } break;

            default: {
                if ((BIT1(p_cfg[i].perm))) { // 其他按键勾选"总关背光"

                    if (BIT6(p_cfg[i].perm)) {
                        // 勾选了"取反",关闭
                        ligth_set_lum(i, 0);
                    } else {
                        // 未勾选"取反",输出
                        // ligth_set_lum(i, p_cfg[i].led_lum);
                        ligth_set_lum(i, p_cfg[i].led_lum / 3); // 总关背光输出,只是输出原本亮度的3/1
                    }
                }
            } break;
        }
    }
}

static void ligth_set_lum(uint8_t channel, uint16_t lum)
{

    APP_PRINTF("channel:%d lum:%d\n", channel, lum);
    if (lum > 100) lum = 100;

    const light_cfg_t *temp_cfg = app_get_light_cfg();

    my_common_light.channel[channel] = lum;

    uint16_t fade_time = temp_cfg[channel].fade_time * 100;
    uint16_t dead_zone = temp_cfg[channel].dead_zone;
    uint8_t lum_curve  = temp_cfg[channel].lum_curve;
    pwm_hw_pins pin    = temp_cfg[channel].led_pin;

    uint16_t pwm_duty = (uint32_t)lum * 2400 / 100;

#ifndef LIGHT_DRIVER_RELAY
    app_set_pwm_hw_fade(pin, pwm_duty, fade_time, dead_zone, lum_curve);
#else
    APP_SET_GPIO(PA8, lum > 0 ? true : false);
#endif
}

// 执行上电状态
static void light_power_up(void)
{
    const light_cfg_t *p_cfg = app_get_light_cfg();

    for (uint8_t i = 0; i < LED_CHANNEL; i++) {

        if (!BIT3(p_cfg[i].perm)) {
            continue;
        }
        ligth_set_lum(i, p_cfg[i].led_lum);
    }
}

// 执行拔卡动作
static void light_devier_ct_remove_card(void)
{
    APP_PRINTF("light_devier_ct_remove_card\n");
    for (uint8_t i = 0; i < LED_CHANNEL; i++) {
        ligth_set_lum(i, 0);
    }
}

#endif