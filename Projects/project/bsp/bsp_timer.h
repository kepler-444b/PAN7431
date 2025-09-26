#ifndef __BSP_TIMER_H
#define __BSP_TIMER_H

#include <stdbool.h>

typedef enum {
    TMR_ONCE_MODE = 0,
    TMR_AUTO_MODE = 1
} TMR_MODE_E;

typedef struct
{
    volatile TMR_MODE_E Mode;
    volatile bool Flag;
    volatile uint32_t Count;
    volatile uint32_t PreLoad;
    void (*Callback)(void *arg);
    void *arg;
} SOFT_TMR;

void bsp_timer_poll(void);
void bsp_timer_init(void);
void bsp_start_timer(uint8_t id, uint32_t period, void (*cb)(void *), void *arg, uint8_t auto_reload);
void bsp_stop_timer(uint8_t _id);
int32_t bsp_get_run_time(void);

#endif
