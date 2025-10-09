#ifndef _CONFIG_H_
#define _CONFIG_H_
#include <stdint.h>
#include "../bsp/bsp_pcb.h"
#include "../bsp/bsp_flash.h"

// Read-only item
#define VER 0x10 // software version

#if defined PANEL
#define TYPE 0x00

#elif defined REPEATER
#define TYPE 0x0D

#elif defined SETTER
#define TYPE 0x7E

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

// Used for information storage of apanel type
typedef struct
{
    uint8_t func;        // 按键功能
    uint8_t group;       // 双控分组
    uint8_t area;        // 按键区域(高4位:总开关分区,低4位:场景分区)
    uint8_t perm;        // 按键权限
    uint8_t scene_group; // 场景分组

    gpio_pin_t led_w_pin;    // 按键所控白灯
    gpio_pin_t led_y_pin;    // 按键所控黄灯
    gpio_pin_t relay_pin[4]; // 按键所控继电器
} panel_cfg_t;

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

const panel_cfg_t *app_get_panel_cfg(void);

reg_t *app_get_reg(void);
#endif
