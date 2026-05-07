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
#include "meshx_base_model_generic.hpp"
#include "meshx_base_model_light.hpp"
#include "meshx_base_model_common.hpp"
#include "meshx_base_model_sensor.hpp"

MESHX_BASE_SERVER_TEMPLATE_PROTO
std::forward_list<typename meshXBaseServerModel MESHX_BASE_SERVER_TEMPLATE_PARAMS::base_server_model_cb_reg_t>
    meshXBaseServerModel MESHX_BASE_SERVER_TEMPLATE_PARAMS::base_server_model_cb_list = { };

MESHX_BASE_SERVER_TEMPLATE_PROTO
std::once_flag meshXBaseServerModel MESHX_BASE_SERVER_TEMPLATE_PARAMS::plat_server_init_flag;

// MESHX_CLIENT_INIT_MAGIC_NO replaced by std::once_flag

/**
 * @brief Construct a new meshXBaseModel object
 * @param[in] model_id Model identifier
 * @param[in] from_ble_cb Callback for handling BLE messages
 * @param[in] model_type Type of the model (server/client)
 */
MESHX_BASE_TEMPLATE_PROTO
meshXBaseModel MESHX_BASE_TEMPLATE_PARAMS::meshXBaseModel(uint32_t model_id, control_task_msg_handle_t from_ble_cb, meshXBaseModelType_t model_type)
    : model_id(model_id), model_type(model_type), from_ble_cb(from_ble_cb)
{
    if(from_ble_cb == nullptr)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "from_ble_cb is NULL");
        status = MESHX_INVALID_ARG;
        return;
    }
    // Callback registration is deferred to meshx_composition to avoid early-boot race conditions
    status = MESHX_SUCCESS;
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
    if (from_ble_cb == nullptr)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "meshXBaseModel[%08" PRIx32 "] from_ble_reg_cb: Invalid state (cb=NULL)", model_id);
        return MESHX_FAIL;
    }
    MESHX_LOGD(MODULE_ID_COMMON, "meshXBaseModel[%08" PRIx32 "] Registering callback %p", model_id, from_ble_cb);
    return control_task_msg_subscribe(CONTROL_TASK_MSG_CODE_FRM_BLE, model_id, from_ble_cb);
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
    return control_task_msg_unsubscribe(CONTROL_TASK_MSG_CODE_FRM_BLE, model_id, from_ble_cb);
}

/*********************************************************************************************************
 * meshXBaseServerModel
 ********************************************************************************************************/



/**
 * @brief Constructor for the meshXBaseServerModel class.
 *
 * This constructor initializes the meshXBaseServerModel class with the given model ID and callback function.
 *
 * @param[in] model_id  The unique identifier of the generic server model.
 * @param[in] from_ble_cb  The callback function to be registered for the model.
 */
MESHX_BASE_SERVER_TEMPLATE_PROTO
meshXBaseServerModel MESHX_BASE_SERVER_TEMPLATE_PARAMS::meshXBaseServerModel(uint32_t model_id, meshx_ptr_t p_plat_model, const control_msg_cb& from_ble_cb)
    : meshXBaseModel<ble_mesh_send_msg_params_t>(model_id, base_from_ble_msg_handle, meshXBaseModelType::MESHX_BASE_MODEL_TYPE_SERVER)
{
    this->p_plat_model = p_plat_model;
    if (!from_ble_cb)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid callback for model_id: %04x", model_id);
        this->status = MESHX_INVALID_ARG;
        return;
    }

    base_server_model_cb_list.push_front({(uint16_t)model_id, p_plat_model, from_ble_cb});
    this->status = MESHX_SUCCESS;
}

/**
 * @brief Sends a BLE message from a server model.
 *
 * @param[in] pdev Pointer to the device structure.
 * @param[in] evt  The event type associated with the message.
 * @param[in] params Pointer to the parameters of the message.
 * @return meshx_err_t Returns an error code indicating the result of the send operation.
 */
MESHX_BASE_SERVER_TEMPLATE_PROTO
meshx_err_t meshXBaseServerModel MESHX_BASE_SERVER_TEMPLATE_PARAMS::base_from_ble_msg_handle(
    dev_struct_t *pdev, control_task_msg_evt_t evt, meshx_ptr_t params)
{
    if (pdev == nullptr || params == nullptr)
        return MESHX_INVALID_ARG;

    auto *param = static_cast<ble_mesh_plat_cb_params_t*>(params);
    meshx_ptr_t plat_model = param->model.p_model;

    bool cb_invoked = false;
    for (auto &node : base_server_model_cb_list)
    {
        if ((uint16_t)evt == node.model_id)
        {
            /* Only invoke if the platform model matches this instance */
            if (node.p_plat_model != nullptr && node.p_plat_model != plat_model)
            {
                continue;
            }

            if (node.cb)
            {
                node.cb(pdev, evt, params);
                cb_invoked = true;
            }
        }
    }

    if (!cb_invoked)
    {
        MESHX_LOGW(MODULE_ID_MODEL_SERVER, "No registered server handled model_id=%04" PRIx32, (uint32_t)evt);
    }

    return cb_invoked ? MESHX_SUCCESS : MESHX_FAIL;
}

MESHX_BASE_SERVER_TEMPLATE_PROTO
meshx_err_t meshXBaseServerModel MESHX_BASE_SERVER_TEMPLATE_PARAMS::from_ble_dereg_cb(void) const
{
    // First, unsubscribe from control task
    meshx_err_t err = control_task_msg_unsubscribe(CONTROL_TASK_MSG_CODE_FRM_BLE, (control_task_msg_evt_t)this->model_id, this->from_ble_cb);

    // Then, remove from our static callback list to prevent dangling pointers
    base_server_model_cb_list.remove_if([this](const base_server_model_cb_reg_t& node) {
        return node.p_plat_model == this->p_plat_model;
    });

    return err;
}

/*********************************************************************************************************
 * meshXBaseClientModel
 ********************************************************************************************************/
MESHX_BASE_CLIENT_TEMPLATE_PROTO
std::forward_list<typename meshXBaseClientModel MESHX_BASE_CLIENT_TEMPLATE_PARAMS::base_client_model_cb_reg_t>
    meshXBaseClientModel MESHX_BASE_CLIENT_TEMPLATE_PARAMS::base_client_model_cb_list = { };

MESHX_BASE_CLIENT_TEMPLATE_PROTO
std::once_flag meshXBaseClientModel MESHX_BASE_CLIENT_TEMPLATE_PARAMS::plat_client_init_flag;

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
 * @tparam baseClientModelDerived_t The derived client model class type
 * @tparam ble_mesh_plat_model_cb_params_t Platform-specific callback parameter type
 * @tparam ble_mesh_send_msg_params_t Platform-specific send message parameter type
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
meshXBaseClientModel MESHX_BASE_CLIENT_TEMPLATE_PARAMS::meshXBaseClientModel(uint32_t model_id, meshx_ptr_t p_plat_model, const control_msg_cb& from_ble_cb)
    : meshXBaseModel<ble_mesh_send_msg_params_t>(model_id, base_from_ble_msg_handle, meshXBaseModelType::MESHX_BASE_MODEL_TYPE_CLIENT)
{
    this->p_plat_model = p_plat_model;
    // Validate model ID and callback - consistent with C implementation
    if (!from_ble_cb)
    {
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Invalid callback");
        this->status = MESHX_INVALID_ARG;
        return;
    }

    std::call_once(plat_client_init_flag, [this]() {
        MESHX_LOGD(MODULE_ID_MODEL_CLIENT, "First-time initialization for client template: %s", get_client_type_name());
    });

    base_client_model_cb_list.push_front({(uint16_t)model_id, p_plat_model, from_ble_cb});
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
meshx_err_t meshXBaseClientModel MESHX_BASE_CLIENT_TEMPLATE_PARAMS::base_txcm_handle_resend(uint16_t model_id, const ble_mesh_plat_model_cb_params_t *param)
{
    base_client_model_resend_ctx_t ctx = {
        .model_id = model_id,
        .param = {}
    };
    memcpy(&ctx.param, param, sizeof(ble_mesh_plat_model_cb_params_t));

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
 * @tparam baseClientModelDerived_t The derived client model class type
 * @tparam ble_mesh_plat_model_cb_params_t Platform-specific callback parameter type
 * @tparam ble_mesh_send_msg_params_t Platform-specific send message parameter type
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
    dev_struct_t *pdev, control_task_msg_evt_t evt, meshx_ptr_t params)
{
    if (pdev == nullptr || params == nullptr)
    {
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Invalid parameters");
        return MESHX_INVALID_ARG;
    }

    // Enhanced logging with template type identification
    MESHX_LOGD(MODULE_ID_MODEL_CLIENT, "Handling message for model_id: %04x", (uint32_t)evt);

    meshx_err_t err = MESHX_SUCCESS;
    auto *param = static_cast<ble_mesh_plat_model_cb_params_t*>(params);
    meshx_ptr_t plat_model = param->model.p_model;
    bool cb_invoked = false;

    for (auto &node : base_client_model_cb_list)
    {
        if ((uint16_t)evt == node.model_id)
        {
            /* Only invoke if the platform model matches this instance */
            if (node.p_plat_model != nullptr && node.p_plat_model != plat_model)
            {
                continue;
            }

            MESHX_LOGD(MODULE_ID_MODEL_CLIENT,
                       "op|src|dst:%04" PRIx32 "|%04x|%04x",
                       param->ctx.opcode, param->ctx.src_addr, param->ctx.dst_addr);

            if (node.cb == nullptr)
            {
                MESHX_LOGW(MODULE_ID_MODEL_CLIENT, "Callback is NULL for model_id: %04x", node.model_id);
                continue;
            }

            if (param->evt == static_cast<decltype(param->evt)>(meshx_base_cli_evt::MESHX_BASE_CLI_TIMEOUT) || param->err_code != MESHX_SUCCESS)
            {
                MESHX_LOGW(MODULE_ID_MODEL_CLIENT, "Message timeout or error, retrying...");
                err = base_txcm_handle_resend(node.model_id, param);
                if (err != MESHX_SUCCESS)
                    MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Resend failed: %d", err);
            }
            else
            {
                err = base_txcm_handle_ack(param->ctx.src_addr);
                if (err != MESHX_SUCCESS)
                    MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Ack failed: %d", err);

                err = node.cb(pdev, evt, param);
            }

            cb_invoked = true;
        }
    }

    if (!cb_invoked)
        MESHX_LOGW(MODULE_ID_MODEL_CLIENT, "No registered client handled model_id=%04" PRIx32, (uint32_t)evt);

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
 * @tparam baseClientModelDerived_t The derived client model class type
 * @tparam ble_mesh_plat_model_cb_params_t Platform-specific callback parameter type
 * @tparam ble_mesh_send_msg_params_t Platform-specific send message parameter type
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
    for (auto &node : base_client_model_cb_list)
    {
        if (param->model_id == node.model_id)
        {
            /* Only invoke if the platform model matches this instance */
            if (node.p_plat_model != nullptr && node.p_plat_model != param->param.model.p_model)
            {
                continue;
            }

            param->param.err_code   = MESHX_TIMEOUT;
            param->param.evt        = static_cast<decltype(param->param.evt)>(meshx_base_cli_evt::MESHX_BASE_CLI_TIMEOUT);
            if(node.cb == nullptr)
            {
                MESHX_LOGW(MODULE_ID_MODEL_CLIENT, "Callback is NULL for model_id: %04x", node.model_id);
                continue;
            }
            err = node.cb(pdev, param->model_id, &param->param);
            cb_invoked = true;
        }
    }

    if (!cb_invoked)
    {
        MESHX_LOGW(MODULE_ID_MODEL_CLIENT, "No registered client handled model_id=%04x", param->model_id);
        return MESHX_SUCCESS; // Consistent with C implementation - graceful handling
    }

    return err;
}

MESHX_BASE_CLIENT_TEMPLATE_PROTO
meshx_err_t meshXBaseClientModel MESHX_BASE_CLIENT_TEMPLATE_PARAMS::from_ble_dereg_cb(void) const
{
    // First, unsubscribe from control task
    meshx_err_t err = control_task_msg_unsubscribe(CONTROL_TASK_MSG_CODE_FRM_BLE, (control_task_msg_evt_t)this->model_id, this->from_ble_cb);

    // Then, remove from our static callback list to prevent dangling pointers
    base_client_model_cb_list.remove_if([this](const base_client_model_cb_reg_t& node) {
        return node.p_plat_model == this->p_plat_model;
    });

    return err;
}


/* Explicit template instantiations to ensure the linker can find the template method bodies
 * defined in this .cpp file.
 */

#if CONFIG_ENABLE_GEN_CLIENT
// meshXBaseModel instantiations
template class meshXBaseModel<meshx_gen_client_send_params_t>;
// meshXBaseClientModel instantiations
template class meshXBaseClientModel<meshXBaseGenericClientModel, meshx_gen_client_send_params_t, meshx_gen_cli_cb_param_t>;
#endif

#if CONFIG_ENABLE_GEN_SERVER
// meshXBaseModel instantiations
template class meshXBaseModel<meshx_gen_server_send_params_t>;
// meshXBaseServerModel instantiations
template class meshXBaseServerModel<meshXBaseGenericServerModel, meshx_gen_server_send_params_t, meshx_gen_server_restore_params_t, meshx_gen_srv_cb_param_t>;
#endif

#if CONFIG_ENABLE_LIGHT_CLIENT
// meshXBaseModel instantiations
template class meshXBaseModel<meshx_gen_light_client_send_params_t>;
// meshXBaseClientModel instantiations
template class meshXBaseClientModel<meshXBaseLightClientModel, meshx_gen_light_client_send_params_t, meshx_gen_light_cli_cb_param_t>;
#endif

#if CONFIG_ENABLE_LIGHT_SERVER
// meshXBaseModel instantiations
template class meshXBaseModel<meshx_light_server_send_params_t>;
// meshXBaseServerModel instantiations
template class meshXBaseServerModel<meshXBaseLightServerModel, meshx_light_server_send_params_t, meshx_light_server_restore_params_t, meshx_lighting_server_cb_param_t>;
#endif

#if CONFIG_ENABLE_CONFIG_SERVER
// meshXBaseModel instantiations
template class meshXBaseModel<meshx_config_server_send_params_t>;
// meshXBaseServerModel instantiations
template class meshXBaseServerModel<meshXBaseConfigServerModel, meshx_config_server_send_params_t, meshx_config_server_restore_params_t, meshx_config_srv_cb_param_t>;
#endif

#if CONFIG_ENABLE_SENSOR_SERVER
// meshXBaseModel instantiations
template class meshXBaseModel<meshx_sensor_server_send_params_t>;
// meshXBaseServerModel instantiations
template class meshXBaseServerModel<meshXBaseSensorServerModel, meshx_sensor_server_send_params_t, meshx_sensor_server_restore_params_t, meshx_sensor_server_cb_param_t>;
#endif
