/**
 * @file meshx_base_model_light.cpp
 * @brief Implementation of the MeshX Light Client model for BLE mesh nodes.
 *
 * This file contains the implementation of the Light Client model class that extends
 * the template-based meshXBaseClientModel. It provides specific functionality for
 * Light BLE mesh models including message validation, opcode handling, and platform
 * integration for the MeshX framework.
 *
 * Key Features:
 * - Light Client model implementation with type-safe templates
 * - Opcode validation for unacknowledged and GET request messages
 * - Platform-specific message sending with TXCM integration
 * - Enhanced error handling and debugging capabilities
 * - Static wrapper functions for C-style callback compatibility
 * - Model initialization and lifecycle management
 *
 * Supported Light Models:
 * - Light Lightness Client
 * - Light CTL Client
 * - Light HSL Client
 * - Light XYL Client
 * - Light LC (Light Control) Client
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright © 2024 - 2025 MeshX
 */

#include "meshx_txcm.h"
#include "meshx_base_model_light.hpp"

/*********************************************************************************************************
 * meshXBaseLightClientModel Implementation
 ********************************************************************************************************/
#if CONFIG_ENABLE_LIGHT_CLIENT

constexpr uint32_t MESHX_CLIENT_INIT_MAGIC_NO = 0x4309;

/**
 * @brief Validates if the given model ID corresponds to a supported Light Client model.
 *
 * This function performs validation of model IDs to ensure they correspond to supported
 * Light Client models in the MeshX BLE mesh framework. This is the Light-specific
 * implementation of the virtual validate_client_model_id function.
 *
 * Supported Light Client Model IDs:
 * - MESHX_MODEL_ID_LIGHT_LIGHTNESS_CLI: Light Lightness Client
 * - MESHX_MODEL_ID_LIGHT_CTL_CLI: Light CTL Client
 * - MESHX_MODEL_ID_LIGHT_HSL_CLI: Light HSL Client
 * - MESHX_MODEL_ID_LIGHT_XYL_CLI: Light XYL Client
 * - MESHX_MODEL_ID_LIGHT_LC_CLI: Light LC (Light Control) Client
 *
 * @param[in] model_id The 32-bit model ID to validate against supported Light client models.
 *
 * @retval MESHX_SUCCESS The model ID is valid and supported.
 * @retval MESHX_FAIL The model ID is not supported or invalid.
 *
 * @note This function is specific to Light models. Other client types (Generic, Sensor, etc.)
 *       will have their own implementations with different supported model IDs.
 */
meshx_err_t meshXBaseLightClientModel::validate_client_model_id(uint32_t model_id)
{
    switch (model_id)
    {
        case MESHX_MODEL_ID_LIGHT_LIGHTNESS_CLI:
        case MESHX_MODEL_ID_LIGHT_CTL_CLI:
        case MESHX_MODEL_ID_LIGHT_HSL_CLI:
        case MESHX_MODEL_ID_LIGHT_XYL_CLI:
        case MESHX_MODEL_ID_LIGHT_LC_CLI:
        // Add more Light client model IDs as needed
            return MESHX_SUCCESS;
        default:
            MESHX_LOGW(MODULE_ID_MODEL_CLIENT, "Invalid Light client model ID: %08" PRIx32, model_id);
            return MESHX_FAIL;
    }
}

/**
 * @brief Validates if a given opcode corresponds to an unacknowledged (SET_UNACK) message.
 *
 * This function performs opcode validation to determine if the specified opcode
 * represents a SET_UNACK operation for Light BLE mesh models. Unacknowledged
 * messages do not require a response from the server, making them suitable for
 * broadcast operations and scenarios where reliability is less critical than performance.
 *
 * Supported Unacknowledged Opcodes:
 * - MESHX_MODEL_OP_LIGHT_LIGHTNESS_SET_UNACK: Light Lightness Set Unacknowledged
 * - MESHX_MODEL_OP_LIGHT_CTL_SET_UNACK: Light CTL Set Unacknowledged
 * - MESHX_MODEL_OP_LIGHT_HSL_SET_UNACK: Light HSL Set Unacknowledged
 * - MESHX_MODEL_OP_LIGHT_XYL_SET_UNACK: Light XYL Set Unacknowledged
 * - MESHX_MODEL_OP_LIGHT_LC_MODE_SET_UNACK: Light LC Mode Set Unacknowledged
 * - MESHX_MODEL_OP_LIGHT_LC_OM_SET_UNACK: Light LC Occupancy Mode Set Unacknowledged
 * - MESHX_MODEL_OP_LIGHT_LC_LIGHT_ONOFF_SET_UNACK: Light LC Light OnOff Set Unacknowledged
 * - MESHX_MODEL_OP_LIGHT_LC_PROPERTY_SET_UNACK: Light LC Property Set Unacknowledged
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
meshx_err_t meshXBaseLightClientModel::meshx_is_unack_opcode(uint32_t opcode)
{
    switch(opcode)
    {
        case MESHX_MODEL_OP_LIGHT_LIGHTNESS_SET_UNACK:
        case MESHX_MODEL_OP_LIGHT_CTL_SET_UNACK:
        case MESHX_MODEL_OP_LIGHT_HSL_SET_UNACK:
        case MESHX_MODEL_OP_LIGHT_XYL_SET_UNACK:
        case MESHX_MODEL_OP_LIGHT_LC_MODE_SET_UNACK:
        case MESHX_MODEL_OP_LIGHT_LC_OM_SET_UNACK:
        case MESHX_MODEL_OP_LIGHT_LC_LIGHT_ONOFF_SET_UNACK:
        case MESHX_MODEL_OP_LIGHT_LC_PROPERTY_SET_UNACK:
            return MESHX_SUCCESS;
        default:
            return MESHX_FAIL;
    }
}

/**
 * @brief Checks if a given opcode is a GET request opcode.
 *
 * This function checks if a given opcode is a GET request opcode for the
 * light client model.
 *
 * @param[in] opcode Opcode to check.
 *
 * @return MESHX_SUCCESS if the opcode is a GET request opcode, MESHX_FAIL otherwise.
 */
meshx_err_t meshXBaseLightClientModel::meshx_is_get_req_opcode(uint16_t opcode)
{
    switch(opcode)
    {
        case MESHX_MODEL_OP_LIGHT_CTL_GET:
        case MESHX_MODEL_OP_LIGHT_HSL_GET:
        case MESHX_MODEL_OP_LIGHT_XYL_GET:
        case MESHX_MODEL_OP_LIGHT_LC_OM_GET:
        case MESHX_MODEL_OP_LIGHT_LC_MODE_GET:
        case MESHX_MODEL_OP_LIGHT_LIGHTNESS_GET:
        case MESHX_MODEL_OP_LIGHT_LC_PROPERTY_GET:
        case MESHX_MODEL_OP_LIGHT_LC_LIGHT_ONOFF_GET:
            return MESHX_SUCCESS;
        default:
            return MESHX_FAIL;
    }
}

/**
 * @brief Send a message using the light client model.
 *
 * This function sends a message from a Light Client model to a specified address
 * within the BLE Mesh network, using the provided opcode and parameters.
 *
 * @param[in] msg_param   Pointer to the message parameters structure.
 * @param[in] msg_param_len Length of the message parameters structure in bytes.
 *
 * @return meshx_err_t  Result of the operation. Returns MESHX_OK on success or an error code on failure.
 */
meshx_err_t meshXBaseLightClientModel::meshx_light_client_txcm_fn_model_send(meshx_light_client_msg_ctx_t *msg_param, size_t msg_param_len)
{
    if(msg_param == nullptr || msg_param_len != sizeof(meshx_light_client_msg_ctx_t))
    {
        return MESHX_INVALID_ARG;
    }

    return meshx_plat_light_client_send_msg(
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
 * @brief Constructor for the meshXBaseLightClientModel class.
 *
 * This constructor is responsible for initializing a meshXBaseLightClientModel
 * object with the given model ID and control task message handle.
 *
 * @param model_id The model ID associated with the light client model
 * @param from_ble_cb The control task message handle associated with the light client model
 *
 * @return None
 */
meshXBaseLightClientModel::meshXBaseLightClientModel(uint32_t model_id, const control_msg_cb& from_ble_cb)
    : meshXBaseClientModel(model_id, from_ble_cb)
{
    status = meshXBaseLightClientModel::plat_model_init();
    if (status != MESHX_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "plat_model_init failed");
        return;
    }
}

/**
 * @brief Initialization function for the Light Client model.
 *
 * This function is responsible for initializing the Light Client model.
 * It checks if the model has been initialized before, and if not, it registers
 * the callback function for transaction control messages and calls the platform
 * specific initialization function.
 *
 * @return meshx_err_t containing the result of the initialization operation.
 */
meshx_err_t meshXBaseLightClientModel::plat_model_init(void)
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
    return meshx_plat_gen_light_client_init();
}

/**
 * @brief Send a message using the light client model.
 *
 * This function sends a message from a light client model to a specified address
 * within the BLE Mesh network, using the provided opcode and parameters.
 *
 * @param[in] params Pointer to the structure containing the message parameters to set.
 *
 * @return meshx_err_t containing the result of the send operation. Returns MESHX_OK on success or an error code on failure.
 */
meshx_err_t meshXBaseLightClientModel::plat_send_msg(meshx_gen_light_client_send_params_t *params)
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

    meshx_light_client_msg_ctx_t send_msg;

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
        (meshx_txcm_fn_model_send_t) &meshXBaseLightClientModel::meshx_light_client_txcm_fn_model_send
    );
    if(err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Failed to send message: %p", (meshx_ptr_t) err);
    }
    return err;
}

#endif /* CONFIG_ENABLE_LIGHT_CLIENT */

/*********************************************************************************************************
 * meshXBaseLightServerModel
 ********************************************************************************************************/
