#include <stdio.h>
#include "bsp_uart.h"
#include "../app/gpio.h"
#include "../device/device_manager.h"

#if defined USE_RTT
#include "../../rtt/SEGGER_RTT.h"
#endif

UART_HandleTypeDef usart1_handle         = {0};
static usart1_rx_buf_t uart1_buf         = {0};
static usart_rx1_callback_t rx1_callback = NULL;
void app_usart1_rx_callback(usart_rx1_callback_t callback)
{
    rx1_callback = callback;
}

UART_HandleTypeDef usart2_handle         = {0};
static usart2_rx_buf_t uart2_buf         = {0};
static usart_rx2_callback_t rx2_callback = NULL;
void app_usart2_rx_callback(usart_rx2_callback_t callback)
{
    rx2_callback = callback;
}

static void bsp_uart_gpio_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_UARTx_RX_GPIO_CLK_ENABLE();
    __HAL_RCC_UARTx_TX_GPIO_CLK_ENABLE();

    // RX1
    GPIO_InitStruct.Pin       = UART1_RX_PIN;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_PULLUP;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = UART1_RX_ALTERNATE_AFn;
    HAL_GPIO_Init(UART1_RX_PORT, &GPIO_InitStruct);
    // TX1
    GPIO_InitStruct.Pin       = UART1_TX_PIN;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Alternate = UART1_TX_ALTERNATE_AFn;
    HAL_GPIO_Init(UART1_TX_PORT, &GPIO_InitStruct);

    // RX2
    GPIO_InitStruct.Pin       = UART2_RX_PIN;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_PULLUP;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = UART2_RX_ALTERNATE_AFn;
    HAL_GPIO_Init(UART2_RX_PORT, &GPIO_InitStruct);
    // TX2
    GPIO_InitStruct.Pin       = UART2_TX_PIN;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Alternate = UART2_TX_ALTERNATE_AFn;
    HAL_GPIO_Init(UART2_TX_PORT, &GPIO_InitStruct);
}

static void bsp_uart_cfg_init(void)
{
    // uart1 配置
    __HAL_RCC_UART1_CLK_ENABLE();
    usart1_handle.Instance = UART1;
#if defined SETTER
    usart1_handle.Init.BaudRate = 115200;
#else
    usart1_handle.Init.BaudRate = 9600;
#endif
    usart1_handle.Init.WordLength = UART_WORDLENGTH_8B;
    usart1_handle.Init.StopBits   = UART_STOPBITS_1;
    usart1_handle.Init.Parity     = UART_PARITY_NONE;
    usart1_handle.Init.Mode       = UART_MODE_TX_RX;
    HAL_UART_Init(&usart1_handle);
    HAL_NVIC_SetPriority(UART1_IRQn, 0, 1);
    HAL_NVIC_EnableIRQ(UART1_IRQn);
    __HAL_UART_ENABLE_IT(&usart1_handle, UART_IT_RXNE);
    __HAL_UART_ENABLE_IT(&usart1_handle, UART_IT_IDLE);

    // uart2 配置
    __HAL_RCC_UART2_CLK_ENABLE();
    usart2_handle.Instance        = UART2;
    usart2_handle.Init.BaudRate   = BAUDRATE;
    usart2_handle.Init.WordLength = UART_WORDLENGTH_8B;
    usart2_handle.Init.StopBits   = UART_STOPBITS_1;
    usart2_handle.Init.Parity     = UART_PARITY_NONE;
    usart2_handle.Init.Mode       = UART_MODE_TX_RX;
    HAL_UART_Init(&usart2_handle);

#ifndef UART2_DEBUG
    HAL_NVIC_SetPriority(UART2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(UART2_IRQn);
    __HAL_UART_ENABLE_IT(&usart2_handle, UART_IT_RXNE);
    __HAL_UART_ENABLE_IT(&usart2_handle, UART_IT_IDLE);
#endif
}

void bsp_uart_init(void)
{
    bsp_uart_gpio_init();
    bsp_uart_cfg_init();
}

void bsp_uart1_send(uint8_t data)
{
    HAL_UART_Transmit(&usart1_handle, &data, 1, 5000);
}

void bsp_uart2_send(uint8_t data)
{
    HAL_UART_Transmit(&usart2_handle, &data, 1, 5000);
}

HAL_StatusTypeDef bsp_uart1_send_buf(uint8_t *data, uint8_t length)
{
    if (data == NULL || length == 0) {
        return HAL_ERROR;
    }
    return HAL_UART_Transmit(&usart1_handle, data, length, 1000);
}

HAL_StatusTypeDef bsp_rs485_send_buf(uint8_t *data, uint8_t length)
{
    // 参数检查
    if (data == NULL || length == 0) {
        return HAL_ERROR;
    }
    APP_SET_GPIO(PB3, true);
    HAL_StatusTypeDef status = HAL_UART_Transmit(&usart1_handle, data, length, 1000);

    if (status == HAL_OK) {
        while (__HAL_UART_GET_FLAG(&usart1_handle, UART_FLAG_TC) == RESET);
    }

    APP_SET_GPIO(PB3, false);
    return status;
}

void USART1_IRQHandler(void)
{
    uint32_t sr = READ_REG(UART1->SR);
    if ((sr & USART_SR_RXNE) != 0) {
        volatile uint8_t ch = (uint8_t)(usart1_handle.Instance->DR & 0xFF);
        if (uart1_buf.len < sizeof(uart1_buf.data)) {
            uart1_buf.data[uart1_buf.len++] = ch;
        }
    }
    if ((sr & USART_SR_IDLE) != 0) {
        __HAL_UART_CLEAR_IDLEFLAG(&usart1_handle);
        if (rx1_callback && uart1_buf.len > 0) {
            rx1_callback(&uart1_buf);
        }
        uart1_buf.len = 0;
    }
    if ((sr & USART_SR_ORE) != 0) {
        __HAL_UART_CLEAR_OREFLAG(&usart1_handle);
        uart1_buf.len = 0;
    }
}

#ifndef UART2_DEBUG
void USART2_IRQHandler(void)
{
    uint32_t sr = READ_REG(UART2->SR);
    if ((sr & USART_SR_RXNE) != 0) {
        volatile uint8_t ch = (uint8_t)(usart2_handle.Instance->DR & 0xFF);
        if (uart2_buf.len < sizeof(uart2_buf.data)) {
            uart2_buf.data[uart2_buf.len++] = ch;
        }
    }
    if ((sr & USART_SR_IDLE) != 0) {
        __HAL_UART_CLEAR_IDLEFLAG(&usart2_handle);
        if (rx2_callback && uart2_buf.len > 0) {
            rx2_callback(&uart2_buf);
        }
        uart2_buf.len = 0;
    }
    if ((sr & USART_SR_ORE) != 0) {
        __HAL_UART_CLEAR_OREFLAG(&usart2_handle);
        uart2_buf.len = 0;
    }
}
#endif

int fputc(int ch, FILE *f)
{

#if defined USE_RTT
    SEGGER_RTT_PutChar(0, ch);
#else
    bsp_uart2_send(ch);
#endif
    return ch;
}
