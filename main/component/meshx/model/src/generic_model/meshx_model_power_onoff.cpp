/**
 * @file meshx_model_power_onoff.cpp
 * @brief Implementation of Generic Power OnOff Model classes for MeshX.
 *        This file contains the implementation of the Generic Power OnOff Server and Client models
 *        for the MeshX BLE mesh framework.
 *
 * Key Features:
 *  - Implements Bluetooth SIG-defined Generic Power OnOff model
 *  - Inherits from meshXServerModel and meshXClientModel templates
 *  - Provides standard Power OnOff control operations
 *  - Integrates with MeshX transmission control
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#include <generic_model/meshx_model_power_onoff.hpp>

#if CONFIG_ENABLE_GEN_POWER_ONOFF_CLIENT
/**
 * @brief Handle Generic Power OnOff state change notifications from the MeshX stack.
 *
 * This function is responsible for handling Generic Power OnOff state change notifications
 * from the MeshX stack. It is called when the MeshX stack receives a state
 * change event from the Generic Power OnOff server model. The function publishes the
 * state change event to the control task framework, which in turn notifies the
 * application about the state change.
 *
 * @param[in] param  Pointer to the Generic Power OnOff client callback parameter structure.
 * @param[in] status Status of the state change event (success or timeout).
 *
 * @return
 *     - MESHX_SUCCESS: Successfully handled the state change notification.
 *     - MESHX_INVALID_ARG: One or more arguments are invalid.
 */
MESHX_GEN_POWER_ONOFF_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPowerOnOffClientModel MESHX_GEN_POWER_ONOFF_CLIENT_MODEL_TEMPLATE_PARAMS
    :: meshx_state_change_notify(const meshx_gen_cli_cb_param_t *param, uint8_t status)
{
    if (!param){
        return MESHX_INVALID_ARG;
    }

    model_state.on_power_up = param->status.onpowerup_status.onpowerup;

    meshx_power_onoff_cli_el_msg_t power_onoff_param =
    {
        .header = {
            .err_code               = param->err_code,
            .model                  = param->model,
            .ctx                    = param->ctx,
            .element_state_change   = MESHX_SUCCESS,
        },
        .state                  = model_state,
    };
    /* Send the state change event to the respective Element */
    if (this->get_parent_element())
    {
        if(this->get_parent_element_state())
        {
            power_onoff_param.header.element_state_change = this->element_state_change_handle();
        }
        else
        {
            MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Parent element state is null");
            power_onoff_param.header.element_state_change = MESHX_NOT_FOUND;
        }
        return this->get_parent_element()->on_model_cb(&power_onoff_param, sizeof(power_onoff_param));
    }
    else
    {
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Parent element is null");
    }

    return MESHX_INVALID_STATE;
}

/**
 * @brief Handle state change request from element.
 *
 * This function is called by the parent element when a state change request
 * is received. It validates the request and returns a result to the element.
 * Note: The actual state is maintained in the element layer, not the model layer.
 *
 * @param[in] curr_el_state Pointer to meshx_gen_power_onoff_model_state_t containing the new state
 * @return
 *     - MESHX_SUCCESS: State change handled successfully
 *     - MESHX_INVALID_ARG: Invalid parameter
 */
MESHX_GEN_POWER_ONOFF_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPowerOnOffClientModel MESHX_GEN_POWER_ONOFF_CLIENT_MODEL_TEMPLATE_PARAMS
    :: element_state_change_handle()
{
    meshx_gen_power_onoff_model_state_t *el_state =
        static_cast<meshx_gen_power_onoff_model_state_t*>(this->get_parent_element_state());
    if(!el_state)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid parameter in element_state_change_handle");
        return MESHX_INVALID_ARG;
    }
    if(memcmp(&model_state, el_state, sizeof(meshx_gen_power_onoff_model_state_t)) != 0)
    {
        MESHX_LOGI(MODULE_ID_MODEL_SERVER,
            "Power OnOff state change request: on_power_up=%d",
            el_state->on_power_up);
        model_state = *el_state;
    }
    else
    {
        return MESHX_INVALID_STATE;
    }
    return MESHX_SUCCESS;
}
/**
 * @brief Creates a meshXGenericPowerOnOffClientModel instance based on a BLE device
 *
 * This function is used to create a meshXGenericPowerOnOffClientModel instance based on a BLE device.
 * It takes the device structure, model ID, and parameters as input and returns a pointer to the created instance.
 *
 * @param[in] p_dev     Pointer to the device structure
 * @param[in] model_id  Model ID associated with the device
 * @param[in] params    Pointer to the parameters associated with the device
 *
 * @return Pointer to the created meshXGenericPowerOnOffClientModel instance
 */
MESHX_GEN_POWER_ONOFF_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPowerOnOffClientModel MESHX_GEN_POWER_ONOFF_CLIENT_MODEL_TEMPLATE_PARAMS
    :: model_from_ble_cb(
        dev_struct_t *p_dev,
        control_task_msg_evt_t model_id,
        meshx_ptr_t params)
)
{
    if(!params || !p_dev)
    {
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Invalid parameters");
        return MESHX_INVALID_ARG;
    }
    if(model_id != MESHX_MODEL_ID_GEN_POWER_ONOFF_CLI)
    {
        /* Callback triggered not for this model */
        return MESHX_SUCCESS;
    }
    const auto *param = static_cast<const meshx_gen_cli_cb_param_t *>(params);

    return std::to_underlying(param->evt) == std::to_underlying(meshx_base_cli_evt::MESHX_BASE_CLI_TIMEOUT) ?
        meshx_state_change_notify(param, MESHX_TIMEOUT) :
        meshx_state_change_notify(param, MESHX_SUCCESS);
}

/**
 * @brief Send a packet to the MeshX stack based on the given parameters
 *
 * This function takes a pointer to a meshx_gen_power_onoff_send_params_t structure as input
 * and sends the corresponding packet to the MeshX stack for processing.
 *
 * @param[in] params Pointer to the meshx_gen_power_onoff_send_params_t structure containing the parameters
 * @return MESHX_SUCCESS on success, or an error code on failure
 */
MESHX_GEN_POWER_ONOFF_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPowerOnOffClientModel MESHX_GEN_POWER_ONOFF_CLIENT_MODEL_TEMPLATE_PARAMS
    :: model_send(meshx_gen_power_onoff_send_params_t *params)
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

    if (params->ctx->opcode == MESHX_MODEL_OP_GEN_ONPOWERUP_GET)
    {
        err = this->get_base_model()->plat_send_msg(&send_params);
    }

    else if (params->ctx->opcode == MESHX_MODEL_OP_GEN_ONPOWERUP_SET ||
             params->ctx->opcode == MESHX_MODEL_OP_GEN_ONPOWERUP_SET_UNACK)
    {
        set.power_set.onpowerup = params->on_power_up;
        // Note: Power OnOff SET operations don't use TID

        err = this->get_base_model()->plat_send_msg(&send_params);
    }
    else
    {
        err = MESHX_INVALID_ARG; // Invalid opcode
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Invalid opcode for Generic Power OnOff Client: %04x", params->ctx->opcode);
    }
    return err;
}
/**
 * @brief A template class for creating Generic Power OnOff Client models.
 *
 * This class is derived from meshXClientModel and provides a convenient interface for
 * creating Generic Power OnOff Client models. It handles the Generic Power OnOff state change
 * notifications from the MeshX stack and publishes the state change event to the
 * element layer.
 *
 * @tparam meshxBaseClientModel_t The type of the meshXBaseClientModel class to be used.
 * @tparam meshx_send_packet_params_t The type of the meshXSendPacketParams structure used
 * for sending packets.
 *
 * @param[in] parent_element A pointer to the parent element (meshXElementIF).
 */
MESHX_GEN_POWER_ONOFF_CLIENT_MODEL_TEMPLATE_PROTO
meshXGenericPowerOnOffClientModel MESHX_GEN_POWER_ONOFF_CLIENT_MODEL_TEMPLATE_PARAMS
    ::meshXGenericPowerOnOffClientModel(
        meshXElementIF *parent_element,
        meshx_ptr_t     parent_element_state)
    : meshXClientModel(nullptr, MESHX_MODEL_ID_GEN_POWER_ONOFF_CLI, parent_element, parent_element_state) {/* Used only for initialization of Parent Class */}

#endif /* CONFIG_ENABLE_GEN_POWER_ONOFF_CLIENT */
/*******************************************************************************************************************/
#if CONFIG_ENABLE_GEN_POWER_ONOFF_SERVER
/**
 * @brief Send a packet to the MeshX stack based on the given parameters
 *
 * This function takes a pointer to a meshx_gen_power_onoff_send_params_t structure as input
 * and sends the corresponding packet to the MeshX stack for processing.
 *
 * @param[in] params Pointer to the meshx_gen_power_onoff_send_params_t structure containing the parameters
 * @return MESHX_SUCCESS on success, or an error code on failure
 */
MESHX_GEN_POWER_ONOFF_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPowerOnOffServerModel MESHX_GEN_POWER_ONOFF_SERVER_MODEL_TEMPLATE_PARAMS
    :: model_send(meshx_gen_power_onoff_send_params_t *params)
{
if (!params|| !params->model || !params->ctx)
    {
        return MESHX_INVALID_ARG;
    }
    params->ctx->opcode = MESHX_MODEL_OP_GEN_ONPOWERUP_STATUS;
    meshx_gen_srv_state_change_t state_change = {
        .onpowerup_set = {
            .onpowerup = params->on_power_up
        }
    };
    meshx_gen_server_send_params_t send_params = {
        .p_model = params->model,
        .p_ctx = params->ctx,
        .state_change = state_change,
        .data_len = sizeof(meshx_state_change_gen_onpowerup_set_t)
    };
    return this->get_base_model()->plat_send_msg(&send_params);
}

/**
 * @brief Callback function for handling BLE mesh events for Generic Power OnOff Server Model
 *
 * @param[in] p_dev     Pointer to the device structure
 * @param[in] model_id  Model ID associated with the event
 * @param[in] params    Pointer to the event parameters
 * @return MESHX_SUCCESS if successful, error code otherwise
 */
MESHX_GEN_POWER_ONOFF_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPowerOnOffServerModel MESHX_GEN_POWER_ONOFF_SERVER_MODEL_TEMPLATE_PARAMS
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
    if(model_id != MESHX_MODEL_ID_GEN_POWER_ONOFF_SRV)
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
            .on_power_up = param->state_change.onpowerup_set.onpowerup
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
MESHX_GEN_POWER_ONOFF_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPowerOnOffServerModel MESHX_GEN_POWER_ONOFF_SERVER_MODEL_TEMPLATE_PARAMS
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

    *msg_ptr = &element_msg;
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
MESHX_GEN_POWER_ONOFF_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPowerOnOffServerModel MESHX_GEN_POWER_ONOFF_SERVER_MODEL_TEMPLATE_PARAMS
    :: element_state_change_handle(void)
{
    auto *el_state =
        static_cast<meshx_gen_power_onoff_model_state_t*>(this->get_parent_element_state());

    if (!el_state) {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid parameter in element_state_change_handle");
        return MESHX_INVALID_ARG;
    }

    if(memcmp(&model_state, el_state, sizeof(meshx_gen_power_onoff_model_state_t)) != 0)
    {
        MESHX_LOGI(MODULE_ID_MODEL_SERVER,
            "Power OnOff state change request: on_power_up=%d",
            el_state->on_power_up);
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
 * @brief Constructor for Generic Power OnOff Server Model
 *
 * @param[in] parent_element Pointer to the parent element (meshXElementIF)
 */
MESHX_GEN_POWER_ONOFF_SERVER_MODEL_TEMPLATE_PROTO
meshXGenericPowerOnOffServerModel MESHX_GEN_POWER_ONOFF_SERVER_MODEL_TEMPLATE_PARAMS
    ::meshXGenericPowerOnOffServerModel(
        meshXElementIF *parent_element,
        meshx_ptr_t     parent_element_state)
    : meshXServerModel(nullptr, MESHX_MODEL_ID_GEN_POWER_ONOFF_SRV, parent_element, parent_element_state) {}

/**
 * @brief Creates and initializes a server model instance for Generic Power OnOff Server.
 *
 * This function handles the platform-specific model creation process for Generic Power OnOff Server models.
 * It initializes server-specific features and cannot be overridden by derived classes.
 *
 * @return meshx_err_t Returns an error code indicating the result of the operation.
 *         - MESHX_SUCCESS on successful model creation and initialization
 *         - MESHX_ERR_NO_MEM if memory allocation fails
 *         - Other error codes for platform-specific failures
 *
 * @note This is a final function and cannot be overridden by derived classes.
 */
MESHX_GEN_POWER_ONOFF_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPowerOnOffServerModel MESHX_GEN_POWER_ONOFF_SERVER_MODEL_TEMPLATE_PARAMS
    :: plat_model_create(void)
{
    meshx_ptr_t p_pub = this->get_pub_struct();
    meshx_ptr_t p_gen = this->get_gen_struct();
    meshx_err_t err = MESHX_SUCCESS;

    err = meshx_plat_power_onoff_gen_srv_create(this->get_plat_model(), &p_pub, &p_gen);
    if(err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to create Generic Power OnOff Server Model");
    }
    else
    {
        this->set_pub_struct(p_pub);
        this->set_gen_struct(p_gen);
    }
    return err;
}

/**
 * @brief Deletes the Generic Power OnOff Server model and its associated resources.
 *
 * This function frees the memory allocated for the Generic Power OnOff Server
 * and sets the pointer to NULL. It also deletes the model publication
 * resources associated with the server.
 *
 * @return
 *     - MESHX_SUCCESS: Model and publication deleted successfully.
 *     - MESHX_FAIL: Failed to delete the model or publication.
 */
MESHX_GEN_POWER_ONOFF_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPowerOnOffServerModel MESHX_GEN_POWER_ONOFF_SERVER_MODEL_TEMPLATE_PARAMS
    :: plat_model_delete(void)
{
    meshx_ptr_t p_pub = this->get_pub_struct();
    meshx_ptr_t p_gen = this->get_gen_struct();

    meshx_err_t err = meshx_plat_gen_srv_delete(&p_pub, &p_gen);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to delete Generic Power OnOff Server Model");
    }
    else
    {
        this->set_pub_struct(nullptr);
        this->set_gen_struct(nullptr);
    }
    return err;
}
#endif /* CONFIG_ENABLE_GEN_POWER_ONOFF_SERVER */

#if CONFIG_ENABLE_GEN_POWER_ONOFF_SETUP_SERVER
/**
 * @brief Send a packet to the MeshX stack based on the given parameters
 *
 * This function takes a pointer to a meshx_gen_power_onoff_send_params_t structure as input
 * and sends the corresponding packet to the MeshX stack for processing.
 *
 * @param[in] params Pointer to the meshx_gen_power_onoff_send_params_t structure containing the parameters
 * @return MESHX_SUCCESS on success, or an error code on failure
 */
MESHX_GEN_POWER_ONOFF_SETUP_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPowerOnOffSetupServerModel MESHX_GEN_POWER_ONOFF_SETUP_SERVER_MODEL_TEMPLATE_PARAMS
    :: model_send(meshx_gen_power_onoff_send_params_t *params)
{
if (!params|| !params->model || !params->ctx)
    {
        return MESHX_INVALID_ARG;
    }
    params->ctx->opcode = MESHX_MODEL_OP_GEN_ONPOWERUP_STATUS;
    meshx_gen_srv_state_change_t state_change = {
        .onpowerup_set = {
            .onpowerup = params->on_power_up
        }
    };
    meshx_gen_server_send_params_t send_params = {
        .p_model = params->model,
        .p_ctx = params->ctx,
        .state_change = state_change,
        .data_len = sizeof(meshx_state_change_gen_onpowerup_set_t)
    };
    return this->get_base_model()->plat_send_msg(&send_params);
}

/**
 * @brief Constructor for Generic Power OnOff Setup Server Model
 *
 * @param[in] parent_element Pointer to the parent element (meshXElementIF)
 */
MESHX_GEN_POWER_ONOFF_SETUP_SERVER_MODEL_TEMPLATE_PROTO
meshXGenericPowerOnOffSetupServerModel MESHX_GEN_POWER_ONOFF_SETUP_SERVER_MODEL_TEMPLATE_PARAMS
    ::meshXGenericPowerOnOffSetupServerModel(
        meshXElementIF *parent_element,
        meshx_ptr_t     parent_element_state)
    : meshXServerModel(nullptr, MESHX_MODEL_ID_GEN_POWER_ONOFF_SETUP_SRV, parent_element, parent_element_state) {}

/**
 * @brief Callback function for handling BLE mesh events for Generic Power OnOff Setup Server Model
 *
 * @param[in] p_dev     Pointer to the device structure
 * @param[in] model_id  Model ID associated with the event
 * @param[in] params    Pointer to the event parameters
 * @return MESHX_SUCCESS if successful, error code otherwise
 */
MESHX_GEN_POWER_ONOFF_SETUP_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPowerOnOffSetupServerModel MESHX_GEN_POWER_ONOFF_SETUP_SERVER_MODEL_TEMPLATE_PARAMS
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
    if(model_id != MESHX_MODEL_ID_GEN_POWER_ONOFF_SETUP_SRV)
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
            .on_power_up = param->state_change.onpowerup_set.onpowerup
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
MESHX_GEN_POWER_ONOFF_SETUP_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPowerOnOffSetupServerModel MESHX_GEN_POWER_ONOFF_SETUP_SERVER_MODEL_TEMPLATE_PARAMS
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

    *msg_ptr = &element_msg;
    *msg_size = sizeof(element_msg);

    return MESHX_SUCCESS;
}
#endif /* CONFIG_ENABLE_GEN_POWER_ONOFF_SETUP_SERVER */
