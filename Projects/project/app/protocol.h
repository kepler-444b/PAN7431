#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_
#include <stdint.h>
#include <stdbool.h>
#include "../app/app.h"

#define BASE_DELAY   50

#define DEFAULT_ADDR (unsigned char[5]){0x5A, 0x4B, 0x3C, 0x2D, 0x0F}
#define COM_DEVICE   (unsigned char[5]){0xAA, 0xAA, 0xAA, 0xAA, 0xAA}

#define TEST_ADDR1   (unsigned char[5]){0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
#define TEST_ADDR2   (unsigned char[5]){0xC1, 0xCC, 0xCC, 0xCC, 0xCC}
#define TEST_ADDR3   (unsigned char[5]){0xCC, 0xCC, 0xCC, 0xCC, 0xCC}

extern uint8_t current_channel;

typedef enum {
    Request     = 0x00, // 请求组网
    NetNewSet   = 0x01, // 设备组网设置
    SourceData  = 0x02, // 数据报文
    RReg        = 0x03, // 读取设备寄存器
    WReg        = 0x04, // 写设置寄存器
    FindSB      = 0x05, // 查找设备
    SBAnswer    = 0x06, // 设备应答
    SBkz        = 0x07, // 设备控制
    RssiTest    = 0x08, // 测试误码率
    WHWReg      = 0x09, // 写红外设置寄存器(红外码库)
    Wmset       = 0x0a, // 写主机配网信息
    FreepReq    = 0x0b, // 随意贴组网设置
    Wset        = 0x0c, // 写从设备设置信息
    RssiEnd     = 0x0d, // Tset rssi
    TestFrame   = 0x0e, // Test frame error_rate
    ForwardData = 0x82, // Tran data

} frame_type;

typedef enum {

    // 命令类型
    COMMON_CMD  = 0x00, // 普通命令
    SPECIAL_CMD = 0x01, // 特殊命令

    // AT 命令
    COMMON_TYPE  = 0xA0, // 普通类型
    GET_MAC_TYPE = 0xA1, // 获取mac地址
    SET_DID_TYPE = 0xA2, // 设置did

    // 快装盒子帧头
    QUICK_SINGLE = 0xE2, // quick (快装盒子)单发串码
    QUICK_MULTI  = 0xE3, // quick (快装盒子)群发串码
    QUICK_END    = 0x30, // quick (快装盒子)结束串码

    // 面板帧头
    PANEL_HEAD   = 0xF1, // panel (面板)通讯数据
    PANEL_SINGLE = 0xF2, // panel (面板)单发串码
    PANEL_MULTI  = 0xF3, // panel (面板)群发串码
    APPLY_CONFIG = 0xF8, // 设置软件回复设置申请
    EXIT_CONFIG  = 0xF9, // 设置软件"退出"配置模式

    // 按键功能
    ALL_CLOSE     = 0x00, // 总关
    ALL_ON_OFF    = 0x01, // 总开关
    CLEAN_ROOM    = 0x02, // 清理模式
    DND_MODE      = 0x03, // 勿扰模式
    LATER_MODE    = 0x04, // 请稍后
    CHECK_OUT     = 0x05, // 退房
    SOS_MODE      = 0x06, // 紧急呼叫
    SERVICE       = 0x07, // 请求服务
    CURTAIN_OPEN  = 0x10, // 窗帘开
    CURTAIN_CLOSE = 0x11, // 窗帘关
    SCENE_MODE    = 0x0D, // 场景模式
    LIGHT_MODE    = 0x0E, // 灯控模式
    NIGHT_LIGHT   = 0x0F, // 夜灯模式
    LIGHT_UP      = 0x18, // 调光上键
    LIGHT_DOWN    = 0x1B, // 调光下键
    DIMMING_3     = 0x1E, // 调光3
    DIMMING_4     = 0x21, // 调光4
    UNLOCKING     = 0x60, // 开锁
    BLUETOOTH     = 0x61, // 蓝牙
    CURTAIN_STOP  = 0x62, // 窗帘停
    VOLUME_ADD    = 0x63, // 音量+
    VOLUME_SUB    = 0x64, // 音量-;
    PLAY_PAUSE    = 0x65, // 播放/暂停
    NEXT_SONG     = 0x66, // 下一首
    LAST_SONG     = 0x67, // 上一首
} panel_frame_e;

typedef struct
{
    uint8_t data[32];
    uint8_t length;
} panel_frame_t;
void app_protocol_init(void);
void app_rf_tx(rf_frame_t *rf_tx);
void app_send_cmd(uint8_t key_number, uint8_t key_status, uint8_t frame_head, uint8_t cmd_type);

#endif
