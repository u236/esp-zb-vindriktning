#ifndef APP_H
#define APP_H

#include <stdint.h>

struct basic_data
{
    uint8_t zcl_version;
    uint8_t application_version;
    uint8_t power_source;
    char    manufacturer_name[16];
    char    model_identifier[16];
    char    sw_build[16];
};

#define ZCL_VERSION             0x03
#define APPLICATION_VERSION     0x01
#define POWER_SOURCE            0x01
#define MANUFACTURER_NAME       "HOMEd"
#define MODEL_IDENTIFIER        "Vindriktning"
#define SW_BUILD                "1.0.1"

#define OTA_MANUFACTURER        0x1234
#define OTA_IMAGE_TYPE          0x0021
#define OTA_FILE_VERSION        0x00000101

#define DEFAULT_ENDPOINT        0x01
#define BUTTON_PIN              9

#define LED_PIN                 10
#define LED_COUNT               6
#define LED_DEFAULT_LEVEL       50

#define PWM_TIMER               LEDC_TIMER_0
#define PWM_CHANNEL             LEDC_CHANNEL_0
#define PWM_MODE                LEDC_LOW_SPEED_MODE
#define PWM_RESOLUTION          LEDC_TIMER_8_BIT
#define PWM_FREQUENCY           10000
#define PWM_PIN                 13

#define I2C_PORT                I2C_NUM_0
#define I2C_SDA_PIN             2
#define I2C_SCL_PIN             3

#define UART_PORT               UART_NUM_1
#define UART_TX_PIN             4
#define UART_RX_PIN             5

#define RESET_COUNT             3
#define RESET_TIMEOUT           3000

#define CO2_MIN_VALUE           400
#define CO2_MAX_VALUE           1500

#define PM25_MIN_VALUE          0
#define PM25_MAX_VALUE          100

#endif
