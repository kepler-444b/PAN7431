#ifndef __BSP_UART_H
#define __BSP_UART_H

#include "py32f0xx.h"
#include <stdint.h>

#define BAUDRATE 115200
#define APP_DEBUG   // 用于开启整个工程的打印信息
#define UART2_DEBUG // USART2 仅用于调试
// #define USE_RTT  // 使用RTT调试

// usart1 相关配置(用于通信)
#define UART1                        USART1
#define UART1_TX_PORT                GPIOB
#define UART1_TX_PIN                 GPIO_PIN_8
#define UART1_RX_PORT                GPIOB
#define UART1_RX_PIN                 GPIO_PIN_2
#define UART1_TX_ALTERNATE_AFn       GPIO_AF8_USART1
#define UART1_RX_ALTERNATE_AFn       GPIO_AF0_USART1
#define UART1_IRQn                   USART1_IRQn
#define __HAL_RCC_UART1_CLK_ENABLE() __HAL_RCC_USART1_CLK_ENABLE()

// uart2 相关配置(用于调试)
#define UART2                                USART2
#define UART2_TX_PORT                        GPIOB
#define UART2_TX_PIN                         GPIO_PIN_6
#define UART2_RX_PORT                        GPIOB
#define UART2_RX_PIN                         GPIO_PIN_7
#define UART2_TX_ALTERNATE_AFn               GPIO_AF4_USART2
#define UART2_RX_ALTERNATE_AFn               GPIO_AF4_USART2
#define UART2_IRQn                           USART2_IRQn
#define __HAL_RCC_UART2_CLK_ENABLE()         __HAL_RCC_USART2_CLK_ENABLE()

#define __HAL_RCC_UARTx_RX_GPIO_CLK_ENABLE() __HAL_RCC_GPIOB_CLK_ENABLE()
#define __HAL_RCC_UARTx_TX_GPIO_CLK_ENABLE() __HAL_RCC_GPIOB_CLK_ENABLE()

#if defined APP_DEBUG
#define APP_PRINTF(...) printf(__VA_ARGS__)
#define APP_PRINTF_BUF(name, buf, len)                        \
    do {                                                      \
        APP_PRINTF("%s: ", (name));                           \
        for (size_t i = 0; i < (len); i++) {                  \
            APP_PRINTF("%02X ", ((const uint8_t *)(buf))[i]); \
        }                                                     \
        APP_PRINTF("\n");                                     \
    } while (0)
#define APP_ERROR(...) \
    APP_PRINTF("[#%s#] ERROR!\n", __func__)

#else

#define APP_PRINTF(...)
#define APP_PRINTF_BUF(name, buf, len)
#define APP_ERROR(fmt, ...)
#endif

#define UART1_RECV_SIZE 255
#define UART2_RECV_SIZE 255

typedef struct {
    uint8_t data[UART1_RECV_SIZE];
    uint8_t len;
} usart1_rx_buf_t;

typedef struct {
    uint8_t data[UART2_RECV_SIZE];
    uint8_t len;
} usart2_rx_buf_t;

typedef void (*usart_rx1_callback_t)(usart1_rx_buf_t *buf);
typedef void (*usart_rx2_callback_t)(usart2_rx_buf_t *buf);
void app_usart1_rx_callback(usart_rx1_callback_t callback);
void app_usart2_rx_callback(usart_rx2_callback_t callback);

void bsp_uart_init(void);
void bsp_uart1_send(uint8_t data);
void bsp_uart2_send(uint8_t data);

HAL_StatusTypeDef bsp_uart1_send_buf(uint8_t *data, uint8_t length);
HAL_StatusTypeDef bsp_rs485_send_buf(uint8_t *data, uint8_t length);

#endif
