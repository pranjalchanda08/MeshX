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

#if defined(__ZEPHYR__)
#define APP_MAIN_RET_TYPE int
#define APP_MAIN_RETURN() return 0
#elif defined(ESP_PLATFORM)
#define APP_MAIN_RET_TYPE void
#define APP_MAIN_RETURN()
#else
#define APP_MAIN_RET_TYPE int
#define APP_MAIN_RETURN() return 0
#endif

static meshx_err_t meshx_app_data_cb(const meshx_app_element_msg_header_t *msg_hdr, const meshx_data_payload_t *data_payload_u);
static meshx_err_t meshx_app_ctrl_cb(const meshx_ctrl_msg_header_t *msg_hdr, const meshx_ctrl_payload_t *msg);
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
APP_MAIN_RET_TYPE CONFIG_APP_MAIN(void)
{
    meshx_err_t err;

    /* Initialize MeshX */
    err = meshx_init(&meshx_config);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "MeshX Init failed: 0x%x", err);
    }

    APP_MAIN_RETURN();
}

/**
 * @brief Application element callback handler.
 *
 * Dispatches inbound state updates, telemetry, and execution acknowledgments
 * for all element types in the active dynamic composition.
 *
 * @param[in] msg_hdr       Pointer to the application element message header.
 * @param[in] data_payload_u Pointer to the element event data payload union.
 *
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG if parameters are NULL.
 */
static meshx_err_t meshx_app_data_cb(const meshx_app_element_msg_header_t *msg_hdr, const meshx_data_payload_t *data_payload_u)
{
    if (!msg_hdr || !data_payload_u)
        return MESHX_INVALID_ARG;

    switch (msg_hdr->element_type)
    {
        case MESHX_ELEMENT_TYPE_RELAY_SERVER:
            MESHX_LOGI(MODULE_ID_COMMON, "Relay Server Element ID: %d, Func ID: %d, Data: %d", msg_hdr->element_id, msg_hdr->func_id, data_payload_u->relay_server_evt.on_off);
            break;
        case MESHX_ELEMENT_TYPE_RELAY_CLIENT:
            MESHX_LOGI(MODULE_ID_COMMON, "Relay Client Element ID: %d, Func ID: %d, Data: %d, Error: %d", msg_hdr->element_id, msg_hdr->func_id, data_payload_u->relay_client_evt.on_off, data_payload_u->relay_client_evt.err_code);
            break;
        case MESHX_ELEMENT_TYPE_LIGHT_CWWW_SERVER:
            switch (msg_hdr->func_id)
            {
                case MESHX_ELEMENT_FUNC_ID_LIGHT_CWWW_SERVER_ONN_OFF:
                    MESHX_LOGI(MODULE_ID_COMMON, "Light CW-WW Server Element ID: %d, Func ID: %d, Data: %d", msg_hdr->element_id, msg_hdr->func_id,
                            data_payload_u->light_cwww_server_evt.state_change.on_off.state);
                    break;
                case MESHX_ELEMENT_FUNC_ID_LIGHT_CWWW_SERVER_CTL:
                    MESHX_LOGI(MODULE_ID_COMMON, "Light CW-WW Server Element ID: %d, Func ID: %d, Data: %d|%d", msg_hdr->element_id, msg_hdr->func_id,
                        data_payload_u->light_cwww_server_evt.state_change.ctl.lightness,
                        data_payload_u->light_cwww_server_evt.state_change.ctl.temperature);
                break;
                default:
                    MESHX_LOGW(MODULE_ID_COMMON, "Unhandled function ID: %d", msg_hdr->func_id);
                    break;
            }
            break;
        case MESHX_ELEMENT_TYPE_LIGHT_CWWW_CLIENT:
            switch (msg_hdr->func_id)
            {
                case MESHX_ELEMENT_FUNC_ID_LIGHT_CWWW_CLIENT_ONN_OFF:
                    MESHX_LOGI(MODULE_ID_COMMON, "Light CW-WW Client Element ID: %d, Func ID: %d, Data: %d, Error: %d", msg_hdr->element_id, msg_hdr->func_id,
                            data_payload_u->light_cwww_client_evt.state_change.on_off.state, data_payload_u->light_cwww_client_evt.err_code);
                    break;
                case MESHX_ELEMENT_FUNC_ID_LIGHT_CWWW_CLIENT_CTL:
                    MESHX_LOGI(MODULE_ID_COMMON, "Light CW-WW Client Element ID: %d, Func ID: %d, Data: %d|%d", msg_hdr->element_id, msg_hdr->func_id,
                            data_payload_u->light_cwww_client_evt.state_change.ctl.lightness,
                            data_payload_u->light_cwww_client_evt.state_change.ctl.temperature);
                    break;
                default:
                    MESHX_LOGW(MODULE_ID_COMMON, "Unhandled function ID: %d", msg_hdr->func_id);
                    break;
            }
            break;
        case MESHX_ELEMENT_TYPE_LIGHT_HSL_SERVER:
            switch (msg_hdr->func_id)
            {
                case MESHX_ELEMENT_FUNC_ID_LIGHT_HSL_SERVER_ONN_OFF:
                    MESHX_LOGI(MODULE_ID_COMMON, "Light HSL Server Element ID: %d, Func ID: %d, Data: %d", msg_hdr->element_id, msg_hdr->func_id,
                            data_payload_u->light_hsl_server_evt.state_change.on_off.state);
                    break;
                case MESHX_ELEMENT_FUNC_ID_LIGHT_HSL_SERVER_HSL:
                    MESHX_LOGI(MODULE_ID_COMMON, "Light HSL Server Element ID: %d, Func ID: %d, HSL: %d|%d|%d", msg_hdr->element_id, msg_hdr->func_id,
                            data_payload_u->light_hsl_server_evt.state_change.hsl.lightness,
                            data_payload_u->light_hsl_server_evt.state_change.hsl.hue,
                            data_payload_u->light_hsl_server_evt.state_change.hsl.saturation);
                    break;
                default:
                    MESHX_LOGW(MODULE_ID_COMMON, "Unhandled function ID: %d", msg_hdr->func_id);
                    break;
            }
            break;
        case MESHX_ELEMENT_TYPE_LIGHT_HSL_CLIENT:
            switch (msg_hdr->func_id)
            {
                case MESHX_ELEMENT_FUNC_ID_LIGHT_HSL_CLIENT_ONN_OFF:
                    MESHX_LOGI(MODULE_ID_COMMON, "Light HSL Client Element ID: %d, Func ID: %d, Data: %d, Error: %d", msg_hdr->element_id, msg_hdr->func_id,
                            data_payload_u->light_hsl_client_evt.state_change.on_off.state, data_payload_u->light_hsl_client_evt.err_code);
                    break;
                case MESHX_ELEMENT_FUNC_ID_LIGHT_HSL_CLIENT_HSL:
                    MESHX_LOGI(MODULE_ID_COMMON, "Light HSL Client Element ID: %d, Func ID: %d, HSL: %d|%d|%d", msg_hdr->element_id, msg_hdr->func_id,
                        data_payload_u->light_hsl_client_evt.state_change.hsl.lightness,
                        data_payload_u->light_hsl_client_evt.state_change.hsl.hue,
                        data_payload_u->light_hsl_client_evt.state_change.hsl.saturation);
                break;
                default:
                    MESHX_LOGW(MODULE_ID_COMMON, "Unhandled function ID: %d", msg_hdr->func_id);
                    break;
            }
            break;
        case MESHX_ELEMENT_TYPE_SENSOR_SERVER:
            MESHX_LOGI(MODULE_ID_COMMON, "Sensor Server Element ID: %d, Func ID: %d, Value: %d", msg_hdr->element_id, msg_hdr->func_id,
                    data_payload_u->sensor_server_evt.state_change.data.value);
            break;
        case MESHX_ELEMENT_TYPE_SENSOR_CLIENT:
            MESHX_LOGI(MODULE_ID_COMMON, "Sensor Client Element ID: %d, Func ID: %d, Value: %d, Error: %d", msg_hdr->element_id, msg_hdr->func_id,
                    data_payload_u->sensor_client_evt.state_change.data.value, data_payload_u->sensor_client_evt.err_code);
            break;
        default:
            MESHX_LOGW(MODULE_ID_COMMON, "Unhandled element type: %d", msg_hdr->element_type);
            break;
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Application control event callback handler.
 *
 * Handles control task system events such as provisioning completion and node reset.
 *
 * @param[in] msg_hdr Pointer to the control message header.
 * @param[in] msg     Pointer to the control message payload union.
 *
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG if parameters are NULL.
 */
static meshx_err_t meshx_app_ctrl_cb(const meshx_ctrl_msg_header_t *msg_hdr, const meshx_ctrl_payload_t *msg)
{
    if (!msg_hdr || !msg)
        return MESHX_INVALID_ARG;

    MESHX_LOGI(MODULE_ID_COMMON, "Control Event Received: ID %d", msg_hdr->evt_id);

    return MESHX_SUCCESS;
}
