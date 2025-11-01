#include "bsp_zero.h"
#include "../bsp/bsp_uart.h"
#include <stdbool.h>
#include <string.h>

#define QUEUE_SIZE   12
#define ZERO_TIMEOUT 40 // 40ms
#define DELAY_TIME   5  // 5ms

typedef struct {
    gpio_pin_t pin;
    bool status;
} gpio_ctrl_t;

static gpio_ctrl_t queue[QUEUE_SIZE];
static volatile uint8_t head = 0;
static volatile uint8_t tail = 0;

static gpio_ctrl_t pending_cmd       = {0};
static volatile bool is_delaying     = false;
static volatile uint16_t delay_count = 0;

static volatile bool zero_failed = false;

static volatile uint32_t zero_timeout_count = 0;

TIM_HandleTypeDef Timer16 = {0};

static bool queue_push(gpio_ctrl_t cmd);
static bool queue_pop(gpio_ctrl_t *cmd);
void bsp_zero_init(void)
{
    // Initialize external interrupt
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitStruct.Mode  = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull  = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pin   = GPIO_PIN_9;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
    HAL_NVIC_SetPriority(EXTI4_15_IRQn, 3, 2);

    // Initialize Timer16
    uint32_t prescaler = (HAL_RCC_GetPCLK1Freq() / 1000) - 1; // 1 ms overflow once
    __HAL_RCC_TIM16_CLK_ENABLE();
    Timer16.Instance               = TIM16;
    Timer16.Init.Period            = 1;
    Timer16.Init.Prescaler         = prescaler;
    Timer16.Init.ClockDivision     = 0;
    Timer16.Init.CounterMode       = TIM_COUNTERMODE_UP;
    Timer16.Init.RepetitionCounter = 0;
    Timer16.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&Timer16) != HAL_OK) APP_ERROR("");
    HAL_NVIC_EnableIRQ(TIM16_IRQn);
    if (HAL_TIM_Base_Start_IT(&Timer16) != HAL_OK) APP_ERROR("");

    head = tail = 0;
}

void EXTI4_15_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_9);
    zero_timeout_count = 0;
    zero_failed        = false;

    if (!is_delaying && queue_pop(&pending_cmd)) {
        is_delaying = true;
        delay_count = 0;
    }
}

void TIM16_IRQHandler(void)
{
    __HAL_TIM_CLEAR_FLAG(&Timer16, TIM_FLAG_UPDATE);

    if (!zero_failed) {
        if (zero_timeout_count < ZERO_TIMEOUT) {
            zero_timeout_count++;
        } else {
            zero_failed = true; // Zero-cross detection failed
        }
    }
    if (is_delaying) {
        delay_count++;
        if (delay_count >= DELAY_TIME) {
            APP_SET_GPIO(pending_cmd.pin, pending_cmd.status);
            // APP_PRINTF("%d %d %d %d\n", APP_GET_GPIO(PB1), APP_GET_GPIO(PA5), APP_GET_GPIO(PA6), APP_GET_GPIO(PB0));
            is_delaying = false;
        }
    }
}

void zero_set_gpio(const gpio_pin_t pin, bool status)
{
    gpio_ctrl_t cmd = {.pin = pin, .status = status};
    if (zero_failed) {
        APP_SET_GPIO(pin, status);
        APP_PRINTF("zero_failed\n");
    } else {
        if (!queue_push(cmd)) {
            APP_PRINTF("zero queue full!\n");
        }
    }
}

static bool queue_push(gpio_ctrl_t cmd)
{
    uint8_t next = (tail + 1) % QUEUE_SIZE;
    if (next == head) return false;
    queue[tail] = cmd;
    tail        = next;
    return true;
}

static bool queue_pop(gpio_ctrl_t *cmd)
{
    if (head == tail) return false;
    *cmd = queue[head];
    head = (head + 1) % QUEUE_SIZE;
    return true;
}
