#ifndef _EVENTBUS_H_
#define _EVENTBUS_H_

// 事件类型定义(根据需求修改)
typedef enum {

    EVENT_RF_RX,           // rf 接收
    EVENT_PANEL_RX,        // 面板接收有效帧
    EVENT_PANEL_RX_MY,
    EVENT_SIMULATE_KEY,    // 模拟按键

    EVENT_REQUEST_NETWORK, // 申请组网
    EVENT_UART1_RX,        // UART1接收
    EVENT_LED_BLINK,    

    // 添加更多事件类型...
    EVENT_COUNT // 自动计算事件数量
} event_type_e;

// 事件回调函数类型(带事件类型和参数)
typedef void (*EventHandler)(event_type_e event, void *params);

// 事件结构体(包含事件类型和参数)
typedef struct
{
    event_type_e type;
    void *params;
} event_t;

// 初始化事件总线
void app_eventbus_init(void);

// 订阅所有事件(只需调用一次)
void app_eventbus_subscribe(EventHandler handler);

// 发布事件(带参数)
void app_eventbus_publish(event_type_e event, void *params);

// 处理事件(在主循环中调用)
void app_eventbus_poll(void);
#endif
