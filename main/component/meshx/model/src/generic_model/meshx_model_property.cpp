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
    :: meshx_state_change_notify(const meshx_gen_cli_cb_param_t *param, uint8_t status) const
{
    if (!param){
        return MESHX_INVALID_ARG;
    }

    meshx_property_cli_el_msg_t property_param = {
        .err_code = status,
        .model = param->model,
        .ctx = param->ctx,
        .property_id = 0, // Property ID will be determined by context
        .property_value = nullptr,
        .access = 0
    };

    // Handle different property status types based on opcode
    switch(param->ctx.opcode) {
        case MESHX_MODEL_OP_GEN_USER_PROPERTIES_STATUS:
            break;
        case MESHX_MODEL_OP_GEN_USER_PROPERTY_STATUS:
            property_param.property_id = param->status.user_property_status.property_id;
            property_param.access = param->status.user_property_status.user_access;
            break;
        case MESHX_MODEL_OP_GEN_ADMIN_PROPERTIES_STATUS:
            break;
        case MESHX_MODEL_OP_GEN_ADMIN_PROPERTY_STATUS:
            property_param.property_id = param->status.admin_property_status.property_id;
            property_param.access = param->status.admin_property_status.user_access;
            break;
        case MESHX_MODEL_OP_GEN_MANUFACTURER_PROPERTIES_STATUS:
            break;
        case MESHX_MODEL_OP_GEN_MANUFACTURER_PROPERTY_STATUS:
            property_param.property_id = param->status.manufacturer_property_status.property_id;
            property_param.access = param->status.manufacturer_property_status.user_access;
            break;
        case MESHX_MODEL_OP_GEN_CLIENT_PROPERTIES_STATUS:
            break;
        default:
            break;
    }

    /* Send the state change event to the respective Element */
    if (this->get_parent_element()) {
        return this->get_parent_element()->on_model_cb(&property_param);
    } else {
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Parent element is null");
    }

    return MESHX_INVALID_STATE;
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
 * @param[in] p_plat_model  A pointer to the platform model (MESHX_MODEL).
 * @param[in] model_id      The unique identifier of the BLE mesh model.
 * @param[in] parent_element A pointer to the parent element (meshXElementIF).
 */
MESHX_GEN_PROPERTY_CLIENT_MODEL_TEMPLATE_PROTO
meshXGenericPropertyClientModel MESHX_GEN_PROPERTY_CLIENT_MODEL_TEMPLATE_PARAMS
    ::meshXGenericPropertyClientModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element)
    : meshXClientModel(p_plat_model, model_id, parent_element) {/* Used only for initialization of Parent Class */}

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
