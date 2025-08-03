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

#include "meshx_err.h"
#include "meshx_light_ctl_client.h"

#define LIGHT_CTL_CLIENT_INIT_MAGIC 0x8932

static uint16_t light_ctl_client_init_flag = 0;

/**
 * @brief Notifies about a change in the CTL (Color Temperature Lightness) state.
 *
 * This function is called to notify the application or upper layers when the CTL state
 * of a light device has changed. It provides the relevant parameters describing the new state.
 *
 * @param[in] param Pointer to a structure containing the CTL state change parameters.
 *
 * @return meshx_err_t Returns an error code indicating the result of the notification operation.
 */
static meshx_err_t meshx_ctl_state_change_notify(meshx_gen_light_cli_cb_param_t *param)
{
    if (!param)
        return MESHX_INVALID_ARG;

    meshx_ctl_cli_el_msg_t el_light_ctl_param = {
        .err_code = MESHX_SUCCESS,
        .ctx = param->ctx,
        .model = param->model
    };
    switch (param->ctx.opcode)
    {
    case MESHX_MODEL_OP_LIGHT_CTL_STATUS:
        el_light_ctl_param.ctl_state.lightness = param->status.ctl_status.present_ctl_lightness;
        el_light_ctl_param.ctl_state.temperature = param->status.ctl_status.present_ctl_temperature;
        break;
    case MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_STATUS:
        el_light_ctl_param.ctl_state.temperature = param->status.ctl_temperature_status.present_ctl_temperature;
        el_light_ctl_param.ctl_state.delta_uv = param->status.ctl_temperature_status.present_ctl_delta_uv;
        break;
    case MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_STATUS:
        el_light_ctl_param.ctl_state.temperature = param->status.ctl_temperature_range_status.range_min;
        el_light_ctl_param.ctl_state.delta_uv = param->status.ctl_temperature_range_status.range_max;
        break;
    case MESHX_MODEL_OP_LIGHT_CTL_DEFAULT_STATUS:
        el_light_ctl_param.ctl_state.lightness = param->status.ctl_default_status.lightness;
        el_light_ctl_param.ctl_state.temperature = param->status.ctl_default_status.temperature;
        el_light_ctl_param.ctl_state.delta_uv = param->status.ctl_default_status.delta_uv;
        break;

    default:
        break;
    }
    if(param->evt == MESHX_GEN_LIGHT_CLI_TIMEOUT)
    {
        el_light_ctl_param.err_code = MESHX_TIMEOUT;
    }

    if (MESHX_ADDR_IS_UNICAST(param->ctx.dst_addr)
    || (MESHX_ADDR_BROADCAST(param->ctx.dst_addr))
    || (MESHX_ADDR_IS_GROUP(param->ctx.dst_addr)
    && (MESHX_SUCCESS == meshx_is_group_subscribed(param->model.p_model, param->ctx.dst_addr))))
    {
        return control_task_msg_publish(
            CONTROL_TASK_MSG_CODE_EL_STATE_CH,
            CONTROL_TASK_MSG_EVT_EL_STATE_CH_SET_CTL,
            &el_light_ctl_param,
            sizeof(meshx_ctl_cli_el_msg_t));
    }
    return MESHX_NOT_SUPPORTED;
}

/**
 * @brief Handles generic light model messages for the Light CTL client.
 *
 * This function processes incoming messages related to the generic light model
 * and performs the necessary actions based on the message event and parameters.
 *
 * @param[in] pdev      Pointer to the device structure containing device-specific information.
 * @param[in] model_id  Identifier for the control task message event associated with the model.
 * @param[in,out] param Pointer to the callback parameter structure for the generic light client.
 *
 * @return meshx_err_t  Error code indicating the result of the operation.
 */
static meshx_err_t meshx_handle_gen_light_msg(
    const dev_struct_t *pdev,
    control_task_msg_evt_t model_id,
    meshx_gen_light_cli_cb_param_t *param
)
{
    if (model_id != MESHX_MODEL_ID_LIGHT_CTL_CLI || !pdev || !param)
        return MESHX_INVALID_ARG;

    MESHX_LOGD(MODULE_ID_MODEL_SERVER, "op|src|dst:%04" PRIx32 "|%04x|%04x",
               param->ctx.opcode, param->ctx.src_addr, param->ctx.dst_addr);
    meshx_err_t err = MESHX_SUCCESS;

    switch (param->evt)
    {
    case MESHX_GEN_LIGHT_CLI_EVT_SET:
    case MESHX_GEN_LIGHT_CLI_PUBLISH:
        err = meshx_ctl_state_change_notify(param);
        if (err)
        {
            MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Failed to notify state change: %d", err);
        }
        break;
    case MESHX_GEN_LIGHT_CLI_TIMEOUT:
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Timeout");
        err = meshx_ctl_state_change_notify(param);
        if (err)
        {
            MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Failed to notify state change: %d", err);
        }
        break;
    default:
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Unknown event: %d", param->evt);
        err = MESHX_NOT_SUPPORTED;
        break;
    }

    return err;
}

/**
 * @brief Registers a callback function for the Light CTL (Color Temperature Lightness) client model.
 *
 * This function associates a user-defined callback with a specific Light CTL client model,
 * allowing the application to handle events or responses related to the model.
 *
 * @param[in] model_id The unique identifier of the Light CTL client model instance.
 * @param[in] cb       The callback function to be registered. This function will be called
 *                     when relevant events occur for the specified model.
 *
 * @return meshx_err_t Returns MESHX_OK on success, or an appropriate error code on failure.
 */
meshx_err_t meshx_light_ctl_cli_reg_cb(uint32_t model_id, meshx_gen_light_cli_cb_t cb)
{
    if (cb == NULL || model_id != MESHX_MODEL_ID_LIGHT_CTL_CLI)
        return MESHX_INVALID_ARG; // Invalid arguments

    return control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_FRM_BLE,
        model_id,
        cb);
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
    meshx_err_t err = MESHX_SUCCESS;

    if (light_ctl_client_init_flag == LIGHT_CTL_CLIENT_INIT_MAGIC)
        return MESHX_SUCCESS;

    err = meshx_gen_light_cli_init();
    if (err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Failed to initialize meshx client");
    }

    return err;
}

/**
 * @brief Creates and initializes a Generic Light Client model instance.
 *
 * This function allocates and sets up a Generic Light Client model, associating it with the provided
 * SIG model context.
 *
 * @param[out] p_model      Pointer to a pointer where the created model instance will be stored.
 * @param[in]  p_sig_model  Pointer to the SIG model context to associate with the client model.
 *
 * @return meshx_err_t      Returns an error code indicating the result of the operation.
 *                         - MESHX_OK on success
 *                         - Appropriate error code otherwise
 */
meshx_err_t meshx_light_ctl_client_create(meshx_light_ctl_client_model_t **p_model, void *p_sig_model)
{
    if (!p_model || !p_sig_model)
    {
        return MESHX_INVALID_ARG;
    }

    *p_model = (meshx_light_ctl_client_model_t *)MESHX_CALOC(1, sizeof(meshx_light_ctl_client_model_t));
    if (!*p_model)
    {
        return MESHX_NO_MEM;
    }

    return meshx_plat_on_off_gen_cli_create(
        p_sig_model,
        &((*p_model)->meshx_light_ctl_client_pub),
        &((*p_model)->meshx_light_ctl_client_gen_cli));
}

/**
 * @brief Delete the Light client model instance.
 *
 * This function deletes an instance of the Light client model, freeing
 * associated resources and setting the model pointer to NULL.
 *
 * @param[in,out] p_model Double pointer to the Light client model instance to be deleted.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully deleted the model.
 *     - MESHX_INVALID_ARG: Invalid argument provided.
 */
meshx_err_t meshx_light_ctl_client_delete(meshx_light_ctl_client_model_t **p_model)
{
    if (p_model == NULL || *p_model == NULL)
    {
        return MESHX_INVALID_ARG;
    }

    meshx_plat_gen_cli_delete(
        &((*p_model)->meshx_light_ctl_client_pub),
        &((*p_model)->meshx_light_ctl_client_gen_cli)
    );

    MESHX_FREE(*p_model);
    *p_model = NULL;

    return MESHX_SUCCESS;
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
            ESP_LOGE(TAG, "Send Generic Light failed");
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
            ESP_LOGE(TAG, "Send Generic Light failed");
            return err;
        }
    }
    return err;
}
