
#include <string.h>
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_zigbee_core.h"
#include "config.h"
#include "fan.h"
#include "led.h"
#include "reset.h"

static const char *tag = "zigbee";
static const esp_partition_t *ota_partition = NULL;
static esp_ota_handle_t ota_handle = 0;
static uint32_t ota_size = 0, ota_offset = 0;
static uint8_t steering_flag = 1;
static struct basic_data basic;

static void set_zcl_string(char *buffer, char *value)
{
    buffer[0] = (char) strlen(value);
    memcpy(buffer + 1, value, buffer[0]);
}

static esp_err_t attribute_handler(esp_zb_zcl_set_attr_value_message_t *message)
{
    if (message->info.dst_endpoint != DEFAULT_ENDPOINT || message->info.status != ESP_ZB_ZCL_STATUS_SUCCESS)
        return ESP_FAIL;

    switch (message->info.cluster)
    {
        case ESP_ZB_ZCL_CLUSTER_ID_ON_OFF:

            if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL && message->attribute.data.value)
            {
                led_set_enabled(*(uint8_t*) message->attribute.data.value);
                return ESP_OK;
            }

            break;

        case ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL:

            if (message->attribute.id == ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U8 && message->attribute.data.value)
            {
                led_set_brightness(*(uint8_t*) message->attribute.data.value);
                return ESP_OK;
            }

            break;

        case ESP_ZB_ZCL_CLUSTER_ID_FAN_CONTROL:

            if (message->attribute.id == ESP_ZB_ZCL_ATTR_FAN_CONTROL_FAN_MODE_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM && message->attribute.data.value)
            {
                fan_set_mode(*(uint8_t*) message->attribute.data.value);
                return ESP_OK;
            }

            break;
    }

    return ESP_FAIL;
}

static esp_err_t ota_handler(esp_zb_zcl_ota_upgrade_value_message_t *message)
{
    esp_err_t result = ESP_OK;

    if (message->info.dst_endpoint != DEFAULT_ENDPOINT || message->info.status != ESP_ZB_ZCL_STATUS_SUCCESS)
        return ESP_FAIL;

    switch (message->upgrade_status)
    {
        case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_START:

            ESP_LOGI(tag, "OTA upgrade started");
            ota_partition = esp_ota_get_next_update_partition(NULL);

            if ((result = esp_ota_begin(ota_partition, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle)) != ESP_OK)
                ESP_LOGE(tag, "OTA uprage begin failed, status: %s", esp_err_to_name(result));

            break;

        case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_APPLY:
            ESP_LOGI(tag, "OTA upgrade apply");
            break;

        case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_RECEIVE:

            ota_size = message->ota_header.image_size;
            ota_offset += message->payload_size;

            ESP_LOGI(tag, "OTA upgrade received %06ld/%ld bytes", ota_offset, ota_size);

            if (message->payload_size && message->payload && (result = esp_ota_write(ota_handle, (const void*) message->payload, message->payload_size)) != ESP_OK)
                ESP_LOGE(tag, "OTA uprage write failed, status: %s", esp_err_to_name(result));

            break;

        case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_FINISH:

            if ((result = esp_ota_end(ota_handle)) != ESP_OK)
                ESP_LOGE(tag, "OTA uprage end failed, status: %s", esp_err_to_name(result));

            if ((result = esp_ota_set_boot_partition(ota_partition)) != ESP_OK)
                ESP_LOGE(tag, "OTA uprage set boot partition failed, status: %s", esp_err_to_name(result));

            ESP_LOGI(tag, "OTA uprage finished: version: 0x%lx, manufacturer code: 0x%x, image type: 0x%x, total size: %ld", message->ota_header.file_version, message->ota_header.manufacturer_code, message->ota_header.image_type, message->ota_header.image_size);
            esp_restart();
            break;

        case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_CHECK:

            if (ota_offset != ota_size)
            {
                ESP_LOGE(tag, "OTA uprage check failed");
                return ESP_FAIL;
            }

            break;

        default:
            ESP_LOGI(tag, "OTA uprage status: 0x%04x", message->upgrade_status);
            break;
    }

    return result;
}

static esp_err_t action_handler(esp_zb_core_action_callback_id_t callback, const void *message)
{
    switch (callback)
    {
        case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
            return attribute_handler((esp_zb_zcl_set_attr_value_message_t*) message);

        case ESP_ZB_CORE_OTA_UPGRADE_VALUE_CB_ID:
            return ota_handler((esp_zb_zcl_ota_upgrade_value_message_t*) message);

        // TODO: set time here?
        // case ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID:
        // {
        //     const esp_zb_zcl_cmd_read_attr_resp_message_t *data = message;
        //     return ESP_OK;
        // }

        case ESP_ZB_CORE_CMD_DEFAULT_RESP_CB_ID:
            return ESP_OK;

        default:
            ESP_LOGW(tag, "ZigBee action 0x%04x callback received", callback);
            return ESP_OK;
    }
}

static void time_task(void *arg)
{
   (void) arg;

    esp_zb_zcl_read_attr_cmd_t request;
    uint16_t attribute_id = ESP_ZB_ZCL_ATTR_TIME_LOCAL_TIME_ID;

    request.zcl_basic_cmd.dst_endpoint = 0x01;
    request.zcl_basic_cmd.src_endpoint = DEFAULT_ENDPOINT;
    request.zcl_basic_cmd.dst_addr_u.addr_short = 0x0000;
    request.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
    request.clusterID = ESP_ZB_ZCL_CLUSTER_ID_TIME;
    request.attr_number = 1;
    request.attr_field = &attribute_id;

    while (true)
    {
        esp_zb_zcl_read_attr_cmd_req(&request);
        vTaskDelay(pdMS_TO_TICKS(12 * 3600 * 1000)); // TODO: read time twice a day?
    }
}

static void zigbee_task(void *arg)
{
    (void) arg;

    esp_zb_platform_config_t platform_config;
    esp_zb_cfg_t zigbee_config;
    esp_zb_zcl_ota_upgrade_client_variable_t ota_data;
    esp_zb_ota_cluster_cfg_t ota_config;
    esp_zb_on_off_cluster_cfg_t on_off_config;
    esp_zb_level_cluster_cfg_t level_config;
    esp_zb_fan_control_cluster_cfg_t fan_config;
    esp_zb_carbon_dioxide_measurement_cluster_cfg_t co2_config;
    esp_zb_pm2_5_measurement_cluster_cfg_t pm25_config;
    esp_zb_attribute_list_t *basic_cluster, *time_cluster, *ota_cluster, *on_off_cluster, *level_cluster, *fan_cluster, *co2_cluster, *pm25_cluster;
    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
    esp_zb_ep_list_t *endpoint_list = esp_zb_ep_list_create();

    memset(&platform_config, 0, sizeof(platform_config));
    memset(&zigbee_config, 0, sizeof(zigbee_config));
    memset(&ota_config, 0, sizeof(ota_config));
    memset(&ota_data, 0, sizeof(ota_data));
    memset(&on_off_config, 0, sizeof(on_off_config));
    memset(&level_config, 0, sizeof(level_config));
    memset(&fan_config, 0, sizeof(fan_config));
    memset(&co2_config, 0, sizeof(co2_config));
    memset(&pm25_config, 0, sizeof(pm25_config));

    platform_config.radio_config.radio_mode = RADIO_MODE_NATIVE;
    platform_config.host_config.host_connection_mode = HOST_CONNECTION_MODE_NONE;

    zigbee_config.esp_zb_role = ESP_ZB_DEVICE_TYPE_ROUTER;
    zigbee_config.install_code_policy = false;
    zigbee_config.nwk_cfg.zczr_cfg.max_children = 16;

    ota_data.timer_query = ESP_ZB_ZCL_OTA_UPGRADE_QUERY_TIMER_COUNT_DEF;
    ota_data.hw_version = 0x0001;
    ota_data.max_data_size = 0x40;

    ota_config.ota_upgrade_manufacturer = OTA_MANUFACTURER;
    ota_config.ota_upgrade_image_type = OTA_IMAGE_TYPE;
    ota_config.ota_upgrade_downloaded_file_ver = OTA_FILE_VERSION;

    on_off_config.on_off = led_enabled();
    level_config.current_level = led_brightness();
    fan_config.fan_mode = fan_mode();
    co2_config.max_measured_value = 0.002; // 2000 / 1e6
    pm25_config.max_measured_value = 1000;

    basic_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_BASIC);
    time_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_TIME);
    ota_cluster = esp_zb_ota_cluster_create(&ota_config);
    on_off_cluster = esp_zb_on_off_cluster_create(&on_off_config);
    level_cluster = esp_zb_level_cluster_create(&level_config);
    fan_cluster = esp_zb_fan_control_cluster_create(&fan_config);
    co2_cluster = esp_zb_carbon_dioxide_measurement_cluster_create(&co2_config);
    pm25_cluster = esp_zb_pm2_5_measurement_cluster_create(&pm25_config);

    esp_zb_platform_config(&platform_config);
    esp_zb_init(&zigbee_config);

    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_ZCL_VERSION_ID,         &basic.zcl_version);
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_APPLICATION_VERSION_ID, &basic.application_version);
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID,        &basic.power_source);
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID,   basic.manufacturer_name);
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID,    basic.model_identifier);
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_SW_BUILD_ID,            basic.sw_build);

    esp_zb_ota_cluster_add_attr(ota_cluster,     ESP_ZB_ZCL_ATTR_OTA_UPGRADE_CLIENT_DATA_ID,   &ota_data);

    esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_time_cluster(cluster_list, time_cluster, ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);
    esp_zb_cluster_list_add_ota_cluster(cluster_list, ota_cluster, ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);
    esp_zb_cluster_list_add_on_off_cluster(cluster_list, on_off_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_level_cluster(cluster_list, level_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_fan_control_cluster(cluster_list, fan_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_carbon_dioxide_measurement_cluster(cluster_list, co2_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_pm2_5_measurement_cluster(cluster_list, pm25_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    esp_zb_ep_list_add_ep(endpoint_list, cluster_list, DEFAULT_ENDPOINT, ESP_ZB_AF_HA_PROFILE_ID, ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID);
    esp_zb_device_register(endpoint_list);

    esp_zb_set_primary_network_channel_set(ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK);
    esp_zb_core_action_handler_register(action_handler);

    esp_zb_start(true);
    esp_zb_main_loop_iteration();
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *data)
{
    esp_zb_app_signal_type_t signal = *data->p_app_signal;
    esp_err_t error = data->esp_err_status;

    switch (signal)
    {
        case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
        case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:

            if (error != ESP_OK)
            {
                ESP_LOGE(tag, "Failed to initialize ZigBee stack (status: %d)", error);
                reset_count_update(0);
            }

            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            break;


        case ESP_ZB_BDB_SIGNAL_STEERING:

            if (error != ESP_OK)
            {
                ESP_LOGW(tag, "Network steering failed, error: %d", error);
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            }
            else
            {
                esp_zb_ieee_addr_t pan_id;

                esp_zb_get_extended_pan_id(pan_id);
                ESP_LOGI(tag, "Successfully joined network (PAN ID: 0x%04x, Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x)", esp_zb_get_pan_id(), pan_id[7], pan_id[6], pan_id[5], pan_id[4], pan_id[3], pan_id[2], pan_id[1], pan_id[0]);

                xTaskCreate(time_task, "time", 4096, NULL, 0, NULL);
                steering_flag = 0;
            }

            break;

        case ESP_ZB_ZDO_SIGNAL_LEAVE:

            if (((esp_zb_zdo_signal_leave_params_t*) esp_zb_app_signal_get_params(data->p_app_signal))->leave_type == ESP_ZB_NWK_LEAVE_TYPE_RESET)
                esp_zb_factory_reset();

            break;

        default:
            ESP_LOGI(tag, "ZDO signal 0x%02x (%s) received, status: %s", signal, esp_zb_zdo_signal_to_string(signal), esp_err_to_name(error));
            break;
    }
}

void zigbee_init(void)
{
    basic.zcl_version = ZCL_VERSION;
    basic.application_version = APPLICATION_VERSION;
    basic.power_source = POWER_SOURCE;

    set_zcl_string(basic.manufacturer_name, MANUFACTURER_NAME);
    set_zcl_string(basic.model_identifier, MODEL_IDENTIFIER);
    set_zcl_string(basic.sw_build, SW_BUILD);

    xTaskCreate(zigbee_task, "zigbee", 4096, NULL, 5, NULL);
}

uint8_t zigbee_steering(void)
{
    return steering_flag;
}
