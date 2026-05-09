/**
 * @file meshx_model_lightness.cpp
 * @brief Implementation of Light Lightness Model classes for MeshX.
 *        This file contains the implementation of the Light Lightness Server and Client models
 *        for the MeshX BLE mesh framework.
 *
 * Key Features:
 *  - Implements Bluetooth SIG-defined Light Lightness model
 *  - Inherits from meshXServerModel and meshXClientModel templates
 *  - Provides standard Light Lightness control operations
 *  - Integrates with MeshX transmission control
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#include <light_model/meshx_model_lightness.hpp>

#if CONFIG_ENABLE_LIGHT_LIGHTNESS_CLIENT
/**
 * @brief Handle Light Lightness state change notifications from the MeshX stack.
 *
 * This function is responsible for handling Light Lightness state change notifications
 * from the MeshX stack and publishing the state change event to the element layer.
 *
 * @param[in] param  Pointer to the Light Lightness client callback parameter structure.
 * @param[in] status Status of the state change event (success or timeout).
 *
 * @return
 *     - MESHX_SUCCESS: Successfully handled the state change notification.
 *     - MESHX_INVALID_ARG: One or more arguments are invalid.
 */
MESHX_LIGHT_LIGHTNESS_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXLightLightnessClientModel MESHX_LIGHT_LIGHTNESS_CLIENT_MODEL_TEMPLATE_PARAMS
    :: meshx_state_change_notify(const meshx_gen_light_cli_cb_param_t *param, uint8_t status)
{
    if (!param){
        return MESHX_INVALID_ARG;
    }

    model_state.present_lightness = param->status.lightness_status.present_lightness;
    model_state.target_lightness = param->status.lightness_status.target_lightness;
    model_state.lightness_last = 0;
    model_state.lightness_default = 0;
    model_state.range_min = 0;
    model_state.range_max = 0;
    model_state.status_code = 0;

    // Store the message in member variable for later use by prepare_element_msg
    element_msg = {
        .header = {
            .err_code               = param->err_code,
            .model                  = param->model,
            .ctx                    = param->ctx,
            .element_state_change   = MESHX_SUCCESS,
        },
        .state                  = model_state,
    };

    return MESHX_SUCCESS;
}

/**
 * @brief Handle state change request from element.
 *
 * This function is called by the parent element when a state change request
 * is received. It validates the request and returns a result to the element.
 * Note: The actual state is maintained in the element layer, not the model layer.
 *
 * @param[in] curr_el_state Pointer to meshx_light_lightness_model_state_t containing the new state
 * @return
 *     - MESHX_SUCCESS: State change handled successfully
 *     - MESHX_INVALID_ARG: Invalid parameter
 */
MESHX_LIGHT_LIGHTNESS_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXLightLightnessClientModel MESHX_LIGHT_LIGHTNESS_CLIENT_MODEL_TEMPLATE_PARAMS
    :: element_state_change_handle()
{
    meshx_light_lightness_model_state_t *el_state =
        static_cast<meshx_light_lightness_model_state_t*>(this->get_parent_element_state());
    if(!el_state)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid parameter in element_state_change_handle");
        return MESHX_INVALID_ARG;
    }
    if(memcmp(&model_state, el_state, sizeof(meshx_light_lightness_model_state_t)) != 0)
    {
        MESHX_LOGI(MODULE_ID_MODEL_SERVER,
            "Lightness state change request: present=%d target=%d last=%d default=%d range_min=%d range_max=%d",
            el_state->present_lightness, el_state->target_lightness, el_state->lightness_last,
            el_state->lightness_default, el_state->range_min, el_state->range_max);
        model_state = *el_state;
    }
    else
    {
        return MESHX_INVALID_STATE;
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Creates a meshXLightLightnessClientModel instance based on a BLE device
 *
 * This function is used to create a meshXLightLightnessClientModel instance based on a BLE device.
 *
 * @return Pointer to the created meshXLightLightnessClientModel instance
 */
MESHX_LIGHT_LIGHTNESS_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXLightLightnessClientModel MESHX_LIGHT_LIGHTNESS_CLIENT_MODEL_TEMPLATE_PARAMS
    :: model_from_ble_cb(
        dev_struct_t *p_dev,
        evt_model_id_t model_id,
        meshx_ptr_t params)
{
    if(!params || !p_dev)
    {
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Invalid parameters");
        return MESHX_INVALID_ARG;
    }
    if(model_id != MESHX_MODEL_ID_LIGHT_LIGHTNESS_CLI)
    {
        /* Callback triggered not for this model */
        return MESHX_SUCCESS;
    }
    const auto *param = static_cast<const meshx_gen_light_cli_cb_param_t *>(params);

    return static_cast<int>(param->evt) == static_cast<int>(meshx_base_cli_evt::MESHX_BASE_CLI_TIMEOUT) ?
        meshx_state_change_notify(param, MESHX_TIMEOUT) :
        meshx_state_change_notify(param, MESHX_SUCCESS);
}

/**
 * @brief Prepare message for element notification
 * @details Returns pointer to the stored element message
 *
 * @param[out] msg_ptr   Pointer to message structure (output parameter)
 * @param[out] msg_size  Size of the message structure (output parameter)
 * @return MESHX_SUCCESS if message prepared successfully, error code otherwise
 */
MESHX_LIGHT_LIGHTNESS_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXLightLightnessClientModel MESHX_LIGHT_LIGHTNESS_CLIENT_MODEL_TEMPLATE_PARAMS
    :: prepare_element_msg(meshx_ptr_t *msg_ptr, size_t *msg_size)
{
    if (!msg_ptr || !msg_size)
    {
        return MESHX_INVALID_ARG;
    }

    *msg_ptr = &element_msg;
    *msg_size = sizeof(element_msg);

    return MESHX_SUCCESS;
}

/**
 * @brief Send a packet to the MeshX stack based on the given parameters
 *
 * This function takes a pointer to a meshx_light_lightness_send_params_t structure as input
 * and sends the corresponding packet to the MeshX stack for processing.
 *
 * @param[in] params Pointer to the meshx_light_lightness_send_params_t structure containing the parameters
 * @return MESHX_SUCCESS on success, or an error code on failure
 */
MESHX_LIGHT_LIGHTNESS_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXLightLightnessClientModel MESHX_LIGHT_LIGHTNESS_CLIENT_MODEL_TEMPLATE_PARAMS
    :: model_send(meshx_light_lightness_send_params_t *params)
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

    if (params->ctx->opcode == MESHX_MODEL_OP_LIGHT_LIGHTNESS_GET ||
        params->ctx->opcode == MESHX_MODEL_OP_LIGHT_LIGHTNESS_LINEAR_GET ||
        params->ctx->opcode == MESHX_MODEL_OP_LIGHT_LIGHTNESS_LAST_GET ||
        params->ctx->opcode == MESHX_MODEL_OP_LIGHT_LIGHTNESS_DEFAULT_GET ||
        params->ctx->opcode == MESHX_MODEL_OP_LIGHT_LIGHTNESS_RANGE_GET)
    {
        err = this->get_base_model()->plat_send_msg(&send_params);
    }
    else if (params->ctx->opcode == MESHX_MODEL_OP_LIGHT_LIGHTNESS_SET ||
             params->ctx->opcode == MESHX_MODEL_OP_LIGHT_LIGHTNESS_SET_UNACK)
    {
        set.lightness_set.lightness = params->lightness;
        set.lightness_set.tid = params->tid;
        set.lightness_set.op_en = false;

        err = this->get_base_model()->plat_send_msg(&send_params);
    }
    else if (params->ctx->opcode == MESHX_MODEL_OP_LIGHT_LIGHTNESS_LINEAR_SET ||
             params->ctx->opcode == MESHX_MODEL_OP_LIGHT_LIGHTNESS_LINEAR_SET_UNACK)
    {
        set.lightness_linear_set.lightness = params->lightness;
        set.lightness_linear_set.tid = params->tid;
        set.lightness_linear_set.op_en = false;

        err = this->get_base_model()->plat_send_msg(&send_params);
    }
    else if (params->ctx->opcode == MESHX_MODEL_OP_LIGHT_LIGHTNESS_DEFAULT_SET ||
             params->ctx->opcode == MESHX_MODEL_OP_LIGHT_LIGHTNESS_DEFAULT_SET_UNACK)
    {
        set.lightness_default_set.lightness = params->lightness;

        err = this->get_base_model()->plat_send_msg(&send_params);
    }
    else if (params->ctx->opcode == MESHX_MODEL_OP_LIGHT_LIGHTNESS_RANGE_SET ||
             params->ctx->opcode == MESHX_MODEL_OP_LIGHT_LIGHTNESS_RANGE_SET_UNACK)
    {
        set.lightness_range_set.range_min = params->range_min;
        set.lightness_range_set.range_max = params->range_max;

        err = this->get_base_model()->plat_send_msg(&send_params);
    }
    else
    {
        err = MESHX_INVALID_ARG;
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Invalid opcode for Light Lightness Client: %04x", params->ctx->opcode);
    }
    return err;
}

MESHX_LIGHT_LIGHTNESS_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXLightLightnessClientModel MESHX_LIGHT_LIGHTNESS_CLIENT_MODEL_TEMPLATE_PARAMS
    :: request_lightness(uint16_t lightness, uint8_t tid)
{
    auto *el = this->get_parent_element();
    if (!el) return MESHX_INVALID_STATE;

    auto *common_ctx = static_cast<meshx_element_common_ctx_t*>(el->get_element_ctx());
    if (!common_ctx) return MESHX_INVALID_STATE;

    meshx_model_t model_ref = {
        .el_id    = el->get_element_idx(),
        .model_id = (uint16_t)this->get_model_id(),
        .pub_addr = common_ctx->pub_addr,
        .p_model  = (MESHX_MODEL*)this->get_plat_model()
    };

    meshx_ctx_t ctx = {
        .app_idx  = common_ctx->app_id,
        .net_idx  = meshx_get_net_key_id(),
        .opcode   = MESHX_MODEL_OP_LIGHT_LIGHTNESS_SET,
        .src_addr = 0,
        .dst_addr = common_ctx->pub_addr,
        .p_ctx    = nullptr
    };

    meshx_light_lightness_send_params_t sp = {
        .model = &model_ref,
        .ctx   = &ctx,
        .tid   = tid,
        .state = {
            .present_lightness = lightness
        }
    };

    return this->model_send(&sp);
}

/**
 * @brief A template class for creating Light Lightness Client models.
 *
 * This class is derived from meshXClientModel and provides a convenient interface for
 * creating Light Lightness Client models. It handles the Light Lightness state change
 * notifications from the MeshX stack and publishes the state change event to the
 * element layer.
 *
 * @param[in] parent_element A pointer to the parent element (meshXElementIF).
 */
MESHX_LIGHT_LIGHTNESS_CLIENT_MODEL_TEMPLATE_PROTO
meshXLightLightnessClientModel MESHX_LIGHT_LIGHTNESS_CLIENT_MODEL_TEMPLATE_PARAMS
    ::meshXLightLightnessClientModel(
        meshXElementIF *parent_element,
        meshx_ptr_t     parent_element_state,
        uint16_t        model_func_id)
    : meshXClientModel(nullptr, MESHX_MODEL_ID_LIGHT_LIGHTNESS_CLI, parent_element, parent_element_state, model_func_id)
{
    /* Used only for initialization of Parent Class */
}
#endif /* CONFIG_ENABLE_LIGHT_LIGHTNESS_CLIENT */

#if CONFIG_ENABLE_LIGHT_LIGHTNESS_SERVER

/**
 * @brief Creates and initializes a server model instance.
 *
 * This function handles the platform-specific model creation process for server models.
 * It initializes server-specific features and cannot be overridden by derived classes.
 *
 * @param[in] p_plat_model_ptr Optional pointer to a pre-allocated platform model structure.
 *
 * @return meshx_err_t Returns an error code indicating the result of the operation.
 *         - MESHX_SUCCESS on successful model creation and initialization
 *         - MESHX_ERR_NO_MEM if memory allocation fails
 *         - Other error codes for platform-specific failures
 *
 * @note This is a final function and cannot be overridden by derived classes.
 */
MESHX_LIGHT_LIGHTNESS_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXLightLightnessServerModel MESHX_LIGHT_LIGHTNESS_SERVER_MODEL_TEMPLATE_PARAMS
    :: plat_model_create(MESHX_MODEL* p_plat_model_ptr)
{
    if (p_plat_model_ptr) {
        this->set_plat_model(p_plat_model_ptr);
    }

    meshx_ptr_t p_pub = this->get_pub_struct();
    meshx_ptr_t p_gen = this->get_gen_struct();
    meshx_err_t err = MESHX_SUCCESS;

    err = meshx_plat_light_lightness_srv_create( this->get_plat_model(), &p_pub, &p_gen );
    if(err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to create Light Lightness Server Model");
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
 * @brief Deletes the Light Lightness Server model and its associated resources.
 *
 * This function frees the memory allocated for the Light Lightness Server
 * and sets the pointer to NULL. It also deletes the model publication
 * resources associated with the server.
 *
 * @return
 *     - MESHX_SUCCESS: Model and publication deleted successfully.
 *     - MESHX_FAIL: Failed to delete the model or publication.
 */
MESHX_LIGHT_LIGHTNESS_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXLightLightnessServerModel MESHX_LIGHT_LIGHTNESS_SERVER_MODEL_TEMPLATE_PARAMS
    :: plat_model_delete(void)
{
    meshx_ptr_t p_pub = this->get_pub_struct();
    meshx_ptr_t p_gen = this->get_gen_struct();

    meshx_err_t err = meshx_plat_light_srv_delete( &p_pub, &p_gen );
    if (err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to delete Light Lightness Server Model");
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
 * This function takes a pointer to a meshx_light_lightness_send_params_t structure as input
 * and sends the corresponding packet to the MeshX stack for processing.
 *
 * @param[in] params Pointer to the meshx_light_lightness_send_params_t structure containing the parameters
 * @return MESHX_SUCCESS on success, or an error code on failure
 */
MESHX_LIGHT_LIGHTNESS_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXLightLightnessServerModel MESHX_LIGHT_LIGHTNESS_SERVER_MODEL_TEMPLATE_PARAMS
    :: model_send(meshx_light_lightness_send_params_t *params)
{
    if (!params || !params->model || !params->ctx)
    {
        return MESHX_INVALID_ARG;
    }
    params->ctx->opcode = MESHX_MODEL_OP_LIGHT_LIGHTNESS_STATUS;
    meshx_lighting_server_state_change_t state_change = {
        .lightness_set = {
            .lightness = params->lightness
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
 * @brief This function is the callback for the Light Lightness Server model.
 *
 * This function is triggered whenever a Light Lightness message is received from the MeshX stack.
 * It handles the Light Lightness state change notifications from the MeshX stack and publishes
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
meshx_err_t meshXLightLightnessServerModel MESHX_LIGHT_LIGHTNESS_SERVER_MODEL_TEMPLATE_PARAMS
    :: model_from_ble_cb(
        dev_struct_t *p_dev,
        evt_model_id_t model_id,
        meshx_ptr_t params
)
{
    if(!params || !p_dev)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid parameters");
        return MESHX_INVALID_ARG;
    }
    if(model_id != MESHX_MODEL_ID_LIGHT_LIGHTNESS_SRV)
    {
        /* Callback triggered not for this model */
        return MESHX_SUCCESS;
    }

    auto *param = static_cast<meshx_lighting_server_cb_param_t *>(params);

    MESHX_LOGD(MODULE_ID_MODEL_SERVER, "op|src|dst:%04" PRIx32 "|%04x|%04x",
               param->ctx.opcode, param->ctx.src_addr, param->ctx.dst_addr);

    meshx_light_lightness_srv_el_msg_t srv_lightness_param = {
        .model = param->model,
        .lightness = param->state_change.lightness_set.lightness
    };
    bool send_reply = (param->ctx.opcode != MESHX_MODEL_OP_LIGHT_LIGHTNESS_SET_UNACK &&
                       param->ctx.opcode != MESHX_MODEL_OP_LIGHT_LIGHTNESS_LINEAR_SET_UNACK);

    switch (param->ctx.opcode)
    {
        case MESHX_MODEL_OP_LIGHT_LIGHTNESS_GET:
        case MESHX_MODEL_OP_LIGHT_LIGHTNESS_LINEAR_GET:
        case MESHX_MODEL_OP_LIGHT_LIGHTNESS_LAST_GET:
        case MESHX_MODEL_OP_LIGHT_LIGHTNESS_DEFAULT_GET:
        case MESHX_MODEL_OP_LIGHT_LIGHTNESS_RANGE_GET:
            break;
        case MESHX_MODEL_OP_LIGHT_LIGHTNESS_SET:
        case MESHX_MODEL_OP_LIGHT_LIGHTNESS_SET_UNACK:
        case MESHX_MODEL_OP_LIGHT_LIGHTNESS_LINEAR_SET:
        case MESHX_MODEL_OP_LIGHT_LIGHTNESS_LINEAR_SET_UNACK:
        {
            if (MESHX_ADDR_IS_UNICAST(param->ctx.dst_addr)
            || (MESHX_ADDR_BROADCAST(param->ctx.dst_addr))
            || (MESHX_ADDR_IS_GROUP(param->ctx.dst_addr)
            && (MESHX_SUCCESS == meshx_is_group_subscribed(&param->model, param->ctx.dst_addr))))
            {
                if (this->get_parent_element())
                {
                    return this->get_parent_element()->on_model_cb(&srv_lightness_param);
                }
                else
                {
                    MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Parent element is null");
                }
            }
            break;
        }
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
        meshx_light_lightness_send_params_t send_params = {
            .model = &param->model,
            .ctx = &param->ctx,
            .lightness = param->state_change.lightness_set.lightness,
            .range_min = 0,
            .range_max = 0,
            .tid = 0
        };

        return this->model_send(&send_params);
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Handle state change request from element.
 *
 * This function is called by the parent element when a state change request
 * is received. It validates the request and returns a result to the element.
 * Note: The actual state is maintained in the element layer, not the model layer.
 *
 * @return
 *     - MESHX_SUCCESS: State change handled successfully
 *     - MESHX_INVALID_ARG: Invalid parameter
 *     - MESHX_INVALID_STATE: No state change detected
 */
MESHX_LIGHT_LIGHTNESS_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXLightLightnessServerModel MESHX_LIGHT_LIGHTNESS_SERVER_MODEL_TEMPLATE_PARAMS
    :: element_state_change_handle(void)
{
    auto *el_state =
        static_cast<meshx_light_lightness_model_state_t*>(this->get_parent_element_state());

    if (!el_state) {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid parameter in element_state_change_handle");
        return MESHX_INVALID_ARG;
    }

    if(memcmp(&model_state, el_state, sizeof(meshx_light_lightness_model_state_t)) != 0)
    {
        MESHX_LOGI(MODULE_ID_MODEL_SERVER,
            "Lightness state change request: present=%d target=%d last=%d default=%d range_min=%d range_max=%d",
            el_state->present_lightness, el_state->target_lightness, el_state->lightness_last,
            el_state->lightness_default, el_state->range_min, el_state->range_max);
        // Update the model state
        *el_state = model_state;
    }
    else
    {
        return MESHX_INVALID_STATE;
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Constructor for Light Lightness Server Model
 *
 * @param[in] parent_element Pointer to the parent element (meshXElementIF)
 */
MESHX_LIGHT_LIGHTNESS_SERVER_MODEL_TEMPLATE_PROTO
meshXLightLightnessServerModel MESHX_LIGHT_LIGHTNESS_SERVER_MODEL_TEMPLATE_PARAMS
    ::meshXLightLightnessServerModel(
        meshXElementIF *parent_element,
        meshx_ptr_t     parent_element_state,
        uint16_t        model_func_id
    )
    : meshXServerModel(nullptr, MESHX_MODEL_ID_LIGHT_LIGHTNESS_SRV, parent_element, parent_element_state, model_func_id)
{
    /* Used only for initialization of Parent Class */
}

MESHX_LIGHT_LIGHTNESS_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXLightLightnessServerModel MESHX_LIGHT_LIGHTNESS_SERVER_MODEL_TEMPLATE_PARAMS
    :: request_status(uint16_t dst_addr)
{
    auto *el_state = static_cast<meshx_light_lightness_model_state_t*>(this->get_parent_element_state());
    auto *el = this->get_parent_element();
    if (!el || !el_state) return MESHX_INVALID_STATE;

    auto *common_ctx = static_cast<meshx_element_common_ctx_t*>(el->get_element_ctx());
    if (!common_ctx) return MESHX_INVALID_STATE;

    meshx_model_t model_ref = {
        .el_id    = el->get_element_idx(),
        .model_id = (uint16_t)this->get_model_id(),
        .pub_addr = common_ctx->pub_addr,
        .p_model  = (MESHX_MODEL*)this->get_plat_model()
    };

    meshx_ctx_t ctx = {
        .app_idx  = common_ctx->app_id,
        .net_idx  = meshx_get_net_key_id(),
        .opcode   = MESHX_MODEL_OP_LIGHT_LIGHTNESS_STATUS,
        .src_addr = 0,
        .dst_addr = (dst_addr == 0) ? common_ctx->pub_addr : dst_addr,
        .p_ctx    = nullptr
    };

    meshx_light_lightness_send_params_t sp = {
        .model = &model_ref,
        .ctx   = &ctx,
        .tid   = 0,
        .state = *el_state
    };

    return this->model_send(&sp);
}

#endif /* CONFIG_ENABLE_LIGHT_LIGHTNESS_SERVER */

