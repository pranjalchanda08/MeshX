/**
 * @file meshx_model_ctl.cpp
 * @brief Implementation of Light CTL Model classes for MeshX.
 *        This file contains the implementation of the Light CTL Server and Client models
 *        for the MeshX BLE mesh framework.
 *
 * Key Features:
 *  - Implements Bluetooth SIG-defined Light CTL model
 *  - Inherits from meshXServerModel and meshXClientModel templates
 *  - Provides standard Light CTL control operations (lightness, temperature, delta UV)
 *  - Integrates with MeshX transmission control
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#include <light_model/meshx_model_ctl.hpp>
#include <meshx_element_class.hpp>

#if CONFIG_ENABLE_LIGHT_CTL_CLIENT
/**
 * @brief Handle Light CTL state change notifications from the MeshX stack.
 *
 * This function is responsible for handling Light CTL state change notifications
 * from the MeshX stack and publishing the state change event to the element layer.
 *
 * @param[in] param  Pointer to the Light CTL client callback parameter structure.
 * @param[in] status Status of the state change event (success or timeout).
 *
 * @return
 *     - MESHX_SUCCESS: Successfully handled the state change notification.
 *     - MESHX_INVALID_ARG: One or more arguments are invalid.
 */
MESHX_LIGHT_CTL_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXLightCTLClientModel MESHX_LIGHT_CTL_CLIENT_MODEL_TEMPLATE_PARAMS
    :: meshx_state_change_notify(const meshx_gen_light_cli_cb_param_t *param, uint8_t status) const
{
    if (!param){
        return MESHX_INVALID_ARG;
    }

    meshx_light_ctl_cli_el_msg_t ctl_param = {
        .err_code = status,
        .model = param->model,
        .ctx = param->ctx,
        .lightness = param->status.ctl_status.present_ctl_lightness,
        .temperature = param->status.ctl_status.present_ctl_temperature,
        .delta_uv = 0,  // CTL status doesn't include delta_uv, only temperature status does
        .temp_range_min = 0,  // Range values come from separate range status message
        .temp_range_max = 0
    };
    /* Send the state change event to the respective Element */
    if (this->get_parent_element()) {
        return this->get_parent_element()->on_model_cb(&ctl_param);
    } else {
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Parent element is null");
    }

    return MESHX_INVALID_STATE;
}

/**
 * @brief Creates a meshXLightCTLClientModel instance based on a BLE device
 *
 * This function is used to create a meshXLightCTLClientModel instance based on a BLE device.
 *
 * @return Pointer to the created meshXLightCTLClientModel instance
 */
MESHX_LIGHT_CTL_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXLightCTLClientModel MESHX_LIGHT_CTL_CLIENT_MODEL_TEMPLATE_PARAMS
    :: model_from_ble_cb(
        dev_struct_t *p_dev,
        control_task_msg_evt_t model_id,
        meshx_ptr_t params)
{
    if(!params || !p_dev)
    {
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Invalid parameters");
        return MESHX_INVALID_ARG;
    }
    if(model_id != MESHX_MODEL_ID_LIGHT_CTL_CLI)
    {
        /* Callback triggered not for this model */
        return MESHX_SUCCESS;
    }
    const auto *param = static_cast<const meshx_gen_light_cli_cb_param_t *>(params);

    return std::to_underlying(param->evt) == std::to_underlying(meshx_base_cli_evt::MESHX_BASE_CLI_TIMEOUT) ?
        meshx_state_change_notify(param, MESHX_TIMEOUT) :
        meshx_state_change_notify(param, MESHX_SUCCESS);
}

/**
 * @brief Send a packet to the MeshX stack based on the given parameters
 *
 * This function takes a pointer to a meshx_light_ctl_send_params_t structure as input
 * and sends the corresponding packet to the MeshX stack for processing.
 *
 * @param[in] params Pointer to the meshx_light_ctl_send_params_t structure containing the parameters
 * @return MESHX_SUCCESS on success, or an error code on failure
 */
MESHX_LIGHT_CTL_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXLightCTLClientModel MESHX_LIGHT_CTL_CLIENT_MODEL_TEMPLATE_PARAMS
    :: model_send(meshx_light_ctl_send_params_t *params)
{
    meshx_err_t err;
    meshx_light_client_set_state_t set;
    if (!params || !params->model || !params->model->p_model)
    {
        return MESHX_INVALID_ARG;
    }

    meshx_gen_light_client_send_params_t send_params;

    send_params.state   = &set;
    send_params.opcode  = static_cast<uint16_t>(params->ctx->opcode);
    send_params.net_idx = params->ctx->net_idx;
    send_params.app_idx = params->ctx->app_idx;
    send_params.addr    = params->model->pub_addr;
    send_params.model   = params->model->p_model;

    if (params->ctx->opcode == MESHX_MODEL_OP_LIGHT_CTL_GET)
    {
        err = this->get_base_model()->plat_send_msg(&send_params);
    }

    else if (params->ctx->opcode == MESHX_MODEL_OP_LIGHT_CTL_SET ||
             params->ctx->opcode == MESHX_MODEL_OP_LIGHT_CTL_SET_UNACK)
    {
        set.ctl_set.ctl_lightness = params->lightness;
        set.ctl_set.ctl_temperature = params->temperature;
        set.ctl_set.ctl_delta_uv = params->delta_uv;
        set.ctl_set.tid = params->tid;
        set.ctl_set.op_en = false;

        err = this->get_base_model()->plat_send_msg(&send_params);
    }
    else if (params->ctx->opcode == MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_GET)
    {
        err = this->get_base_model()->plat_send_msg(&send_params);
    }
    else if (params->ctx->opcode == MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_SET ||
             params->ctx->opcode == MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_SET_UNACK)
    {
        set.ctl_temperature_range_set.range_min = params->temperature; // Using temperature field for range_min
        set.ctl_temperature_range_set.range_max = params->delta_uv;    // Using delta_uv field for range_max

        err = this->get_base_model()->plat_send_msg(&send_params);
    }
    else
    {
        err = MESHX_INVALID_ARG; // Invalid opcode
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Invalid opcode for Light CTL Client: %04x", params->ctx->opcode);
    }
    return err;
}

/**
 * @brief A template class for creating Light CTL Client models.
 *
 * This class is derived from meshXClientModel and provides a convenient interface for
 * creating Light CTL Client models. It handles the Light CTL state change
 * notifications from the MeshX stack and publishes the state change event to the
 * element layer.
 *
 * @tparam meshxBaseClientModel_t The type of the meshXBaseClientModel class to be used.
 * @tparam meshx_send_packet_params_t The type of the meshXSendPacketParams structure used
 * for sending packets.
 *
 * @param[in] p_plat_model  A pointer to the platform model (MESHX_MODEL).
 * @param[in] model_id      The unique identifier of the BLE mesh model.
 * @param[in] parent_element A pointer to the parent element (meshXElementIF).
 */
MESHX_LIGHT_CTL_CLIENT_MODEL_TEMPLATE_PROTO
meshXLightCTLClientModel MESHX_LIGHT_CTL_CLIENT_MODEL_TEMPLATE_PARAMS
    ::meshXLightCTLClientModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element)
    : meshXClientModel(p_plat_model, model_id, parent_element) {/* Used only for initialization of Parent Class */}
#endif /* CONFIG_ENABLE_LIGHT_CTL_CLIENT */

#if CONFIG_ENABLE_LIGHT_CTL_SERVER

/**
 * @brief Creates and initializes a server model instance.
 *
 * This function handles the platform-specific model creation process for server models.
 * It initializes server-specific features and cannot be overridden by derived classes.
 *
 * @return meshx_err_t Returns an error code indicating the result of the operation.
 *         - MESHX_SUCCESS on successful model creation and initialization
 *         - MESHX_ERR_NO_MEM if memory allocation fails
 *         - Other error codes for platform-specific failures
 *
 * @note This is a final function and cannot be overridden by derived classes.
 */
MESHX_LIGHT_CTL_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXLightCTLServerModel MESHX_LIGHT_CTL_SERVER_MODEL_TEMPLATE_PARAMS
    :: plat_model_create(void)
{
    meshx_ptr_t p_pub = this->get_pub_struct();
    meshx_ptr_t p_gen = this->get_gen_struct();
    meshx_err_t err = MESHX_SUCCESS;

    err = meshx_plat_light_ctl_srv_create( this->get_plat_model(), &p_pub, &p_gen );
    if(err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to create Light CTL Server Model");
    }
    else
    {
        /* Set the publication and generic structures */
        this->set_pub_struct(p_pub);
        this->set_gen_struct(p_gen);
    }
    return err;
}

/**
 * @brief Deletes the Light CTL Server model and its associated resources.
 *
 * This function frees the memory allocated for the Light CTL Server
 * and sets the pointer to NULL. It also deletes the model publication
 * resources associated with the server.
 *
 * @return
 *     - MESHX_SUCCESS: Model and publication deleted successfully.
 *     - MESHX_FAIL: Failed to delete the model or publication.
 */
MESHX_LIGHT_CTL_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXLightCTLServerModel MESHX_LIGHT_CTL_SERVER_MODEL_TEMPLATE_PARAMS
    :: plat_model_delete(void)
{
    meshx_ptr_t p_pub = this->get_pub_struct();
    meshx_ptr_t p_gen = this->get_gen_struct();

    meshx_err_t err = meshx_plat_light_srv_delete( &p_pub, &p_gen );
    if (err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to delete Light CTL Server Model");
    }
    else
    {
        /* Set the publication and generic structures to NULL */
        this->set_pub_struct(nullptr);
        this->set_gen_struct(nullptr);
    }

    return err;
}

/**
 * @brief Send a packet to the MeshX stack based on the given parameters
 *
 * This function takes a pointer to a meshx_light_ctl_send_params_t structure as input
 * and sends the corresponding packet to the MeshX stack for processing.
 *
 * @param[in] params Pointer to the meshx_light_ctl_send_params_t structure containing the parameters
 * @return MESHX_SUCCESS on success, or an error code on failure
 */
MESHX_LIGHT_CTL_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXLightCTLServerModel MESHX_LIGHT_CTL_SERVER_MODEL_TEMPLATE_PARAMS
    :: model_send(meshx_light_ctl_send_params_t *params)
{
    if (!params || !params->model || !params->ctx)
    {
        return MESHX_INVALID_ARG;
    }
    params->ctx->opcode = MESHX_MODEL_OP_LIGHT_CTL_STATUS;
    meshx_lighting_server_state_change_t state_change = {
        .ctl_set = {
            .lightness = params->lightness,
            .temperature = params->temperature,
            .delta_uv = params->delta_uv
        }
    };
    meshx_light_server_send_params_t send_params = {
        .p_model = params->model,
        .p_ctx = params->ctx,
        .state_change = &state_change
    };
    return this->get_base_model()->plat_send_msg(&send_params);
}

/**
 * @brief This function is the callback for the Light CTL Server model.
 *
 * This function is triggered whenever a Light CTL message is received from the MeshX stack.
 * It handles the Light CTL state change notifications from the MeshX stack and publishes
 * the state change event to the element layer.
 *
 * @param[in] p_dev       A pointer to the device structure.
 * @param[in] model_id    The unique identifier of the BLE mesh model.
 * @param[in] params      A pointer to the callback parameter structure containing the details of the
 *                        received message.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_INVALID_ARG: Invalid argument
 *    - MESHX_FAIL: Other failures
 */
meshx_err_t meshXLightCTLServerModel MESHX_LIGHT_CTL_SERVER_MODEL_TEMPLATE_PARAMS
    :: model_from_ble_cb(
        dev_struct_t *p_dev,
        control_task_msg_evt_t model_id,
        meshx_ptr_t params)
{
    if(!params || !p_dev)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid parameters");
        return MESHX_INVALID_ARG;
    }
    if(model_id != MESHX_MODEL_ID_LIGHT_CTL_SRV)
    {
        /* Callback triggered not for this model */
        return MESHX_SUCCESS;
    }

    auto *param = static_cast<meshx_lighting_server_cb_param_t *>(params);

    MESHX_LOGD(MODULE_ID_MODEL_SERVER, "op|src|dst:%04" PRIx32 "|%04x|%04x",
               param->ctx.opcode, param->ctx.src_addr, param->ctx.dst_addr);

    meshx_light_ctl_srv_el_msg_t srv_ctl_param = {
        .model = param->model,
        .lightness = param->state_change.ctl_set.lightness,
        .temperature = param->state_change.ctl_set.temperature,
        .delta_uv = param->state_change.ctl_set.delta_uv
    };
    bool send_reply = (param->ctx.opcode != MESHX_MODEL_OP_LIGHT_CTL_SET_UNACK);
    switch (param->ctx.opcode)
    {
        case MESHX_MODEL_OP_LIGHT_CTL_GET:
            break;
        case MESHX_MODEL_OP_LIGHT_CTL_SET:
        case MESHX_MODEL_OP_LIGHT_CTL_SET_UNACK:
        {
            if (MESHX_ADDR_IS_UNICAST(param->ctx.dst_addr)
            || (MESHX_ADDR_BROADCAST(param->ctx.dst_addr))
            || (MESHX_ADDR_IS_GROUP(param->ctx.dst_addr)
            && (MESHX_SUCCESS == meshx_is_group_subscribed(&param->model, param->ctx.dst_addr))))
            {
                if (this->get_parent_element())
                {
                    return this->get_parent_element()->on_model_cb(&srv_ctl_param);
                }
                else
                {
                    MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Parent element is null");
                }
            }
            break;
        }
        case MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_GET:
            break;
        case MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_SET:
        case MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_SET_UNACK:
            // Handle temperature range operations
            break;
        default:
            break;
    }
    if (send_reply
        /* This is meant to notify the respective publish client */
        || param->ctx.src_addr != param->model.pub_addr)
    {
        /* Here the message was received from unregistered source and mention the state to the respective client */
        MESHX_LOGD(MODULE_ID_MODEL_SERVER, "PUB: src|pub %x|%x", param->ctx.src_addr, param->model.pub_addr);
        param->ctx.dst_addr = param->model.pub_addr;

        // Create a parameter structure for sending the response
        meshx_light_ctl_send_params_t send_params = {
            .model = &param->model,
            .ctx = &param->ctx,
            .lightness = param->state_change.ctl_set.lightness,
            .temperature = param->state_change.ctl_set.temperature,
            .delta_uv = param->state_change.ctl_set.delta_uv,
            .tid = 0  // TID not used in server response
        };

        return this->model_send(&send_params);
    }
    return MESHX_SUCCESS;
}
#endif /* CONFIG_ENABLE_LIGHT_CTL_SERVER */
