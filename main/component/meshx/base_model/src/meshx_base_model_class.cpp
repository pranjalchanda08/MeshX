/**
 * @file meshx_base_model_class.cpp
 * @brief Implementation of the MeshX base model classes for BLE mesh nodes.
 *
 * This file contains the implementation of the template-based base model classes
 * for both client and server models in the MeshX BLE mesh framework. It provides
 * the core functionality for model initialization, callback management, message
 * handling, and transmission control.
 *
 * Key Features:
 * - Template-based architecture for type safety and performance
 * - Static callback dispatching with per-template instance management
 * - Enhanced debugging with template type identification
 * - Model validation and error handling
 * - Integration with the MeshX transmission control module (TXCM)
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright © 2024 - 2025 MeshX
 */

#include "meshx_base_model_class.hpp"

#define MESHX_CLIENT_INIT_MAGIC_NO 0x1121

/**
 * @brief Constructor for the meshXBaseModel class.
 *
 * This constructor initializes a meshXBaseModel object with the given model ID, callback function, and model type.
 *
 * @params[in] model_id      The unique identifier of the generic server model.
 * @params[in] from_ble_cb   The callback function to be registered for the model.
 * @params[in] model_type    The type of the generic server model.
 */
MESHX_BASE_TEMPLATE_PROTO
meshXBaseModel MESHX_BASE_TEMPLATE_PARAMS::meshXBaseModel(uint32_t model_id, const control_msg_cb& from_ble_cb, meshXBaseModelType_t model_type)
    : model_id(model_id), model_type(model_type), from_ble_cb(from_ble_cb)
{
    if(from_ble_cb == nullptr)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "from_ble_cb is NULL");
        status = MESHX_INVALID_ARG;
        return;
    }
    status = from_ble_reg_cb();
    if (status != MESHX_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "from_ble_reg_cb failed");
        return;
    }
}

/**
 * @brief Destructor for the meshXBaseModel class.
 *
 * This destructor deregisters the callback function associated with the model ID and calls the platform specific model de-initialization function.
 */
MESHX_BASE_TEMPLATE_PROTO
meshXBaseModel MESHX_BASE_TEMPLATE_PARAMS ::~meshXBaseModel()
{
    from_ble_dereg_cb();
}

/**
 * @brief Subscribes to a BLE message associated with the given model ID.
 *
 * This function subscribes to a BLE message associated with the given model ID,
 * allowing the server to handle events or messages related to that model.
 *
 * @return meshx_err_t Returns an error code indicating the result of the subscription.
 *                     Possible values include success or specific error codes.
 */
MESHX_BASE_TEMPLATE_PROTO
meshx_err_t meshXBaseModel MESHX_BASE_TEMPLATE_PARAMS::from_ble_reg_cb(void) const
{
    return control_task_msg_subscribe(CONTROL_TASK_MSG_CODE_FRM_BLE, model_id, (control_task_msg_handle_t)from_ble_cb);
}

/**
 * @brief Deregisters a callback function associated with a model ID.
 *
 * This function deregisters the callback function associated with the given model ID,
 * allowing the server to stop handling events or messages related to that model.
 *
 * @return meshx_err_t Returns an error code indicating the result of the deregistration.
 *                     Possible values include success or specific error codes.
 */
MESHX_BASE_TEMPLATE_PROTO
meshx_err_t meshXBaseModel MESHX_BASE_TEMPLATE_PARAMS::from_ble_dereg_cb(void) const
{
    return control_task_msg_unsubscribe(CONTROL_TASK_MSG_CODE_FRM_BLE, model_id, (control_task_msg_handle_t)from_ble_cb);
}

/*********************************************************************************************************
 * meshXBaseServerModel
 ********************************************************************************************************/

MESHX_BASE_SERVER_TEMPLATE_PROTO
uint16_t meshXBaseServerModel MESHX_BASE_SERVER_TEMPLATE_PARAMS::plat_server_init = 0;

/**
 * @brief Constructor for the meshXBaseServerModel class.
 *
 * This constructor initializes the meshXBaseServerModel class with the given model ID and callback function.
 *
 * @params[in] model_id  The unique identifier of the generic server model.
 * @params[in] from_ble_cb  The callback function to be registered for the model.
 */
MESHX_BASE_SERVER_TEMPLATE_PROTO
meshXBaseServerModel MESHX_BASE_SERVER_TEMPLATE_PARAMS::meshXBaseServerModel(uint32_t model_id, const control_msg_cb& from_ble_cb)
    : meshXBaseModel<ble_mesh_send_msg_params>(model_id, from_ble_cb, meshXBaseModelType::MESHX_BASE_MODEL_TYPE_SERVER)
{
}

/*********************************************************************************************************
 * meshXBaseClientModel
 ********************************************************************************************************/
enum class meshx_base_cli_evt
{
    MESHX_BASE_CLI_EVT_GET = MESHX_BIT(0),
    MESHX_BASE_CLI_EVT_SET = MESHX_BIT(1),
    MESHX_BASE_CLI_PUBLISH = MESHX_BIT(2),
    MESHX_BASE_CLI_TIMEOUT = MESHX_BIT(3),
    MESHX_BASE_CLI_EVT_ALL = (       \
            MESHX_BASE_CLI_EVT_GET | \
            MESHX_BASE_CLI_EVT_SET | \
            MESHX_BASE_CLI_PUBLISH | \
            MESHX_BASE_CLI_TIMEOUT)
};

using meshx_base_cli_evt_t = enum meshx_base_cli_evt;

MESHX_BASE_CLIENT_TEMPLATE_PROTO
std::forward_list<typename meshXBaseClientModel MESHX_BASE_CLIENT_TEMPLATE_PARAMS::base_client_model_cb_reg_t>
    meshXBaseClientModel MESHX_BASE_CLIENT_TEMPLATE_PARAMS::base_client_model_cb_list = {};

MESHX_BASE_CLIENT_TEMPLATE_PROTO
uint16_t meshXBaseClientModel MESHX_BASE_CLIENT_TEMPLATE_PARAMS::plat_client_init = 0;


/**
 * @brief Constructor for the meshXBaseClientModel template class.
 *
 * This constructor initializes a template-based Generic Client model instance with
 * comprehensive validation, callback registration, and platform-specific initialization.
 * It provides enhanced type safety and debugging capabilities compared to the C implementation.
 *
 * Key Initialization Steps:
 * 1. Validates the model ID against supported Generic Client models
 * 2. Validates the callback function is not null
 * 3. Calls the base class constructor with static message handler
 * 4. Performs platform-specific model initialization via derived class
 * 5. Registers the callback in the template-specific callback list
 * 6. Sets up initialization protection using magic number
 *
 * @tparam baseClientModelDerived The derived client model class type
 * @tparam ble_mesh_plat_model_cb_params Platform-specific callback parameter type
 * @tparam ble_mesh_send_msg_params Platform-specific send message parameter type
 *
 * @param[in] model_id The unique 32-bit identifier of the Generic Client model.
 *                     Must be a supported model ID (see validate_client_model_id()).
 * @param[in] from_ble_cb The callback function to handle BLE mesh messages.
 *                        Must not be null/empty.
 *
 * @post If successful, this->status == MESHX_SUCCESS and the model is ready for use.
 * @post If validation fails, this->status == MESHX_INVALID_ARG and initialization is aborted.
 *
 * @note Each template instantiation maintains its own static callback list and message handler.
 * @note The constructor calls the derived class's plat_model_init() for platform setup.
 * @see validate_client_model_id() for supported model IDs.
 * @see base_from_ble_msg_handle() for the static message handling mechanism.
 */
MESHX_BASE_CLIENT_TEMPLATE_PROTO
meshXBaseClientModel MESHX_BASE_CLIENT_TEMPLATE_PARAMS::meshXBaseClientModel(uint32_t model_id, const control_msg_cb& from_ble_cb)
    : meshXBaseModel<ble_mesh_send_msg_params>(model_id, base_from_ble_msg_handle, meshXBaseModelType::MESHX_BASE_MODEL_TYPE_CLIENT)
{
    // Validate model ID and callback - consistent with C implementation
    if (!from_ble_cb || static_cast<baseClientModelDerived*>(this)->validate_client_model_id(model_id) != MESHX_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "[%s] Invalid model_id (%08" PRIx32 ") or callback", get_client_type_name(), model_id);
        this->status = MESHX_INVALID_ARG;
        return;
    }

    if (plat_client_init == MESHX_CLIENT_INIT_MAGIC_NO)
        this->status = MESHX_SUCCESS;
    plat_client_init = MESHX_CLIENT_INIT_MAGIC_NO;

    static_cast <baseClientModelDerived*>(this)->plat_model_init(); // call the Derived platform specific model initialization function

    base_client_model_cb_list.push_front({model_id, from_ble_cb});
    this->status = MESHX_SUCCESS;
}

/**
 * @brief Resend an acknowledgement message for the given source address.
 *
 * This function resends an acknowledgement message for the given source address.
 *
 * @param[in] src_addr The source address associated with the acknowledgement message.
 *
 * @retval MESHX_SUCCESS if the acknowledgement message was resent successfully, otherwise an error code.
 */
MESHX_BASE_CLIENT_TEMPLATE_PROTO
meshx_err_t meshXBaseClientModel MESHX_BASE_CLIENT_TEMPLATE_PARAMS::base_txcm_handle_ack(uint16_t src_addr)
{
    meshx_err_t err = MESHX_SUCCESS;
    err = meshx_txcm_request_send(MESHX_TXCM_SIG_ACK, src_addr, nullptr, 0, nullptr);
    return err;
}
/**
 * @brief Resend a message for the given model ID and parameter.
 *
 * This function resends a message for the given model ID and parameter.
 *
 * @param[in] model_id  The unique identifier of the generic client model.
 * @param[in] param     The parameter associated with the re-sending.
 *
 * @retval MESHX_SUCCESS if the message was resent successfully, otherwise an error code.
 */
MESHX_BASE_CLIENT_TEMPLATE_PROTO
meshx_err_t meshXBaseClientModel MESHX_BASE_CLIENT_TEMPLATE_PARAMS::base_txcm_handle_resend(uint16_t model_id, const ble_mesh_plat_model_cb_params *param)
{
    base_client_model_resend_ctx_t ctx = {
        .model_id = model_id
    };
    memcpy(&ctx.param, param, sizeof(ble_mesh_plat_model_cb_params));

    return meshx_txcm_request_send(
        MESHX_TXCM_SIG_RESEND,
        MESHX_ADDR_UNASSIGNED,
        &ctx,
        sizeof(base_client_model_resend_ctx_t),
        NULL);
}

/**
 * @brief Template-based static message handler for BLE Mesh Generic Client models.
 *
 * This is the core static message handling function that processes incoming BLE mesh
 * messages for all instances of a specific template instantiation. It provides a
 * centralized dispatch mechanism with enhanced debugging, validation, and error handling.
 *
 * Key Features:
 * - Template-specific static dispatch (one handler per template instantiation)
 * - Model ID validation against supported Generic Client models
 * - Enhanced logging with template type identification
 * - Timeout and error handling with automatic retry mechanism
 * - ACK handling for reliable message delivery
 * - Callback routing based on registered model IDs
 *
 * Message Processing Flow:
 * 1. Validates input parameters and model ID
 * 2. Searches the template-specific callback list for matching model ID
 * 3. Handles timeout/error cases with retry mechanism via TXCM
 * 4. Processes successful messages and handles ACK
 * 5. Invokes the registered application callback
 *
 * @tparam baseClientModelDerived The derived client model class type
 * @tparam ble_mesh_plat_model_cb_params Platform-specific callback parameter type
 * @tparam ble_mesh_send_msg_params Platform-specific send message parameter type
 *
 * @param[in] pdev Pointer to the device structure associated with the BLE Mesh node.
 *                 Must not be NULL.
 * @param[in] evt Control task message event type (typically contains model context).
 * @param[in] params Pointer to platform-specific message parameters structure.
 *                   Must not be NULL and contain valid model_id.
 *
 * @retval MESHX_SUCCESS Message processed successfully by registered callback.
 * @retval MESHX_INVALID_ARG Invalid parameters or unsupported model ID.
 * @retval Other Error codes from callback execution or TXCM operations.
 *
 * @note This function is automatically registered as the message handler during construction.
 * @note Each template instantiation gets its own static instance of this function.
 * @note Enhanced with template type identification for improved debugging.
 * @see base_txcm_handle_resend() for retry mechanism details.
 * @see base_txcm_handle_ack() for ACK processing details.
 */
MESHX_BASE_CLIENT_TEMPLATE_PROTO
meshx_err_t meshXBaseClientModel MESHX_BASE_CLIENT_TEMPLATE_PARAMS::base_from_ble_msg_handle(
    dev_struct_t *pdev, control_task_msg_evt_t evt, ble_mesh_plat_model_cb_params *params)
{
    if (pdev == nullptr || params == nullptr)
    {
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "[%s] Invalid parameters", get_client_type_name());
        return MESHX_INVALID_ARG;
    }

    // Enhanced logging with template type identification
    MESHX_LOGD(MODULE_ID_MODEL_CLIENT, "[%s] Handling message for model_id: %04x",
               get_client_type_name(), params->model_id);

    meshx_err_t err = MESHX_SUCCESS;
    auto *param = static_cast<ble_mesh_plat_model_cb_params*>(params);
    bool cb_invoked = false;

    for (auto &node : base_client_model_cb_list)
    {
        if (param->model_id == node.model_id)
        {
            MESHX_LOGD(MODULE_ID_MODEL_CLIENT,
                       "[%s] op|src|dst:%04" PRIx32 "|%04x|%04x",
                       get_client_type_name(),
                       param->ctx.opcode, param->ctx.src_addr, param->ctx.dst_addr);

            if (node.cb == nullptr)
            {
                MESHX_LOGW(MODULE_ID_MODEL_CLIENT, "[%s] Callback is NULL for model_id: %04x", get_client_type_name(), node.model_id);
                continue;
            }

            if (param->evt == meshx_base_cli_evt::MESHX_BASE_CLI_TIMEOUT || param->err_code != MESHX_SUCCESS)
            {
                MESHX_LOGW(MODULE_ID_MODEL_CLIENT, "[%s] Message timeout or error, retrying...", get_client_type_name());
                err = base_txcm_handle_resend(node.model_id, param);
                if (err != MESHX_SUCCESS)
                    MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "[%s] Resend failed: %d", get_client_type_name(), err);
            }
            else
            {
                err = base_txcm_handle_ack(param->ctx.src_addr);
                if (err != MESHX_SUCCESS)
                    MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "[%s] Ack failed: %d", get_client_type_name(), err);

                err = node.cb(pdev, evt, param);
            }

            cb_invoked = true;
        }
    }

    if (!cb_invoked)
        MESHX_LOGW(MODULE_ID_MODEL_CLIENT, "[%s] No registered client handled model_id=%04x", get_client_type_name(), param->model_id);

    return err;
}

/**
 * @brief Template-based TXCM (Transmission Control Module) message handler for timeout scenarios.
 *
 * This function handles control task messages from the Transmission Control Module (TXCM),
 * specifically for timeout and resend scenarios in Generic Client models. It processes
 * timeout events by setting appropriate error codes and invoking registered callbacks
 * to notify the application layer of transmission failures.
 *
 * Key Responsibilities:
 * - Processes TXCM timeout notifications for failed message transmissions
 * - Sets timeout error codes (MESHX_TIMEOUT, MESHX_BASE_CLI_TIMEOUT)
 * - Routes timeout events to registered application callbacks
 * - Provides template-specific error logging and debugging
 * - Maintains consistency with C implementation error handling patterns
 *
 * Timeout Processing Flow:
 * 1. Searches template-specific callback list for matching model ID
 * 2. Sets timeout error codes in the parameter structure
 * 3. Validates callback function is not null
 * 4. Invokes registered callback with timeout notification
 * 5. Returns gracefully if no callback is registered (consistent with C impl)
 *
 * @tparam baseClientModelDerived The derived client model class type
 * @tparam ble_mesh_plat_model_cb_params Platform-specific callback parameter type
 * @tparam ble_mesh_send_msg_params Platform-specific send message parameter type
 *
 * @param[in] pdev Pointer to the device structure associated with the BLE Mesh node.
 *                 Must not be NULL.
 * @param[in] evt Control task message event type (unused in current implementation).
 * @param[in] param Pointer to resend context containing model ID and callback parameters.
 *                  Must not be NULL and contain valid model_id.
 *
 * @retval MESHX_SUCCESS Timeout processed successfully or no callback registered.
 * @retval Other Error codes from callback execution.
 *
 * @note This function is called by the TXCM when message transmission timeouts occur.
 * @note Each template instantiation maintains its own callback list and handler.
 * @note Enhanced with template type identification for debugging purposes.
 * @see base_from_ble_msg_handle() for normal message processing.
 * @see base_txcm_handle_resend() for retry mechanism details.
 */
MESHX_BASE_CLIENT_TEMPLATE_PROTO
meshx_err_t meshXBaseClientModel MESHX_BASE_CLIENT_TEMPLATE_PARAMS::base_handle_txcm_msg(dev_struct_t *pdev, control_task_msg_evt_t evt, base_client_model_resend_ctx_t *param)
{
    MESHX_UNUSED(evt);
    meshx_err_t err = MESHX_SUCCESS;
    bool cb_invoked = false;
    for (base_client_model_cb_reg_t node : base_client_model_cb_list)
    {
        if (param->model_id == node.model_id)
        {
            param->param.err_code   = MESHX_TIMEOUT;
            param->param.evt        = meshx_base_cli_evt::MESHX_BASE_CLI_TIMEOUT;
            if(node.cb == nullptr)
            {
                MESHX_LOGW(MODULE_ID_MODEL_CLIENT, "[%s] Callback is NULL for model_id: %04x", get_client_type_name(), node.model_id);
                continue;
            }
            err = node.cb(pdev, param->model_id, &param->param);
            cb_invoked = true;
        }
    }

    if (!cb_invoked)
    {
        MESHX_LOGW(MODULE_ID_MODEL_CLIENT, "[%s] No registered client handled model_id=%04x", get_client_type_name(), param->model_id);
        return MESHX_SUCCESS; // Consistent with C implementation - graceful handling
    }

    return err;
}
