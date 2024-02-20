#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config.h"
#include "esp_log.h"
#include "led_strip.h"
#include "led.h"
#include "reset.h"
#include "zigbee.h"

static led_strip_handle_t handle;
static uint16_t co2_value = 0, pm25_value = 0;

static void set_level(uint8_t *level, uint8_t target)
{
    if (*level > target)
        (*level)--;
    else if (*level < target)
        (*level)++;
}

static void set_color(uint16_t min, uint16_t max, uint16_t value, uint8_t level, uint8_t *color)
{
    uint16_t mid = (max - min) / 2;

    if (value <= min)
    {
        color[0] = 0;
        color[1] = level;
    }
    else if (value >= max)
    {
        color[0] = level;
        color[1] = 0;
    }
    else if (value < mid)
    {
        color[0] = (uint8_t) ((float) level / (mid - min) * (value - min));
        color[1] = level;
    }
    else
    {
        color[0] = level;
        color[1] = (uint8_t) ((float) level / (max - mid) * (max - value));
    }
}

static void led_task(void *arg)
{
    (void) arg;

    led_strip_config_t led_config;
    led_strip_rmt_config_t rmt_config;
    uint8_t count = 0, pulse = 0, max_level = LED_MAX_LEVEL, co2_level = 0, pm25_level = 0, zigbee_level = 0;

    led_config.max_leds = LED_COUNT;
    led_config.strip_gpio_num = LED_PIN;
    rmt_config.resolution_hz = 10666666;

    led_strip_new_rmt_device(&led_config, &rmt_config, &handle);
        led_strip_clear(handle);

    while (true)
    {
        if (reset_pending())
        {
            pulse ^= 1;
            led_strip_set_pixel(handle, 2, pulse ? LED_MAX_LEVEL : 0, 0, 0);
            led_strip_set_pixel(handle, 3, pulse ? LED_MAX_LEVEL : 0, 0, 0);
            led_strip_refresh(handle);
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        else
        {
            uint8_t color[2];

            if (co2_value)
            {
                if (co2_value > CO2_MAX_VALUE)
                {
                    set_level(&co2_level, pulse ? 0 : max_level);
                    led_strip_set_pixel(handle, 4, co2_level, 0, 0);
                    led_strip_set_pixel(handle, 5, co2_level, 0, 0);
                }
                else
                {
                    set_level(&co2_level, max_level);
                    set_color(CO2_MIN_VALUE, CO2_MAX_VALUE, co2_value, co2_level, color);
                    led_strip_set_pixel(handle, 4, color[0], color[1], 0);
                    led_strip_set_pixel(handle, 5, color[0], color[1], 0);
                }
            }

            if (pm25_value)
            {
                if (pm25_value > PM25_MAX_VALUE)
                {
                    set_level(&pm25_level, pulse ? 0 : max_level);
                    led_strip_set_pixel(handle, 0, pm25_level, 0, 0);
                    led_strip_set_pixel(handle, 1, pm25_level, 0, 0);
                }
                else
                {
                    set_level(&pm25_level, max_level);
                    set_color(PM25_MIN_VALUE, PM25_MAX_VALUE, pm25_value, pm25_level, color);
                    led_strip_set_pixel(handle, 0, color[0], color[1], 0);
                    led_strip_set_pixel(handle, 1, color[0], color[1], 0);
                }
            }

            if (zigbee_steering() || zigbee_level || (!co2_value && !pm25_value))
            {
                set_level(&zigbee_level, pulse ? 0 : max_level);
                led_strip_set_pixel(handle, 2, 0, 0, zigbee_level);
                led_strip_set_pixel(handle, 3, 0, 0, zigbee_level);
            }

            if (++count >= 50)
            {
                pulse ^= 1;
                count = 0;
            }

            led_strip_refresh(handle);
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }

    vTaskDelete(NULL);
}

void led_init(void)
{
    xTaskCreate(led_task, "led", 4096, NULL, 0, NULL);
}

void led_set_co2(uint16_t value)
{
    co2_value = value;
}

void led_set_pm25(uint16_t value)
{
    pm25_value = value;
}