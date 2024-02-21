#include <string.h>
#include "driver/ledc.h"
#include "config.h"
#include "esp_log.h"
#include "nvs_flash.h"

// TODO: add logs?

static uint8_t mode;

static void update_pwm(void)
{
    switch (mode)
    {
        case 0:  ledc_set_duty(PWM_MODE, PWM_CHANNEL, 0); break;
        case 1:  ledc_set_duty(PWM_MODE, PWM_CHANNEL, 90); break;
        case 2:  ledc_set_duty(PWM_MODE, PWM_CHANNEL, 130); break;
        case 3:  ledc_set_duty(PWM_MODE, PWM_CHANNEL, 255); break;
        default: return;
    }

    ledc_update_duty(PWM_MODE, PWM_CHANNEL);
}

void fan_init(void)
{
    nvs_handle_t handle;
    ledc_timer_config_t timer_config;
    ledc_channel_config_t channel_config;

    nvs_flash_init_partition("nvs");
    nvs_open_from_partition("nvs", "nvs", NVS_READONLY, &handle);

    if (nvs_get_u8(handle, "fan_mode", &mode) != ESP_OK)
        mode = 3;

    nvs_close(handle);

    memset(&timer_config, 0, sizeof(timer_config));
    memset(&channel_config, 0, sizeof(channel_config));

    timer_config.speed_mode = PWM_MODE;
    timer_config.duty_resolution = PWM_RESOLUTION;
    timer_config.timer_num = PWM_TIMER;
    timer_config.freq_hz = PWM_FREQUENCY;

    channel_config.gpio_num = PWM_PIN;
    channel_config.speed_mode = PWM_MODE;
    channel_config.channel = PWM_CHANNEL;
    channel_config.timer_sel = PWM_TIMER;

    ledc_timer_config(&timer_config);
    ledc_channel_config(&channel_config);

    update_pwm();
}

void fan_set_mode(uint8_t value)
{
    nvs_handle_t handle;

    if (mode == value)
        return;

    mode = value;

    nvs_flash_init_partition("nvs");
    nvs_open_from_partition("nvs", "nvs", NVS_READWRITE, &handle);
    nvs_set_u8(handle, "fan_mode", mode);
    nvs_commit(handle);
    nvs_close(handle);

    update_pwm();
}

uint8_t fan_mode(void)
{
    return mode;
}