#ifndef _BSP_TM5020A_H_
#define _BSP_TM5020A_H_

#include <stdint.h>
#include <stdbool.h>

void bsp_tm5020a_init(void);
void bsp_tm5020a_set(uint8_t pin, bool status);

#endif