#include "protocol.h"
#include "../app/base.h"
#include "../app/config.h"
#include "../app/eventbus.h"
#include "../bsp/bsp_timer.h"
#include "../bsp/bsp_uart.h"
#include "pan211.h"
#include <string.h>

void app_rf_rx_check(rf_frame_t *buf);
void app_usart1_rx(usart1_rx_buf_t *buf);
void app_usart2_rx(usart2_rx_buf_t *buf);
static void protocol_event_handler(event_type_e event, void *params);
static void app_save_cfg(uint8_t *cfg_data, uint8_t length);
static void app_save_reg(uint8_t reg_addr, uint8_t *reg_data, uint8_t length);
static void app_creat_frame(rf_frame_t *frame, rf_frame_type type, const reg_t *reg);
static void delay_send_find_ack(void *arg);
static void delay_forward_data(void *arg);
static void delay_switch_channel(void *arg);
static void delay_clear_last_data(void *arg);

static uint8_t last_data[52] = {0};

void app_protocol_init(void)
{
#ifndef SETTER
    app_rf_rx_callback(app_rf_rx_check);
    app_usart1_rx_callback(app_usart1_rx);
    app_usart2_rx_callback(app_usart2_rx);
#endif
    app_eventbus_subscribe(protocol_event_handler);
}

static void protocol_event_handler(event_type_e event, void *params)
{
    switch (event) {
        case EVENT_REQUEST_NETWORK: {
            static rf_frame_t rf_Request;
            reg_t *Request_cfg = app_get_reg();

            app_creat_frame(&rf_Request, Request, Request_cfg);
            rf_Request.rf_data[10] = 0x0b;

            PAN211_SetChannel(NET_CHANNEL);
            memcpy(&rf_Request.rf_data[11], &Request_cfg->ver, 11);
            rf_Request.rf_len = RF_PAYLOAD;
            app_rf_tx(&rf_Request, true);
            APP_PRINTF_BUF("Request_tx_ch1", rf_Request.rf_data, 22);
            bsp_start_timer(1, 1000, delay_switch_channel, NULL, TMR_ONCE_MODE);

        } break;
        default:
            return;
    }
}

void app_usart1_rx(usart1_rx_buf_t *buf)
{
    static rf_frame_t temp_rf_frame;
    memset(temp_rf_frame.rf_data, 0, sizeof(temp_rf_frame.rf_data));
    temp_rf_frame.rf_len = 0;
    // APP_PRINTF_BUF("buf", buf->data, buf->len);
#if defined SETTER
    memcpy(temp_rf_frame.rf_data, buf->data, buf->len);
    temp_rf_frame.rf_len = buf->len;
    app_eventbus_publish(EVENT_UART1_RX, &temp_rf_frame);

#endif
}

void app_usart2_rx(usart2_rx_buf_t *buf)
{
    APP_PRINTF_BUF("buf", buf->data, buf->len);
}

void app_rf_rx_check(rf_frame_t *buf)
{
    reg_t *reg = app_get_reg();
    // APP_PRINTF_BUF("buf", buf->rf_data, buf->rf_len);
    uint16_t tag_device_addr = MAKE_U16(buf->rf_data[2], buf->rf_data[3]);
    uint16_t src_room_addr   = MAKE_U16(buf->rf_data[8], buf->rf_data[9]);

    uint16_t my_room_addr   = MAKE_U16(reg->room_h, reg->room_l);
    uint16_t my_device_addr = MAKE_U16(reg->cpadd_h, reg->cpadd_l);

    uint8_t cmd = buf->rf_data[4];

    if (src_room_addr != my_room_addr && cmd != Request && cmd != FindSB) {
        APP_PRINTF("my_room_addr:%04X src_room_addr:%04X\n", my_room_addr, src_room_addr);
        return;
    }
    if (tag_device_addr != my_device_addr && tag_device_addr != 0xFFFF) {
        APP_PRINTF("my_device_addr:%04X tag_device_addr:%04X\n", my_device_addr, tag_device_addr);
        return;
    }

    // APP_PRINTF_BUF("buf", buf->rf_data, buf->rf_len);
    uint8_t *data_p  = &buf->rf_data[11];
    uint8_t data_len = buf->rf_data[10];

    static rf_frame_t rf_rx;
    switch (cmd) {
        case Request: {
            app_save_reg(4, &buf->rf_data[11], 5);
            app_creat_frame(&rf_rx, SBAnswer, reg); // Return ACK
            rf_rx.rf_data[10] = 0x14;
            memcpy(&rf_rx.rf_data[11], &reg->ver, 20);
            rf_rx.rf_len = RF_PAYLOAD;
            app_rf_tx(&rf_rx, true);
            app_eventbus_publish(EVENT_LED_BLINK, NULL);

        } break;
        case RReg: {
            uint8_t read_addr   = buf->rf_data[11];
            uint8_t read_length = buf->rf_data[12];

            APP_PRINTF("read_addr:%d read_length:%d\n", read_addr, read_length);
            app_creat_frame(&rf_rx, SBAnswer, reg);
            rf_rx.rf_data[10] = read_length;

            memcpy(&rf_rx.rf_data[11], (uint8_t *)&reg + read_addr, read_length);
            rf_rx.rf_len = RF_PAYLOAD;
            app_rf_tx(&rf_rx, true);
            APP_PRINTF_BUF("RReg_ack", &rf_rx.rf_data, rf_rx.rf_len);

        } break;
        case WReg: {
            uint8_t reg_addr   = buf->rf_data[7]; // The starting address of the register
            uint8_t *temp_data = &buf->rf_data[11];
            if (buf->rf_data[7] == 20) { // 写 CFG

                uint8_t reg_length = buf->rf_data[10]; // Data write length

                if (temp_data[37] != 0x00 && temp_data[37] != 0x14) { // 只收普通面板和常供电面板的配置信息
                    break;
                }
                app_save_cfg(temp_data, reg_length); // Save cfg

            } else { // 写 REG
                uint8_t reg_length = 1;
                app_save_reg(reg_addr, temp_data, 1); // Save reg
            }
            app_eventbus_publish(EVENT_LED_BLINK, NULL);
        } break;
        case SourceData:
        case ForwardData: {

            uint16_t delay_forward = reg->zuwflag * 10 + BASE_DELAY;
            bool need_forward      = true;
            if (cmd == ForwardData) {
                // Compare with last forwarded data to avoid duplication
                if (memcmp(last_data, data_p, data_len) == 0) {
                    APP_PRINTF("same data\n");
                    need_forward = false;
                } else {
                    APP_PRINTF("different data\n");
                }
            } else {
                APP_PRINTF("SourceData\n");
            }

            if (need_forward) {
                // Create RF frame for forwarding
                app_creat_frame(&rf_rx, ForwardData, reg);
                rf_rx.rf_data[10] = data_len;

                memcpy(&rf_rx.rf_data[11], data_p, data_len);
                rf_rx.rf_len = RF_PAYLOAD;
                // Start delayed transmission
                bsp_start_timer(6, delay_forward, delay_forward_data, &rf_rx, TMR_ONCE_MODE);
                // Update last data cache
                memcpy(last_data, data_p, data_len);
                bsp_start_timer(10, 2000, delay_clear_last_data, NULL, TMR_ONCE_MODE);
#if defined PANEL
                // Execute local panel action
                static panel_frame_t temp_panel_frame;
                memcpy(temp_panel_frame.data, data_p, data_len);
                temp_panel_frame.length = data_len;
                app_eventbus_publish(EVENT_PANEL_RX, &temp_panel_frame);

#elif defined REPEATER
                // DelayMs(1);
                app_eventbus_publish(EVENT_LED_BLINK, NULL);
                bsp_rs485_send_buf(rf_rx.rf_data, rf_rx.rf_len);
#elif defined LIGHT_DRIVER_CT
                static panel_frame_t temp_panel_frame;
                memcpy(temp_panel_frame.data, data_p, data_len);
                temp_panel_frame.length = data_len;
                app_eventbus_publish(EVENT_LED_BLINK, NULL);
                app_eventbus_publish(EVENT_LIGHT_RX, &temp_panel_frame);
#endif
            }
        } break;
        case SBkz: {
            static panel_frame_t temp_panel_frame;
            uint8_t *panel_data_p     = &buf->rf_data[12];
            uint8_t panel_data_length = 2;
            memcpy(temp_panel_frame.data, panel_data_p, panel_data_length);
            temp_panel_frame.length = panel_data_length;
            app_eventbus_publish(EVENT_SIMULATE_KEY, &temp_panel_frame);
        } break;
        case RssiTest: {
            // Retuen AckRssi
            app_creat_frame(&rf_rx, RssiEnd, reg);
            rf_rx.rf_data[10] = 0x00;
            rf_rx.rf_len      = RF_PAYLOAD;
            DelayMs(1);
            APP_PRINTF_BUF("AckRssi", rf_rx.rf_data, rf_rx.rf_len);
            app_rf_tx(&rf_rx, false);

            // Start test rssi
            APP_PRINTF("PAN211_StartCarrierWave\n");
            PAN211_ClearIRQFlags(0xFF);
            PAN211_SetChannel(app_get_reg()->channel);
            PAN211_StartCarrierWave();
            DelayMs(5000);
            PAN211_ExitCarrierWave();
            APP_PRINTF("PAN211_ExitCarrierWave\n");
            PAN211_RxStart();
        } break;
        case TestFrame: { // Return TestFrame
            app_creat_frame(&rf_rx, TestFrame, reg);
            rf_rx.rf_data[10] = 0x02;
            memcpy(&rf_rx.rf_data[11], &buf->rf_data[11], 0x02);
            rf_rx.rf_len = RF_PAYLOAD;
            app_rf_tx(&rf_rx, false);
        } break;
        case FindSB: { // Return FindSB

            uint16_t tag_room = MAKE_U16(buf->rf_data[11], buf->rf_data[12]);
            if (tag_room != 0xFFFF && tag_room != my_room_addr) {
                APP_PRINTF("[FindSB] tag_room:%04X  my_room:%04X\n", tag_room, my_room_addr);
                return;
            }
            uint8_t tag_device_type = buf->rf_data[13];
            if (buf->rf_data[13] != 0xFF && buf->rf_data[13] != reg->cplei) {
                APP_PRINTF("[FindSB] tag_device_type:%02X my_device_type:%02X\n", tag_device_type, reg->cplei);
                return;
            }

            app_creat_frame(&rf_rx, SBAnswer, reg);
            rf_rx.rf_data[10] = 0x14;
            memcpy(&rf_rx.rf_data[11], &reg->ver, 0x14);
            rf_rx.rf_len = RF_PAYLOAD;

            uint32_t delay_ms = (uint32_t)(reg->zuwflag * 50 + BASE_DELAY);
            bsp_start_timer(5, delay_ms, delay_send_find_ack, &rf_rx, TMR_ONCE_MODE);
        } break;
        default:
            return;
    }
}

// Recover to the prior channel after a specified delay
static void delay_switch_channel(void *arg)
{
    PAN211_SetChannel(app_get_reg()->channel);
    APP_PRINTF("----switch_channel:%d----\n", app_get_reg()->channel);
}

static void delay_send_find_ack(void *arg)
{
    APP_PRINTF("FindSB\n");
    rf_frame_t *frame = (rf_frame_t *)arg;
    app_rf_tx(frame, true);
    bsp_stop_timer(5);
}

static void delay_forward_data(void *arg)
{
    rf_frame_t *frame = (rf_frame_t *)arg;
    app_rf_tx(frame, true);
}

static void delay_clear_last_data(void *arg)
{
    memset(last_data, 0, sizeof(last_data));
    // APP_PRINTF("delay_clear_last_data\n");
}

void app_rf_tx(rf_frame_t *rf_tx, bool repeat)
{
    // APP_PRINTF_BUF("rf_tx", rf_tx->rf_data, rf_tx->rf_len);
    PAN211_WriteFIFO(rf_tx->rf_data, rf_tx->rf_len);
    PAN211_TxStart();
    if (repeat) {
        while (!IRQDetected());
        PAN211_TxStart();
        while (!IRQDetected());
        PAN211_TxStart();
    }
}

#if defined PANEL
// 构造按键数据帧
void app_send_cmd(uint8_t key_number, uint8_t key_status, uint8_t frame_head, uint8_t cmd_type)
{
    static panel_frame_t send_frame;
    memset(&send_frame, 0, sizeof(send_frame));
    const panel_cfg_t *temp_cfg = app_get_panel_cfg();

    send_frame.data[0] = PANEL_HEAD;
    if (cmd_type == SPECIAL_CMD) { // special cmd
        if (temp_cfg[key_number].func == CURTAIN_OPEN || temp_cfg[key_number].func == CURTAIN_CLOSE) {
            // 若是窗帘正在"开/关"过程中,则"停止"
            send_frame.data[1] = CURTAIN_STOP;
            send_frame.data[2] = false;
        }
    }
#if 0
    else if (cmd_type == COMMON_CMD || (cmd_type == SPECIAL_CMD && temp_cfg[key_number].func == LATER_MODE)) {
        // 普通命令 或 特殊命令里的"请稍后"
        send_frame.data[1] = temp_cfg[key_number].func;
        send_frame.data[2] = key_status;
        if (BIT4(temp_cfg[key_number].perm) && BIT6(temp_cfg[key_number].perm)) { // "只开" + "取反"
            send_frame.data[2] = false;
        } else if (BIT4(temp_cfg[key_number].perm)) { // "只开"
            send_frame.data[2] = true;
        } else if (BIT6(temp_cfg[key_number].perm)) { // "取反"
            send_frame.data[2] = !key_status;
        } else {
            send_frame.data[2] = key_status; // default
        }
    }
#else
    else if (cmd_type == COMMON_CMD) {
        send_frame.data[1] = temp_cfg[key_number].func;
        send_frame.data[2] = key_status; // default

        if (temp_cfg[key_number].func == SCENE_MODE) {
            if (BIT4(temp_cfg[key_number].perm)) { // "只开"
                send_frame.data[2] = true;
            }
        }
    }

#endif

    // Set group,area,perm and scene_group
    send_frame.data[3] = temp_cfg[key_number].group;
    send_frame.data[4] = temp_cfg[key_number].area;
    send_frame.data[6] = temp_cfg[key_number].perm;
    send_frame.data[7] = temp_cfg[key_number].scene_group;
    // Calculate and set the CRC
    send_frame.data[5] = panel_crc(send_frame.data, 5);

    // 发送给自己，加一个按键号
    send_frame.data[8] = key_number;
    send_frame.length  = 9;
    APP_PRINTF_BUF("send_frame", send_frame.data, send_frame.length);
    app_eventbus_publish(EVENT_PANEL_RX_MY, &send_frame);
    // 发送给其他设备，去掉按键号
    send_frame.length = 8;

    static rf_frame_t rf_tx;
    reg_t *temp_reg = app_get_reg();
    app_creat_frame(&rf_tx, SourceData, temp_reg);
    rf_tx.rf_data[10] = send_frame.length;
    memcpy(&rf_tx.rf_data[11], send_frame.data, send_frame.length);

    uint8_t *data_p  = &rf_tx.rf_data[11];
    uint8_t data_len = rf_tx.rf_data[10];

    memcpy(last_data, data_p, data_len);

    rf_tx.rf_len = RF_PAYLOAD;
    app_rf_tx(&rf_tx, true);
    // APP_PRINTF_BUF("send", rf_tx.rf_data, rf_tx.rf_len);
}
#endif

// Used for creating RF frames
static void app_creat_frame(rf_frame_t *frame, rf_frame_type type, const reg_t *reg)
{
    frame->rf_data[0] = reg->cpadd_h;
    frame->rf_data[1] = reg->cpadd_l;

    frame->rf_data[4] = type;
    frame->rf_data[5] = reg->cplei;
    frame->rf_data[6] = reg->zuwflag;
    frame->rf_data[7] = 0x00;
    frame->rf_data[8] = reg->room_h;
    frame->rf_data[9] = reg->room_l;

    uint16_t addr     = (type == SourceData || type == ForwardData) ? 0xFFFF : 0x0000;
    frame->rf_data[2] = (addr >> 8) & 0xFF;
    frame->rf_data[3] = addr & 0xFF;
}

static void app_save_cfg(uint8_t *cfg_data, uint8_t length)
{
    static uint32_t temp_cfg[10];
    app_uint8_to_uint32(cfg_data, 40, temp_cfg, sizeof(temp_cfg));
    __disable_irq();
    bsp_flash_write(FLASH_BASE_ADDR, temp_cfg, sizeof(temp_cfg));
    __enable_irq();
    app_load_config(CFG);
}

static void app_save_reg(uint8_t reg_addr, uint8_t *reg_data, uint8_t length)
{
    if (length == 1) {
        if ((reg_addr < 4 || reg_addr > 8) && reg_addr != 12) {
            APP_ERROR("Invalid single register address");
            return;
        }
    } else if (length == 5) {
        if (reg_addr != 4) {
            APP_ERROR("Batch write must start from address 4");
            return;
        }
    } else {
        return;
    }
    // From register mapping to array
    static const uint8_t reg_to_arr[] = {[4] = 0, [5] = 1, [6] = 2, [7] = 3, [8] = 4, [12] = 5};

    reg_t *reg = app_get_reg();

    static uint8_t old_reg[6] = {0};
    static uint8_t new_reg[6] = {0};

    old_reg[0] = reg->channel;
    old_reg[1] = reg->zuwflag;
    old_reg[2] = reg->room_h;
    old_reg[3] = reg->room_l;
    old_reg[4] = reg->forward_en;
    old_reg[5] = reg->key;

    memcpy(new_reg, old_reg, sizeof(new_reg));

    for (uint8_t i = 0; i < length; i++) {
        uint8_t current_addr = reg_addr + i;
        uint8_t arr_index    = reg_to_arr[current_addr];
        new_reg[arr_index]   = reg_data[i];
    }

    if (memcmp(old_reg, new_reg, sizeof(old_reg)) == 0) {
        APP_PRINTF("reg is not change\n");
    } else {
        APP_PRINTF("reg is change\n");
        static uint32_t temp_reg[2];
        app_uint8_to_uint32(new_reg, sizeof(new_reg), temp_reg, sizeof(temp_reg));
        __disable_irq();
        bsp_flash_write(FLASH_REG_ADDR, temp_reg, sizeof(temp_reg));
        __enable_irq();
        app_load_config(REG);
    }
}
