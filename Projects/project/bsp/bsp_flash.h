#ifndef _BSP_FLASH_
#define _BSP_FLASH_

#define FLASH_IF_OK      0
#define FLASH_IF_ERROR   1

#define FLASH_BASE_ADDR 0x0800F000 // 存放串码
#define FLASH_REG_ADDR  0x0800F200 // 存放寄存器

uint8_t bsp_flash_erase(uint32_t start_addr, uint32_t size);
uint8_t bsp_flash_write(uint32_t start_addr, uint32_t *src_data, uint32_t size);
uint8_t bsp_flash_read(uint32_t start_addr, uint32_t *dest_data, uint32_t size);


#endif
