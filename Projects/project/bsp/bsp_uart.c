#include <stdio.h>
#include "bsp_uart.h"

UART_HandleTypeDef usart1_handle         = {0};
static usart1_rx_buf_t uart1_buf         = {0};
static usart_rx1_callback_t rx1_callback = NULL;
void app_usart1_rx_callback(usart_rx1_callback_t callback)
{
    rx1_callback = callback;
}

UART_HandleTypeDef usart2_handle = {0};

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
#ifndef UART2_DEBUG
    // RX2
    GPIO_InitStruct.Pin       = UART2_RX_PIN;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_PULLUP;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = UART2_RX_ALTERNATE_AFn;
    HAL_GPIO_Init(UART2_RX_PORT, &GPIO_InitStruct);
#endif
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
    usart1_handle.Instance        = UART1;
    usart1_handle.Init.BaudRate   = BAUDRATE;
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
    // 参数检查
    if (data == NULL || length == 0) {
        return HAL_ERROR;
    }
    // 直接使用HAL_UART_Transmit发送所有数据
    return HAL_UART_Transmit(&usart1_handle, data, length, 5000);
}

void USART1_IRQHandler(void)
{
    uint32_t sr = READ_REG(UART1->SR);

    // 处理接收数据
    if ((sr & USART_SR_RXNE) != 0) {
        uint8_t ch = (uint8_t)(usart1_handle.Instance->DR & 0xFF);
        if (uart1_buf.len < sizeof(uart1_buf.data)) {
            uart1_buf.data[uart1_buf.len++] = ch;
        }
        // 读取DR会自动清除RXNE标志
    }

    // 处理空闲中断
    if ((sr & USART_SR_IDLE) != 0) {
        __HAL_UART_CLEAR_IDLEFLAG(&usart1_handle);
        if (rx1_callback && uart1_buf.len > 0) {
            rx1_callback(&uart1_buf);
        }
        uart1_buf.len = 0;
    }

    // 关键修复：添加溢出错误处理
    if ((sr & USART_SR_ORE) != 0) {
        // 清除溢出错误标志（通过读取SR然后读取DR）
        __HAL_UART_CLEAR_OREFLAG(&usart1_handle);
        // 重置接收状态，防止卡死
        uart1_buf.len = 0;
    }
}

int fputc(int ch, FILE *f)
{
    // bsp_uart1_send(ch);
    bsp_uart2_send(ch);
    return ch;
}
