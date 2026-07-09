#ifndef _DEVICE_MANAGER_H_
#define _DEVICE_MANAGER_H_
#include "panel.h"
#include "setter.h"
#include "repeater.h"
#include "light_driver_ct.h"
#include "panel_power.h"

// #define SETTER // 设置器
#define PANEL // 灯控面板
// #define REPEATER // 转发器
// #define LIGHT_DRIVER_CT // 灯驱
// #define PANEL_POWER   // 取电面板

#define CONFIG_NUMBER 6 // 配置信息个数
#define LED_CHANNEL   2 // LED 路数

#if defined PANEL
#define RELAY_NUMBER 4

#define PANEL_A20    // 是否为 A20 系列
#define PANEL_TD     // 横向面板

#endif

#if defined LIGHT_DRIVER_CT
#define LIGHT_DRIVER_RELAY // 继电器灯驱

#endif

#define PWM_DIR
#define ZERO_ENABLE

void app_jump_device(void);
#endif
