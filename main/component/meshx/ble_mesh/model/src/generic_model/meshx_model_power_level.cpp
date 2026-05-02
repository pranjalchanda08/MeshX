/**
 * @file meshx_model_power_level.cpp
 * @brief Implementation of Generic Power Level Model classes for MeshX.
 *        This file contains the implementation of the Generic Power Level Server and Client models
 *        for the MeshX BLE mesh framework.
 *
 * Key Features:
 *  - Implements Bluetooth SIG-defined Generic Power Level model
 *  - Inherits from meshXServerModel and meshXClientModel templates
 *  - Provides standard Power Level control operations (GET, SET, LAST, DEFAULT, RANGE)
 *  - Integrates with MeshX transmission control
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#include <generic_model/meshx_model_power_level.hpp>

#if CONFIG_ENABLE_GEN_POWER_LEVEL_CLIENT
/**
 * @brief Handle Generic Power Level state change notifications from the MeshX stack.
 *
 * This function is responsible for handling Generic Power Level state change notifications
 * from the MeshX stack. It is called when the MeshX stack receives a state
 * change event from the Generic Power Level server model. The function publishes the
 * state change event to the control task framework, which in turn notifies the
 * application about the state change.
 *
 * @param[in] param  Pointer to the Generic Power Level client callback parameter structure.
 * @param[in] status Status of the state change event (success or timeout).
 *
 * @return
 *     - MESHX_SUCCESS: Successfully handled the state change notification.
 *     - MESHX_INVALID_ARG: One or more arguments are invalid.
 */
MESHX_GEN_POWER_LEVEL_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPowerLevelClientModel MESHX_GEN_POWER_LEVEL_CLIENT_MODEL_TEMPLATE_PARAMS
    :: meshx_state_change_notify(const meshx_gen_cli_cb_param_t *param, uint8_t status)
{
    if (!param){
        return MESHX_INVALID_ARG;
    }

    model_state.present_power = param->status.power_level_status.present_power;
    model_state.target_power  = param->status.power_level_status.target_power;
    model_state.remain_time   = param->status.power_level_status.remain_time;

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
 * @brief Prepare message for element notification
 * @details Returns pointer to the stored element message
 *
 * @param[out] msg_ptr   Pointer to message structure (output parameter)
 * @param[out] msg_size  Size of the message structure (output parameter)
 * @return MESHX_SUCCESS if message prepared successfully, error code otherwise
 */
MESHX_GEN_POWER_LEVEL_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPowerLevelClientModel MESHX_GEN_POWER_LEVEL_CLIENT_MODEL_TEMPLATE_PARAMS
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
 * @brief Handle state change request from element.
 *
 * This function is called by the parent element when a state change request
 * is received. It validates the request and returns a result to the element.
 * Note: The actual state is maintained in the element layer, not the model layer.
 *
 * @param[in] curr_el_state Pointer to meshx_gen_power_level_model_state_t containing the new state
 * @return
 *     - MESHX_SUCCESS: State change handled successfully
 *     - MESHX_INVALID_ARG: Invalid parameter
 */
MESHX_GEN_POWER_LEVEL_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPowerLevelClientModel MESHX_GEN_POWER_LEVEL_CLIENT_MODEL_TEMPLATE_PARAMS
    :: element_state_change_handle()
{
    meshx_gen_power_level_model_state_t *el_state =
        static_cast<meshx_gen_power_level_model_state_t*>(this->get_parent_element_state());
    if(!el_state)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid parameter in element_state_change_handle");
        return MESHX_INVALID_ARG;
    }
    if(memcmp(&model_state, el_state, sizeof(meshx_gen_power_level_model_state_t)) != 0)
    {
        MESHX_LOGI(MODULE_ID_MODEL_SERVER,
            "Power Level state change request: present=%d target=%d remaining=%d",
            el_state->present_power, el_state->target_power, el_state->remain_time);
        model_state = *el_state;
    }
    else
    {
        return MESHX_INVALID_STATE;
    }
    return MESHX_SUCCESS;
}
/**
 * @brief Creates a meshXGenericPowerLevelClientModel instance based on a BLE device
 *
 * This function is used to create a meshXGenericPowerLevelClientModel instance based on a BLE device.
 * It takes the device structure, model ID, and parameters as input and returns a pointer to the created instance.
 *
 * @param[in] p_dev     Pointer to the device structure
 * @param[in] model_id  Model ID associated with the device
 * @param[in] params    Pointer to the parameters associated with the device
 *
 * @return Pointer to the created meshXGenericPowerLevelClientModel instance
 */
MESHX_GEN_POWER_LEVEL_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPowerLevelClientModel MESHX_GEN_POWER_LEVEL_CLIENT_MODEL_TEMPLATE_PARAMS
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
    if(model_id != MESHX_MODEL_ID_GEN_POWER_LEVEL_CLI)
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
 * @brief Send a packet to the MeshX stack based on the given parameters
 *
 * This function takes a pointer to a meshx_gen_power_level_send_params_t structure as input
 * and sends the corresponding packet to the MeshX stack for processing.
 *
 * @param[in] params Pointer to the meshx_gen_power_level_send_params_t structure containing the parameters
 * @return MESHX_SUCCESS on success, or an error code on failure
 */
MESHX_GEN_POWER_LEVEL_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPowerLevelClientModel MESHX_GEN_POWER_LEVEL_CLIENT_MODEL_TEMPLATE_PARAMS
    :: model_send(meshx_gen_power_level_send_params_t *params)
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

    if (params->ctx->opcode == MESHX_MODEL_OP_GEN_POWER_LEVEL_GET)
    {
        err = this->get_base_model()->plat_send_msg(&send_params);
    }
    else if (params->ctx->opcode == MESHX_MODEL_OP_GEN_POWER_LAST_GET)
    {
        err = this->get_base_model()->plat_send_msg(&send_params);
    }
    else if (params->ctx->opcode == MESHX_MODEL_OP_GEN_POWER_DEFAULT_GET)
    {
        err = this->get_base_model()->plat_send_msg(&send_params);
    }
    else if (params->ctx->opcode == MESHX_MODEL_OP_GEN_POWER_RANGE_GET)
    {
        err = this->get_base_model()->plat_send_msg(&send_params);
    }

    else if (params->ctx->opcode == MESHX_MODEL_OP_GEN_POWER_LEVEL_SET ||
             params->ctx->opcode == MESHX_MODEL_OP_GEN_POWER_LEVEL_SET_UNACK)
    {
        set.power_level_set.power       = params->power_level;
        set.power_level_set.tid         = params->tid;
        set.power_level_set.trans_time  = params->transition_time;
        set.power_level_set.delay       = params->delay;

        err = this->get_base_model()->plat_send_msg(&send_params);
    }
    else if (params->ctx->opcode == MESHX_MODEL_OP_GEN_POWER_DEFAULT_SET ||
             params->ctx->opcode == MESHX_MODEL_OP_GEN_POWER_DEFAULT_SET_UNACK)
    {
        set.power_default_set.power = params->power_default;

        err = this->get_base_model()->plat_send_msg(&send_params);
    }
    else if (params->ctx->opcode == MESHX_MODEL_OP_GEN_POWER_RANGE_SET ||
             params->ctx->opcode == MESHX_MODEL_OP_GEN_POWER_RANGE_SET_UNACK)
    {
        set.power_range_set.range_min = params->power_range_min;
        set.power_range_set.range_max = params->power_range_max;

        err = this->get_base_model()->plat_send_msg(&send_params);
    }
    else
    {
        err = MESHX_INVALID_ARG; // Invalid opcode
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Invalid opcode for Generic Power Level Client: %04x", params->ctx->opcode);
    }
    return err;
}
/**
 * @brief A template class for creating Generic Power Level Client models.
 *
 * This class is derived from meshXClientModel and provides a convenient interface for
 * creating Generic Power Level Client models. It handles the Generic Power Level state change
 * notifications from the MeshX stack and publishes the state change event to the
 * element layer.
 *
 * @tparam meshxBaseClientModel_t The type of the meshXBaseClientModel class to be used.
 * @tparam meshx_send_packet_params_t The type of the meshXSendPacketParams structure used
 * for sending packets.
 *
 * @param[in] parent_element A pointer to the parent element (meshXElementIF).
 */
MESHX_GEN_POWER_LEVEL_CLIENT_MODEL_TEMPLATE_PROTO
meshXGenericPowerLevelClientModel MESHX_GEN_POWER_LEVEL_CLIENT_MODEL_TEMPLATE_PARAMS
    ::meshXGenericPowerLevelClientModel(
        meshXElementIF *parent_element,
        meshx_ptr_t     parent_element_state,
        uint16_t        model_func_id)
    : meshXClientModel(nullptr, MESHX_MODEL_ID_GEN_POWER_LEVEL_CLI, parent_element, parent_element_state, model_func_id)
{
    /* Used only for initialization of Parent Class */
}

#endif /* CONFIG_ENABLE_GEN_POWER_LEVEL_CLIENT */
/*******************************************************************************************************************/
#if CONFIG_ENABLE_GEN_POWER_LEVEL_SERVER
/**
 * @brief Send a packet to the MeshX stack based on the given parameters
 *
 * This function takes a pointer to a meshx_gen_power_level_send_params_t structure as input
 * and sends the corresponding packet to the MeshX stack for processing.
 *
 * @param[in] params Pointer to the meshx_gen_power_level_send_params_t structure containing the parameters
 * @return MESHX_SUCCESS on success, or an error code on failure
 */
MESHX_GEN_POWER_LEVEL_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPowerLevelServerModel MESHX_GEN_POWER_LEVEL_SERVER_MODEL_TEMPLATE_PARAMS
    :: model_send(meshx_gen_power_level_send_params_t *params)
{
if (!params|| !params->model || !params->ctx)
    {
        return MESHX_INVALID_ARG;
    }
    params->ctx->opcode = MESHX_MODEL_OP_GEN_POWER_LEVEL_STATUS;
    meshx_gen_srv_state_change_t state_change = {
        .power_level_set = {
            .power = params->power_level
        }
    };
    meshx_gen_server_send_params_t send_params = {
        .p_model        = params->model,
        .p_ctx          = params->ctx,
        .state_change   = state_change,
        .data_len       = sizeof(meshx_state_change_gen_power_level_set_t)
    };
    return this->get_base_model()->plat_send_msg(&send_params);
}

/**
 * @brief Callback function for handling BLE mesh events for Generic Power Level Server Model
 *
 * @param[in] p_dev     Pointer to the device structure
 * @param[in] model_id  Model ID associated with the event
 * @param[in] params    Pointer to the callback parameter structure containing the details of the
 *                        received message
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_INVALID_ARG: Invalid argument
 *    - MESHX_FAIL: Other failures
 */
MESHX_GEN_POWER_LEVEL_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPowerLevelServerModel MESHX_GEN_POWER_LEVEL_SERVER_MODEL_TEMPLATE_PARAMS
    :: model_from_ble_cb(dev_struct_t *p_dev, evt_model_id_t model_id, meshx_ptr_t params)
{
    if(!params || !p_dev)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid parameters");
        return MESHX_INVALID_ARG;
    }
    if(model_id != MESHX_MODEL_ID_GEN_POWER_LEVEL_SRV)
    {
        /* Callback triggered not for this model */
        return MESHX_SUCCESS;
    }

    auto *param = static_cast<meshx_gen_srv_cb_param_t *>(params);

    // Store the message in member variable for later use by prepare_element_msg
    element_msg = {
        .header = {
            .model                  = param->model,
            .element_state_change   = MESHX_SUCCESS,
        },
        .state = {
            .present_power  = param->state_change.power_level_set.power,
            .target_power   = 0,
            .remain_time    = 0,
            .power_default  = param->state_change.power_default_set.power,
            .range = {
                .range_min = param->state_change.power_range_set.range_min,
                .range_max = param->state_change.power_range_set.range_max
            }
        }
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
MESHX_GEN_POWER_LEVEL_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPowerLevelServerModel MESHX_GEN_POWER_LEVEL_SERVER_MODEL_TEMPLATE_PARAMS
    :: prepare_element_msg(meshx_ptr_t *msg_ptr, size_t *msg_size)
{
    if (!msg_ptr || !msg_size)
    {
        return MESHX_INVALID_ARG;
    }

    if (!element_msg_prepared)
    {
        return MESHX_NOT_SUPPORTED;
    }

    *msg_ptr  = &element_msg;
    *msg_size = sizeof(element_msg);

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
MESHX_GEN_POWER_LEVEL_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPowerLevelServerModel MESHX_GEN_POWER_LEVEL_SERVER_MODEL_TEMPLATE_PARAMS
    :: element_state_change_handle(void)
{
    auto *el_state =
        static_cast<meshx_gen_power_level_model_state_t*>(this->get_parent_element_state());

    if (!el_state) {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid parameter in element_state_change_handle");
        return MESHX_INVALID_ARG;
    }

    if(memcmp(&model_state, el_state, sizeof(meshx_gen_power_level_model_state_t)) != 0)
    {
        MESHX_LOGI(MODULE_ID_MODEL_SERVER,
            "Power Level state change request: present=%d target=%d remaining=%d",
            el_state->present_power, el_state->target_power, el_state->remain_time);
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
 * @brief Constructor for Generic Power Level Server Model
 *
 * @param[in] parent_element Pointer to the parent element (meshXElementIF)
 */
MESHX_GEN_POWER_LEVEL_SERVER_MODEL_TEMPLATE_PROTO
meshXGenericPowerLevelServerModel MESHX_GEN_POWER_LEVEL_SERVER_MODEL_TEMPLATE_PARAMS
    ::meshXGenericPowerLevelServerModel(
        meshXElementIF *parent_element,
        meshx_ptr_t     parent_element_state,
        uint16_t        model_func_id
    )
    : meshXServerModel(nullptr, MESHX_MODEL_ID_GEN_POWER_LEVEL_SRV, parent_element, parent_element_state, model_func_id)
{}

/**
 * @brief Creates and initializes a server model instance for Generic Power Level Server.
 *
 * This function handles the platform-specific model creation process for Generic Power Level Server models.
 * It initializes server-specific features and cannot be overridden by derived classes.
 *
 * @return meshx_err_t Returns an error code indicating the result of the operation.
 *         - MESHX_SUCCESS on successful model creation and initialization
 *         - MESHX_ERR_NO_MEM if memory allocation fails
 *         - Other error codes for platform-specific failures
 *
 * @note This is a final function and cannot be overridden by derived classes.
 */
MESHX_GEN_POWER_LEVEL_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPowerLevelServerModel MESHX_GEN_POWER_LEVEL_SERVER_MODEL_TEMPLATE_PARAMS
    :: plat_model_create(void)
{
    meshx_ptr_t p_pub = this->get_pub_struct();
    meshx_ptr_t p_gen = this->get_gen_struct();
    meshx_err_t err = MESHX_SUCCESS;

    err = meshx_plat_power_level_gen_srv_create(this->get_plat_model(), &p_pub, &p_gen);
    if(err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to create Generic Power Level Server Model");
    }
    else
    {
        this->set_pub_struct(p_pub);
        this->set_gen_struct(p_gen);
    }
    return err;
}

/**
 * @brief Deletes the Generic Power Level Server model and its associated resources.
 *
 * This function frees the memory allocated for the Generic Power Level Server
 * and sets the pointer to NULL. It also deletes the model publication
 * resources associated with the server.
 *
 * @return
 *     - MESHX_SUCCESS: Model and publication deleted successfully.
 *     - MESHX_FAIL: Failed to delete the model or publication.
 */
MESHX_GEN_POWER_LEVEL_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPowerLevelServerModel MESHX_GEN_POWER_LEVEL_SERVER_MODEL_TEMPLATE_PARAMS
    :: plat_model_delete(void)
{
    meshx_ptr_t p_pub = this->get_pub_struct();
    meshx_ptr_t p_gen = this->get_gen_struct();

    meshx_err_t err = meshx_plat_gen_srv_delete(&p_pub, &p_gen);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to delete Generic Power Level Server Model");
    }
    else
    {
        this->set_pub_struct(nullptr);
        this->set_gen_struct(nullptr);
    }
    return err;
}
#endif /* CONFIG_ENABLE_GEN_POWER_LEVEL_SERVER */

#if CONFIG_ENABLE_GEN_POWER_LEVEL_SETUP_SERVER
/**
 * @brief Constructor for Generic Power Level Setup Server Model
 *
 * @param[in] parent_element Pointer to the parent element (meshXElementIF)
 */
MESHX_GEN_POWER_LEVEL_SETUP_SERVER_MODEL_TEMPLATE_PROTO
meshXGenericPowerLevelSetupServerModel MESHX_GEN_POWER_LEVEL_SETUP_SERVER_MODEL_TEMPLATE_PARAMS
    ::meshXGenericPowerLevelSetupServerModel(
        meshXElementIF *parent_element,
        meshx_ptr_t     parent_element_state,
        uint16_t        model_func_id
    )
    : meshXServerModel(nullptr, MESHX_MODEL_ID_GEN_POWER_LEVEL_SETUP_SRV, parent_element, parent_element_state, model_func_id)
{}

/**
 * @brief Send a packet to the MeshX stack based on the given parameters
 *
 * This function takes a pointer to a meshx_gen_power_level_send_params_t structure as input
 * and sends the corresponding packet to the MeshX stack for processing.
 *
 * @param[in] params Pointer to the meshx_gen_power_level_send_params_t structure containing the parameters
 * @return MESHX_SUCCESS on success, or an error code on failure
 */
MESHX_GEN_POWER_LEVEL_SETUP_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPowerLevelSetupServerModel MESHX_GEN_POWER_LEVEL_SETUP_SERVER_MODEL_TEMPLATE_PARAMS
    :: model_send(meshx_gen_power_level_send_params_t *params)
{
if (!params|| !params->model || !params->ctx)
    {
        return MESHX_INVALID_ARG;
    }
    params->ctx->opcode = MESHX_MODEL_OP_GEN_POWER_LEVEL_STATUS;
    meshx_gen_srv_state_change_t state_change = {
        .power_level_set = {
            .power = params->power_level
        }
    };
    meshx_gen_server_send_params_t send_params = {
        .p_model        = params->model,
        .p_ctx          = params->ctx,
        .state_change   = state_change,
        .data_len       = sizeof(meshx_state_change_gen_power_level_set_t)
    };
    return this->get_base_model()->plat_send_msg(&send_params);
}

/**
 * @brief Constructor for Generic Power Level Setup Server Model
 *
 * @param[in] parent_element Pointer to the parent element (meshXElementIF)
 */
MESHX_GEN_POWER_LEVEL_SETUP_SERVER_MODEL_TEMPLATE_PROTO
meshXGenericPowerLevelSetupServerModel MESHX_GEN_POWER_LEVEL_SETUP_SERVER_MODEL_TEMPLATE_PARAMS
    ::meshXGenericPowerLevelSetupServerModel(
        meshXElementIF *parent_element,
        meshx_ptr_t     parent_element_state,
        uint16_t        model_func_id
    )
    : meshXServerModel(nullptr, MESHX_MODEL_ID_GEN_POWER_LEVEL_SETUP_SRV, parent_element, parent_element_state, model_func_id)
{}

/**
 * @brief Callback function for handling BLE mesh events for Generic Power Level Setup Server Model
 *
 * @param[in] p_dev     Pointer to the device structure
 * @param[in] model_id  Model ID associated with the event
 * @param[in] params    Pointer to the event parameters
 * @return MESHX_SUCCESS if successful, error code otherwise
 */
MESHX_GEN_POWER_LEVEL_SETUP_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPowerLevelSetupServerModel MESHX_GEN_POWER_LEVEL_SETUP_SERVER_MODEL_TEMPLATE_PARAMS
    :: model_from_ble_cb(dev_struct_t *p_dev, evt_model_id_t model_id, meshx_ptr_t params)
{
    if(!params || !p_dev)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid parameters");
        return MESHX_INVALID_ARG;
    }
    if(model_id != MESHX_MODEL_ID_GEN_POWER_LEVEL_SETUP_SRV)
    {
        /* Callback triggered not for this model */
        return MESHX_SUCCESS;
    }

    auto *param = static_cast<meshx_gen_srv_cb_param_t *>(params);

    // Store the message in member variable for later use by prepare_element_msg
    element_msg = {
        .header = {
            .model                  = param->model,
            .element_state_change   = MESHX_SUCCESS,
        },
        .state = {
            .present_power  = 0,
            .target_power   = 0,
            .remain_time    = 0,
            .power_default  = param->state_change.power_default_set.power,
            .range = {
                .range_min = param->state_change.power_range_set.range_min,
                .range_max = param->state_change.power_range_set.range_max
            }
        }
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
MESHX_GEN_POWER_LEVEL_SETUP_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPowerLevelSetupServerModel MESHX_GEN_POWER_LEVEL_SETUP_SERVER_MODEL_TEMPLATE_PARAMS
    :: prepare_element_msg(meshx_ptr_t *msg_ptr, size_t *msg_size)
{
    if (!msg_ptr || !msg_size)
    {
        return MESHX_INVALID_ARG;
    }

    if (!element_msg_prepared)
    {
        return MESHX_NOT_SUPPORTED;
    }

    *msg_ptr  = &element_msg;
    *msg_size = sizeof(element_msg);

    return MESHX_SUCCESS;
}
#endif /* CONFIG_ENABLE_GEN_POWER_LEVEL_SETUP_SERVER */
