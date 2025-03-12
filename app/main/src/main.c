/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file main.c
 * @brief Main application file for initializing MeshX on ESP32.
 *
 * This file contains the entry point for the application which initializes
 * the MeshX library and handles any initialization errors.
 */

#include "meshx.h"

#define TAG __func__

#define CONFIG_MESHX_NVS_SAVE_PERIOD_MS 1000

static meshx_err_t meshx_app_data_cb(const meshx_app_element_msg_header_t *msg_hdr, const meshx_data_payload_t *data_payload_u);
static meshx_err_t meshx_app_ctrl_cb(const meshx_ctrl_msg_header_t *msg_hdr, const meshx_ctrl_payload_t *msg);
/**
 * @brief Array of element components with their respective types and counts.
 *
 * This array holds the configuration for different types of elements in the MeshX application.
 * Each entry in the array consists of an element type and the corresponding count defined in the configuration.
 *
 */
static element_comp_t element_comp_arr[] = {
    {MESHX_ELEMENT_TYPE_RELAY_SERVER, CONFIG_RELAY_SERVER_COUNT},
    {MESHX_ELEMENT_TYPE_RELAY_CLIENT, CONFIG_RELAY_CLIENT_COUNT},
    {MESHX_ELEMENT_TYPE_LIGHT_CWWW_SERVER, CONFIG_LIGHT_CWWW_SRV_COUNT},
    {MESHX_ELEMENT_TYPE_LIGHT_CWWW_CLIENT, CONFIG_LIGHT_CWWW_CLIENT_COUNT}
};

/**
 * @brief Configuration for the MeshX library.
 */
static const meshx_config_t meshx_config = {
    .cid = CONFIG_CID_ID,
    .pid = CONFIG_PID_ID,
    .app_ctrl_cb = &meshx_app_ctrl_cb,
    .app_element_cb = &meshx_app_data_cb,
    .product_name = CONFIG_PRODUCT_NAME,
    .element_comp_arr = element_comp_arr,
    .element_comp_arr_len = ARRAY_SIZE(element_comp_arr),
    .meshx_nvs_save_period = CONFIG_MESHX_NVS_SAVE_PERIOD_MS,
};

/**
 * @brief Main application entry point.
 *
 * This function initializes the MeshX library and logs an error message
 * if the initialization fails.
 */
void app_main(void)
{
    meshx_err_t err;

    err = meshx_init(&meshx_config);
    if (err)
    {
        ESP_LOGE(TAG, "MeshX Init failed (err: 0x%x)", err);
        return;
    }
}

static meshx_err_t meshx_app_data_cb(const meshx_app_element_msg_header_t *msg_hdr, const meshx_data_payload_t *data_payload_u)
{
    if (!msg_hdr || !data_payload_u)
        return ESP_ERR_INVALID_ARG;

    switch (msg_hdr->element_type)
    {
    case MESHX_ELEMENT_TYPE_RELAY_SERVER:
        ESP_LOGI(TAG, "Relay Server Element ID: %d, Func ID: %d, Data: %d", msg_hdr->element_id, msg_hdr->func_id, data_payload_u->relay_server_evt.on_off);
        break;
    case MESHX_ELEMENT_TYPE_RELAY_CLIENT:
        ESP_LOGI(TAG, "Relay Client Element ID: %d, Func ID: %d, Data: %d", msg_hdr->element_id, msg_hdr->func_id, data_payload_u->relay_client_evt.on_off);
        break;
    case MESHX_ELEMENT_TYPE_LIGHT_CWWW_SERVER:
        switch (msg_hdr->func_id)
        {
        case MESHX_ELEMENT_FUNC_ID_LIGHT_CWWW_SERVER_ONN_OFF:
            ESP_LOGI(TAG, "Light CW-WW Server Element ID: %d, Func ID: %d, Data: %d", msg_hdr->element_id, msg_hdr->func_id,
                     data_payload_u->light_cwww_server_evt.state_change.on_off.state);
            break;
        case MESHX_ELEMENT_FUNC_ID_LIGHT_CWWW_SERVER_CTL:
            ESP_LOGI(TAG, "Light CW-WW Server Element ID: %d, Func ID: %d, Data: %d|%d", msg_hdr->element_id, msg_hdr->func_id,
                     data_payload_u->light_cwww_server_evt.state_change.ctl.lightness,
                     data_payload_u->light_cwww_server_evt.state_change.ctl.temperature);
            break;
        default:
            ESP_LOGW(TAG, "Unhandled function ID: %d", msg_hdr->func_id);
            break;
        }
        break;
    case MESHX_ELEMENT_TYPE_LIGHT_CWWW_CLIENT:
        switch (msg_hdr->func_id)
        {
        case MESHX_ELEMENT_FUNC_ID_LIGHT_CWWW_SERVER_ONN_OFF:
            ESP_LOGI(TAG, "Light CW-WW Client Element ID: %d, Func ID: %d, Data: %d", msg_hdr->element_id, msg_hdr->func_id,
                     data_payload_u->light_cwww_server_evt.state_change.on_off.state);
            break;
        case MESHX_ELEMENT_FUNC_ID_LIGHT_CWWW_SERVER_CTL:
            ESP_LOGI(TAG, "Light CW-WW Client Element ID: %d, Func ID: %d, Data: %d|%d", msg_hdr->element_id, msg_hdr->func_id,
                     data_payload_u->light_cwww_client_evt.state_change.ctl.lightness,
                     data_payload_u->light_cwww_client_evt.state_change.ctl.temperature);
            break;
        default:
            ESP_LOGW(TAG, "Unhandled function ID: %d", msg_hdr->func_id);
            break;
        }
        break;
    default:
        ESP_LOGW(TAG, "Unhandled element type: %d", msg_hdr->element_type);
        break;
    }
    return MESHX_SUCCESS;
}

static meshx_err_t meshx_app_ctrl_cb(const meshx_ctrl_msg_header_t *msg_hdr, const meshx_ctrl_payload_t *msg)
{
    return MESHX_SUCCESS;
}
