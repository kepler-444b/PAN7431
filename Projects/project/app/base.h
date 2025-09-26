#ifndef _BASE_H_
#define _BASE_H_

#include <stdbool.h>
#include <stdint.h>

#define PUYA_UID_BASE          (0x1FFF0E00UL) // UID 起始地址
#define PUYA_UID_SIZE          128            // UID 大小(字节)

#define BIT0(flag)             ((bool)((flag) & 0x01)) // 第0位
#define BIT1(flag)             ((bool)((flag) & 0x02)) // 第1位
#define BIT2(flag)             ((bool)((flag) & 0x04)) // 第2位
#define BIT3(flag)             ((bool)((flag) & 0x08)) // 第3位
#define BIT4(flag)             ((bool)((flag) & 0x10)) // 第4位
#define BIT5(flag)             ((bool)((flag) & 0x20)) // 第5位
#define BIT6(flag)             ((bool)((flag) & 0x40)) // 第6位
#define BIT7(flag)             ((bool)((flag) & 0x80)) // 第7位

#define L_BIT(byte)            ((uint8_t)((byte) & 0x0F))        // 低4位
#define H_BIT(byte)            ((uint8_t)(((byte) >> 4) & 0x0F)) // 高4位

#define MAKE_U16(high, low)    (((uint16_t)(high) << 8) | (low))
#define HI8(u16)               ((uint8_t)(((u16) >> 8) & 0xFF))
#define LO8(u16)               ((uint8_t)((u16) & 0xFF))

#define COMBINE_U16(high, low) (((uint16_t)(high) << 8) | (uint16_t)(low))

uint16_t app_calculate_average(const uint16_t *buffer, uint16_t count);
uint8_t app_calculate_std_dev(const uint8_t *array, uint8_t count, uint8_t mean);

void app_get_uid(uint8_t *uid);
bool app_uint8_to_uint32(const uint8_t *input, size_t input_count, uint32_t *output, size_t output_count);
bool app_uint32_to_uint8(const uint32_t *input, size_t input_count, uint8_t *output, size_t output_count);
uint16_t app_get_crc(uint8_t *buffer, uint8_t len);
uint8_t panel_crc(uint8_t *rxbuf, uint8_t len);

#endif
