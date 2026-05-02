/**
 * @file meshx_model_battery.cpp
 * @brief Implementation of Generic Battery Model classes for MeshX.
 *        This file contains the implementation of the Generic Battery Server and Client models
 *        for the MeshX BLE mesh framework.
 *
 * Key Features:
 *  - Implements Bluetooth SIG-defined Generic Battery model
 *  - Inherits from meshXServerModel and meshXClientModel templates
 *  - Provides standard Battery status operations
 *  - Integrates with MeshX transmission control
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#include <generic_model/meshx_model_battery.hpp>

#if CONFIG_ENABLE_GEN_BATTERY_CLIENT
/**
 * @brief Handle Generic Battery state change notifications from the MeshX stack.
 *
 * This function is responsible for handling Generic Battery state change notifications
 * from the MeshX stack. It is called when the MeshX stack receives a state
 * change event from the Generic Battery server model. The function publishes the
 * state change event to the control task framework, which in turn notifies the
 * application about the state change.
 *
 * @param[in] param  Pointer to the Generic Battery client callback parameter structure.
 * @param[in] status Status of the state change event (success or timeout).
 *
 * @return
 *     - MESHX_SUCCESS: Successfully handled the state change notification.
 *     - MESHX_INVALID_ARG: One or more arguments are invalid.
 */
MESHX_GEN_BATTERY_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericBatteryClientModel MESHX_GEN_BATTERY_CLIENT_MODEL_TEMPLATE_PARAMS
    :: meshx_state_change_notify(const meshx_gen_cli_cb_param_t *param, uint8_t status)
{
    if (!param){
        return MESHX_INVALID_ARG;
    }

    model_state.battery_level       = param->status.battery_status.battery_level;
    model_state.time_to_discharge   = param->status.battery_status.time_to_discharge;
    model_state.time_to_charge      = param->status.battery_status.time_to_charge;
    model_state.presence            = param->status.battery_status.presence;
    model_state.charge_level        = param->status.battery_status.charge_level;
    model_state.charge_type         = param->status.battery_status.charge_type;

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
MESHX_GEN_BATTERY_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericBatteryClientModel MESHX_GEN_BATTERY_CLIENT_MODEL_TEMPLATE_PARAMS
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
 * @param[in] curr_el_state Pointer to meshx_gen_battery_model_state_t containing the new state
 * @return
 *     - MESHX_SUCCESS: State change handled successfully
 *     - MESHX_INVALID_ARG: Invalid parameter
 */
MESHX_GEN_BATTERY_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericBatteryClientModel MESHX_GEN_BATTERY_CLIENT_MODEL_TEMPLATE_PARAMS
    :: element_state_change_handle()
{
    meshx_gen_battery_model_state_t *el_state =
        static_cast<meshx_gen_battery_model_state_t*>(this->get_parent_element_state());
    if(!el_state)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid parameter in element_state_change_handle");
        return MESHX_INVALID_ARG;
    }
    if(memcmp(&model_state, el_state, sizeof(meshx_gen_battery_model_state_t)) != 0)
    {
        MESHX_LOGI(MODULE_ID_MODEL_SERVER,
            "Battery state change request: level=%d discharge=%d charge=%d",
            el_state->battery_level, el_state->time_to_discharge, el_state->time_to_charge);
        model_state = *el_state;
    }
    else
    {
        return MESHX_INVALID_STATE;
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Creates a meshXGenericBatteryClientModel instance based on a BLE device
 *
 * This function is used to create a meshXGenericBatteryClientModel instance based on a BLE device.
 * It takes the device structure, model ID, and parameters as input and returns a pointer to the created instance.
 *
 * @param[in] p_dev     Pointer to the device structure
 * @param[in] model_id  Model ID associated with the device
 * @param[in] params    Pointer to the parameters associated with the device
 *
 * @return Pointer to the created meshXGenericBatteryClientModel instance
 */
MESHX_GEN_BATTERY_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericBatteryClientModel MESHX_GEN_BATTERY_CLIENT_MODEL_TEMPLATE_PARAMS
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
    if(model_id != MESHX_MODEL_ID_GEN_BATTERY_CLI)
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
 * This function takes a pointer to a meshx_gen_battery_send_params_t structure as input
 * and sends the corresponding packet to the MeshX stack for processing.
 *
 * @param[in] params Pointer to the meshx_gen_battery_send_params_t structure containing the parameters
 * @return MESHX_SUCCESS on success, or an error code on failure
 */
MESHX_GEN_BATTERY_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericBatteryClientModel MESHX_GEN_BATTERY_CLIENT_MODEL_TEMPLATE_PARAMS
    :: model_send(meshx_gen_battery_send_params_t *params)
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

    if (params->ctx->opcode == MESHX_MODEL_OP_GEN_BATTERY_GET)
    {
        err = this->get_base_model()->plat_send_msg(&send_params);
    }
    else
    {
        err = MESHX_INVALID_ARG; // Invalid opcode
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Invalid opcode for Generic Battery Client: %04x", params->ctx->opcode);
    }
    return err;
}

/**
 * @brief A template class for creating Generic Battery Client models.
 *
 * This class is derived from meshXClientModel and provides a convenient interface for
 * creating Generic Battery Client models. It handles the Generic Battery state change
 * notifications from the MeshX stack and publishes the state change event to the
 * element layer.
 *
 * @tparam meshxBaseClientModel_t The type of the meshXBaseClientModel class to be used.
 * @tparam meshx_send_packet_params_t The type of the meshXSendPacketParams structure used
 * for sending packets.
 *
 * @param[in] parent_element A pointer to the parent element (meshXElementIF).
 */
MESHX_GEN_BATTERY_CLIENT_MODEL_TEMPLATE_PROTO
meshXGenericBatteryClientModel MESHX_GEN_BATTERY_CLIENT_MODEL_TEMPLATE_PARAMS
    ::meshXGenericBatteryClientModel(
        meshXElementIF *parent_element,
        meshx_ptr_t     parent_element_state,
        uint16_t        model_func_id)
    : meshXClientModel(nullptr, MESHX_MODEL_ID_GEN_BATTERY_CLI, parent_element, parent_element_state, model_func_id)
{
    /* Used only for initialization of Parent Class */
}

#endif /* CONFIG_ENABLE_GEN_BATTERY_CLIENT */
/*******************************************************************************************************************/
#if CONFIG_ENABLE_GEN_BATTERY_SERVER
/**
 * @brief Send a packet to the MeshX stack based on the given parameters
 *
 * This function takes a pointer to a meshx_gen_battery_send_params_t structure as input
 * and sends the corresponding packet to the MeshX stack for processing.
 *
 * @param[in] params Pointer to the meshx_gen_battery_send_params_t structure containing the parameters
 * @return MESHX_SUCCESS on success, or an error code on failure
 */
MESHX_GEN_BATTERY_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericBatteryServerModel MESHX_GEN_BATTERY_SERVER_MODEL_TEMPLATE_PARAMS
    :: model_send(meshx_gen_battery_send_params_t *params)
{
    if (!params|| !params->model || !params->ctx)
    {
        return MESHX_INVALID_ARG;
    }
    params->ctx->opcode = MESHX_MODEL_OP_GEN_BATTERY_STATUS;
    // Battery server doesn't modify state directly - just responds with current status
    meshx_gen_srv_state_change_t state_change = {0}; // Empty state change for battery status
    meshx_gen_server_send_params_t send_params = {
        .p_model = params->model,
        .p_ctx = params->ctx,
        .state_change = state_change,
        .data_len = 0  // No state data for battery status response
    };
    return this->get_base_model()->plat_send_msg(&send_params);
}

/**
 * @brief Callback function for handling BLE mesh events for the Generic Battery Server Model
 *
 * @param[in] p_dev     Pointer to the device structure
 * @param[in] model_id  Model ID associated with the event
 * @param[in] params    Pointer to the event parameters
 * @return MESHX_SUCCESS if successful, error code otherwise
 */
MESHX_GEN_BATTERY_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericBatteryServerModel MESHX_GEN_BATTERY_SERVER_MODEL_TEMPLATE_PARAMS
    :: model_from_ble_cb(
        dev_struct_t *p_dev,
        evt_model_id_t model_id,
        meshx_ptr_t params)
{
    if(!params || !p_dev)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid parameters");
        return MESHX_INVALID_ARG;
    }
    if(model_id != MESHX_MODEL_ID_GEN_BATTERY_SRV)
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
            .battery_level       = param->state_change.battery_status.battery_level,
            .time_to_discharge   = param->state_change.battery_status.time_to_discharge,
            .time_to_charge      = param->state_change.battery_status.time_to_charge,
            .presence            = param->state_change.battery_status.presence,
            .charge_level        = param->state_change.battery_status.charge_level,
            .charge_type         = param->state_change.battery_status.charge_type,
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
MESHX_GEN_BATTERY_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericBatteryServerModel MESHX_GEN_BATTERY_SERVER_MODEL_TEMPLATE_PARAMS
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
MESHX_GEN_BATTERY_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericBatteryServerModel MESHX_GEN_BATTERY_SERVER_MODEL_TEMPLATE_PARAMS
    :: element_state_change_handle(void)
{
    auto *el_state =
        static_cast<meshx_gen_battery_model_state_t*>(this->get_parent_element_state());

    if (!el_state) {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid parameter in element_state_change_handle");
        return MESHX_INVALID_ARG;
    }

    if(memcmp(&model_state, el_state, sizeof(meshx_gen_battery_model_state_t)) != 0)
    {
        MESHX_LOGI(MODULE_ID_MODEL_SERVER,
            "Battery state change request: level=%d discharge=%d charge=%d",
            el_state->battery_level, el_state->time_to_discharge, el_state->time_to_charge);
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
 * @brief Constructor for the Generic Battery Server Model
 *
 * @param[in] parent_element Pointer to the parent element (meshXElementIF)
 */
MESHX_GEN_BATTERY_SERVER_MODEL_TEMPLATE_PROTO
meshXGenericBatteryServerModel MESHX_GEN_BATTERY_SERVER_MODEL_TEMPLATE_PARAMS
    ::meshXGenericBatteryServerModel(
        meshXElementIF *parent_element,
        meshx_ptr_t     parent_element_state,
        uint16_t        model_func_id
    )
    : meshXServerModel(nullptr, MESHX_MODEL_ID_GEN_BATTERY_SRV, parent_element, parent_element_state, model_func_id)
{
    this->plat_model_create();
}

/**
 * @brief Creates and initializes a server model instance for the Generic Battery Server.
 *
 * This function handles the platform-specific model creation process for the Generic Battery Server models.
 * It initializes server-specific features and cannot be overridden by derived classes.
 *
 * @return meshx_err_t Returns an error code indicating the result of the operation.
 *         - MESHX_SUCCESS on successful model creation and initialization
 *         - MESHX_ERR_NO_MEM if memory allocation fails
 *         - Other error codes for platform-specific failures
 *
 * @note This is a final function and cannot be overridden by derived classes.
 */
MESHX_GEN_BATTERY_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericBatteryServerModel MESHX_GEN_BATTERY_SERVER_MODEL_TEMPLATE_PARAMS
    :: plat_model_create(void)
{
    meshx_ptr_t p_pub = this->get_pub_struct();
    meshx_ptr_t p_gen = this->get_gen_struct();
    meshx_err_t err = MESHX_SUCCESS;

    err = meshx_plat_battery_gen_srv_create(this->get_plat_model(), &p_pub, &p_gen);
    if(err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to create Generic Battery Server Model");
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
 * @brief Deletes the Generic Battery Server model and its associated resources.
 *
 * This function frees the memory allocated for the Generic Battery Server
 * and sets the pointer to NULL. It also deletes the model publication
 * resources associated with the server.
 *
 * @return
 *     - MESHX_SUCCESS: Model and publication deleted successfully.
 *     - MESHX_FAIL: Failed to delete the model or publication.
 */
MESHX_GEN_BATTERY_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericBatteryServerModel MESHX_GEN_BATTERY_SERVER_MODEL_TEMPLATE_PARAMS
    :: plat_model_delete(void)
{
    meshx_ptr_t p_pub = this->get_pub_struct();
    meshx_ptr_t p_gen = this->get_gen_struct();

    meshx_err_t err = meshx_plat_gen_srv_delete(&p_pub, &p_gen);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to delete Generic Battery Server Model");
    }
    else
    {
        this->set_pub_struct(nullptr);
        this->set_gen_struct(nullptr);
    }
    return err;
}
#endif /* CONFIG_ENABLE_GEN_BATTERY_SERVER */
