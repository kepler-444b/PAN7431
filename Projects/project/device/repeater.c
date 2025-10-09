#include "repeater.h"
#include "../bsp/bsp_pcb.h"
#include "../bsp/bsp_timer.h"
#include "../bsp/bsp_uart.h"
#include "../app/eventbus.h"

#if defined REPEATER

#define MAX_KEY_COUNT 600
static uint16_t key_count = 0;
static bool key_trigger   = false;
static bool led2_status   = false;

static void app_check_key(void *arg);
static void repeater_event_handler(event_type_e event, void *params);

void app_repeater_init(void)
{
    bsp_repeater_init();
    app_eventbus_subscribe(repeater_event_handler);
    bsp_start_timer(7, 5, app_check_key, NULL, TMR_AUTO_MODE);
}

static void app_check_key(void *arg)
{
    if (APP_GET_GPIO(PA8) == false) {
        if (!key_trigger) {
            if (++key_count >= MAX_KEY_COUNT) {
                key_count   = MAX_KEY_COUNT;
                key_trigger = true;
                app_eventbus_publish(EVENT_REQUEST_NETWORK, NULL);
            }
        }
    } else {
        key_count   = 0;
        key_trigger = false;
    }
}

static void repeater_event_handler(event_type_e event, void *params)
{
    switch (event) {
        case EVENT_LED_BLINK: {
            led2_status = !led2_status;
            APP_SET_GPIO(PB0, led2_status);
        } break;
        default:
            return;
    }
}

#endif