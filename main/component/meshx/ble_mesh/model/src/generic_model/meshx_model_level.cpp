/**
 * @file meshx_model_level.cpp
 * @brief Implementation of Generic Level Model classes for MeshX.
 *        This file contains the implementation of the Generic Level Server and Client models
 *        for the MeshX BLE mesh framework.
 *
 * Key Features:
 *  - Implements Bluetooth SIG-defined Generic Level model
 *  - Inherits from meshXServerModel and meshXClientModel templates
 *  - Provides standard Level control operations (SET, GET, DELTA, MOVE)
 *  - Integrates with MeshX transmission control
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#include <generic_model/meshx_model_level.hpp>

#if CONFIG_ENABLE_GEN_LEVEL_CLIENT
/**
 * @brief Handle Generic Level state change notifications from the MeshX stack.
 *
 * This function is responsible for handling Generic Level state change notifications
 * from the MeshX stack. It is called when the MeshX stack receives a state
 * change event from the Generic Level server model. The function publishes the
 * state change event to the control task framework, which in turn notifies the
 * application about the state change.
 *
 * @param[in] param  Pointer to the Generic Level client callback parameter structure.
 * @param[in] status Status of the state change event (success or timeout).
 *
 * @return
 *     - MESHX_SUCCESS: Successfully handled the state change notification.
 *     - MESHX_INVALID_ARG: One or more arguments are invalid.
 */
MESHX_GEN_LEVEL_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericLevelClientModel MESHX_GEN_LEVEL_CLIENT_MODEL_TEMPLATE_PARAMS
    :: meshx_state_change_notify(const meshx_gen_cli_cb_param_t *param, uint8_t status)
{
    if (!param){
        return MESHX_INVALID_ARG;
    }

    model_state.present_level   = param->status.level_status.present_level;
    model_state.target_level    = param->status.level_status.target_level;
    model_state.remaining_time  = param->status.level_status.remain_time;

    // Prepare the message (store in member variable)
    element_msg = {
        .header = {
            .err_code               = param->err_code,
            .model                  = param->model,
            .ctx                    = param->ctx,
            .element_state_change   = MESHX_SUCCESS,  // Will be set by base layer
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
 * @param[in] curr_el_state Pointer to meshx_gen_level_model_state_t containing the new state
 * @return
 *     - MESHX_SUCCESS: State change handled successfully
 *     - MESHX_INVALID_ARG: Invalid parameter
 */
MESHX_GEN_LEVEL_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericLevelClientModel MESHX_GEN_LEVEL_CLIENT_MODEL_TEMPLATE_PARAMS
    :: element_state_change_handle()
{
    meshx_gen_level_model_state_t *el_state =
        static_cast<meshx_gen_level_model_state_t*>(this->get_parent_element_state());
    if(!el_state)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid parameter in element_state_change_handle");
        return MESHX_INVALID_ARG;
    }
    if(memcmp(&model_state, el_state, sizeof(meshx_gen_level_model_state_t)) != 0)
    {
        MESHX_LOGI(MODULE_ID_MODEL_SERVER,
            "Level state change request: present=%d target=%d remaining=%d",
            el_state->present_level, el_state->target_level, el_state->remaining_time);
        model_state = *el_state;
    }
    else
    {
        return MESHX_INVALID_STATE;
    }
    return MESHX_SUCCESS;
}
/**
 * @brief Creates a meshXGenericLevelClientModel instance based on a BLE device
 *
 * This function is used to create a meshXGenericLevelClientModel instance based on a BLE device.
 * It takes the device structure, model ID, and parameters as input and returns a pointer to the created instance.
 *
 * @param[in] p_dev     Pointer to the device structure
 * @param[in] model_id  Model ID associated with the device
 * @param[in] params    Pointer to the parameters associated with the device
 *
 * @return Pointer to the created meshXGenericLevelClientModel instance
 */
MESHX_GEN_LEVEL_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericLevelClientModel MESHX_GEN_LEVEL_CLIENT_MODEL_TEMPLATE_PARAMS
    :: model_from_ble_cb(
        dev_struct_t *p_dev,
        evt_model_id_t model_id,
        meshx_ptr_t params
)
{
    if(!params || !p_dev)
    {
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Invalid parameters");
        return MESHX_INVALID_ARG;
    }
    if(model_id != MESHX_MODEL_ID_GEN_LEVEL_CLI)
    {
        /* Callback triggered not for this model */
        return MESHX_SUCCESS;
    }
    const auto *param = static_cast<const meshx_gen_cli_cb_param_t *>(params);

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
MESHX_GEN_LEVEL_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericLevelClientModel MESHX_GEN_LEVEL_CLIENT_MODEL_TEMPLATE_PARAMS
    :: prepare_element_msg(meshx_ptr_t *msg_ptr, size_t *msg_size)
{
    if (!msg_ptr || !msg_size)
    {
        return MESHX_INVALID_ARG;
    }

    *msg_ptr  = &element_msg;
    *msg_size = sizeof(element_msg);

    return MESHX_SUCCESS;
}

/**
 * @brief Send a packet to the MeshX stack based on the given parameters
 *
 * This function takes a pointer to a meshx_gen_level_send_params_t structure as input
 * and sends the corresponding packet to the MeshX stack for processing.
 *
 * @param[in] params Pointer to the meshx_gen_level_send_params_t structure containing the parameters
 * @return MESHX_SUCCESS on success, or an error code on failure
 */
MESHX_GEN_LEVEL_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericLevelClientModel MESHX_GEN_LEVEL_CLIENT_MODEL_TEMPLATE_PARAMS
    :: model_send(meshx_gen_level_send_params_t *params)
{
    meshx_err_t err;
    meshx_gen_cli_set_t set;
    if (!params || !params->model || !params->model->p_model)
    {
        return MESHX_INVALID_ARG;
    }

    meshx_gen_client_send_params_t send_params;

    send_params.state   = &set;
    send_params.opcode  = static_cast<uint16_t>(params->ctx->opcode);
    send_params.net_idx = params->ctx->net_idx;
    send_params.app_idx = params->ctx->app_idx;
    send_params.addr    = params->model->pub_addr;
    send_params.model   = params->model->p_model;

    if (params->ctx->opcode == MESHX_MODEL_OP_GEN_LEVEL_GET)
    {
        err = this->get_base_model()->plat_send_msg(&send_params);
    }

    else if (params->ctx->opcode == MESHX_MODEL_OP_GEN_LEVEL_SET ||
             params->ctx->opcode == MESHX_MODEL_OP_GEN_LEVEL_SET_UNACK)
    {
        set.level_set.level      = params->level;
        set.level_set.tid        = params->tid;
        set.level_set.trans_time = params->transition_time;
        set.level_set.delay      = params->delay;

        err = this->get_base_model()->plat_send_msg(&send_params);
    }
    else if (params->ctx->opcode == MESHX_MODEL_OP_GEN_DELTA_SET ||
             params->ctx->opcode == MESHX_MODEL_OP_GEN_DELTA_SET_UNACK)
    {
        set.delta_set.level      = params->level; // Using level field for delta value
        set.delta_set.tid        = params->tid;
        set.delta_set.trans_time = params->transition_time;
        set.delta_set.delay      = params->delay;

        err = this->get_base_model()->plat_send_msg(&send_params);
    }
    else if (params->ctx->opcode == MESHX_MODEL_OP_GEN_MOVE_SET ||
             params->ctx->opcode == MESHX_MODEL_OP_GEN_MOVE_SET_UNACK)
    {
        set.move_set.delta_level = params->level; // Using level field for move delta
        set.move_set.tid         = params->tid;
        set.move_set.trans_time  = params->transition_time;
        set.move_set.delay       = params->delay;

        err = this->get_base_model()->plat_send_msg(&send_params);
    }
    else
    {
        err = MESHX_INVALID_ARG; // Invalid opcode
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Invalid opcode for Generic Level Client: %04x", params->ctx->opcode);
    }
    return err;
}
/**
 * @brief A template class for creating Generic Level Client models.
 *
 * This class is derived from meshXClientModel and provides a convenient interface for
 * creating Generic Level Client models. It handles the Generic Level state change
 * notifications from the MeshX stack and publishes the state change event to the
 * element layer.
 *
 * @tparam meshxBaseClientModel_t The type of the meshXBaseClientModel class to be used.
 * @tparam meshx_send_packet_params_t The type of the meshXSendPacketParams structure used
 * for sending packets.
 *
 * @param[in] parent_element A pointer to the parent element (meshXElementIF).
 */
MESHX_GEN_LEVEL_CLIENT_MODEL_TEMPLATE_PROTO
meshXGenericLevelClientModel MESHX_GEN_LEVEL_CLIENT_MODEL_TEMPLATE_PARAMS
    ::meshXGenericLevelClientModel(
        meshXElementIF *parent_element,
        meshx_ptr_t     parent_element_state,
        uint16_t        model_func_id)
    : meshXClientModel(nullptr, MESHX_MODEL_ID_GEN_LEVEL_CLI, parent_element, parent_element_state, model_func_id)
{
    /* Used only for initialization of Parent Class */
}

#endif /* CONFIG_ENABLE_GEN_LEVEL_CLIENT */
/*******************************************************************************************************************/
#if CONFIG_ENABLE_GEN_LEVEL_SERVER
/**
 * @brief Send a packet to the MeshX stack based on the given parameters
 *
 * This function takes a pointer to a meshx_gen_level_send_params_t structure as input
 * and sends the corresponding packet to the MeshX stack for processing.
 *
 * @param[in] params Pointer to the meshx_gen_level_send_params_t structure containing the parameters
 * @return MESHX_SUCCESS on success, or an error code on failure
 */
MESHX_GEN_LEVEL_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericLevelServerModel MESHX_GEN_LEVEL_SERVER_MODEL_TEMPLATE_PARAMS
    :: model_send(meshx_gen_level_send_params_t *params)
{
if (!params|| !params->model || !params->ctx)
    {
        return MESHX_INVALID_ARG;
    }
    params->ctx->opcode = MESHX_MODEL_OP_GEN_LEVEL_STATUS;
    meshx_gen_srv_state_change_t state_change = {
        .level_set = {
            .level = params->level
        }
    };
    meshx_gen_server_send_params_t send_params = {
        .p_model        = params->model,
        .p_ctx          = params->ctx,
        .state_change   = state_change,
        .data_len       = sizeof(meshx_state_change_gen_level_set_t)
    };
    return this->get_base_model()->plat_send_msg(&send_params);
}

/**
 * @brief Callback function for handling BLE mesh events for Generic Level Server Model
 *
 * @param[in] p_dev     Pointer to the device structure
 * @param[in] model_id  Model ID associated with the event
 * @param[in] params    Pointer to the event parameters
 * @return MESHX_SUCCESS if successful, error code otherwise
 */
MESHX_GEN_LEVEL_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericLevelServerModel MESHX_GEN_LEVEL_SERVER_MODEL_TEMPLATE_PARAMS
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
    if(model_id != MESHX_MODEL_ID_GEN_LEVEL_SRV)
    {
        /* Callback triggered not for this model */
        return MESHX_SUCCESS;
    }

    auto *param = static_cast<meshx_gen_srv_cb_param_t *>(params);

    model_state.present_level = param->state_change.level_set.level;

    // Initialize flag - message not prepared yet
    element_msg_prepared = false;

    // Prepare the message (store in member variable)
    element_msg = {
        .header = {
            .model = param->model,
            .element_state_change = MESHX_SUCCESS,  // Will be set by base layer
        },
        .state = model_state,
    };
    element_msg_prepared = true;

    return MESHX_SUCCESS;
}

/**
 * @brief Prepare message for element notification
 * @details Returns pointer to the stored element message if it has been prepared
 *
 * @param[out] msg_ptr   Pointer to message structure (output parameter)
 * @param[out] msg_size  Size of the message structure (output parameter)
 * @return MESHX_SUCCESS if message prepared successfully, error code otherwise
 */
MESHX_GEN_LEVEL_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericLevelServerModel MESHX_GEN_LEVEL_SERVER_MODEL_TEMPLATE_PARAMS
    :: prepare_element_msg(meshx_ptr_t *msg_ptr, size_t *msg_size)
{
    if (!msg_ptr || !msg_size)
    {
        return MESHX_INVALID_ARG;
    }

    if (!element_msg_prepared)
    {
        // Message was not prepared, return error
        return MESHX_NOT_SUPPORTED;
    }

    *msg_ptr  = &element_msg;
    *msg_size = sizeof(element_msg);

    return MESHX_SUCCESS;
}

/**
 * @brief Constructor for Generic Level Server Model
 *
 * @param[in] parent_element Pointer to the parent element (meshXElementIF)
 */
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
MESHX_GEN_LEVEL_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericLevelServerModel MESHX_GEN_LEVEL_SERVER_MODEL_TEMPLATE_PARAMS
    :: element_state_change_handle(void)
{
    auto *el_state =
        static_cast<meshx_gen_level_model_state_t*>(this->get_parent_element_state());

    if (!el_state) {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid parameter in element_state_change_handle");
        return MESHX_INVALID_ARG;
    }

    if(memcmp(&model_state, el_state, sizeof(meshx_gen_level_model_state_t)) != 0)
    {
        MESHX_LOGI(MODULE_ID_MODEL_SERVER,
            "Level state change request: present=%d target=%d remaining=%d",
            el_state->present_level, el_state->target_level, el_state->remaining_time);
        // Update the model state
        *el_state = model_state;
    }
    else
    {
        return MESHX_INVALID_STATE;
    }
    return MESHX_SUCCESS;
}

MESHX_GEN_LEVEL_SERVER_MODEL_TEMPLATE_PROTO
meshXGenericLevelServerModel MESHX_GEN_LEVEL_SERVER_MODEL_TEMPLATE_PARAMS
    ::meshXGenericLevelServerModel(
        meshXElementIF *parent_element,
        meshx_ptr_t     parent_element_state,
        uint16_t        model_func_id
    )
    : meshXServerModel(nullptr, MESHX_MODEL_ID_GEN_LEVEL_SRV, parent_element, parent_element_state, model_func_id)
{
    this->plat_model_create();
}

/**
 * @brief Creates and initializes a server model instance for Generic Level Server.
 *
 * This function handles the platform-specific model creation process for Generic Level Server models.
 * It initializes server-specific features and cannot be overridden by derived classes.
 *
 * @return meshx_err_t Returns an error code indicating the result of the operation.
 *         - MESHX_SUCCESS on successful model creation and initialization
 *         - MESHX_ERR_NO_MEM if memory allocation fails
 *         - Other error codes for platform-specific failures
 *
 * @note This is a final function and cannot be overridden by derived classes.
 */
MESHX_GEN_LEVEL_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericLevelServerModel MESHX_GEN_LEVEL_SERVER_MODEL_TEMPLATE_PARAMS
    :: plat_model_create(void)
{
    meshx_ptr_t p_pub = this->get_pub_struct();
    meshx_ptr_t p_gen = this->get_gen_struct();
    meshx_err_t err = MESHX_SUCCESS;

    err = meshx_plat_level_gen_srv_create(this->get_plat_model(), &p_pub, &p_gen);
    if(err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to create Generic Level Server Model");
    }
    else
    {
        this->set_pub_struct(p_pub);
        this->set_gen_struct(p_gen);
    }
    return err;
}

/**
 * @brief Deletes the Generic Level Server model and its associated resources.
 *
 * This function frees the memory allocated for the Generic Level Server
 * and sets the pointer to NULL. It also deletes the model publication
 * resources associated with the server.
 *
 * @return
 *     - MESHX_SUCCESS: Model and publication deleted successfully.
 *     - MESHX_FAIL: Failed to delete the model or publication.
 */
MESHX_GEN_LEVEL_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericLevelServerModel MESHX_GEN_LEVEL_SERVER_MODEL_TEMPLATE_PARAMS
    :: plat_model_delete(void)
{
    meshx_ptr_t p_pub = this->get_pub_struct();
    meshx_ptr_t p_gen = this->get_gen_struct();

    meshx_err_t err = meshx_plat_gen_srv_delete(&p_pub, &p_gen);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to delete Generic Level Server Model");
    }
    else
    {
        this->set_pub_struct(nullptr);
        this->set_gen_struct(nullptr);
    }
    return err;
}

#endif /* CONFIG_ENABLE_GEN_LEVEL_SERVER */
