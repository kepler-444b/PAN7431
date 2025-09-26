#include "py32f0xx.h"
#include "adc.h"
#include "../app/adc.h"
#include "../bsp/bsp_uart.h"
#include "py32f0xx_hal_adc.h"

#define ADC_TO_VOLTAGE_MV(adc_value) (uint16_t)((adc_value * 3300) / 4095) / 10

ADC_HandleTypeDef AdcHandle;
ADC_ChannelConfTypeDef sConfig;
uint32_t gADCxConvertedData = 0;
uint32_t adc_value;

void app_adc_init(void)
{
    __HAL_RCC_ADC_FORCE_RESET();
    __HAL_RCC_ADC_RELEASE_RESET();
    __HAL_RCC_ADC_CLK_ENABLE();

    AdcHandle.Instance = ADC1;

    if (HAL_ADCEx_Calibration_Start(&AdcHandle) != HAL_OK) {
        APP_ERROR();
    }

    AdcHandle.Instance                   = ADC1;
    AdcHandle.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;
    AdcHandle.Init.Resolution            = ADC_RESOLUTION_12B;
    AdcHandle.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    AdcHandle.Init.ScanConvMode          = ADC_SCAN_DIRECTION_BACKWARD;
    AdcHandle.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    AdcHandle.Init.LowPowerAutoWait      = ENABLE;
    AdcHandle.Init.ContinuousConvMode    = ENABLE;
    AdcHandle.Init.DiscontinuousConvMode = DISABLE;
    AdcHandle.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    AdcHandle.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    AdcHandle.Init.DMAContinuousRequests = ENABLE;
    AdcHandle.Init.Overrun               = ADC_OVR_DATA_OVERWRITTEN;
    AdcHandle.Init.SamplingTimeCommon    = ADC_SAMPLETIME_13CYCLES_5;

    if (HAL_ADC_Init(&AdcHandle) != HAL_OK) {
        APP_ERROR();
    }
    sConfig.Rank    = ADC_RANK_CHANNEL_NUMBER;
    sConfig.Channel = ADC_CHANNEL_4;
    if (HAL_ADC_ConfigChannel(&AdcHandle, &sConfig) != HAL_OK) {
        APP_ERROR();
    }
    if (HAL_ADC_Start_DMA(&AdcHandle, &gADCxConvertedData, 1) != HAL_OK) {
        APP_ERROR();
    }
}

uint16_t app_get_adc_value(void)
{
    if (__HAL_DMA_GET_FLAG(DMA1, DMA_ISR_TCIF1)) {
        __HAL_DMA_CLEAR_FLAG(DMA1, DMA_IFCR_CTCIF1);
        return ADC_TO_VOLTAGE_MV((unsigned int)gADCxConvertedData);
    } else {
        return ERR_VOL;
    }
}
