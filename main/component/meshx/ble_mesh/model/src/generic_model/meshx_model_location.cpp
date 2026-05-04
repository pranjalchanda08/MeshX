/**
 * @file meshx_model_location.cpp
 * @brief Implementation of Generic Location Model classes for MeshX.
 *        This file contains the implementation of the Generic Location Server and Client models
 *        for the MeshX BLE mesh framework.
 *
 * Key Features:
 *  - Implements Bluetooth SIG-defined Generic Location model
 *  - Inherits from meshXServerModel and meshXClientModel templates
 *  - Provides standard Location control operations (Global/Local coordinates)
 *  - Integrates with MeshX transmission control
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#include <generic_model/meshx_model_location.hpp>

#if CONFIG_ENABLE_GEN_LOCATION_CLIENT
/**
 * @brief Handle Generic Location state change notifications from the MeshX stack.
 *
 * This function is responsible for handling Generic Location state change notifications
 * from the MeshX stack. It is called when the MeshX stack receives a state
 * change event from the Generic Location server model. The function publishes the
 * state change event to the control task framework, which in turn notifies the
 * application about the state change.
 *
 * @param[in] param  Pointer to the Generic Location client callback parameter structure.
 * @param[in] status Status of the state change event (success or timeout).
 *
 * @return
 *     - MESHX_SUCCESS: Successfully handled the state change notification.
 *     - MESHX_INVALID_ARG: One or more arguments are invalid.
 */
MESHX_GEN_LOCATION_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericLocationClientModel MESHX_GEN_LOCATION_CLIENT_MODEL_TEMPLATE_PARAMS
    :: meshx_state_change_notify(const meshx_gen_cli_cb_param_t *param, uint8_t status)
{
    if (!param){
        return MESHX_INVALID_ARG;
    }

    // Initialize model_state with default values
    model_state.global_latitude     = 0;
    model_state.global_longitude    = 0;
    model_state.global_altitude     = 0;
    model_state.local_north         = 0;
    model_state.local_east          = 0;
    model_state.local_altitude      = 0;
    model_state.floor_number        = 0;
    model_state.uncertainty         = 0;

    // Handle different location status types based on opcode
    switch(param->ctx.opcode) {
        case MESHX_MODEL_OP_GEN_LOC_GLOBAL_STATUS:
            model_state.global_latitude     = param->status.location_global_status.global_latitude;
            model_state.global_longitude    = param->status.location_global_status.global_longitude;
            model_state.global_altitude     = param->status.location_global_status.global_altitude;
            break;
        case MESHX_MODEL_OP_GEN_LOC_LOCAL_STATUS:
            model_state.local_north         = param->status.location_local_status.local_north;
            model_state.local_east          = param->status.location_local_status.local_east;
            model_state.local_altitude      = param->status.location_local_status.local_altitude;
            model_state.floor_number        = param->status.location_local_status.floor_number;
            model_state.uncertainty         = param->status.location_local_status.uncertainty;
            break;
        default:
            break;
    }

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
MESHX_GEN_LOCATION_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericLocationClientModel MESHX_GEN_LOCATION_CLIENT_MODEL_TEMPLATE_PARAMS
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
 * @brief Handle state change request from element.
 *
 * This function is called by the parent element when a state change request
 * is received. It validates the request and returns a result to the element.
 * Note: The actual state is maintained in the element layer, not the model layer.
 *
 * @param[in] curr_el_state Pointer to meshx_gen_location_model_state_t containing the new state
 * @return
 *     - MESHX_SUCCESS: State change handled successfully
 *     - MESHX_INVALID_ARG: Invalid parameter
 */
MESHX_GEN_LOCATION_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericLocationClientModel MESHX_GEN_LOCATION_CLIENT_MODEL_TEMPLATE_PARAMS
    :: element_state_change_handle()
{
    meshx_gen_location_model_state_t *el_state =
        static_cast<meshx_gen_location_model_state_t*>(this->get_parent_element_state());
    if(!el_state)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid parameter in element_state_change_handle");
        return MESHX_INVALID_ARG;
    }
    if(memcmp(&model_state, el_state, sizeof(meshx_gen_location_model_state_t)) != 0)
    {
        MESHX_LOGI(MODULE_ID_MODEL_SERVER,
            "Location state change request: global_lat=%d global_lon=%d local_lat=%d local_lon=%d",
            el_state->global_latitude, el_state->global_longitude, el_state->local_latitude, el_state->local_longitude);
        model_state = *el_state;
    }
    else
    {
        return MESHX_INVALID_STATE;
    }
    return MESHX_SUCCESS;
}
/**
 * @brief Creates a meshXGenericLocationClientModel instance based on a BLE device
 *
 * This function is used to create a meshXGenericLocationClientModel instance based on a BLE device.
 * It takes the device structure, model ID, and parameters as input and returns a pointer to the created instance.
 *
 * @param[in] p_dev     Pointer to the device structure
 * @param[in] model_id  Model ID associated with the device
 * @param[in] params    Pointer to the parameters associated with the device
 *
 * @return Pointer to the created meshXGenericLocationClientModel instance
 */
MESHX_GEN_LOCATION_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericLocationClientModel MESHX_GEN_LOCATION_CLIENT_MODEL_TEMPLATE_PARAMS
    :: model_from_ble_cb(
        dev_struct_t  *p_dev,
        evt_model_id_t model_id,
        meshx_ptr_t    params)
{
    if(!params || !p_dev)
    {
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Invalid parameters");
        return MESHX_INVALID_ARG;
    }
    if(model_id != MESHX_MODEL_ID_GEN_LOCATION_CLI)
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
 * This function takes a pointer to a meshx_gen_location_send_params_t structure as input
 * and sends the corresponding packet to the MeshX stack for processing.
 *
 * @param[in] params Pointer to the meshx_gen_location_send_params_t structure containing the parameters
 * @return MESHX_SUCCESS on success, or an error code on failure
 */
MESHX_GEN_LOCATION_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericLocationClientModel MESHX_GEN_LOCATION_CLIENT_MODEL_TEMPLATE_PARAMS
    :: model_send(meshx_gen_location_send_params_t *params)
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

    if (params->ctx->opcode == MESHX_MODEL_OP_GEN_LOC_GLOBAL_GET)
    {
        err = this->get_base_model()->plat_send_msg(&send_params);
    }
    else if (params->ctx->opcode == MESHX_MODEL_OP_GEN_LOC_LOCAL_GET)
    {
        err = this->get_base_model()->plat_send_msg(&send_params);
    }
    else
    {
        err = MESHX_INVALID_ARG; // Invalid opcode
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Invalid opcode for Generic Location Client: %04x", params->ctx->opcode);
    }
    return err;
}
/**
 * @brief A template class for creating Generic Location Client models.
 *
 * This class is derived from meshXClientModel and provides a convenient interface for
 * creating Generic Location Client models. It handles the Generic Location state change
 * notifications from the MeshX stack and publishes the state change event to the
 * element layer.
 *
 * @tparam meshxBaseClientModel_t The type of the meshXBaseClientModel class to be used.
 * @tparam meshx_send_packet_params_t The type of the meshXSendPacketParams structure used
 * for sending packets.
 *
 * @param[in] parent_element  A pointer to the parent element (meshXElementIF).
 */
MESHX_GEN_LOCATION_CLIENT_MODEL_TEMPLATE_PROTO
meshXGenericLocationClientModel MESHX_GEN_LOCATION_CLIENT_MODEL_TEMPLATE_PARAMS
    ::meshXGenericLocationClientModel(
        meshXElementIF *parent_element,
        meshx_ptr_t     parent_element_state,
        uint16_t        model_func_id)
    : meshXClientModel(nullptr, MESHX_MODEL_ID_GEN_LOCATION_CLI, parent_element, parent_element_state, model_func_id)
{
    /* Used only for initialization of Parent Class */
}

#endif /* CONFIG_ENABLE_GEN_LOCATION_CLIENT */
/*******************************************************************************************************************/
#if CONFIG_ENABLE_GEN_LOCATION_SERVER
/**
 * @brief Send a packet to the MeshX stack based on the given parameters
 *
 * This function takes a pointer to a meshx_gen_location_send_params_t structure as input
 * and sends the corresponding packet to the MeshX stack for processing.
 *
 * @param[in] params Pointer to the meshx_gen_location_send_params_t structure containing the parameters
 * @return MESHX_SUCCESS on success, or an error code on failure
 */
MESHX_GEN_LOCATION_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericLocationServerModel MESHX_GEN_LOCATION_SERVER_MODEL_TEMPLATE_PARAMS
    :: model_send(meshx_gen_location_send_params_t *params)
{
if (!params|| !params->model || !params->ctx)
    {
        return MESHX_INVALID_ARG;
    }
    // For server, we need to determine which status to send based on context
    // For simplicity, we'll send global location status
    params->ctx->opcode = MESHX_MODEL_OP_GEN_LOC_GLOBAL_STATUS;
    meshx_gen_srv_state_change_t state_change = {
        .loc_global_set = {
            .latitude   = params->global_latitude,
            .longitude  = params->global_longitude,
            .altitude   = params->global_altitude
        }
    };
    meshx_gen_server_send_params_t send_params = {
        .p_model        = params->model,
        .p_ctx          = params->ctx,
        .state_change   = state_change,
        .data_len       = sizeof(meshx_state_change_gen_loc_global_set_t)
    };
    return this->get_base_model()->plat_send_msg(&send_params);
}

/**
 * @brief Callback function for handling BLE mesh events for Generic Location Server Model
 *
 * @param[in] p_dev     Pointer to the device structure
 * @param[in] model_id  Model ID associated with the event
 * @param[in] params    Pointer to the event parameters
 * @return MESHX_SUCCESS if successful, error code otherwise
 */
MESHX_GEN_LOCATION_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericLocationServerModel MESHX_GEN_LOCATION_SERVER_MODEL_TEMPLATE_PARAMS
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
    if(model_id != MESHX_MODEL_ID_GEN_LOCATION_SRV)
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
            .global_latitude    = param->state_change.loc_global_set.latitude,
            .global_longitude   = param->state_change.loc_global_set.longitude,
            .global_altitude    = param->state_change.loc_global_set.altitude,
            .local_north        = param->state_change.loc_local_set.north,
            .local_east         = param->state_change.loc_local_set.east,
            .local_altitude     = param->state_change.loc_local_set.altitude,
            .floor_number       = param->state_change.loc_local_set.floor_number,
            .uncertainty        = param->state_change.loc_local_set.uncertainty
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
MESHX_GEN_LOCATION_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericLocationServerModel MESHX_GEN_LOCATION_SERVER_MODEL_TEMPLATE_PARAMS
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
MESHX_GEN_LOCATION_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericLocationServerModel MESHX_GEN_LOCATION_SERVER_MODEL_TEMPLATE_PARAMS
    :: element_state_change_handle(void)
{
    auto *el_state =
        static_cast<meshx_gen_location_model_state_t*>(this->get_parent_element_state());

    if (!el_state) {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid parameter in element_state_change_handle");
        return MESHX_INVALID_ARG;
    }

    if(memcmp(&model_state, el_state, sizeof(meshx_gen_location_model_state_t)) != 0)
    {
        MESHX_LOGI(MODULE_ID_MODEL_SERVER,
            "Location state change request: global_lat=%d global_lon=%d local_lat=%d local_lon=%d",
            el_state->global_latitude, el_state->global_longitude, el_state->local_latitude, el_state->local_longitude);
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
 * @brief Constructor for Generic Location Server Model
 *
 * @param[in] parent_element Pointer to the parent element (meshXElementIF)
 */
MESHX_GEN_LOCATION_SERVER_MODEL_TEMPLATE_PROTO
meshXGenericLocationServerModel MESHX_GEN_LOCATION_SERVER_MODEL_TEMPLATE_PARAMS
    ::meshXGenericLocationServerModel(
        meshXElementIF *parent_element,
        meshx_ptr_t     parent_element_state,
        uint16_t        model_func_id
    )
    : meshXServerModel(nullptr, MESHX_MODEL_ID_GEN_LOCATION_SRV, parent_element, parent_element_state, model_func_id)
{}

/**
 * @brief Creates and initializes a server model instance for Generic Location Server.
 *
 * This function handles the platform-specific model creation process for Generic Location Server models.
 * It initializes server-specific features and cannot be overridden by derived classes.
 *
 * @return meshx_err_t Returns an error code indicating the result of the operation.
 *         - MESHX_SUCCESS on successful model creation and initialization
 *         - MESHX_ERR_NO_MEM if memory allocation fails
 *         - Other error codes for platform-specific failures
 *
 * @note This is a final function and cannot be overridden by derived classes.
 */
MESHX_GEN_LOCATION_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericLocationServerModel MESHX_GEN_LOCATION_SERVER_MODEL_TEMPLATE_PARAMS
    :: plat_model_create(MESHX_MODEL* p_plat_model_ptr)
{
    if (p_plat_model_ptr) {
        this->set_plat_model(p_plat_model_ptr);
    }
    meshx_ptr_t p_pub = this->get_pub_struct();
    meshx_ptr_t p_gen = this->get_gen_struct();
    meshx_err_t err = MESHX_SUCCESS;

    err = meshx_plat_location_gen_srv_create(this->get_plat_model(), &p_pub, &p_gen);
    if(err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to create Generic Location Server Model");
    }
    else
    {
        this->set_pub_struct(p_pub);
        this->set_gen_struct(p_gen);
    }
    return err;
}

/**
 * @brief Deletes the Generic Location Server model and its associated resources.
 *
 * This function frees the memory allocated for the Generic Location Server
 * and sets the pointer to NULL. It also deletes the model publication
 * resources associated with the server.
 *
 * @return
 *     - MESHX_SUCCESS: Model and publication deleted successfully.
 *     - MESHX_FAIL: Failed to delete the model or publication.
 */
MESHX_GEN_LOCATION_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericLocationServerModel MESHX_GEN_LOCATION_SERVER_MODEL_TEMPLATE_PARAMS
    :: plat_model_delete(void)
{
    meshx_ptr_t p_pub = this->get_pub_struct();
    meshx_ptr_t p_gen = this->get_gen_struct();

    meshx_err_t err = meshx_plat_gen_srv_delete(&p_pub, &p_gen);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to delete Generic Location Server Model");
    }
    else
    {
        this->set_pub_struct(nullptr);
        this->set_gen_struct(nullptr);
    }
    return err;
}
#endif /* CONFIG_ENABLE_GEN_LOCATION_SERVER */

#if CONFIG_ENABLE_GEN_LOCATION_SETUP_SERVER
/**
 * @brief Send a packet to the MeshX stack based on the given parameters
 *
 * This function takes a pointer to a meshx_gen_location_send_params_t structure as input
 * and sends the corresponding packet to the MeshX stack for processing.
 *
 * @param[in] params Pointer to the meshx_gen_location_send_params_t structure containing the parameters
 * @return MESHX_SUCCESS on success, or an error code on failure
 */
MESHX_GEN_LOCATION_SETUP_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericLocationSetupServerModel MESHX_GEN_LOCATION_SETUP_SERVER_MODEL_TEMPLATE_PARAMS
    :: model_send(meshx_gen_location_send_params_t *params)
{
if (!params|| !params->model || !params->ctx)
    {
        return MESHX_INVALID_ARG;
    }
    params->ctx->opcode = MESHX_MODEL_OP_GEN_LOC_GLOBAL_STATUS;
    meshx_gen_srv_state_change_t state_change = {
        .loc_global_set = {
            .latitude   = params->global_latitude,
            .longitude  = params->global_longitude,
            .altitude   = params->global_altitude
        }
    };
    meshx_gen_server_send_params_t send_params = {
        .p_model      = params->model,
        .p_ctx        = params->ctx,
        .state_change = state_change,
        .data_len = sizeof(meshx_state_change_gen_loc_global_set_t)
    };
    return this->get_base_model()->plat_send_msg(&send_params);
}

/**
 * @brief Constructor for Generic Location Setup Server Model
 *
 * @param[in] parent_element Pointer to the parent element (meshXElementIF)
 */
MESHX_GEN_LOCATION_SETUP_SERVER_MODEL_TEMPLATE_PROTO
meshXGenericLocationSetupServerModel MESHX_GEN_LOCATION_SETUP_SERVER_MODEL_TEMPLATE_PARAMS
    ::meshXGenericLocationSetupServerModel(
        meshXElementIF *parent_element,
        meshx_ptr_t     parent_element_state,
        uint16_t        model_func_id
    )
    : meshXServerModel(nullptr, MESHX_MODEL_ID_GEN_LOCATION_SETUP_SRV, parent_element, parent_element_state, model_func_id)
{}

/**
 * @brief Callback function for handling BLE mesh events for Generic Location Setup Server Model
 *
 * @param[in] p_dev     Pointer to the device structure
 * @param[in] model_id  Model ID associated with the event
 * @param[in] params    Pointer to the event parameters
 * @return MESHX_SUCCESS if successful, error code otherwise
 */
MESHX_GEN_LOCATION_SETUP_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericLocationSetupServerModel MESHX_GEN_LOCATION_SETUP_SERVER_MODEL_TEMPLATE_PARAMS
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
    if(model_id != MESHX_MODEL_ID_GEN_LOCATION_SETUP_SRV)
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
            .global_latitude    = param->state_change.loc_global_set.latitude,
            .global_longitude   = param->state_change.loc_global_set.longitude,
            .global_altitude    = param->state_change.loc_global_set.altitude,
            .local_north        = param->state_change.loc_local_set.north,
            .local_east         = param->state_change.loc_local_set.east,
            .local_altitude     = param->state_change.loc_local_set.altitude,
            .floor_number       = param->state_change.loc_local_set.floor_number,
            .uncertainty        = param->state_change.loc_local_set.uncertainty
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
MESHX_GEN_LOCATION_SETUP_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericLocationSetupServerModel MESHX_GEN_LOCATION_SETUP_SERVER_MODEL_TEMPLATE_PARAMS
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
#endif /* CONFIG_ENABLE_GEN_LOCATION_SETUP_SERVER */
