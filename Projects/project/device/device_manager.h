#ifndef _DEVICE_MANAGER_H_
#define _DEVICE_MANAGER_H_
#include "panel.h"
#include "tran_device.h"

// #define TRAN_DEVICE // 转换器
#define PANEL_KEY // 灯控面板

#if defined PANEL_KEY

#define PANEL_6KEY_A11
// #define PANEL_4KEY_A11

#if defined PANEL_6KEY_A11
#define KEY_NUMBER   6
#define RELAY_NUMBER 4
#endif

#if defined PANEL_4KEY_A11
#define KEY_NUMBER   4
#define RELAY_NUMBER 4
#endif

#endif

// #define PWM_DIR
// #define ZERO_ENABLE

void app_jump_device(void);
#endif
