/**
 * @file meshx_base_model_generic.cpp
 * @brief Implementation of the MeshX Generic Client model for BLE mesh nodes.
 *
 * This file contains the implementation of the Generic Client model class that extends
 * the template-based meshXBaseClientModel. It provides specific functionality for
 * Generic BLE mesh models including message validation, opcode handling, and platform
 * integration for the MeshX framework.
 *
 * Key Features:
 * - Generic Client model implementation with type-safe templates
 * - Opcode validation for unacknowledged and GET request messages
 * - Platform-specific message sending with TXCM integration
 * - Enhanced error handling and debugging capabilities
 * - Static wrapper functions for C-style callback compatibility
 * - Model initialization and lifecycle management
 *
 * Supported Generic Models:
 * - Generic OnOff Client
 * - Generic Level Client
 * - Generic Power OnOff Client
 * - Generic Power Level Client
 * - Generic Battery Client
 * - Generic Location Client
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright © 2024 - 2025 MeshX
 */

#include "meshx_txcm.h"
#include "meshx_base_model_generic.hpp"

/*********************************************************************************************************
 * meshXBaseGenericClientModel Implementation
 ********************************************************************************************************/
#if CONFIG_ENABLE_GEN_CLIENT

constexpr uint32_t MESHX_CLIENT_INIT_MAGIC_NO = 0x1121;

/**
 * @brief Validates if the given model ID corresponds to a supported Generic Client model.
 *
 * This function performs validation of model IDs to ensure they correspond to supported
 * Generic Client models in the MeshX BLE mesh framework. This is the Generic-specific
 * implementation of the virtual validate_client_model_id function.
 *
 * Supported Generic Client Model IDs:
 * - MESHX_MODEL_ID_GEN_ONOFF_CLI: Generic OnOff Client
 * - MESHX_MODEL_ID_GEN_LEVEL_CLI: Generic Level Client
 * - MESHX_MODEL_ID_GEN_POWER_ONOFF_CLI: Generic Power OnOff Client
 * - MESHX_MODEL_ID_GEN_POWER_LEVEL_CLI: Generic Power Level Client
 * - MESHX_MODEL_ID_GEN_BATTERY_CLI: Generic Battery Client
 * - MESHX_MODEL_ID_GEN_LOCATION_CLI: Generic Location Client
 *
 * @param[in] model_id The 32-bit model ID to validate against supported Generic client models.
 *
 * @retval MESHX_SUCCESS The model ID is valid and supported.
 * @retval MESHX_FAIL The model ID is not supported or invalid.
 *
 * @note This function is specific to Generic models. Other client types (Lighting, Sensor, etc.)
 *       will have their own implementations with different supported model IDs.
 */
MESHX_BASE_GENERIC_CLIENT_TEMPLATE_PROTO
meshx_err_t meshXBaseGenericClientModel MESHX_BASE_GENERIC_CLIENT_TEMPLATE_PARAMS
    ::validate_client_model_id(uint32_t model_id)
{
    switch (model_id)
    {
        case MESHX_MODEL_ID_GEN_ONOFF_CLI:
        case MESHX_MODEL_ID_GEN_LEVEL_CLI:
        case MESHX_MODEL_ID_GEN_POWER_ONOFF_CLI:
        case MESHX_MODEL_ID_GEN_POWER_LEVEL_CLI:
        case MESHX_MODEL_ID_GEN_BATTERY_CLI:
        case MESHX_MODEL_ID_GEN_LOCATION_CLI:
        // Add more Generic client model IDs as needed
            return MESHX_SUCCESS;
        default:
            MESHX_LOGW(MODULE_ID_MODEL_CLIENT, "Invalid Generic client model ID: %08" PRIx32, model_id);
            return MESHX_FAIL;
    }
}

/**
 * @brief Validates if a given opcode corresponds to an unacknowledged (SET_UNACK) message.
 *
 * This function performs opcode validation to determine if the specified opcode
 * represents a SET_UNACK operation for Generic BLE mesh models. Unacknowledged
 * messages do not require a response from the server, making them suitable for
 * broadcast operations and scenarios where reliability is less critical than performance.
 *
 * Supported Unacknowledged Opcodes:
 * - MESHX_MODEL_OP_GEN_ONOFF_SET_UNACK: Generic OnOff Set Unacknowledged
 * - MESHX_MODEL_OP_GEN_LEVEL_SET_UNACK: Generic Level Set Unacknowledged
 * - MESHX_MODEL_OP_GEN_ONPOWERUP_SET_UNACK: Generic OnPowerUp Set Unacknowledged
 * - MESHX_MODEL_OP_GEN_POWER_LEVEL_SET_UNACK: Generic Power Level Set Unacknowledged
 * - MESHX_MODEL_OP_GEN_LOC_GLOBAL_SET_UNACK: Generic Location Global Set Unacknowledged
 * - MESHX_MODEL_OP_GEN_LOC_LOCAL_SET_UNACK: Generic Location Local Set Unacknowledged
 * - MESHX_MODEL_OP_GEN_MANUFACTURER_PROPERTY_SET_UNACK: Manufacturer Property Set Unacknowledged
 * - MESHX_MODEL_OP_GEN_ADMIN_PROPERTY_SET_UNACK: Admin Property Set Unacknowledged
 * - MESHX_MODEL_OP_GEN_USER_PROPERTY_SET_UNACK: User Property Set Unacknowledged
 *
 * @param[in] opcode The 32-bit BLE mesh model opcode to validate.
 *
 * @retval MESHX_SUCCESS The opcode is a valid unacknowledged SET operation.
 * @retval MESHX_FAIL The opcode is not an unacknowledged SET operation.
 *
 * @note This function is used internally for transmission control and reliability decisions.
 * @note Unacknowledged messages are typically used for broadcast/multicast scenarios.
 * @see meshx_is_get_req_opcode() for GET request validation.
 */
MESHX_BASE_GENERIC_CLIENT_TEMPLATE_PROTO
meshx_err_t meshXBaseGenericClientModel MESHX_BASE_GENERIC_CLIENT_TEMPLATE_PARAMS
    ::meshx_is_unack_opcode(uint32_t opcode)
{
    switch(opcode)
    {
        case MESHX_MODEL_OP_GEN_ONOFF_SET_UNACK:
        case MESHX_MODEL_OP_GEN_LEVEL_SET_UNACK:
        case MESHX_MODEL_OP_GEN_ONPOWERUP_SET_UNACK:
        case MESHX_MODEL_OP_GEN_POWER_LEVEL_SET_UNACK:
        case MESHX_MODEL_OP_GEN_LOC_GLOBAL_SET_UNACK:
        case MESHX_MODEL_OP_GEN_LOC_LOCAL_SET_UNACK:
        case MESHX_MODEL_OP_GEN_MANUFACTURER_PROPERTY_SET_UNACK:
        case MESHX_MODEL_OP_GEN_ADMIN_PROPERTY_SET_UNACK:
        case MESHX_MODEL_OP_GEN_USER_PROPERTY_SET_UNACK:
            return MESHX_SUCCESS;
        default:
            return MESHX_FAIL;
    }
}

/**
 * @brief Checks if a given opcode is a GET request opcode.
 *
 * This function checks if a given opcode is a GET request opcode for the
 * generic client model.
 *
 * @param[in] opcode Opcode to check.
 *
 * @return MESHX_SUCCESS if the opcode is a GET request opcode, MESHX_FAIL otherwise.
 */
MESHX_BASE_GENERIC_CLIENT_TEMPLATE_PROTO
meshx_err_t meshXBaseGenericClientModel MESHX_BASE_GENERIC_CLIENT_TEMPLATE_PARAMS
    ::meshx_is_get_req_opcode(uint16_t opcode)
{
    switch(opcode)
    {
        case MESHX_MODEL_OP_GEN_ONOFF_GET:
        case MESHX_MODEL_OP_GEN_LEVEL_GET:
        case MESHX_MODEL_OP_GEN_ONPOWERUP_GET:
        case MESHX_MODEL_OP_GEN_POWER_LEVEL_GET:
        case MESHX_MODEL_OP_GEN_BATTERY_GET:
        case MESHX_MODEL_OP_GEN_LOC_GLOBAL_GET:
        case MESHX_MODEL_OP_GEN_LOC_LOCAL_GET:
        case MESHX_MODEL_OP_GEN_MANUFACTURER_PROPERTIES_GET:
        case MESHX_MODEL_OP_GEN_MANUFACTURER_PROPERTY_GET:
        case MESHX_MODEL_OP_GEN_ADMIN_PROPERTIES_GET:
        case MESHX_MODEL_OP_GEN_ADMIN_PROPERTY_GET:
        case MESHX_MODEL_OP_GEN_USER_PROPERTIES_GET:
        case MESHX_MODEL_OP_GEN_USER_PROPERTY_GET:
        case MESHX_MODEL_OP_GEN_CLIENT_PROPERTIES_GET:
            return MESHX_SUCCESS;
        default:
            return MESHX_FAIL;
    }
}

/**
 * @brief Send a message using the generic client model.
 *
 * This function sends a message from a Generic Client model to a specified address
 * within the BLE Mesh network, using the provided opcode and parameters.
 *
 * @param[in] msg_param   Pointer to the message parameters structure.
 * @param[in] msg_param_len Length of the message parameters structure in bytes.
 *
 * @return meshx_err_t  Result of the operation. Returns MESHX_OK on success or an error code on failure.
 */
MESHX_BASE_GENERIC_CLIENT_TEMPLATE_PROTO
meshx_err_t meshXBaseGenericClientModel MESHX_BASE_GENERIC_CLIENT_TEMPLATE_PARAMS
    ::meshx_gen_client_txcm_fn_model_send(meshx_gen_client_msg_ctx_t *msg_param, size_t msg_param_len)
{
    if(msg_param == nullptr || msg_param_len != sizeof(meshx_gen_client_msg_ctx_t))
    {
        return MESHX_INVALID_ARG;
    }

    return meshx_plat_gen_cli_send_msg(
        msg_param->model,
        &msg_param->state,
        msg_param->opcode,
        msg_param->addr,
        msg_param->net_idx,
        msg_param->app_idx,
        (meshx_is_get_req_opcode(msg_param->opcode) == MESHX_SUCCESS)
    );
}

/**
 * @brief Constructor for the meshXBaseGenericClientModel class.
 *
 * This constructor is responsible for initializing a meshXBaseGenericClientModel
 * object with the given model ID and control task message handle.
 *
 * @param model_id The model ID associated with the generic client model
 * @param from_ble_cb The control task message handle associated with the generic client model
 *
 * @return None
 */
MESHX_BASE_GENERIC_CLIENT_TEMPLATE_PROTO
meshXBaseGenericClientModel MESHX_BASE_GENERIC_CLIENT_TEMPLATE_PARAMS
    ::meshXBaseGenericClientModel(uint32_t model_id, const control_msg_cb& from_ble_cb)
    : meshXBaseClientModel(model_id, from_ble_cb)
{
    set_status(meshXBaseGenericClientModel::plat_model_init());
    if (get_status() != MESHX_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "plat_model_init failed");
        return;
    }
}

/**
 * @brief Initialization function for the Generic OnOff Client model.
 *
 * This function is responsible for initializing the Generic OnOff Client model.
 * It checks if the model has been initialized before, and if not, it registers
 * the callback function for transaction control messages and calls the platform
 * specific initialization function.
 *
 * @return meshx_err_t containing the result of the initialization operation.
 */
MESHX_BASE_GENERIC_CLIENT_TEMPLATE_PROTO
meshx_err_t meshXBaseGenericClientModel MESHX_BASE_GENERIC_CLIENT_TEMPLATE_PARAMS
    :: plat_model_init(void)
{
    if(plat_client_init == MESHX_CLIENT_INIT_MAGIC_NO)
    {
        return MESHX_SUCCESS;
    }
    plat_client_init = MESHX_CLIENT_INIT_MAGIC_NO;

    if(meshx_err_t err = meshx_txcm_event_cb_reg((meshx_txcm_cb_t) &base_handle_txcm_msg);
        err != MESHX_SUCCESS
    ){

        return err;
    }
    return meshx_plat_gen_cli_init();
}

/**
 * @brief Send a message using the generic client model.
 *
 * This function sends a message from a generic client model to a specified address
 * within the BLE Mesh network, using the provided opcode and parameters.
 *
 * @param[in] params Pointer to the structure containing the message parameters to set.
 *
 * @return meshx_err_t containing the result of the send operation. Returns MESHX_OK on success or an error code on failure.
 */
MESHX_BASE_GENERIC_CLIENT_TEMPLATE_PROTO
meshx_err_t meshXBaseGenericClientModel MESHX_BASE_GENERIC_CLIENT_TEMPLATE_PARAMS
    :: plat_send_msg(meshx_gen_client_send_params_t *params)
{
    if (!params || !params->model || !params->state)
    {
        return MESHX_INVALID_ARG;
    }

    meshx_err_t err = MESHX_SUCCESS;
    bool is_unack = meshx_is_unack_opcode(params->opcode) == MESHX_SUCCESS;
    /* Broadcast / Multicast will not be sending an ACK. Hence, it is not required to queue */
    meshx_txcm_sig_t req_type = (is_unack || (MESHX_ADDR_IS_UNICAST(params->addr) == false)) ?
                                MESHX_TXCM_SIG_DIRECT_SEND : MESHX_TXCM_SIG_ENQ_SEND;

    meshx_gen_client_msg_ctx_t send_msg;

    send_msg.model   = params->model;
    send_msg.opcode  = params->opcode;
    send_msg.addr    = params->addr;
    send_msg.net_idx = params->net_idx;
    send_msg.app_idx = params->app_idx;

    memcpy(&send_msg.state, params->state, sizeof(send_msg.state));

    err = meshx_txcm_request_send(
        req_type,
        send_msg.addr,
        &send_msg,
        sizeof(send_msg),
        (meshx_txcm_fn_model_send_t) &meshXBaseGenericClientModel::meshx_gen_client_txcm_fn_model_send
    );
    if(err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Failed to send message: %p", (meshx_ptr_t) err);
    }
    return err;
}

#endif /* CONFIG_ENABLE_GEN_CLIENT */

/*********************************************************************************************************
 * meshXBaseGenericServerModel
 ********************************************************************************************************/
#if CONFIG_ENABLE_GEN_SERVER

constexpr uint32_t MESHX_SERVER_INIT_MAGIC_NO = 0x1121;
/**
 * @brief Constructor for the meshXBaseGenericServerModel class.
 *
 * This constructor initializes a meshXBaseGenericServerModel object with the given
 * model ID and control task message handle.
 *
 * @param model_id The model ID associated with the generic server model.
 * @param from_ble_cb The control task message handle associated with the generic server model.
 *
 * @return None
 */
MESHX_BASE_GENERIC_SERVER_TEMPLATE_PROTO
meshXBaseGenericServerModel MESHX_BASE_GENERIC_SERVER_TEMPLATE_PARAMS
    :: meshXBaseGenericServerModel(uint32_t model_id, const control_msg_cb& from_ble_cb)
    : meshXBaseServerModel(model_id, from_ble_cb)
{
    set_status(meshXBaseGenericServerModel::plat_model_init());
    if (get_status() != MESHX_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "plat_model_init failed");
        return;
    }
}

/**
 * @brief Initialization function for the Generic OnOff Server model.
 *
 * This function is responsible for initializing the Generic OnOff Server model.
 * It checks if the model has been initialized before, and if not, it registers
 * the callback function for transaction control messages and calls the platform
 * specific initialization function.
 *
 * @return meshx_err_t containing the result of the initialization operation.
 */
MESHX_BASE_GENERIC_SERVER_TEMPLATE_PROTO
meshx_err_t meshXBaseGenericServerModel MESHX_BASE_GENERIC_SERVER_TEMPLATE_PARAMS
    ::plat_model_init(void)
{
    if(plat_server_init == MESHX_SERVER_INIT_MAGIC_NO)
    {
        return MESHX_SUCCESS;
    }
    plat_server_init = MESHX_SERVER_INIT_MAGIC_NO;
    return meshx_plat_gen_srv_init();
}

/**
 * @brief Validate a server status opcode.
 *
 * This function determines whether the provided opcode corresponds to a status message
 * in the Generic Server group in the MeshX protocol.
 *
 * @param[in] opcode The opcode to validate.
 * @return meshx_err_t containing the result of the validation operation.
 *                     Returns MESHX_SUCCESS if the opcode is valid, or an error code otherwise.
 */
MESHX_BASE_GENERIC_SERVER_TEMPLATE_PROTO
meshx_err_t meshXBaseGenericServerModel MESHX_BASE_GENERIC_SERVER_TEMPLATE_PARAMS
    ::validate_server_status_opcode(uint16_t opcode)
{
    switch(opcode)
    {
        case MESHX_MODEL_OP_GEN_ONOFF_STATUS:
        case MESHX_MODEL_OP_GEN_LEVEL_STATUS:
        case MESHX_MODEL_OP_GEN_DEF_TRANS_TIME_STATUS:
        case MESHX_MODEL_OP_GEN_ONPOWERUP_STATUS:
        case MESHX_MODEL_OP_GEN_POWER_LEVEL_STATUS:
        case MESHX_MODEL_OP_GEN_POWER_LAST_STATUS:
        case MESHX_MODEL_OP_GEN_POWER_DEFAULT_STATUS:
        case MESHX_MODEL_OP_GEN_POWER_RANGE_STATUS:
        case MESHX_MODEL_OP_GEN_BATTERY_STATUS:
        case MESHX_MODEL_OP_GEN_LOC_GLOBAL_STATUS:
        case MESHX_MODEL_OP_GEN_LOC_LOCAL_STATUS:
        case MESHX_MODEL_OP_GEN_MANUFACTURER_PROPERTIES_STATUS:
        case MESHX_MODEL_OP_GEN_MANUFACTURER_PROPERTY_STATUS:
            return MESHX_SUCCESS;
        default:
            return MESHX_FAIL;
    }
}

/**
 * @brief Sends a status message for the Generic Server model.
 *
 * This function sends a status message for the Generic Server model to the BLE Mesh network.
 * It checks if the provided model and context are valid, and if the opcode is within the
 * range of supported Generic Server opcodes.
 *
 * @param[in] params Pointer to the Generic Server sending params
 *
 * @return
 *     - MESHX_SUCCESS: Successfully sent the status message.
 *     - MESHX_INVALID_ARG: Invalid argument provided.
 *     - MESHX_ERR_PLAT: Platform-specific error occurred.
 */
MESHX_BASE_GENERIC_SERVER_TEMPLATE_PROTO
meshx_err_t meshXBaseGenericServerModel MESHX_BASE_GENERIC_SERVER_TEMPLATE_PARAMS
    ::plat_send_msg(meshx_gen_server_send_params_t *params)
{
    if (   params == nullptr
        || params->p_model == nullptr
        || params->p_ctx == nullptr
        || params->data_len == 0
        || params->p_ctx->dst_addr == MESHX_ADDR_UNASSIGNED
    )
    {
        return MESHX_INVALID_ARG;
    }
    if (validate_server_status_opcode((uint16_t)params->p_ctx->opcode) != MESHX_SUCCESS)
    {
        return MESHX_INVALID_ARG;
    }

    return meshx_plat_gen_srv_send_status(
        params->p_model,
        params->p_ctx,
       &params->state_change,
        params->data_len
    );
}

/**
 * @brief Restores the state of the Generic Server model.
 *
 * This function sets the user data of the specified model to the given state.
 * It checks if the model pointer is valid before proceeding with the operation.
 *
 * @param[in] param Pointer to the structure containing the model and state to be restored.
 *
 * @return
 *     - MESHX_SUCCESS: State restored successfully.
 *     - MESHX_INVALID_ARG: Invalid model pointer.
 */
MESHX_BASE_GENERIC_SERVER_TEMPLATE_PROTO
meshx_err_t meshXBaseGenericServerModel MESHX_BASE_GENERIC_SERVER_TEMPLATE_PARAMS
    :: server_state_restore(meshx_gen_server_restore_params_t* param)
{
    if(!param || !param->p_model)
    {
        return MESHX_INVALID_ARG;
    }

    uint16_t state_len = 0;
    switch(this->get_model_id())
    {
        case MESHX_MODEL_ID_GEN_ONOFF_SRV:
            state_len = sizeof(param->state_change.onoff);
            break;
        case MESHX_MODEL_ID_GEN_LEVEL_SRV:
            state_len = sizeof(param->state_change.level);
            break;
        case MESHX_MODEL_ID_GEN_POWER_ONOFF_SRV:
            state_len = sizeof(param->state_change.onpowerup);
            break;
        case MESHX_MODEL_ID_GEN_POWER_LEVEL_SRV:
            state_len = sizeof(param->state_change.power_level);
            break;
        case MESHX_MODEL_ID_GEN_BATTERY_SRV:
            state_len = sizeof(param->state_change.battery);
            break;
        case MESHX_MODEL_ID_GEN_LOCATION_SRV:
            state_len = sizeof(param->state_change.location);
            break;
        case MESHX_MODEL_ID_GEN_DEF_TRANS_TIME_SRV:
            state_len = sizeof(param->state_change.def_trans_time);
            break;
        default:
            return MESHX_NOT_SUPPORTED;
    }
    return meshx_plat_set_gen_srv_state(param->p_model, &param->state_change, state_len);
}

#endif /* CONFIG_ENABLE_GEN_SERVER */
