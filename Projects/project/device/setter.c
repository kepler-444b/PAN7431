#include <string.h>
#include <stdlib.h>
#include "setter.h"
#include "pan211.h"
#include "../bsp/bsp_pcb.h"
#include "../bsp/bsp_uart.h"
#include "../bsp/bsp_timer.h"
#include "../app/protocol.h"
#include "../app/eventbus.h"
#include "../app/app.h"

#if defined SETTER

#define SETTER_HEAD(arr, third_byte) \
    do {                                  \
        (arr)[0] = 0xAC;                  \
        (arr)[1] = 0x85;                  \
        (arr)[2] = (third_byte);          \
        (arr)[3] = 0x40;                  \
    } while (0)

#define MASK_BIT6          (0x00000040UL)
#define MASK_BIT7          (0x00000080UL)

#define RSSI_SAMPLE_COUNT  500
#define RSSI_REP_FRAME_LEN 17

typedef struct
{
    uint8_t value[100];
    uint8_t count;
    uint8_t min_value;
    uint8_t max_value;
    uint8_t average_value;
    uint8_t std_dev_value;
    uint32_t sum_value;

} history_value_t;

static int sample_count   = 0;
static int local_max_rssi = -200;
static bool rssi_enabled  = false;

history_value_t my_history_value;
static uint16_t current_device_addr;
static uint16_t current_room_number;
static uint8_t recv_from_rfware[70];

static void rssi_init(void);
static int rssi_get_real_value(void);
static void tran_delay_stop_rassi(void *arg);
static void recv_from_rf(rf_frame_t *buf);
static void recv_from_soft(usart1_rx_buf_t *buf);

void app_setter_init(void)
{
    bsp_setter_init();
    app_usart1_rx_callback(recv_from_soft);
    app_rf_rx_callback(recv_from_rf);
    APP_PRINTF("app_SETTER_init\n");
}

bool rssi_is_enabled(void)
{
    return rssi_enabled;
}

static void rssi_init(void)
{
    rssi_enabled   = false;
    sample_count   = 0;
    local_max_rssi = -200;
    PAN211_RxStart();
}

static int rssi_get_real_value(void)
{
    __disable_irq();
    uint8_t RssiCode[2];
    uint8_t tmp = PAN211_ReadReg(0x46);

    tmp = tmp & 0x7F;
    PAN211_WriteReg(0x46, tmp | MASK_BIT7);
    PAN211_WriteReg(0x46, tmp | MASK_BIT7 | MASK_BIT6);
    PAN211_ReadRegs(0x7E, RssiCode, 2);
    int RSSIdBm = (int)(((RssiCode[0] + ((uint16_t)RssiCode[1] << 8)) & 0x3FFF) - 16384) / 4;
    PAN211_WriteReg(0x46, tmp);
    __enable_irq();
    return RSSIdBm;
}

void rssi_update(void)
{
    if (!rssi_enabled) return;

    if (sample_count == 0) {
        local_max_rssi = -200;
    }

    int current_rssi = rssi_get_real_value();
    if (current_rssi > local_max_rssi) {
        local_max_rssi = current_rssi;
    }
    sample_count++;
    if (sample_count >= RSSI_SAMPLE_COUNT) {
        int abs_value     = abs(local_max_rssi);
        uint8_t new_value = (uint8_t)abs_value;
        static uint8_t rssi_frame[7];
        SETTER_HEAD(rssi_frame, SourceData);
        rssi_frame[3] = 0x03;
        rssi_frame[4] = new_value;
        rssi_frame[5] = 0x0D;
        rssi_frame[6] = 0x0A;
        bsp_uart1_send_buf(rssi_frame, 7);

        if (my_history_value.count < sizeof(my_history_value.value) / sizeof(my_history_value.value[0])) {
            my_history_value.value[my_history_value.count] = new_value;
            my_history_value.sum_value += new_value;

            if (my_history_value.count == 0) {
                my_history_value.min_value = new_value;
                my_history_value.max_value = new_value;
            } else {
                if (new_value < my_history_value.min_value) {
                    my_history_value.min_value = new_value;
                }
                if (new_value > my_history_value.max_value) {
                    my_history_value.max_value = new_value;
                }
            }
            my_history_value.count++;
        }
        APP_PRINTF("%d\n", new_value);
        sample_count = 0;
    }
}

static void tran_delay_stop_rassi(void *arg)
{
    APP_PRINTF("rssi_enabled = false\n");
    rssi_enabled = false;
    PAN211_RxStart();
    my_history_value.average_value = my_history_value.sum_value / my_history_value.count;
    my_history_value.std_dev_value = app_calculate_std_dev(my_history_value.value, my_history_value.count, my_history_value.average_value);

    APP_PRINTF("mean value:%d min_value:%d max_value:%d std_dev:%d\n",
               my_history_value.average_value,
               my_history_value.max_value,
               my_history_value.min_value,
               my_history_value.std_dev_value);

    uint8_t rssi_rep[RSSI_REP_FRAME_LEN];
    rssi_rep[0] = (current_device_addr >> 8) & 0xFF;
    rssi_rep[1] = current_device_addr & 0xFF;

    rssi_rep[2]  = 0x00;
    rssi_rep[3]  = 0x00;
    rssi_rep[4]  = RssiTest;
    rssi_rep[5]  = 0x00;
    rssi_rep[6]  = 0x00;
    rssi_rep[7]  = 0x00;
    rssi_rep[8]  = (current_room_number >> 8) & 0xFF;
    rssi_rep[9]  = current_room_number & 0XFF;
    rssi_rep[10] = 0x04;
    rssi_rep[11] = my_history_value.average_value;
    rssi_rep[12] = my_history_value.max_value;
    rssi_rep[13] = my_history_value.min_value;
    rssi_rep[14] = my_history_value.std_dev_value;
    rssi_rep[15] = 0x0D;
    rssi_rep[16] = 0x0A;

    bsp_uart1_send_buf(rssi_rep, RSSI_REP_FRAME_LEN);

    my_history_value.count         = 0;
    my_history_value.sum_value     = 0;
    my_history_value.max_value     = 0;
    my_history_value.min_value     = 0;
    my_history_value.average_value = 0;
}

static void recv_from_soft(usart1_rx_buf_t *buf)
{
    uint8_t order = buf->data[2];
    uint8_t len   = buf->data[3];

    if ((buf->data[0] == 0x01) && (buf->data[1] == 0x02) && (buf->data[2] == 0x03)) {

        static uint8_t temp[6] = {0xac, 0x85, 0xff, 0x01, 0x02, 0x03};
        APP_SET_GPIO(PA5, true);
        bsp_uart1_send_buf(temp, 6);

        APP_PRINTF("----connect to soft----\n");
        return;
    } else if ((buf->data[0] != 0xac) || (buf->data[1] != 0x85)) {
        return;
    }
    static rf_frame_t obj;
    memset(obj.rf_data, 0, sizeof(obj.rf_data));
    obj.rf_len = 0;

    // APP_PRINTF_BUF("[recv from software]", buf->data, buf->len);
    switch (order) {
        case 0x00: // 接收到的码,源码返回
            bsp_uart1_send_buf(&buf->data[4], len);
            break;
        case 0x01: // 数据转发到无线2.4G
            memcpy(obj.rf_data, &buf->data[4], len);
            obj.rf_len = RF_PAYLOAD;
            app_rf_tx(&obj);
            break;
        case 0x02: // 接收到的数据转发给软件
            break;
        case 0x03: // 读取信道和发送设置码
            break;
        case 0x04: {                         // 设置信道和发送设置码
            PAN211_SetChannel(buf->data[4]); // Switch to channel 1
            APP_PRINTF("---- switch channel[%d] ----\n", buf->data[4]);
        } break;
        case 0x05:
            break;
        case 0x06:
            break;
        case 0x07:
            break;
        case 0x08:
            break;
        case 0x09: // 软件通讯 AC 85 09 XX+data
            break;
        default:
            return;
    }
}

static bool led_status;
static void recv_from_rf(rf_frame_t *buf)
{
    led_status = !led_status;
    APP_SET_GPIO(PA6, led_status);
    uint8_t cmd = buf->rf_data[4];
    uint8_t len = buf->rf_data[10];
#if 1
    if (cmd == RssiEnd) {
        current_device_addr = (buf->rf_data[0] << 8) | buf->rf_data[1];
        current_room_number = (buf->rf_data[8] << 8) | buf->rf_data[9];
        rssi_init();
        rssi_enabled = true;
        bsp_start_timer(4, 5000, tran_delay_stop_rassi, NULL, TMR_ONCE_MODE);
        APP_PRINTF("rssi_enabled = true\n");
        return;
    } else {
        SETTER_HEAD(recv_from_rfware, SourceData);
        recv_from_rfware[3] = 0x40;
        uint8_t new_len     = len + 15;
        memcpy(&recv_from_rfware[4], buf->rf_data, new_len);
        recv_from_rfware[new_len]     = 0x0D;              // "\r"
        recv_from_rfware[new_len + 1] = 0x0A;              // "\n"
        bsp_uart1_send_buf(recv_from_rfware, new_len + 2); /// 发送数据
    }
#else
    switch (cmd) {
        case RssiEnd: {
            current_device_addr = (buf->rf_data[0] << 8) | buf->rf_data[1];
            current_room_number = (buf->rf_data[8] << 8) | buf->rf_data[9];
            rssi_init();
            rssi_enabled = true;
            bsp_start_timer(4, 5000, tran_delay_stop_rassi, NULL, TMR_ONCE_MODE);
            APP_PRINTF("rssi_enabled = true\n");
            return;
        } break;
        case Request:
        case SBAnswer:
        case SourceData:
        case RssiTest:
        case TestFrame:
        case FindSB:
        case ForwardData:
            SETTER_HEAD(recv_from_rfware, SourceData);
            break;
        default:
            return;
    }
    recv_from_rfware[3] = 0x40;
    uint8_t new_len     = len + 15;
    memcpy(&recv_from_rfware[4], buf->rf_data, new_len);
    recv_from_rfware[new_len]     = 0x0D;              // "\r"
    recv_from_rfware[new_len + 1] = 0x0A;              // "\n"
    bsp_uart1_send_buf(recv_from_rfware, new_len + 2); // 发送数据
    // APP_PRINTF("1\n");
// APP_PRINTF_BUF("recv_from_rfware", recv_from_rfware, 70);
#endif
}

#endif