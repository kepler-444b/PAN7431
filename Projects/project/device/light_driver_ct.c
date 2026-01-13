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

#if defined LIGHT_DRIVER_CT

#define FADE_TIMER 1000 // 渐变时间

typedef struct
{
    uint8_t func;        // 按键功能
    uint8_t group;       // 双控分组
    uint8_t area;        // 按键区域(高4位:总开关分区,低4位:场景分区)
    uint8_t perm;        // 按键权限
    uint8_t scene_group; // 场景分组

    pwm_hw_pins led_pin;
    uint8_t status;
} light_data_t;

typedef struct
{
    bool c_status;     // 霍尔元件当前状态
    bool l_status;     // 霍尔元件上次状态
    uint8_t count;     // 霍尔元件触发计数
    uint16_t timerout; // 计数超时

    bool led_status;      // LED 状态
    bool led_blink;       // LED 闪烁
    uint16_t blink_count; // LED 闪烁计数

} common_light_t;

// 函数声明
static void light_all_close(frame_t *data);
static void light_all_on_off(frame_t *data);
static void light_light_mode(frame_t *data);
static void light_scene_mode(frame_t *data);

static void light_proce_cmd(void *arg);
static void light_data_check(frame_t *data);
static void ligth_set_lum(uint8_t channel, uint16_t lum, uint16_t timer);
static void light_devier_ct_event_handler(event_type_e event, void *params);
static void light_simulate_lum(uint8_t lum_1, uint16_t lum_2);

// 全局变量
static light_data_t my_light_data;
static common_light_t my_common_light;

void light_driver_ct_init(void)
{
    bsp_light_driver_ct_init();
    pwm_hw_init();

    app_pwm_hw_add_pin(PWM_PA8);
    app_pwm_hw_add_pin(PWM_PB3);

    app_eventbus_subscribe(light_devier_ct_event_handler);
    bsp_start_timer(4, 10, light_proce_cmd, NULL, TMR_AUTO_MODE);
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
            APP_PRINTF("do something123\n");

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

static void light_data_check(frame_t *data)
{
    APP_PRINTF_BUF("data", data->data, data->length);

    if (data->data[0] != PANEL_HEAD) {
        return;
    }
    const light_cfg_t *p_cfg = app_get_light_cfg();

    switch (data->data[1]) {
        case ALL_CLOSE:
            light_all_close(data);
            break;
        case ALL_ON_OFF:
            light_all_on_off(data);
            break;
        case LIGHT_MODE:
            light_light_mode(data);
            break;
        case SCENE_MODE:
            light_scene_mode(data);
            break;
        default:
            break;
    }
}

static void light_simulate_lum(uint8_t lum_1, uint16_t lum_2)
{
    ligth_set_lum(0, lum_1, FADE_TIMER);
    ligth_set_lum(1, lum_2, FADE_TIMER);
}

static void light_all_close(frame_t *data)
{
    for (uint8_t i = 0; i < LED_CHANNEL; i++) {
        const light_cfg_t *p_cfg = app_get_light_cfg();
        if (!BIT5(p_cfg[i].perm))
            continue; // 跳过没有勾选"总关"的路

        bool area_match = (H_BIT(data->data[4]) == 0xF || H_BIT(data->data[4]) == H_BIT(p_cfg->area));
        if (!area_match)
            continue;

        ligth_set_lum(i, 0, FADE_TIMER);
    }
}
static void light_all_on_off(frame_t *data)
{
    for (uint8_t i = 0; i < LED_CHANNEL; i++) {
        const light_cfg_t *p_cfg = app_get_light_cfg();
        if (!BIT0(p_cfg[i].perm))
            continue; // 跳过没有勾选"总开关"的路

        bool area_match = (H_BIT(data->data[4]) == 0xF || H_BIT(data->data[4]) == H_BIT(p_cfg->area));
        if (!area_match)
            continue;
        if (BIT7(p_cfg->perm) && !data->data[2]) { // 如果勾选了"不总开",则不受"总开关"的"总开"控制
            ligth_set_lum(i, 0, FADE_TIMER);
        } else if (!BIT7(p_cfg->perm)) { // 如果未勾选"不总开"
            ligth_set_lum(i, data->data[2] ? 0 : p_cfg[i].led_lum, FADE_TIMER);
        }
    }
}

static void light_light_mode(frame_t *obj)
{
    APP_PRINTF("light_light_mode\n");
    uint8_t src_func   = obj->data[1];
    uint8_t src_status = obj->data[2];

    uint8_t src_group       = obj->data[3];
    uint8_t src_area        = obj->data[4];
    uint8_t src_perm        = obj->data[6];
    uint8_t src_scene_group = obj->data[7];

    for (uint8_t i = 0; i < LED_CHANNEL; i++) {
        const light_cfg_t *p_cfg = app_get_light_cfg();
        if (p_cfg[i].func != src_func || p_cfg[i].group != src_group || p_cfg[i].area != src_area || p_cfg[i].perm != src_perm) {
            continue;
        } else {
            ligth_set_lum(i, src_status ? p_cfg[i].led_lum : 0, FADE_TIMER);
        }
    }
}

static void light_scene_mode(frame_t *obj)
{

    uint8_t src_status      = obj->data[2];
    uint8_t src_scene_group = obj->data[7];

    for (uint8_t i = 0; i < LED_CHANNEL; i++) {
        const light_cfg_t *p_cfg = app_get_light_cfg();

        uint8_t scene_group = src_scene_group & p_cfg[i].scene_group;

        for (uint8_t bit = 0; bit < 8; bit++) {
            bool is_bit_set = (scene_group & (1 << bit)) != 0; // 判断这一位是否置1
            if (!is_bit_set) continue;                         // 位没置1就跳过

            ligth_set_lum(i, src_status ? p_cfg[i].scene_lum[bit] : 0, FADE_TIMER); // 设置灯光
        }
    }
}

static void ligth_set_lum(uint8_t channel, uint16_t lum, uint16_t timer)
{
    if (lum > 100) lum = 100;
    uint16_t pwm_duty = (uint32_t)lum * 2400 / 100;
    switch (channel) {
        case 0:
            app_set_pwm_hw_fade(PWM_PB3, pwm_duty, timer);
            break;
        case 1:
            app_set_pwm_hw_fade(PWM_PA8, pwm_duty, timer);
            break;
        default:
            break;
    }
}

#endif