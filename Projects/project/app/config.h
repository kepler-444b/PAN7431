#ifndef _CONFIG_H_
#define _CONFIG_H_
#include <stdint.h>
#include "../bsp/bsp_pcb.h"
#include "../bsp/bsp_flash.h"
#include "../app/pwm_hw.h"

#if defined PANEL
#if defined PANEL_A20

#define VER 0x15 // A20面板软件版本 V1.5

#else
#define VER 0x15 // A18面板软件版本 V1.5

#endif
#endif

#if defined LIGHT_DRIVER_CT

#if defined LIGHT_DRIVER_RELAY
#define VER 0x10 // 继电器版本

#else
#define VER 0x10 // PWM版本

#endif

#endif

#if defined PANEL_POWER
#define VER 0x01
#endif

#if defined SETTER
#define VER 0x10
#endif
// 只读项目
#if defined PANLE
#define TYPE 0x00 // 灯控面板

#elif defined REPEATER
#define TYPE 0x0D // 转发器

#elif defined SETTER
#define TYPE 0x7E // 设置器

#elif defined LIGHT_DRIVER_CT
#ifndef LIGHT_DRIVER_RELAY
#define TYPE 0x17 // 普通灯驱

#else
#define TYPE 0x19 // 继电器灯驱

#endif
#elif defined PANEL_POWER
#define TYPE 0x20 // 取电面板

#endif

#define TX_DB 0x09 // TX power
#define TX_DR 0xFA // TX data rate

typedef enum {
    CFG = 0x00,
    REG = 0x01,
} cfg_addr;

typedef enum {
    KEY_1 = 0x01,
    KEY_2 = 0x02,
    KEY_3 = 0x03,
    KEY_4 = 0x04,
    KEY_6 = 0x06,
} key_number;

typedef enum {
    SIM_1KEY = 0x01,
    SIM_2KEY = 0x02,
    SIM_3KEY = 0x03,
    SIM_4KEY = 0x04,
    SIM_6KEY = 0x06,
} sim_key_numer_e;

// Used for information storage of apanel type
typedef struct
{
    uint8_t func;        // 按键功能
    uint8_t group;       // 双控分组
    uint8_t area;        // 按键区域(高4位:总开关分区,低4位:场景分区)
    uint8_t perm;        // 按键权限
    uint8_t scene_group; // 场景分组

    gpio_pin_t led_w_pin;    // 按键所控白灯
    pwm_hw_pins led_y_pin;   // 按键所控黄灯
    gpio_pin_t relay_pin[4]; // 按键所控继电器
} panel_cfg_t;

typedef struct
{
    uint8_t func;        // 按键功能
    uint8_t group;       // 双控分组
    uint8_t area;        // 按键区域(高4位:总开关分区,低4位:场景分区)
    uint8_t perm;        // 按键权限
    uint8_t scene_group; // 场景分组

    uint8_t led_lum;      // 本路调光
    uint8_t scene_lum[8]; // 场景对应亮度
    uint8_t fade_time;    // 渐变时间
    uint16_t dead_zone;   // 死区
    uint8_t lum_curve;    // 调光曲线
    pwm_hw_pins led_pin;

} light_cfg_t;

typedef struct {
    uint8_t ver;        // 0:  程序版本
    uint8_t cpadd_h;    // 1:  产品地址高位
    uint8_t cpadd_l;    // 2:  产品地址低位
    uint8_t cplei;      // 3:  产品类型
    uint8_t channel;    // 4:  信道
    uint8_t zuwflag;    // 5:  组网标识
    uint8_t room_h;     // 6:  房间H
    uint8_t room_l;     // 7:  房间L
    uint8_t forward_en; // 8:  转发标识使能
    uint8_t tx_db;      // 9:  发送功率
    uint8_t tx_su;      // 10: 通讯速率
    uint8_t try;        // 11: 测试项
    uint8_t key;        // 12: 灯控按键数量
    uint8_t reserve2;
    uint8_t reserve3;
    uint8_t reserve4;
    uint8_t reserve5;
    uint8_t reserve6;
    uint8_t reserve7;
    uint8_t reserve8;

} reg_t;

static uint8_t my_uid[12] = {0};
void app_load_config(cfg_addr addr);

const light_cfg_t *app_get_light_cfg(void);
const uint8_t app_get_light_driver_type(void);

const panel_cfg_t *app_get_panel_cfg(void);
const uint8_t app_get_panel_type(void);

const uint8_t *app_get_cfg(void);

reg_t *app_get_reg(void);
const uint8_t app_get_sim_key_number(void);

#endif
