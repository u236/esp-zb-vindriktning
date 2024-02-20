#include "nvs_flash.h"
#include "led.h"
#include "pm1006.h"
#include "reset.h"
#include "scd40.h"
#include "zigbee.h"

void app_main(void)
{
    nvs_flash_init();
    led_init();
    reset_init();
    scd40_init();
    pm1006_init();
    zigbee_init();
}
