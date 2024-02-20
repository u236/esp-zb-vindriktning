#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config.h"
#include "esp_log.h"
#include "led_strip.h"
#include "led.h"
#include "reset.h"
#include "zigbee.h"

static led_strip_handle_t led_handle;

static void led_task(void *arg)
{
    (void) arg;

    led_strip_config_t led_config;
    led_strip_rmt_config_t rmt_config;
    uint8_t status = 0;

    led_config.max_leds = LED_COUNT;
    led_config.strip_gpio_num = LED_PIN;
    rmt_config.resolution_hz = 10000000;

    led_strip_new_rmt_device(&led_config, &rmt_config, &led_handle);

    while (true)
    {
        for (uint8_t i = 0; i < LED_COUNT; i++)
            led_strip_set_pixel(led_handle, i, 0, 0, 0);

        status ^= 1;

        if (reset_pending())
        {
            led_strip_set_pixel(led_handle, 5, status ? 20 : 0, 0, 0);
            led_strip_refresh(led_handle);
            vTaskDelay(200);
        }
        else if (zigbee_steering())
        {
            led_strip_set_pixel(led_handle, 5, 0, 0, status ? 20 : 0);
            led_strip_refresh(led_handle);
            vTaskDelay(500);
        }
        else
        {
            // TODO: display sensors state here
            led_strip_set_pixel(led_handle, 5, 5, 5, 0);
            led_strip_refresh(led_handle);
            break;
        }
    }

    vTaskDelete(NULL);
}

void led_init(void)
{
    xTaskCreate(led_task, "led", 4096, NULL, 0, NULL);
}
