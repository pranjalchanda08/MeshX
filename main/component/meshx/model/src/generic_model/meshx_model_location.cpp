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
#include <meshx_element_class.hpp>

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
    :: meshx_state_change_notify(const meshx_gen_cli_cb_param_t *param, uint8_t status) const
{
    if (!param){
        return MESHX_INVALID_ARG;
    }

    meshx_location_cli_el_msg_t location_param = {
        .err_code = status,
        .model = param->model,
        .ctx = param->ctx,
        .global_latitude = 0,
        .global_longitude = 0,
        .global_altitude = 0,
        .local_north = 0,
        .local_east = 0,
        .local_altitude = 0,
        .floor_number = 0,
        .uncertainty = 0
    };

    // Handle different location status types based on opcode
    switch(param->ctx.opcode) {
        case MESHX_MODEL_OP_GEN_LOC_GLOBAL_STATUS:
            location_param.global_latitude = param->status.location_global_status.global_latitude;
            location_param.global_longitude = param->status.location_global_status.global_longitude;
            location_param.global_altitude = param->status.location_global_status.global_altitude;
            break;
        case MESHX_MODEL_OP_GEN_LOC_LOCAL_STATUS:
            location_param.local_north = param->status.location_local_status.local_north;
            location_param.local_east = param->status.location_local_status.local_east;
            location_param.local_altitude = param->status.location_local_status.local_altitude;
            location_param.floor_number = param->status.location_local_status.floor_number;
            location_param.uncertainty = param->status.location_local_status.uncertainty;
            break;
        default:
            break;
    }

    /* Send the state change event to the respective Element */
    if (this->get_parent_element()) {
        return this->get_parent_element()->on_model_cb(&location_param);
    } else {
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Parent element is null");
    }

    return MESHX_INVALID_STATE;
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
        dev_struct_t *p_dev,
        control_task_msg_evt_t model_id,
        meshx_ptr_t params)
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

    return std::to_underlying(param->evt) == std::to_underlying(meshx_base_cli_evt::MESHX_BASE_CLI_TIMEOUT) ?
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
 * @param[in] p_plat_model  A pointer to the platform model (MESHX_MODEL).
 * @param[in] model_id      The unique identifier of the BLE mesh model.
 * @param[in] parent_element  A pointer to the parent element (meshXElementIF).
 */
MESHX_GEN_LOCATION_CLIENT_MODEL_TEMPLATE_PROTO
meshXGenericLocationClientModel MESHX_GEN_LOCATION_CLIENT_MODEL_TEMPLATE_PARAMS
    ::meshXGenericLocationClientModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element)
    : meshXClientModel(p_plat_model, model_id, parent_element) {/* Used only for initialization of Parent Class */}

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
            .latitude = params->global_latitude,
            .longitude = params->global_longitude,
            .altitude = params->global_altitude
        }
    };
    meshx_gen_server_send_params_t send_params = {
        .p_model = params->model,
        .p_ctx = params->ctx,
        .state_change = state_change,
        .data_len = sizeof(meshx_state_change_gen_loc_global_set_t)
    };
    return this->get_base_model()->plat_send_msg(&send_params);
}

MESHX_GEN_LOCATION_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericLocationServerModel MESHX_GEN_LOCATION_SERVER_MODEL_TEMPLATE_PARAMS
    :: model_from_ble_cb(dev_struct_t *p_dev, control_task_msg_evt_t model_id, meshx_ptr_t params)
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
    meshx_location_srv_el_msg_t msg = {
        .model = &param->model,
        .global = {
            .latitude = param->state_change.loc_global_set.latitude,
            .longitude = param->state_change.loc_global_set.longitude,
            .altitude = param->state_change.loc_global_set.altitude
        },
        .local = {
            .north = param->state_change.loc_local_set.north,
            .east = param->state_change.loc_local_set.east,
            .altitude = param->state_change.loc_local_set.altitude,
            .floor_number = param->state_change.loc_local_set.floor_number,
            .uncertainty = param->state_change.loc_local_set.uncertainty
        }
    };

    if (this->get_parent_element()) {
        return this->get_parent_element()->on_model_cb(&msg);
    }

    MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Parent element is null");
    return MESHX_INVALID_STATE;
}

MESHX_GEN_LOCATION_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericLocationServerModel MESHX_GEN_LOCATION_SERVER_MODEL_TEMPLATE_PARAMS
    :: plat_model_create(void)
{
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
            .latitude = params->global_latitude,
            .longitude = params->global_longitude,
            .altitude = params->global_altitude
        }
    };
    meshx_gen_server_send_params_t send_params = {
        .p_model = params->model,
        .p_ctx = params->ctx,
        .state_change = state_change,
        .data_len = sizeof(meshx_state_change_gen_loc_global_set_t)
    };
    return this->get_base_model()->plat_send_msg(&send_params);
}

MESHX_GEN_LOCATION_SETUP_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericLocationSetupServerModel MESHX_GEN_LOCATION_SETUP_SERVER_MODEL_TEMPLATE_PARAMS
    :: model_from_ble_cb(dev_struct_t *p_dev, control_task_msg_evt_t model_id, meshx_ptr_t params)
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
    // Setup server forwards state changes to its base server model
    meshx_location_srv_el_msg_t msg = {
        .model = &param->model,
        .global = {
            .latitude = param->state_change.loc_global_set.latitude,
            .longitude = param->state_change.loc_global_set.longitude,
            .altitude = param->state_change.loc_global_set.altitude
        },
        .local = {
            .north = param->state_change.loc_local_set.north,
            .east = param->state_change.loc_local_set.east,
            .altitude = param->state_change.loc_local_set.altitude,
            .floor_number = param->state_change.loc_local_set.floor_number,
            .uncertainty = param->state_change.loc_local_set.uncertainty
        }
    };

    if (this->get_parent_element()) {
        return this->get_parent_element()->on_model_cb(&msg);
    }

    MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Parent element is null");
    return MESHX_INVALID_STATE;
}
#endif /* CONFIG_ENABLE_GEN_LOCATION_SETUP_SERVER */
