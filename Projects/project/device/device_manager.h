#ifndef _DEVICE_MANAGER_H_
#define _DEVICE_MANAGER_H_
#include "panel.h"
#include "setter.h"
#include "repeater.h"
#include "light_driver_ct.h"

// #define SETTER          // 设置器
#define PANEL // 灯控面板
// #define REPEATER        // 转发器
// #define LIGHT_DRIVER_CT // 色温灯驱

#if defined PANEL
#define RELAY_NUMBER  4
#define CONFIG_NUMBER 6 // 配置信息个数

#endif

// #define PWM_DIR
#define ZERO_ENABLE

void app_jump_device(void);
#endif
