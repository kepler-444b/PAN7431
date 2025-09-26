#include "device_manager.h"

void app_jump_device(void)
{
#if defined PANEL
    panel_device_init();
#elif defined SETTER
    app_setter_init();
#elif defined REPEATER
    app_repeater_init();
#endif
}
