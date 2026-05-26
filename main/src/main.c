/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file main.c
 * @brief Main application file
 *
 * This file contains the entry point for the application which initializes
 * the MeshX library and handles any initialization errors.
 */

#include "meshx.h"

#define CONFIG_MESHX_NVS_SAVE_PERIOD_MS 1000

static void meshx_app_data_cb(const meshx_msg_data_t *msg);
static void meshx_app_ctrl_cb(const meshx_msg_ctrl_t *msg);
/**
 * @brief Configuration for the MeshX library.
 */
static const meshx_config_t meshx_config = {
    .app_ctrl_cb            = &meshx_app_ctrl_cb,
    .app_element_cb         = &meshx_app_data_cb,
    .meshx_nvs_save_period  = CONFIG_MESHX_NVS_SAVE_PERIOD_MS,
    .meshx_log_level        = MESHX_LOG_VERBOSE,
};

/**
 * @brief Main application entry point.
 *
 * This function initializes the MeshX library and logs an error message
 * if the initialization fails.
 */
void CONFIG_APP_MAIN(void)
{
    meshx_err_t err;

    /* Initialize MeshX */
    err = meshx_init(&meshx_config);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "MeshX Init failed: 0x%x", err);
    }
}

static void meshx_app_data_cb(const meshx_msg_data_t *msg)
{
    if (!msg)
        return;

    switch (msg->element_type)
    {
    case MESHX_ELEMENT_TYPE_RELAY_SERVER:
        MESHX_LOGI(MODULE_ID_COMMON, "Relay Server Element ID: %d, Func ID: %d, Data: %d", msg->element_id, msg->func_id, ((const meshx_api_relay_server_evt_t*)msg->payload)->on_off);
        break;
    case MESHX_ELEMENT_TYPE_RELAY_CLIENT:
        MESHX_LOGI(MODULE_ID_COMMON, "Relay Client Element ID: %d, Func ID: %d, Data: %d, Error: %d", msg->element_id, msg->func_id, ((const meshx_api_relay_client_evt_t*)msg->payload)->on_off, ((const meshx_api_relay_client_evt_t*)msg->payload)->err_code);
        break;
    case MESHX_ELEMENT_TYPE_LIGHT_CWWW_SERVER:
        switch (msg->func_id)
        {
        case MESHX_ELEMENT_FUNC_ID_LIGHT_CWWW_SERVER_ONN_OFF:
            MESHX_LOGI(MODULE_ID_COMMON, "Light CW-WW Server Element ID: %d, Func ID: %d, Data: %d", msg->element_id, msg->func_id,
                     ((const meshx_api_light_cwww_server_evt_t*)msg->payload)->state_change.on_off.state);
            break;
        case MESHX_ELEMENT_FUNC_ID_LIGHT_CWWW_SERVER_CTL:
            MESHX_LOGI(MODULE_ID_COMMON, "Light CW-WW Server Element ID: %d, Func ID: %d, Data: %d|%d", msg->element_id, msg->func_id,
                     ((const meshx_api_light_cwww_server_evt_t*)msg->payload)->state_change.ctl.lightness,
                     ((const meshx_api_light_cwww_server_evt_t*)msg->payload)->state_change.ctl.temperature);
            break;
        default:
            MESHX_LOGW(MODULE_ID_COMMON, "Unhandled function ID: %d", msg->func_id);
            break;
        }
        break;
    case MESHX_ELEMENT_TYPE_LIGHT_CWWW_CLIENT:
        switch (msg->func_id)
        {
        case MESHX_ELEMENT_FUNC_ID_LIGHT_CWWW_CLIENT_ONN_OFF:
            MESHX_LOGI(MODULE_ID_COMMON, "Light CW-WW Client Element ID: %d, Func ID: %d, Data: %d, Error: %d", msg->element_id, msg->func_id,
                     ((const meshx_api_light_cwww_client_evt_t*)msg->payload)->state_change.on_off.state, ((const meshx_api_light_cwww_client_evt_t*)msg->payload)->err_code);
            break;
        case MESHX_ELEMENT_FUNC_ID_LIGHT_CWWW_CLIENT_CTL:
            MESHX_LOGI(MODULE_ID_COMMON, "Light CW-WW Client Element ID: %d, Func ID: %d, Data: %d|%d", msg->element_id, msg->func_id,
                     ((const meshx_api_light_cwww_client_evt_t*)msg->payload)->state_change.ctl.lightness,
                     ((const meshx_api_light_cwww_client_evt_t*)msg->payload)->state_change.ctl.temperature);
            break;
        default:
            MESHX_LOGW(MODULE_ID_COMMON, "Unhandled function ID: %d", msg->func_id);
            break;
        }
        break;
    default:
        MESHX_LOGW(MODULE_ID_COMMON, "Unhandled element type: %d", msg->element_type);
        break;
    }
    return;
}

static void meshx_app_ctrl_cb(const meshx_msg_ctrl_t *msg)
{
    if (!msg)
        return;

    MESHX_LOGI(MODULE_ID_COMMON, "Control Event Received: ID 0x%x", msg->msg_id);

    return;
}
