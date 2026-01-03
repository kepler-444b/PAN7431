#include <string.h>
#include "py32f0xx.h"
#include "config.h"
#include "app.h"
#include "../app/config.h"
#include "../bsp/bsp_uart.h"
#include "../app/base.h"
#include "../app/protocol.h"
#include "../app/eventbus.h"

#if defined PANEL
static panel_cfg_t my_panel_cfg[6] = {0};
static uint8_t panel_type; // 0x00 普通面板 0x14 长供电面板

#endif
static reg_t my_reg = {0};
static uint8_t sim_key_number; // 软件模拟按键数量

// The default CFG of panel
static uint8_t DEF_PANEL_CONFIG[40] = {0xF2, 0x0E, 0x0E, 0x0E, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0x21, 0x21, 0x21, 0x00, 0x00, 0x00,
                                       0x00, 0x0E, 0x00, 0x00, 0x44, 0x00, 0x21, 0x00, 0x0E, 0x00, 0x00, 0x21, 0x00, 0xB0, 0x21, 0x84, 0x00, 0x00, 0x00, 0x00};

#if defined PANEL
static void app_load_panel_a11(uint8_t *data, uint8_t length);
static void app_panel_get_relay_num(uint8_t *data, const gpio_pin_t *relay_map);
#endif

void app_load_config(cfg_addr addr)
{
    static uint32_t read_data[10] = {0};
    static uint8_t new_data[40]   = {0};
    memset(read_data, 0, sizeof(read_data));
    memset(new_data, 0, sizeof(new_data));

#if 0 // Erese all flash
    __disable_irq();
    HAL_FLASH_Unlock();
    bsp_flash_erase(FLASH_REG_ADDR, sizeof(read_data));
    bsp_flash_erase(FLASH_BASE_ADDR, sizeof(read_data));
    HAL_FLASH_Lock();
    __enable_irq();
#endif
    switch (addr) {
        case CFG: { // Read the CFG
            __disable_irq();
            if (bsp_flash_read(FLASH_BASE_ADDR, read_data, sizeof(read_data)) != FLASH_IF_OK) {
                APP_ERROR("app_flash_read failed\n");
            }
            __enable_irq();
            if (read_data[0] == 0xFFFFFFFF) { // If the CFG is not configured,then the default CFG will be used
                APP_PRINTF("cfg is null\n");
                memcpy(new_data, DEF_PANEL_CONFIG, sizeof(DEF_PANEL_CONFIG));
#if defined HW_6KEY
                new_data[38]   = SIM_6KEY;
                sim_key_number = SIM_6KEY;
#elif defined HW_4KEY
                new_data[38]   = SIM_4KEY;
                sim_key_number = SIM_4KEY;
#endif
            } else {
                if (app_uint32_to_uint8(read_data, sizeof(read_data) / sizeof(read_data[0]), new_data, sizeof(new_data)) != true) {
                    APP_ERROR("app_uint32_to_uint8 error\n");
                }
            }
#if defined PANEL_6KEY_A11 || defined PANEL_4KEY_A11
            app_load_panel_a11(new_data, sizeof(new_data) / sizeof(new_data[0]));
#endif
        } break;
        case REG: { // Read the REG
            __disable_irq();
            if (bsp_flash_read(FLASH_REG_ADDR, read_data, sizeof(read_data)) != FLASH_IF_OK) {
                APP_ERROR("app_flash_read failed\n");
            }
            __enable_irq();
            if (read_data[0] == 0xFFFFFFFF) { // If the RGE is not configured,then the default will be used
                APP_PRINTF("reg is null\n");
                memset(new_data, 0, sizeof(new_data));
                // default reg
                new_data[0] = 0x01; // channel
                new_data[1] = 0x02; // zuwflag
                new_data[2] = 0x03; // room_h
                new_data[3] = 0x04; // room_l
                new_data[4] = 0x05; // forward_en
#if defined PANEL
                new_data[5] = sim_key_number; // key_number
#endif
            } else {
                if (app_uint32_to_uint8(read_data, sizeof(read_data) / sizeof(read_data[0]), new_data, sizeof(new_data)) != true) {
                    APP_ERROR("app_uint32_to_uint8 error\n");
                }
            }

            app_get_uid(my_uid);
            uint16_t cpadd = app_get_crc(my_uid, sizeof(my_uid));

            // Fill the (R) fields in the REG
            my_reg.ver     = VER;
            my_reg.cpadd_h = cpadd >> 8;
            my_reg.cpadd_l = cpadd & 0xFF;
#if defined PANEL
            my_reg.cplei = panel_type;
#else
            my_reg.cplei = TYPE;
#endif
            my_reg.tx_db = TX_DB;
            my_reg.tx_su = TX_DR;

            // Fill the (R/W) fields in the REG
            memcpy(&my_reg.channel, new_data, 5);
            my_reg.channel    = new_data[0];
            my_reg.zuwflag    = new_data[1];
            my_reg.room_h     = new_data[2];
            my_reg.room_l     = new_data[3];
            my_reg.forward_en = new_data[4];
            my_reg.key        = sim_key_number;
#if 1
            APP_PRINTF("ver:%02X\n", my_reg.ver);
            APP_PRINTF("cpadd:%02X%02X\n", my_reg.cpadd_h, my_reg.cpadd_l);
            APP_PRINTF("cplei:%02X\n", my_reg.cplei);
            APP_PRINTF("channel:%02X\n", my_reg.channel);
            APP_PRINTF("zuwflag:%02X\n", my_reg.zuwflag);
            APP_PRINTF("room:%02X%02X\n", my_reg.room_h, my_reg.room_l);
            APP_PRINTF("forward_en:%02X\n", my_reg.forward_en);
            APP_PRINTF("tx_db:%d\n", my_reg.tx_db);
            APP_PRINTF("tx_su:%d\n", my_reg.tx_su);
#if defined PANEL
            APP_PRINTF("key:%d\n", my_reg.key);
#endif
#endif
        } break;
        default:
            break;
    }
}

#if defined PANEL_6KEY_A11 || defined PANEL_4KEY_A11
static void app_load_panel_a11(uint8_t *data, uint8_t length)
{

    gpio_pin_t RELAY_GPIO_MAP[CONFIG_NUMBER] = RELAY_GPIO_MAP_DEF;
    gpio_pin_t LED_W_GPIO_MAP[CONFIG_NUMBER] = LED_W_GPIO_MAP_DEF;
    for (uint8_t i = 0; i < CONFIG_NUMBER; i++) {
        panel_cfg_t *const p_cfg = &my_panel_cfg[i];
        if (i < 4) {
            p_cfg->func        = data[i + 1];
            p_cfg->group       = data[i + 5];
            p_cfg->area        = data[i + 9];
            p_cfg->perm        = data[i + 13];
            p_cfg->scene_group = data[i + 17];
        } else if (i == 4) {
            p_cfg->func        = data[21];
            p_cfg->group       = data[22];
            p_cfg->area        = data[23];
            p_cfg->perm        = data[26];
            p_cfg->scene_group = data[27];
        } else if (i == 5) {
            p_cfg->func        = data[28];
            p_cfg->group       = data[29];
            p_cfg->area        = data[30];
            p_cfg->perm        = data[31];
            p_cfg->scene_group = data[32];
        }
        p_cfg->led_w_pin = LED_W_GPIO_MAP[i];
    }

    sim_key_number = data[length - 2];
    panel_type     = data[length - 3];
    app_panel_get_relay_num(data, RELAY_GPIO_MAP);
#if 1
    for (uint8_t i = 0; i < KEY_NUMBER; i++) {
        panel_cfg_t *const p_cfg = &my_panel_cfg[i];
        APP_PRINTF("%02X %02X %02X %02X %02X |", p_cfg->func, p_cfg->group, p_cfg->area, p_cfg->perm, p_cfg->scene_group);
        for (uint8_t j = 0; j < 4; j++) {
            const char *relay_name = app_get_gpio_name(p_cfg->relay_pin[j]);
            APP_PRINTF("%-3s ", relay_name);
        }
        APP_PRINTF("\n");
    }
    APP_PRINTF("device_type:%02X\n", panel_type);
    APP_PRINTF("sim_key_number:%02X\n", sim_key_number);
    APP_PRINTF("\n");
#endif
}
#endif

// Bind the relay's pins to key
#if defined PANEL
static void app_panel_get_relay_num(uint8_t *data, const gpio_pin_t *relay_map)
{
    const uint8_t base_offset = 34;
    for (uint8_t i = 0; i < 6; i++) {
        uint8_t byte_offset          = base_offset + (i / 2);
        uint8_t relay_num            = (i % 2) ? H_BIT(data[byte_offset]) : L_BIT(data[byte_offset]);
        my_panel_cfg[i].relay_pin[0] = BIT0(relay_num) ? relay_map[0] : DEF;
        my_panel_cfg[i].relay_pin[1] = BIT1(relay_num) ? relay_map[1] : DEF;
        my_panel_cfg[i].relay_pin[2] = BIT2(relay_num) ? relay_map[2] : DEF;
        my_panel_cfg[i].relay_pin[3] = BIT3(relay_num) ? relay_map[3] : DEF;
    }
}

const panel_cfg_t *app_get_panel_cfg(void)
{
    return my_panel_cfg;
}
const uint8_t app_get_panel_type(void)
{
    return panel_type;
}
#endif

reg_t *app_get_reg(void)
{
    return &my_reg;
}
uint8_t app_get_sim_key_number(void)
{
    return sim_key_number;
}
