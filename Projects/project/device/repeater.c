#include "repeater.h"
#include "../bsp/bsp_pcb.h"
#include "../bsp/bsp_timer.h"
#include "../app/eventbus.h"

#if defined REPEATER

#define MAX_KEY_COUNT 600
static uint16_t key_count = 0;

void app_check_key(void *arg);

void app_repeater_init(void)
{
    bsp_repeater_init();
    bsp_start_timer(7, 5, app_check_key, NULL, TMR_AUTO_MODE);
}

void app_check_key(void *arg)
{
    if (APP_GET_GPIO(PA8) == false) {
        if (++key_count >= MAX_KEY_COUNT) {
            key_count = MAX_KEY_COUNT;
            app_eventbus_publish(EVENT_REQUEST_NETWORK, NULL);
        }
    } else {
        key_count = 0;
    }
}

#endif