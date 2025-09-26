#include "py32f0xx.h"
#include "bsp_flash.h"

uint8_t bsp_flash_erase(uint32_t start_addr, uint32_t size)
{
    uint32_t PAGEError                     = 0;
    FLASH_EraseInitTypeDef EraseInitStruct = {0};

    uint32_t nb_pages = (size + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE;

    EraseInitStruct.TypeErase   = FLASH_TYPEERASE_PAGEERASE;
    EraseInitStruct.PageAddress = start_addr;
    EraseInitStruct.NbPages     = nb_pages;

    if (HAL_FLASHEx_Erase(&EraseInitStruct, &PAGEError) != HAL_OK) {
        return FLASH_IF_ERROR;
    }
    return FLASH_IF_OK;
}

uint8_t bsp_flash_write(uint32_t start_addr, uint32_t *src_data, uint32_t size)
{
    HAL_FLASH_Unlock();
    if (bsp_flash_erase(start_addr, size) != FLASH_IF_OK) {
        return FLASH_IF_ERROR;
    }

    uint32_t flash_program_start = start_addr;
    uint32_t flash_program_end   = start_addr + size;
    uint32_t *src                = src_data;

    while (flash_program_start < flash_program_end) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_PAGE, flash_program_start, src) != HAL_OK) {
            return FLASH_IF_ERROR;
        }
        flash_program_start += FLASH_PAGE_SIZE;
        src += FLASH_PAGE_SIZE / 4;
    }
    HAL_FLASH_Lock();
    return FLASH_IF_OK;
}

uint8_t bsp_flash_read(uint32_t start_addr, uint32_t *dest_data, uint32_t size)
{
    HAL_FLASH_Unlock();
    uint32_t addr = 0;

    while (addr < size) {
        *dest_data++ = HW32_REG(start_addr + addr);
        addr += 4;
    }
    HAL_FLASH_Lock();
    return FLASH_IF_OK;
}
