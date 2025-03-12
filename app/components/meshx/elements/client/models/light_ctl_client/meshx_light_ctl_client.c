/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_light_ctl_client.c
 * @brief Implementation of the Light CTL Client model for ESP32 BLE Mesh.
 *
 * This file contains the implementation of the Light CTL Client model, including
 * initialization, callback registration, and event handling.
 *
 * @author Pranjal Chanda
 */

#include "meshx_light_ctl_client.h"

#define TAG __func__

#define LIGHT_CTL_CLIENT_INIT_MAGIC 0x8932

static uint16_t light_ctl_client_init_flag = 0;

SLIST_HEAD(light_ctl_cli_cb_reg_head, light_ctl_cli_cb_reg);
static struct light_ctl_cli_cb_reg_head light_ctl_cli_cb_reg_table = SLIST_HEAD_INITIALIZER(light_ctl_cli_cb_reg_table);
static SemaphoreHandle_t light_ctl_cli_cb_reg_mutex;

/**
 * @brief Dispatch the registered callbacks for the Light CTL Client events.
 *
 * This function iterates through the registered callback table and calls the
 * appropriate callback functions based on the event bitmap.
 *
 * @param[in] param Pointer to the BLE Mesh Light Client callback parameters.
 * @param[in] evt The Light CTL Client event.
 */
static void light_ctl_cli_reg_cb_dispatch(const esp_ble_mesh_light_client_cb_param_t *param, light_ctl_cli_evt_t evt)
{
    if (xSemaphoreTake(light_ctl_cli_cb_reg_mutex, portMAX_DELAY) == pdTRUE)
    {
        light_ctl_cli_cb_reg_t *ptr;
        SLIST_FOREACH(ptr, &light_ctl_cli_cb_reg_table, next)
        {
            if ((evt & ptr->evt_bmap) && ptr->cb != NULL && ptr->cb(param, evt)) // Call the registered callback
            {
                break;
            }
        }
        xSemaphoreGive(light_ctl_cli_cb_reg_mutex);
    }
    else
    {
        ESP_LOGW(TAG, "Failed to take mutex for callback dispatch");
    }
}

/**
 * @brief BLE Mesh Light Client callback function.
 *
 * This function is called by the BLE Mesh stack when a Light Client event occurs.
 * It logs the event details and dispatches the registered callbacks.
 *
 * @param[in] event The Light Client event.
 * @param[in] param Pointer to the BLE Mesh Light Client callback parameters.
 */
void meshx_light_client_cb(esp_ble_mesh_light_client_cb_event_t event,
                                  const esp_ble_mesh_light_client_cb_param_t *param)
{
    ESP_LOGD(TAG, "evt|err|op|src|dst: %02x|%d|%04x|%04x|%04x",
            event, param->error_code, (unsigned)param->params->ctx.recv_op, param->params->ctx.addr, param->params->ctx.recv_dst);
    if(param->error_code == MESHX_SUCCESS)
    {
        light_ctl_cli_reg_cb_dispatch(param, (light_ctl_cli_evt_t)BIT(event));
    }
}

/**
 * @brief Register a callback function for the Light CTL Client.
 *
 * This function registers a callback function that will be called when specific
 * events occur in the Light CTL Client model.
 *
 * @param[in] cb                The callback function to register.
 * @param[in] config_evt_bmap   A bitmap representing the configuration events to register for.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_INVALID_ARG: Invalid argument
 *    - MESHX_FAIL: Other failures
 */
meshx_err_t meshx_light_ctl_cli_reg_cb(light_cli_cb cb, uint32_t config_evt_bmap)
{
    if (cb == NULL || config_evt_bmap == 0)
        return MESHX_INVALID_ARG; // Invalid arguments

    struct light_ctl_cli_cb_reg *new_entry = (struct light_ctl_cli_cb_reg *) malloc(sizeof(struct light_ctl_cli_cb_reg));
    if (new_entry == NULL)
        return MESHX_NO_MEM; // Memory allocation failed

    new_entry->cb = cb;
    new_entry->evt_bmap = config_evt_bmap;

    if (xSemaphoreTake(light_ctl_cli_cb_reg_mutex, portMAX_DELAY) == pdTRUE)
    {
        SLIST_INSERT_HEAD(&light_ctl_cli_cb_reg_table, new_entry, next);
        xSemaphoreGive(light_ctl_cli_cb_reg_mutex);
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Initialize the Light CTL Client model.
 *
 * This function initializes the Light CTL Client model by registering the
 * Light Client callback with the BLE Mesh stack.
 *
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
meshx_err_t meshx_light_ctl_client_init()
{
    if (light_ctl_client_init_flag == LIGHT_CTL_CLIENT_INIT_MAGIC)
        return MESHX_SUCCESS;

    light_ctl_cli_cb_reg_mutex = xSemaphoreCreateMutex();
    if (light_ctl_cli_cb_reg_mutex == NULL)
        return MESHX_NO_MEM; // Failed to create mutex

    meshx_err_t err = esp_ble_mesh_register_light_client_callback((esp_ble_mesh_light_client_cb_t)&meshx_light_client_cb);
    if (err == MESHX_SUCCESS)
    {
        light_ctl_client_init_flag = LIGHT_CTL_CLIENT_INIT_MAGIC;
    }
    return err;
}

/**
 * @brief Send a Light CTL message.
 *
 * This function sends a Light CTL message with the specified parameters.
 *
 * @param[in] params Pointer to the structure containing the message parameters.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - Appropriate error code on failure
 */
meshx_err_t meshx_light_ctl_send_msg(light_ctl_send_args_t * params)
{
    meshx_err_t err = MESHX_SUCCESS;
    bool send_msg = false;
    esp_ble_mesh_client_common_param_t common = {0};
    esp_ble_mesh_light_client_set_state_t set = {0};

    if(params == NULL)
        return MESHX_INVALID_ARG;

    common.model        = params->model;
    common.opcode       = params->opcode;
    common.ctx.addr     = params->addr;
    common.ctx.net_idx  = params->net_idx;
    common.ctx.app_idx  = params->app_idx;
    common.msg_timeout  = 0; /* 0 indicates that timeout value from menuconfig will be used */
    common.ctx.send_ttl = 3;
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 2, 0)
    common.msg_role = ROLE_NODE;
#endif

    if(params->opcode == ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_SET ||
       params->opcode == ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_SET_UNACK)
    {
        set.ctl_set.op_en           = false;
        set.ctl_set.tid             = params->tid;
        set.ctl_set.ctl_delta_uv    = params->delta_uv;
        set.ctl_set.ctl_lightness   = params->lightness;
        set.ctl_set.ctl_temperature = params->temperature;
        send_msg = true;
    }
    else if(params->opcode == ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_SET ||
            params->opcode == ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_SET_UNACK)
    {
        set.ctl_temperature_range_set.range_min = params->temp_range_min;
        set.ctl_temperature_range_set.range_max = params->temp_range_max;
        send_msg = true;
    }
    else{
        err = esp_ble_mesh_client_model_send_msg(common.model, &common.ctx, common.opcode, 0, NULL, 0, true, ROLE_NODE);
        if (err)
        {
            ESP_LOGE(TAG, "Send Generic OnOff failed");
            return err;
        }
    }
    if(send_msg)
    {
        err = esp_ble_mesh_light_client_set_state(&common, &set);
        if (err)
        {
            ESP_LOGE(TAG, "Light CTL Client Send Message failed: (%d)", err);
        }
    }
    return err;
}

/**
 * @brief Sends a message to control the light temperature.
 *
 * This function sends a message to adjust the light temperature using the provided parameters.
 *
 * @param[in] params Pointer to a structure containing the parameters for the light temperature control message.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_INVALID_ARG: Invalid argument
 *    - MESHX_FAIL: Sending message failed
 */
meshx_err_t meshx_light_ctl_temperature_send_msg(light_ctl_send_args_t * params)
{
    meshx_err_t err = MESHX_SUCCESS;
    esp_ble_mesh_client_common_param_t common = {0};
    esp_ble_mesh_light_client_set_state_t set = {0};

    if(params == NULL)
        return MESHX_INVALID_ARG;

    common.model        = params->model;
    common.opcode       = params->opcode;
    common.ctx.addr     = params->addr;
    common.ctx.net_idx  = params->net_idx;
    common.ctx.app_idx  = params->app_idx;
    common.msg_timeout  = 0; /* 0 indicates that timeout value from menuconfig will be used */
    common.ctx.send_ttl = 3;
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 2, 0)
    common.msg_role = ROLE_NODE;
#endif

    if(params->opcode != ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_GET)
    {
        set.ctl_temperature_set.op_en           = false;
        set.ctl_temperature_set.tid             = params->tid;
        set.ctl_temperature_set.ctl_delta_uv    = params->delta_uv;
        set.ctl_temperature_set.ctl_temperature = params->temperature;

        err = esp_ble_mesh_light_client_set_state(&common, &set);
        if (err)
        {
            ESP_LOGE(TAG, "Light CTL Client Send Message failed: (%d)", err);
        }
    }
    else{
        err = esp_ble_mesh_client_model_send_msg(common.model, &common.ctx, common.opcode, 0, NULL, 0, true, ROLE_NODE);
        if (err)
        {
            ESP_LOGE(TAG, "Send Generic OnOff failed");
            return err;
        }
    }
    return err;
}
