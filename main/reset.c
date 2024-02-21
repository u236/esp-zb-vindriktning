#include <string.h>
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_zigbee_core.h"
#include "nvs_flash.h"
#include "config.h"
#include "reset.h"

static const char *tag = "reset";
static TaskHandle_t button_handle, timer_handle;
static gptimer_handle_t timer;
static uint8_t count, reset_flag = 0;

static void IRAM_ATTR button_handler(void *arg)
{
    (void) arg;
    xTaskResumeFromISR(button_handle);
}

static bool IRAM_ATTR timer_handler(gptimer_handle_t timer, const gptimer_alarm_event_data_t *event, void *arg)
{
    (void) event;
    (void) arg;
    gptimer_stop(timer);
    xTaskResumeFromISR(timer_handle);
    return true;
}

static void button_task(void *arg)
{
    (void) arg;

    gpio_config_t config;
    uint8_t check = 0;

    memset(&config, 0, sizeof(config));
    config.pin_bit_mask = 1ULL << BUTTON_PIN;
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_ENABLE;
    config.intr_type = GPIO_INTR_ANYEDGE;

    gpio_config(&config);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_PIN, button_handler, NULL);

    while (true)
    {
        vTaskSuspend(NULL);

        if (gpio_get_level(BUTTON_PIN))
        {
            if (!check)
                continue;

            gptimer_stop(timer);
            check = 0;
        }
        else if (!check)
        {
            gptimer_set_raw_count(timer, 0);
            gptimer_start(timer);
            check = 1;
        }
    }
}

static void timer_task(void *arg)
{
    (void) arg;

    gptimer_config_t timer_config;
    gptimer_event_callbacks_t timer_callbacks;
    gptimer_alarm_config_t alarm_config;

    memset(&timer_config, 0, sizeof(timer_config));
    memset(&timer_callbacks, 0, sizeof(timer_callbacks));
    memset(&alarm_config, 0, sizeof(alarm_config));

    timer_config.clk_src = GPTIMER_CLK_SRC_DEFAULT;
    timer_config.direction = GPTIMER_COUNT_UP;
    timer_config.resolution_hz = 1000;
    timer_callbacks.on_alarm = timer_handler;
    alarm_config.alarm_count = RESET_TIMEOUT;

    gptimer_new_timer(&timer_config, &timer);
    gptimer_register_event_callbacks(timer, &timer_callbacks, NULL);
    gptimer_set_alarm_action(timer, &alarm_config);
    gptimer_enable(timer);

    vTaskSuspend(NULL);
    reset_count_update(0);
    esp_zb_factory_reset();
}

static void reset_task(void *arg)
{
    (void) arg;

    if (count >= RESET_COUNT)
    {
        ESP_LOGW(tag, "Pending");
        reset_flag = 1;
        count = 0;
    }

    reset_count_update(count);
    vTaskDelay(pdMS_TO_TICKS(RESET_TIMEOUT));

    if (!reset_flag)
    {
        reset_count_update(0);
        vTaskDelete(NULL);
    }

    esp_zb_factory_reset();
}

void reset_init(void)
{
    nvs_handle_t handle;

    nvs_flash_init_partition("nvs");
    nvs_open_from_partition("nvs", "nvs", NVS_READONLY, &handle);

    if (nvs_get_u8(handle, "reset_count", &count) != ESP_OK)
        count = 0;

    nvs_close(handle);
    count++;

    xTaskCreate(button_task, "button", 4096, NULL, 0, &button_handle);
    xTaskCreate(timer_task,  "timer",  4096, NULL, 0, &timer_handle);
    xTaskCreate(reset_task,  "reset",  4096, NULL, 5, NULL);
}

void reset_count_update(uint8_t count)
{
    nvs_handle_t handle;
    uint8_t check = 0;

    nvs_flash_init_partition("nvs");
    nvs_open_from_partition("nvs", "nvs", NVS_READWRITE, &handle);
    nvs_get_u8(handle, "reset_count", &check);

    if (check != count)
    {
        ESP_LOGI(tag, "Count is %d", count);
        nvs_set_u8(handle, "reset_count", count);
        nvs_commit(handle);
    }

    nvs_close(handle);
}

uint8_t reset_pending(void)
{
    return reset_flag;
}


// TODO: reset led and fan