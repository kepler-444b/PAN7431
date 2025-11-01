#include "main.h"
#include "pan211.h"
#include "../../app/app.h"
#include "../../app/iwdg.h"
#include "../../app/base.h"
#include "../../app/eventbus.h"
#include "../../app/config.h"
#include "../../app/protocol.h"
#include "../../bsp/bsp_uart.h"
#include "../../bsp/bsp_timer.h"
#include "../../app/pwm.h"
#include "../../device/device_manager.h"

static rf_frame_t my_rf_rx;
static rf_rx_callback_t g_rf_rx = NULL;

void app_rf_rx_callback(rf_rx_callback_t callback)
{
    g_rf_rx = callback;
}

int main(void)
{
    // Reset of all peripherals, Initializes the Systick
    HAL_Init();
    APP_SystemClockConfig();
    HAL_Delay(100);

    PAN211_Init();
    PAN211_SetRxAddr(DEFAULT_ADDR, 5);
    PAN211_SetTxAddr(DEFAULT_ADDR, 5);
    PAN211_ClearIRQFlags(0xFF);
    bsp_uart_init();

#if defined PANEL
    app_load_config(CFG);
#endif
    app_load_config(REG);
    bsp_timer_init();
    app_iwdg_init();
    app_eventbus_init();
    app_protocol_init();
    app_jump_device();
    PAN211_RxStart();
    PAN211_SetChannel(app_get_reg()->channel);

    while (1) {
        bsp_timer_poll();
        app_eventbus_poll();
#if defined SETTER
        if (!rssi_is_enabled()) {
#endif
            if (IRQDetected()) {
                if (PAN211_GetIRQFlags() & RF_IT_RX_IRQ) {

                    PAN211_ReadFIFO(my_rf_rx.rf_data, RF_PAYLOAD);
                    my_rf_rx.rf_len = RF_PAYLOAD;
                    if (g_rf_rx) {
                        g_rf_rx(&my_rf_rx);
                    }
                } else if (PAN211_GetIRQFlags() & RF_IT_TX_IRQ) {
                    PAN211_RxStart(); // Switch to RX mode
                    // APP_PRINTF("PAN211_RxStart\n");
                }
                PAN211_ClearIRQFlags(0xFF);
            }
#if defined SETTER
        } else if (rssi_is_enabled()) {
            rssi_update();
        }
#endif
    }
}

static void APP_SystemClockConfig(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSI;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.HSIDiv              = RCC_HSI_DIV1;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_24MHz;
    RCC_OscInitStruct.HSEState            = RCC_HSE_OFF;
    RCC_OscInitStruct.LSIState            = RCC_LSI_ON;
    RCC_OscInitStruct.LSEState            = RCC_LSE_OFF;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSI;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        APP_ERROR("");
    }
    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK) {
        APP_ERROR("");
    }
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @param  None
 * @retval None
 */
void APP_ErrorHandler(void)
{
    while (1) {
    }
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
    /* User can add his own implementation to report the file name and line number,
       for example: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* Infinite loop */
    while (1) {
    }
}
#endif
