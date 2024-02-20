#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config.h"
#include "nvs_flash.h"
#include "led_strip.h"
#include "reset.h"
#include "zigbee.h"

// TODO: add logs?

static led_strip_handle_t led_handle;
static uint8_t enabled, brightness;
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
    uint16_t mid = min + (max - min) / 2;

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
    uint8_t count = 0, pulse = 1, co2_level = 0, pm25_level = 0, zigbee_level = 0;

    led_config.max_leds = LED_COUNT;
    led_config.strip_gpio_num = LED_PIN;
    rmt_config.resolution_hz = 10666666;

    led_strip_new_rmt_device(&led_config, &rmt_config, &led_handle);
    led_strip_clear(led_handle);

    while (true)
    {
        if (reset_pending())
        {
            pulse ^= 1;
            led_strip_set_pixel(led_handle, 2, pulse ? LED_DEFAULT_LEVEL : 0, 0, 0);
            led_strip_set_pixel(led_handle, 3, pulse ? LED_DEFAULT_LEVEL : 0, 0, 0);
            led_strip_refresh(led_handle);
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        else
        {
            uint8_t color[2];

            if (co2_value)
            {
                if (co2_value > CO2_MAX_VALUE)
                {
                    set_level(&co2_level, enabled && pulse ? brightness : 0);
                    led_strip_set_pixel(led_handle, 4, co2_level, 0, 0);
                    led_strip_set_pixel(led_handle, 5, co2_level, 0, 0);
                }
                else
                {
                    set_level(&co2_level, enabled ? brightness : 0);
                    set_color(CO2_MIN_VALUE, CO2_MAX_VALUE, co2_value, co2_level, color);
                    led_strip_set_pixel(led_handle, 4, color[0], color[1], 0);
                    led_strip_set_pixel(led_handle, 5, color[0], color[1], 0);
                }
            }

            if (pm25_value)
            {
                if (pm25_value > PM25_MAX_VALUE)
                {
                    set_level(&pm25_level, enabled && pulse ? brightness : 0);
                    led_strip_set_pixel(led_handle, 0, pm25_level, 0, 0);
                    led_strip_set_pixel(led_handle, 1, pm25_level, 0, 0);
                }
                else
                {
                    set_level(&pm25_level, enabled ? brightness : 0);
                    set_color(PM25_MIN_VALUE, PM25_MAX_VALUE, pm25_value, pm25_level, color);
                    led_strip_set_pixel(led_handle, 0, color[0], color[1], 0);
                    led_strip_set_pixel(led_handle, 1, color[0], color[1], 0);
                }
            }

            if (zigbee_steering() || zigbee_level || (!co2_value && !pm25_value))
            {
                set_level(&zigbee_level, enabled && pulse ? brightness : 0);
                led_strip_set_pixel(led_handle, 2, 0, 0, zigbee_level);
                led_strip_set_pixel(led_handle, 3, 0, 0, zigbee_level);
            }

            if (++count >= 50)
            {
                pulse ^= 1;
                count = 0;
            }

            led_strip_refresh(led_handle);
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }

    vTaskDelete(NULL);
}

void led_init(void)
{
    nvs_handle_t handle;

    nvs_flash_init_partition("nvs");
    nvs_open_from_partition("nvs", "nvs", NVS_READONLY, &handle);

    if (nvs_get_u8(handle, "led_enabled", &enabled) != ESP_OK)
        enabled = 1;

    if (nvs_get_u8(handle, "led_brightness", &brightness) != ESP_OK)
        brightness = LED_DEFAULT_LEVEL;

    nvs_close(handle);
    xTaskCreate(led_task, "led", 8192, NULL, 0, NULL);
}

void led_set_enabled(uint8_t value)
{
    nvs_handle_t handle;

    if (enabled == value)
        return;

    enabled = value;

    nvs_flash_init_partition("nvs");
    nvs_open_from_partition("nvs", "nvs", NVS_READWRITE, &handle);
    nvs_set_u8(handle, "led_enabled", enabled);
    nvs_commit(handle);
    nvs_close(handle);
}

void led_set_brightness(uint8_t value)
{
    nvs_handle_t handle;

    if (brightness == value)
        return;

    brightness = value;

    nvs_flash_init_partition("nvs");
    nvs_open_from_partition("nvs", "nvs", NVS_READWRITE, &handle);
    nvs_set_u8(handle, "led_brightness", brightness);
    nvs_commit(handle);
    nvs_close(handle);
}

void led_set_co2(uint16_t value)
{
    co2_value = value;
}

void led_set_pm25(uint16_t value)
{
    pm25_value = value;
}

uint8_t led_enabled(void)
{
    return enabled;
}

uint8_t led_brightness(void)
{
    return brightness;
}