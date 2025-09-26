#include "device_manager.h"

void app_jump_device(void)
{
#if defined PANEL_KEY
    panel_device_init();
#elif defined TRAN_DEVICE
    app_tran_device_init();
#endif
}
