#ifndef _APP_H_
#define _APP_H_
#include <stdint.h>

#define RF_PAYLOAD  64
#define NET_CHANNEL 1 // net_work channrl

typedef struct {
    uint8_t rf_data[RF_PAYLOAD];
    uint8_t rf_len;
} rf_frame_t;

typedef void (*rf_rx_callback_t)(rf_frame_t *rf_buf);
void app_rf_rx_callback(rf_rx_callback_t callback);

#endif
