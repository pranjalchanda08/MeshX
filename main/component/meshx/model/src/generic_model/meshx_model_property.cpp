/**
 * @file meshx_model_property.cpp
 * @brief Implementation of Generic Property Model classes for MeshX.
 *        This file contains the implementation of the Generic Property Server and Client models
 *        for the MeshX BLE mesh framework.
 *
 * Key Features:
 *  - Implements Bluetooth SIG-defined Generic Property models
 *  - Supports Manufacturer, Admin, User, and Client Property servers
 *  - Inherits from meshXServerModel and meshXClientModel templates
 *  - Provides standard Property control operations (GET, SET operations by property ID)
 *  - Integrates with MeshX transmission control
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#include <generic_model/meshx_model_property.hpp>
#include <meshx_element_class.hpp>

#if CONFIG_ENABLE_GEN_PROPERTY_CLIENT
/**
 * @brief Handle Generic Property state change notifications from the MeshX stack.
 *
 * This function is responsible for handling Generic Property state change notifications
 * from the MeshX stack. It is called when the MeshX stack receives a state
 * change event from the Generic Property server model. The function publishes the
 * state change event to the control task framework, which in turn notifies the
 * application about the state change.
 *
 * @param[in] param  Pointer to the Generic Property client callback parameter structure.
 * @param[in] status Status of the state change event (success or timeout).
 *
 * @return
 *     - MESHX_SUCCESS: Successfully handled the state change notification.
 *     - MESHX_INVALID_ARG: One or more arguments are invalid.
 */
MESHX_GEN_PROPERTY_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPropertyClientModel MESHX_GEN_PROPERTY_CLIENT_MODEL_TEMPLATE_PARAMS
    :: meshx_state_change_notify(const meshx_gen_cli_cb_param_t *param, uint8_t status)
{
    if (!param){
        return MESHX_INVALID_ARG;
    }

    // Handle different property status types based on opcode
    switch(param->ctx.opcode) {
        case MESHX_MODEL_OP_GEN_USER_PROPERTIES_STATUS:
            model_state.property_id = 0;
            model_state.property_value = nullptr;
            model_state.access = 0;
            break;
        case MESHX_MODEL_OP_GEN_USER_PROPERTY_STATUS:
            model_state.property_id = param->status.user_property_status.property_id;
            model_state.property_value = nullptr;
            model_state.access = param->status.user_property_status.user_access;
            break;
        case MESHX_MODEL_OP_GEN_ADMIN_PROPERTIES_STATUS:
            model_state.property_id = 0;
            model_state.property_value = nullptr;
            model_state.access = 0;
            break;
        case MESHX_MODEL_OP_GEN_ADMIN_PROPERTY_STATUS:
            model_state.property_id = param->status.admin_property_status.property_id;
            model_state.property_value = nullptr;
            model_state.access = param->status.admin_property_status.user_access;
            break;
        case MESHX_MODEL_OP_GEN_MANUFACTURER_PROPERTIES_STATUS:
            model_state.property_id = 0;
            model_state.property_value = nullptr;
            model_state.access = 0;
            break;
        case MESHX_MODEL_OP_GEN_MANUFACTURER_PROPERTY_STATUS:
            model_state.property_id = param->status.manufacturer_property_status.property_id;
            model_state.property_value = nullptr;
            model_state.access = param->status.manufacturer_property_status.user_access;
            break;
        case MESHX_MODEL_OP_GEN_CLIENT_PROPERTIES_STATUS:
            model_state.property_id = 0;
            model_state.property_value = nullptr;
            model_state.access = 0;
            break;
        default:
            model_state.property_id = 0;
            model_state.property_value = nullptr;
            model_state.access = 0;
            break;
    }

    meshx_property_cli_el_msg_t property_param =
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
            property_param.header.element_state_change = this->element_state_change_handle();
        }
        else
        {
            MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Parent element state is null");
            property_param.header.element_state_change = MESHX_NOT_FOUND;
        }
        return this->get_parent_element()->on_model_cb(&property_param, sizeof(property_param));
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
 * @param[in] curr_el_state Pointer to meshx_gen_property_model_state_t containing the new state
 * @return
 *     - MESHX_SUCCESS: State change handled successfully
 *     - MESHX_INVALID_ARG: Invalid parameter
 */
MESHX_GEN_PROPERTY_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPropertyClientModel MESHX_GEN_PROPERTY_CLIENT_MODEL_TEMPLATE_PARAMS
    :: element_state_change_handle()
{
    meshx_gen_property_model_state_t *el_state =
        static_cast<meshx_gen_property_model_state_t*>(this->get_parent_element_state());
    if(!el_state)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid parameter in element_state_change_handle");
        return MESHX_INVALID_ARG;
    }
    if(el_state->property_id != model_state.property_id)
    {
        el_state->property_id = model_state.property_id;
    }
    if(el_state->access != model_state.access)
    {
        el_state->access = model_state.access;
    }
    // Note: property_value is a pointer and should be handled carefully
    // The actual value management is typically done at the element level
    return MESHX_SUCCESS;
}
/**
 * @brief Creates a meshXGenericPropertyClientModel instance based on a BLE device
 *
 * This function is used to create a meshXGenericPropertyClientModel instance based on a BLE device.
 * It takes the device structure, model ID, and parameters as input and returns a pointer to the created instance.
 *
 * @param[in] p_dev     Pointer to the device structure
 * @param[in] model_id  Model ID associated with the device
 * @param[in] params    Pointer to the parameters associated with the device
 *
 * @return Pointer to the created meshXGenericPropertyClientModel instance
 */
MESHX_GEN_PROPERTY_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPropertyClientModel MESHX_GEN_PROPERTY_CLIENT_MODEL_TEMPLATE_PARAMS
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
    if(model_id != MESHX_MODEL_ID_GEN_PROP_CLI)
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
 * This function takes a pointer to a meshx_gen_property_send_params_t structure as input
 * and sends the corresponding packet to the MeshX stack for processing.
 *
 * @param[in] params Pointer to the meshx_gen_property_send_params_t structure containing the parameters
 * @return MESHX_SUCCESS on success, or an error code on failure
 */
MESHX_GEN_PROPERTY_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPropertyClientModel MESHX_GEN_PROPERTY_CLIENT_MODEL_TEMPLATE_PARAMS
    :: model_send(meshx_gen_property_send_params_t *params)
{
    meshx_err_t err;
    meshx_gen_cli_set_t set; // Not used for property GET operations
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

    // Property client operations are primarily GET operations
    if (params->ctx->opcode == MESHX_MODEL_OP_GEN_MANUFACTURER_PROPERTIES_GET)
    {
        err = this->get_base_model()->plat_send_msg(&send_params);
    }
    else if (params->ctx->opcode == MESHX_MODEL_OP_GEN_MANUFACTURER_PROPERTY_GET)
    {
        // For specific property GET, we don't need to set anything in the union
        // The property ID would be passed through other means if needed
        err = this->get_base_model()->plat_send_msg(&send_params);
    }
    else if (params->ctx->opcode == MESHX_MODEL_OP_GEN_ADMIN_PROPERTIES_GET)
    {
        err = this->get_base_model()->plat_send_msg(&send_params);
    }
    else if (params->ctx->opcode == MESHX_MODEL_OP_GEN_ADMIN_PROPERTY_GET)
    {
        err = this->get_base_model()->plat_send_msg(&send_params);
    }
    else if (params->ctx->opcode == MESHX_MODEL_OP_GEN_USER_PROPERTIES_GET)
    {
        err = this->get_base_model()->plat_send_msg(&send_params);
    }
    else if (params->ctx->opcode == MESHX_MODEL_OP_GEN_USER_PROPERTY_GET)
    {
        err = this->get_base_model()->plat_send_msg(&send_params);
    }
    else if (params->ctx->opcode == MESHX_MODEL_OP_GEN_CLIENT_PROPERTIES_GET)
    {
        err = this->get_base_model()->plat_send_msg(&send_params);
    }
    else
    {
        err = MESHX_INVALID_ARG; // Invalid opcode
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Invalid opcode for Generic Property Client: %04x", params->ctx->opcode);
    }
    return err;
}
/**
 * @brief A template class for creating Generic Property Client models.
 *
 * This class is derived from meshXClientModel and provides a convenient interface for
 * creating Generic Property Client models. It handles the Generic Property state change
 * notifications from the MeshX stack and publishes the state change event to the
 * element layer.
 *
 * @tparam meshxBaseClientModel_t The type of the meshXBaseClientModel class to be used.
 * @tparam meshx_send_packet_params_t The type of the meshXSendPacketParams structure used
 * for sending packets.
 *
 * @param[in] parent_element A pointer to the parent element (meshXElementIF).
 */
MESHX_GEN_PROPERTY_CLIENT_MODEL_TEMPLATE_PROTO
meshXGenericPropertyClientModel MESHX_GEN_PROPERTY_CLIENT_MODEL_TEMPLATE_PARAMS
    ::meshXGenericPropertyClientModel(
        meshXElementIF *parent_element,
        meshx_ptr_t     parent_element_state)
    : meshXClientModel(nullptr, MESHX_MODEL_ID_GEN_PROP_CLI, parent_element, parent_element_state) {/* Used only for initialization of Parent Class */}

#endif /* CONFIG_ENABLE_GEN_PROPERTY_CLIENT */
/*******************************************************************************************************************/
#if CONFIG_ENABLE_GEN_ADMIN_PROPERTY_SERVER
/**
 * @brief Send a packet to the MeshX stack based on the given parameters
 *
 * This function takes a pointer to a meshx_gen_property_send_params_t structure as input
 * and sends the corresponding packet to the MeshX stack for processing.
 *
 * @param[in] params Pointer to the meshx_gen_property_send_params_t structure containing the parameters
 * @return MESHX_SUCCESS on success, or an error code on failure
 */
MESHX_GEN_ADMIN_PROPERTY_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericAdminPropertyServerModel MESHX_GEN_ADMIN_PROPERTY_SERVER_MODEL_TEMPLATE_PARAMS
    :: model_send(meshx_gen_property_send_params_t *params)
{
if (!params|| !params->model || !params->ctx)
    {
        return MESHX_INVALID_ARG;
    }
    params->ctx->opcode = MESHX_MODEL_OP_GEN_ADMIN_PROPERTIES_STATUS;
    meshx_gen_srv_state_change_t state_change = {
        .admin_property_set = {
            .id = params->property_id,
            .access = params->access,
            .value = params->property_value
        }
    };
    meshx_gen_server_send_params_t send_params = {
        .p_model = params->model,
        .p_ctx = params->ctx,
        .state_change = state_change,
        .data_len = sizeof(meshx_state_change_gen_admin_property_set_t)
    };
    return this->get_base_model()->plat_send_msg(&send_params);
}

/**
 * @brief Callback function for handling BLE mesh events for Generic Admin Property Server Model
 *
 * @param[in] p_dev     Pointer to the device structure
 * @param[in] model_id  Model ID associated with the event
 * @param[in] params    Pointer to the event parameters
 * @return MESHX_SUCCESS if successful, error code otherwise
 */
MESHX_GEN_ADMIN_PROPERTY_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericAdminPropertyServerModel MESHX_GEN_ADMIN_PROPERTY_SERVER_MODEL_TEMPLATE_PARAMS
    :: model_from_ble_cb(dev_struct_t *p_dev, control_task_msg_evt_t model_id, meshx_ptr_t params)
{
    if(!params || !p_dev)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid parameters");
        return MESHX_INVALID_ARG;
    }
    if(model_id != MESHX_MODEL_ID_GEN_ADMIN_PROP_SRV)
    {
        /* Callback triggered not for this model */
        return MESHX_SUCCESS;
    }

    auto *param = static_cast<meshx_gen_srv_cb_param_t *>(params);
    meshx_property_srv_el_msg_t msg = {
        .model = &param->model,
        .property_id = param->state_change.admin_property_set.id,
        .property_value = param->state_change.admin_property_set.value,
        .access = param->state_change.admin_property_set.access
    };

    if (this->get_parent_element()) {
        return this->get_parent_element()->on_model_cb(&msg);
    }

    MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Parent element is null");
    return MESHX_INVALID_STATE;
}

/**
 * @brief Constructor for Generic Admin Property Server Model
 *
 * @param[in] parent_element Pointer to the parent element (meshXElementIF)
 */
MESHX_GEN_ADMIN_PROPERTY_SERVER_MODEL_TEMPLATE_PROTO
meshXGenericAdminPropertyServerModel MESHX_GEN_ADMIN_PROPERTY_SERVER_MODEL_TEMPLATE_PARAMS
    ::meshXGenericAdminPropertyServerModel(
        meshXElementIF *parent_element,
        meshx_ptr_t     parent_element_state)
    : meshXServerModel(nullptr, MESHX_MODEL_ID_GEN_ADMIN_PROP_SRV, parent_element, parent_element_state) {}

/**
 * @brief Creates and initializes a server model instance for Generic Admin Property Server.
 *
 * This function handles the platform-specific model creation process for Generic Admin Property Server models.
 * It initializes server-specific features and cannot be overridden by derived classes.
 *
 * @return meshx_err_t Returns an error code indicating the result of the operation.
 *         - MESHX_SUCCESS on successful model creation and initialization
 *         - MESHX_ERR_NO_MEM if memory allocation fails
 *         - Other error codes for platform-specific failures
 *
 * @note This is a final function and cannot be overridden by derived classes.
 */
MESHX_GEN_ADMIN_PROPERTY_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericAdminPropertyServerModel MESHX_GEN_ADMIN_PROPERTY_SERVER_MODEL_TEMPLATE_PARAMS
    :: plat_model_create(void)
{
    meshx_ptr_t p_pub = this->get_pub_struct();
    meshx_ptr_t p_gen = this->get_gen_struct();
    meshx_err_t err = MESHX_SUCCESS;

    err = meshx_plat_admin_property_gen_srv_create(this->get_plat_model(), &p_pub, &p_gen);
    if(err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to create Generic Admin Property Server Model");
    }
    else
    {
        this->set_pub_struct(p_pub);
        this->set_gen_struct(p_gen);
    }
    return err;
}

/**
 * @brief Deletes the Generic Admin Property Server model and its associated resources.
 *
 * This function frees the memory allocated for the Generic Admin Property Server
 * and sets the pointer to NULL. It also deletes the model publication
 * resources associated with the server.
 *
 * @return
 *     - MESHX_SUCCESS: Model and publication deleted successfully.
 *     - MESHX_FAIL: Failed to delete the model or publication.
 */
MESHX_GEN_ADMIN_PROPERTY_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericAdminPropertyServerModel MESHX_GEN_ADMIN_PROPERTY_SERVER_MODEL_TEMPLATE_PARAMS
    :: plat_model_delete(void)
{
    meshx_ptr_t p_pub = this->get_pub_struct();
    meshx_ptr_t p_gen = this->get_gen_struct();

    meshx_err_t err = meshx_plat_gen_srv_delete(&p_pub, &p_gen);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to delete Generic Admin Property Server Model");
    }
    else
    {
        this->set_pub_struct(nullptr);
        this->set_gen_struct(nullptr);
    }
    return err;
}

#endif /* CONFIG_ENABLE_GEN_ADMIN_PROPERTY_SERVER */

#if CONFIG_ENABLE_GEN_MANU_PROP_SERVER
/**
 * @brief Send a packet to the MeshX stack based on the given parameters
 *
 * This function takes a pointer to a meshx_gen_property_send_params_t structure as input
 * and sends the corresponding packet to the MeshX stack for processing.
 *
 * @param[in] params Pointer to the meshx_gen_property_send_params_t structure containing the parameters
 * @return MESHX_SUCCESS on success, or an error code on failure
 */
MESHX_GEN_MANUFACTURER_PROPERTY_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericManufacturerPropertyServerModel MESHX_GEN_MANUFACTURER_PROPERTY_SERVER_MODEL_TEMPLATE_PARAMS
    :: model_send(meshx_gen_property_send_params_t *params)
{
if (!params|| !params->model || !params->ctx)
    {
        return MESHX_INVALID_ARG;
    }
    params->ctx->opcode = MESHX_MODEL_OP_GEN_MANUFACTURER_PROPERTY_STATUS;
    meshx_gen_srv_state_change_t state_change = {
        .manu_property_set = {
            .id = params->property_id,
            .access = params->access
        }
    };
    meshx_gen_server_send_params_t send_params = {
        .p_model = params->model,
        .p_ctx = params->ctx,
        .state_change = state_change,
        .data_len = sizeof(meshx_state_change_gen_manu_property_set_t)
    };
    return this->get_base_model()->plat_send_msg(&send_params);
}

/**
 * @brief Callback function for handling BLE mesh events for Generic Manufacturer Property Server Model
 *
 * @param[in] p_dev     Pointer to the device structure
 * @param[in] model_id  Model ID associated with the event
 * @param[in] params    Pointer to the event parameters
 * @return MESHX_SUCCESS if successful, error code otherwise
 */
MESHX_GEN_MANUFACTURER_PROPERTY_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericManufacturerPropertyServerModel MESHX_GEN_MANUFACTURER_PROPERTY_SERVER_MODEL_TEMPLATE_PARAMS
    :: model_from_ble_cb(dev_struct_t *p_dev, control_task_msg_evt_t model_id, meshx_ptr_t params)
{
    if(!params || !p_dev)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid parameters");
        return MESHX_INVALID_ARG;
    }
    if(model_id != MESHX_MODEL_ID_GEN_MANUFACTURER_PROP_SRV)
    {
        /* Callback triggered not for this model */
        return MESHX_SUCCESS;
    }

    auto *param = static_cast<meshx_gen_srv_cb_param_t *>(params);
    meshx_property_srv_el_msg_t msg;

    msg.model = &param->model;
    msg.access = param->state_change.manu_property_set.access;
    msg.property_id = param->state_change.manu_property_set.id;

    if (this->get_parent_element()) {
        return this->get_parent_element()->on_model_cb(&msg);
    }

    MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Parent element is null");
    return MESHX_INVALID_STATE;
}

/**
 * @brief Constructor for Generic Manufacturer Property Server Model
 *
 * @param[in] parent_element Pointer to the parent element (meshXElementIF)
 */
MESHX_GEN_MANUFACTURER_PROPERTY_SERVER_MODEL_TEMPLATE_PROTO
meshXGenericManufacturerPropertyServerModel MESHX_GEN_MANUFACTURER_PROPERTY_SERVER_MODEL_TEMPLATE_PARAMS
    ::meshXGenericManufacturerPropertyServerModel(
        meshXElementIF *parent_element,
        meshx_ptr_t     parent_element_state)
    : meshXServerModel(nullptr, MESHX_MODEL_ID_GEN_MANUFACTURER_PROP_SRV, parent_element, parent_element_state) {}

/**
 * @brief Creates and initializes a server model instance for Generic Manufacturer Property Server.
 *
 * This function handles the platform-specific model creation process for Generic Manufacturer Property Server models.
 * It initializes server-specific features and cannot be overridden by derived classes.
 *
 * @return meshx_err_t Returns an error code indicating the result of the operation.
 *         - MESHX_SUCCESS on successful model creation and initialization
 *         - MESHX_ERR_NO_MEM if memory allocation fails
 *         - Other error codes for platform-specific failures
 *
 * @note This is a final function and cannot be overridden by derived classes.
 */
MESHX_GEN_MANUFACTURER_PROPERTY_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericManufacturerPropertyServerModel MESHX_GEN_MANUFACTURER_PROPERTY_SERVER_MODEL_TEMPLATE_PARAMS
    :: plat_model_create(void)
{
    meshx_ptr_t p_pub = this->get_pub_struct();
    meshx_ptr_t p_gen = this->get_gen_struct();
    meshx_err_t err = MESHX_SUCCESS;

    err = meshx_plat_manu_property_gen_srv_create(this->get_plat_model(), &p_pub, &p_gen);
    if(err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to create Generic Manufacturer Property Server Model");
    }
    else
    {
        this->set_pub_struct(p_pub);
        this->set_gen_struct(p_gen);
    }
    return err;
}

/**
 * @brief Deletes the Generic Manufacturer Property Server model and its associated resources.
 *
 * This function frees the memory allocated for the Generic Manufacturer Property Server
 * and sets the pointer to NULL. It also deletes the model publication
 * resources associated with the server.
 *
 * @return
 *     - MESHX_SUCCESS: Model and publication deleted successfully.
 *     - MESHX_FAIL: Failed to delete the model or publication.
 */
MESHX_GEN_MANUFACTURER_PROPERTY_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericManufacturerPropertyServerModel MESHX_GEN_MANUFACTURER_PROPERTY_SERVER_MODEL_TEMPLATE_PARAMS
    :: plat_model_delete(void)
{
    meshx_ptr_t p_pub = this->get_pub_struct();
    meshx_ptr_t p_gen = this->get_gen_struct();

    meshx_err_t err = meshx_plat_gen_srv_delete(&p_pub, &p_gen);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to delete Generic Manufacturer Property Server Model");
    }
    else
    {
        this->set_pub_struct(nullptr);
        this->set_gen_struct(nullptr);
    }
    return err;
}

#endif /* CONFIG_ENABLE_GEN_MANU_PROP_SERVER */

#if CONFIG_ENABLE_GEN_USER_PROPERTY_SERVER
/**
 * @brief Send a packet to the MeshX stack based on the given parameters
 *
 * This function takes a pointer to a meshx_gen_property_send_params_t structure as input
 * and sends the corresponding packet to the MeshX stack for processing.
 *
 * @param[in] params Pointer to the meshx_gen_property_send_params_t structure containing the parameters
 * @return MESHX_SUCCESS on success, or an error code on failure
 */
MESHX_GEN_USER_PROPERTY_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericUserPropertyServerModel MESHX_GEN_USER_PROPERTY_SERVER_MODEL_TEMPLATE_PARAMS
    :: model_send(meshx_gen_property_send_params_t *params)
{
if (!params|| !params->model || !params->ctx)
    {
        return MESHX_INVALID_ARG;
    }
    params->ctx->opcode = MESHX_MODEL_OP_GEN_USER_PROPERTY_STATUS;
    meshx_gen_srv_state_change_t state_change = {
        .user_property_set = {
            .id = params->property_id,
            .value = params->property_value
        }
    };
    meshx_gen_server_send_params_t send_params = {
        .p_model = params->model,
        .p_ctx = params->ctx,
        .state_change = state_change,
        .data_len = sizeof(meshx_state_change_gen_user_property_set_t)
    };
    return this->get_base_model()->plat_send_msg(&send_params);
}

/**
 * @brief Callback function for handling BLE mesh events for Generic User Property Server Model
 *
 * @param[in] p_dev     Pointer to the device structure
 * @param[in] model_id  Model ID associated with the event
 * @param[in] params    Pointer to the event parameters
 * @return MESHX_SUCCESS if successful, error code otherwise
 */
MESHX_GEN_USER_PROPERTY_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericUserPropertyServerModel MESHX_GEN_USER_PROPERTY_SERVER_MODEL_TEMPLATE_PARAMS
    :: model_from_ble_cb(dev_struct_t *p_dev, control_task_msg_evt_t model_id, meshx_ptr_t params)
{
    if(!params || !p_dev)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid parameters");
        return MESHX_INVALID_ARG;
    }
    if(model_id != MESHX_MODEL_ID_GEN_USER_PROP_SRV)
    {
        /* Callback triggered not for this model */
        return MESHX_SUCCESS;
    }

    auto *param = static_cast<meshx_gen_srv_cb_param_t *>(params);
    meshx_property_srv_el_msg_t msg;

    msg.model = &param->model;
    msg.property_id = param->state_change.user_property_set.id;
    msg.property_value = param->state_change.user_property_set.value;

    if (this->get_parent_element()) {
        return this->get_parent_element()->on_model_cb(&msg);
    }

    MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Parent element is null");
    return MESHX_INVALID_STATE;
}

/**
 * @brief Constructor for Generic User Property Server Model
 *
 * @param[in] parent_element Pointer to the parent element (meshXElementIF)
 */
MESHX_GEN_USER_PROPERTY_SERVER_MODEL_TEMPLATE_PROTO
meshXGenericUserPropertyServerModel MESHX_GEN_USER_PROPERTY_SERVER_MODEL_TEMPLATE_PARAMS
    ::meshXGenericUserPropertyServerModel(
        meshXElementIF *parent_element,
        meshx_ptr_t     parent_element_state)
    : meshXServerModel(nullptr, MESHX_MODEL_ID_GEN_USER_PROP_SRV, parent_element, parent_element_state) {}

/**
 * @brief Creates and initializes a server model instance for Generic User Property Server.
 *
 * This function handles the platform-specific model creation process for Generic User Property Server models.
 * It initializes server-specific features and cannot be overridden by derived classes.
 *
 * @return meshx_err_t Returns an error code indicating the result of the operation.
 *         - MESHX_SUCCESS on successful model creation and initialization
 *         - MESHX_ERR_NO_MEM if memory allocation fails
 *         - Other error codes for platform-specific failures
 *
 * @note This is a final function and cannot be overridden by derived classes.
 */
MESHX_GEN_USER_PROPERTY_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericUserPropertyServerModel MESHX_GEN_USER_PROPERTY_SERVER_MODEL_TEMPLATE_PARAMS
    :: plat_model_create(void)
{
    meshx_ptr_t p_pub = this->get_pub_struct();
    meshx_ptr_t p_gen = this->get_gen_struct();
    meshx_err_t err = MESHX_SUCCESS;

    err = meshx_plat_user_property_gen_srv_create(this->get_plat_model(), &p_pub, &p_gen);
    if(err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to create Generic User Property Server Model");
    }
    else
    {
        this->set_pub_struct(p_pub);
        this->set_gen_struct(p_gen);
    }
    return err;
}

/**
 * @brief Deletes the Generic User Property Server model and its associated resources.
 *
 * This function frees the memory allocated for the Generic User Property Server
 * and sets the pointer to NULL. It also deletes the model publication
 * resources associated with the server.
 *
 * @return
 *     - MESHX_SUCCESS: Model and publication deleted successfully.
 *     - MESHX_FAIL: Failed to delete the model or publication.
 */
MESHX_GEN_USER_PROPERTY_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericUserPropertyServerModel MESHX_GEN_USER_PROPERTY_SERVER_MODEL_TEMPLATE_PARAMS
    :: plat_model_delete(void)
{
    meshx_ptr_t p_pub = this->get_pub_struct();
    meshx_ptr_t p_gen = this->get_gen_struct();

    meshx_err_t err = meshx_plat_gen_srv_delete(&p_pub, &p_gen);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to delete Generic User Property Server Model");
    }
    else
    {
        this->set_pub_struct(nullptr);
        this->set_gen_struct(nullptr);
    }
    return err;
}

#endif /* CONFIG_ENABLE_GEN_USER_PROPERTY_SERVER */

#if CONFIG_ENABLE_GEN_CLIENT_PROPERTY_SERVER
/**
 * @brief Send a packet to the MeshX stack based on the given parameters
 *
 * This function takes a pointer to a meshx_gen_property_send_params_t structure as input
 * and sends the corresponding packet to the MeshX stack for processing.
 *
 * @param[in] params Pointer to the meshx_gen_property_send_params_t structure containing the parameters
 * @return MESHX_SUCCESS on success, or an error code on failure
 */
MESHX_GEN_CLIENT_PROPERTY_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericClientPropertyServerModel MESHX_GEN_CLIENT_PROPERTY_SERVER_MODEL_TEMPLATE_PARAMS
    :: model_send(meshx_gen_property_send_params_t *params)
{
if (!params|| !params->model || !params->ctx)
    {
        return MESHX_INVALID_ARG;
    }
    params->ctx->opcode = MESHX_MODEL_OP_GEN_CLIENT_PROPERTIES_STATUS;
    // Client Property Server doesn't modify state, just responds with available properties
    meshx_gen_srv_state_change_t state_change = {0}; // Empty state change for client properties
    meshx_gen_server_send_params_t send_params = {
        .p_model = params->model,
        .p_ctx = params->ctx,
        .state_change = state_change,
        .data_len = 0  // No state data for client properties status
    };
    return this->get_base_model()->plat_send_msg(&send_params);
}

/**
 * @brief Constructor for Generic Client Property Server Model
 *
 * @param[in] parent_element Pointer to the parent element (meshXElementIF)
 */
MESHX_GEN_CLIENT_PROPERTY_SERVER_MODEL_TEMPLATE_PROTO
meshXGenericClientPropertyServerModel MESHX_GEN_CLIENT_PROPERTY_SERVER_MODEL_TEMPLATE_PARAMS
    ::meshXGenericClientPropertyServerModel(
        meshXElementIF *parent_element,
        meshx_ptr_t     parent_element_state)
    : meshXServerModel(nullptr, MESHX_MODEL_ID_GEN_CLIENT_PROP_SRV, parent_element, parent_element_state) {}

/**
 * @brief Callback function for handling BLE mesh events for Generic Client Property Server Model
 *
 * @param[in] p_dev     Pointer to the device structure
 * @param[in] model_id  Model ID associated with the event
 * @param[in] params    Pointer to the event parameters
 * @return MESHX_SUCCESS if successful, error code otherwise
 */
MESHX_GEN_CLIENT_PROPERTY_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericClientPropertyServerModel MESHX_GEN_CLIENT_PROPERTY_SERVER_MODEL_TEMPLATE_PARAMS
    :: model_from_ble_cb(dev_struct_t *p_dev, control_task_msg_evt_t model_id, meshx_ptr_t params)
{
    if(!params || !p_dev)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid parameters");
        return MESHX_INVALID_ARG;
    }
    if(model_id != MESHX_MODEL_ID_GEN_CLIENT_PROP_SRV)
    {
        /* Callback triggered not for this model */
        return MESHX_SUCCESS;
    }

    auto *param = static_cast<meshx_gen_srv_cb_param_t *>(params);
    meshx_property_srv_el_msg_t msg;

    msg.model = &param->model;
    msg.property_id = param->state_change.manu_property_set.id;
    msg.property_value = nullptr; // Client properties don't have values
    msg.access = 0; // Client properties have fixed access permissions

    if (this->get_parent_element()) {
        return this->get_parent_element()->on_model_cb(&msg);
    }

    MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Parent element is null");
    return MESHX_INVALID_STATE;
}

#endif /* CONFIG_ENABLE_GEN_CLIENT_PROPERTY_SERVER */
