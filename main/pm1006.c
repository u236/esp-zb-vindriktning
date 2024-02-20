#include <string.h>
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_zigbee_core.h"
#include "config.h"
#include "led.h"
#include "zigbee.h"

static const char *tag = "pm1006";

static void pm1006_task(void *arg)
{
    (void) arg;

    uart_config_t uart;
    TickType_t tick = xTaskGetTickCount();
    uint8_t command[5] = {0x11, 0x02, 0x0B, 0x01, 0xE1}, header[3] = {0x16, 0x11, 0x0B}, buffer[256], length;

    memset(&uart, 0, sizeof(uart));

    uart.baud_rate = 9600;
    uart.data_bits = UART_DATA_8_BITS;
    uart.stop_bits = UART_STOP_BITS_1;

    uart_driver_install(UART_PORT, sizeof(buffer), 0, 0, NULL, 0);
    uart_param_config(UART_PORT, &uart);
    uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    while (true)
    {
        vTaskDelayUntil(&tick, pdMS_TO_TICKS(5000));

        uart_write_bytes(UART_PORT, command, sizeof(command));
        uart_wait_tx_done(UART_PORT, pdMS_TO_TICKS(1000));

        if ((length = uart_read_bytes(UART_PORT, buffer, sizeof(buffer) - 1, pdMS_TO_TICKS(1000))) == 20 && !memcmp(buffer, header, sizeof(header)))
        {
            uint8_t checksum = 0;

            for (uint8_t i = 0; i < 20; i++)
                checksum += buffer[i];

            if (!checksum)
            {
                float value = buffer[5] << 8 | buffer[6];

                if (!zigbee_steering())
                    esp_zb_zcl_set_attribute_val(DEFAULT_ENDPOINT, ESP_ZB_ZCL_CLUSTER_ID_PM2_5_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_PM2_5_MEASUREMENT_MEASURED_VALUE_ID, &value, false);

                ESP_LOGI(tag, "PM25 is %.0f µg/m³", value);
                led_set_pm25((uint16_t) value);
                continue;
            }
        }

        ESP_LOGE(tag, "Data request failed");
    }
}

void pm1006_init(void)
{
    xTaskCreate(pm1006_task, "pm1006", 4096, NULL, 0, NULL);
}
