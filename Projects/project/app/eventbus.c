#include <stdio.h>
#include "eventbus.h"
#include "../bsp/bsp_uart.h"

#define MAX_EVENT_HANDLERS 8  // 最大事件处理器数量(最多有8个模块/组件订阅事件)
#define EVENT_QUEUE_SIZE   16 // 事件队列大小(最多能缓存16个待处理的事件)

// 事件处理器数组
static EventHandler eventHandlers[MAX_EVENT_HANDLERS];
static uint8_t handlerCount = 0;

// 事件队列
static event_t eventQueue[EVENT_QUEUE_SIZE];
static uint8_t queueHead = 0;
static uint8_t queueTail = 0;
static uint8_t queueSize = 0;

// 初始化事件总线
void app_eventbus_init(void)
{
    handlerCount = 0;
    queueHead = queueTail = queueSize = 0;
}

// 订阅所有事件(只需调用一次)
void app_eventbus_subscribe(EventHandler handler)
{
    if (handlerCount >= MAX_EVENT_HANDLERS) {
        return;
    }
    eventHandlers[handlerCount++] = handler;
}

// 发布事件(带参数)
void app_eventbus_publish(event_type_e event, void *params)
{
    if (event >= EVENT_COUNT)
        return;

    if (queueSize >= EVENT_QUEUE_SIZE) {
        APP_ERROR("queue full");
        return; // 队列满
    }
    eventQueue[queueHead] = (event_t){event, params};
    queueHead             = (queueHead + 1) % EVENT_QUEUE_SIZE;
    queueSize++;
}

// 处理事件(在主循环中调用)
void app_eventbus_poll(void)
{
    while (queueSize > 0) {
        event_t currentEvent = eventQueue[queueTail];
        queueTail            = (queueTail + 1) % EVENT_QUEUE_SIZE;
        queueSize--;

        // 调用所有订阅者的处理函数
        for (uint8_t i = 0; i < handlerCount; i++) {
            if (eventHandlers[i]) {
                eventHandlers[i](currentEvent.type, currentEvent.params);
            }
        }
    }
}
