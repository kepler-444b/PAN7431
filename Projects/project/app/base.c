#include "py32f0xx.h"
#include <string.h>
#include <math.h>
#include "base.h"
#include "../app/config.h"
#include "../bsp/bsp_uart.h"

bool app_uint8_to_uint32(const uint8_t *input, size_t input_count, uint32_t *output, size_t output_count)
{
    if (!input || !output)
        return false;
    if (output_count < (input_count + 3) / 4) {
        return false;
    }

    for (size_t i = 0; i < (input_count + 3) / 4; i++) {
        uint32_t packed      = 0;
        size_t bytes_to_pack = (input_count - i * 4) >= 4 ? 4 : (input_count - i * 4);

        for (size_t j = 0; j < bytes_to_pack; j++) {
            packed |= (uint32_t)input[i * 4 + j] << (j * 8); // 小端序
        }
        output[i] = packed;
    }
    return true;
}

bool app_uint32_to_uint8(const uint32_t *input, size_t input_count, uint8_t *output, size_t output_count)
{
    if (!input || !output)
        return false;
    if (output_count < input_count * 4) {
        return false;
    }

    for (size_t i = 0; i < input_count; i++) {
        for (size_t j = 0; j < 4; j++) {
            output[i * 4 + j] = (uint8_t)((input[i] >> (j * 8)) & 0xFF); // 小端序
        }
    }
    return true;
}

void app_get_uid(uint8_t *uid)
{
    uint32_t uid0 = HAL_GetUIDw0();
    uint32_t uid1 = HAL_GetUIDw1();
    uint32_t uid2 = HAL_GetUIDw2();

    memcpy(uid, &uid0, 4);
    memcpy(uid + 4, &uid1, 4);
    memcpy(uid + 8, &uid2, 4);
}

uint16_t app_calculate_average(const uint16_t *buffer, uint16_t count)
{
    if (buffer == NULL || count == 0) {
        return 0;
    }
    uint32_t sum = 0;
    for (uint16_t i = 0; i < count; i++) {
        sum += buffer[i];
    }
    return (uint16_t)(sum / count);
}

uint8_t app_calculate_std_dev(const uint8_t *array, uint8_t count, uint8_t mean)
{
    if (count == 0 || count == 1) {
        return 0.0f;
    }

    float sum_squared_diff = 0.0f;
    const float mean_f     = (float)mean;

    uint8_t i             = 0;
    const uint8_t count_4 = count & ~0x03;

    for (; i < count_4; i += 4) {
        float diff0 = (float)array[i] - mean_f;
        float diff1 = (float)array[i + 1] - mean_f;
        float diff2 = (float)array[i + 2] - mean_f;
        float diff3 = (float)array[i + 3] - mean_f;

        sum_squared_diff += diff0 * diff0 + diff1 * diff1 +
                            diff2 * diff2 + diff3 * diff3;
    }
    for (; i < count; i++) {
        float diff = (float)array[i] - mean_f;
        sum_squared_diff += diff * diff;
    }

    float variance = sum_squared_diff / (count - 1);
    float std_dev  = sqrtf(variance);

    float rounded = roundf(std_dev);

    if (rounded < 0) {
        return 0;
    } else if (rounded > 255) {
        return 255;
    }
    return (uint8_t)rounded;
}

uint16_t app_get_crc(uint8_t *buffer, uint8_t len)
{
    uint16_t wcrc = 0XFFFF;
    uint8_t temp;
    uint8_t CRC_L;
    uint8_t CRC_H;
    uint16_t i = 0, j = 0;
    for (i = 0; i < len; i++) {
        temp = *buffer & 0X00FF;
        buffer++;
        wcrc ^= temp;
        for (j = 0; j < 8; j++) {
            if (wcrc & 0X0001) {
                wcrc >>= 1;
                wcrc ^= 0XA001;
            } else {
                wcrc >>= 1;
            }
        }
    }
    CRC_L = wcrc & 0xff;
    CRC_H = wcrc >> 8;
    return ((CRC_L << 8) | CRC_H);
}

// CRC check for the panel
uint8_t panel_crc(uint8_t *rxbuf, uint8_t len)
{
    uint8_t sum = 0;
    for (uint8_t i = 0; i < len; i++) {
        sum += rxbuf[i];
    }
    return (uint8_t)(0xFF - sum + 1);
}