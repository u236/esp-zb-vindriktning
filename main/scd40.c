#include <string.h>
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_zigbee_core.h"
#include "config.h"
#include "led.h"
#include "scd40.h"
#include "zigbee.h"

static const char *tag = "scd40";

static uint8_t get_crc(const uint8_t *data)
{
    uint8_t crc = 0xFF;

    for (uint8_t i = 0; i < 2; i++)
    {
        crc ^= data[i];

        for (uint8_t j = 0; j < 8; j++)
            crc = crc & 0x80 ? (crc << 1) ^ 0x31 : crc << 1;
    }

    return crc;
}

static void send_command(uint16_t command, uint16_t delay)
{
    i2c_cmd_handle_t link = i2c_cmd_link_create();

	i2c_master_start(link);
	i2c_master_write_byte(link, (SCD40_ADDRESS << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(link, command >> 8, true);
    i2c_master_write_byte(link, command, true);
	i2c_master_stop(link);

    i2c_master_cmd_begin(I2C_PORT, link, 1000);
	i2c_cmd_link_delete(link);

    vTaskDelay(pdMS_TO_TICKS(delay));
}

static bool read_data(uint16_t *data, uint8_t count)
{
    i2c_cmd_handle_t link = i2c_cmd_link_create();
    uint8_t buffer[count * 3];

    i2c_master_start(link);
    i2c_master_write_byte(link, (SCD40_ADDRESS << 1) | I2C_MASTER_READ, true);
    i2c_master_read(link, buffer, sizeof(buffer), I2C_MASTER_LAST_NACK);
    i2c_master_stop(link);

    i2c_master_cmd_begin(I2C_PORT, link, 1000);
    i2c_cmd_link_delete(link);

    for (uint8_t i = 0; i < count; i++)
    {
        uint8_t *item = buffer + i * 3;

        if (item[2] != get_crc(item))
            return false;

        data[i] = item[0] << 8 | item[1];
    }

    return true;
}

static void scd40_task(void *arg)
{
    (void) arg;

    i2c_config_t i2c;
    TickType_t tick;
    uint16_t buffer[3];

    memset(&i2c, 0, sizeof(i2c));

    i2c.mode = I2C_MODE_MASTER;
    i2c.sda_io_num = I2C_SDA_PIN;
    i2c.scl_io_num = 3; //I2C_SCL_PIN;
    i2c.master.clk_speed = 100000;

    i2c_driver_install(I2C_PORT, i2c.mode, 0, 0, 0);
    i2c_param_config(I2C_PORT, &i2c);

    send_command(SCD40_WAKE_UP, 20);
    send_command(SCD40_STOP_PERIODIC_MEASUREMENT, 500);
    send_command(SCD40_REINIT, 20);
    send_command(SCD40_GET_SERIAL_NUMBER, 1);

    if (read_data(buffer, 3))
    {
        ESP_LOGI(tag, "Serial number is %04X%04X%04X", buffer[0], buffer[1], buffer[2]);
    }
    else
    {
        ESP_LOGE(tag, "Serial number request failed");
    }

    send_command(SCD40_START_PERIODIC_MEASUREMENT, 1);
    tick = xTaskGetTickCount();

    while (true)
    {
        vTaskDelayUntil(&tick, pdMS_TO_TICKS(5000));
        send_command(SCD40_READ_MEASUREMENT, 1);

        if (read_data(buffer, 3))
        {
            float value = buffer[0] / 1e6;

            if (!zigbee_steering())
                esp_zb_zcl_set_attribute_val(DEFAULT_ENDPOINT, ESP_ZB_ZCL_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_CARBON_DIOXIDE_MEASUREMENT_MEASURED_VALUE_ID, &value, false);

            ESP_LOGI(tag, "CO2 is %d ppm", buffer[0]);
            led_set_co2(buffer[0]);
            continue;
        }

        ESP_LOGE(tag, "Data request failed");
    }
}

void scd40_init(void)
{
    xTaskCreate(scd40_task, "scd40", 4096, NULL, 0, NULL);
}
