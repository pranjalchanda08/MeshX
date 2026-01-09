/**
 * @file meshx_base_model_generic.hpp
 * @brief Header file for MeshX Generic Client and Server model declarations.
 *
 * This header file contains the declarations for Generic BLE mesh model classes
 * that extend the template-based meshXBaseClientModel and meshXBaseServerModel.
 * It defines the specific implementation for Generic models including message
 * contexts, callback structures, and the main Generic Client model class.
 *
 * Key Components:
 * - Generic Client message context structures for TXCM integration
 * - Resend context for timeout and retry mechanisms
 * - meshXBaseGenericClientModel class with Generic-specific functionality
 * - Platform-specific message sending and handling interfaces
 * - Opcode validation for Generic model operations
 *
 * Template Specialization:
 * This file provides concrete implementations of the template-based base classes
 * specifically tailored for Generic BLE mesh models, ensuring type safety and
 * optimal performance for Generic OnOff, Level, Power, Battery, and Location models.
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright © 2024 - 2025 MeshX
 */
#ifndef _MESHX_BASE_MODEL_GENERIC_H_
#define _MESHX_BASE_MODEL_GENERIC_H_

#include <meshx_base_model_class.hpp>

#define MESHX_BASE_GENERIC_SERVER_TEMPLATE_PROTO
#define MESHX_BASE_GENERIC_SERVER_TEMPLATE_PARAMS

#define MESHX_BASE_GENERIC_CLIENT_TEMPLATE_PROTO
#define MESHX_BASE_GENERIC_CLIENT_TEMPLATE_PARAMS

#if CONFIG_ENABLE_GEN_CLIENT
/*********************************************************************************************************
 * meshXBaseGenericClientModel
 ********************************************************************************************************/
/**
 * @struct meshx_gen_cli_resend_ctx
 * @brief Structure containing the model ID and parameter for generic client model message re-sending.
 *
 * This structure is used to store the model ID and parameter associated with a generic client model message re-sending.
 */
using meshx_gen_cli_resend_ctx = struct meshx_gen_cli_resend_ctx
{
    uint16_t model_id;              /**< Model ID associated with the re-sending. */
    meshx_gen_cli_cb_param_t param; /**< Parameter associated with the re-sending. */
};
/**
 * @struct meshx_gen_client_msg_ctx
 * @brief Structure containing the message context for generic client model messages.
 *
 * This structure is used to store the message context for generic client model messages, including the model context, state parameters, opcode, destination address, network index, and application key index.
 */
using meshx_gen_client_msg_ctx_t = struct meshx_gen_client_msg_ctx
{
    meshx_ptr_t model;         /**< Model context associated with the message. */
    uint16_t opcode;           /**< Opcode associated with the message. */
    uint16_t addr;             /**< Destination address associated with the message. */
    uint16_t net_idx;          /**< Network index associated with the message. */
    uint16_t app_idx;          /**< Application key index associated with the message. */
    meshx_gen_cli_set_t state; /**< State parameters associated with the message. */
};

/**
 * @class meshXBaseGenericClientModel
 * @brief Template specialization of meshXBaseClientModel for Generic BLE mesh models.
 *
 * This class provides a concrete implementation of the template-based meshXBaseClientModel
 * specifically designed for Generic BLE mesh client models. It inherits all the template
 * benefits including type safety, static callback dispatching, and enhanced debugging
 * while providing Generic-specific functionality.
 *
 * Key Features:
 * - Inherits template-based architecture from meshXBaseClientModel
 * - Provides Generic-specific opcode validation and message handling
 * - Implements platform-specific message sending via TXCM integration
 * - Supports all Generic model types (OnOff, Level, Power, Battery, Location)
 * - Enhanced error handling and debugging with template type identification
 * - Static wrapper functions for C-style callback compatibility
 *
 * Template Parameters:
 * - baseClientModelDerived_t: meshXBaseGenericClientModel (CRTP pattern)
 * - ble_mesh_plat_model_cb_params_t: meshx_gen_cli_cb_param_t
 * - ble_mesh_send_msg_params_t: meshx_gen_client_send_params_t
 *
 * Supported Operations:
 * - Generic OnOff SET/GET operations
 * - Generic Level SET/GET operations
 * - Generic Power Level operations
 * - Generic Battery status operations
 * - Generic Location operations
 * - Property-based operations (Manufacturer, Admin, User)
 *
 * @note This class uses private inheritance to maintain encapsulation while
 *       providing access to base functionality through friendship.
 * @see meshXBaseClientModel for base template functionality.
 * @see meshx_gen_cli_cb_param_t for callback parameter structure.
 * @see meshx_gen_client_send_params_t for send parameter structure.
 */
MESHX_BASE_GENERIC_CLIENT_TEMPLATE_PROTO
class meshXBaseGenericClientModel : private meshXBaseClientModel<meshXBaseGenericClientModel, meshx_gen_client_send_params_t, meshx_gen_cli_cb_param_t>
{
private:
    /* @copydoc meshXBaseClientModel::plat_model_init */
    meshx_err_t plat_model_init(void) override;

    /* @copydoc meshXBaseClientModel::validate_client_model_id */
    meshx_err_t validate_client_model_id(uint32_t model_id) override;

    static meshx_err_t meshx_is_unack_opcode(uint32_t opcode);
    static meshx_err_t meshx_is_get_req_opcode(uint16_t opcode);
    static meshx_err_t meshx_gen_client_txcm_fn_model_send(meshx_gen_client_msg_ctx_t *msg_param, size_t msg_param_len);

public:
    meshx_err_t plat_send_msg(meshx_gen_client_send_params_t *params) override;

    meshXBaseGenericClientModel() = delete;
    meshXBaseGenericClientModel(uint32_t model_id, const control_msg_cb &from_ble_cb);

    virtual ~meshXBaseGenericClientModel() = default;
};

#endif /* CONFIG_ENABLE_GEN_CLIENT */

#if CONFIG_ENABLE_GEN_SERVER
/*********************************************************************************************************
 * meshXBaseGenericServerModel
 ********************************************************************************************************/

using meshx_gen_server_send_params_t = struct meshx_gen_server_send_params
{
    meshx_model_t *p_model;                     /**< Pointer to the server model. */
    meshx_ctx_t *p_ctx;                         /**< Pointer to the context. */
    meshx_gen_srv_state_change_t state_change;  /**< State change information. Platform Interface. */
    size_t data_len;                            /**< Length of the data. */
};

using meshx_gen_server_restore_params_t = struct meshx_gen_server_restore_params
{
    meshx_model_t *p_model;                     /**< Pointer to the server model. */
    meshx_gen_server_state_t state_change;      /**< State change information. Platform Interface. */
};
/**
 * @class meshXBaseGenericServerModel
 * @brief Template specialization of meshXBaseServerModel for Generic BLE mesh models.
 *
 * This class provides a concrete implementation of the template-based meshXBaseServerModel
 * specifically designed for Generic BLE mesh server models. It inherits all the template
 * benefits including type safety, static callback dispatching, and enhanced debugging
 * while providing Generic-specific functionality.
 *
 * Key Features:
 * - Inherits template-based architecture from meshXBaseServerModel
 * - Provides Generic-specific opcode validation and message handling
 * - Implements platform-specific message sending
 * - Supports all Generic model types (OnOff, Level, Power, Battery, Location)
 * - Enhanced error handling and debugging with template type identification
 * - Static wrapper functions for C-style callback compatibility
 *
 * Template Parameters:
 * - baseServerModelDerived_t: meshXBaseGenericServerModel (CRTP pattern)
 * - ble_mesh_send_msg_params_t: meshx_gen_server_send_params_t
 *
 * Supported Operations:
 * - Generic OnOff status updates
 * - Generic Level status updates
 * - Generic Power Level status updates
 * - Generic Battery status updates
 * - Generic Location status updates
 * - Property-based status updates (Manufacturer, Admin, User)
 *
 * @note This class uses private inheritance to maintain encapsulation while
 *       providing access to base functionality through friendship.
 * @see meshXBaseServerModel for base template functionality.
 * @see meshx_gen_server_send_params_t for send parameter structure.
 */
MESHX_BASE_GENERIC_SERVER_TEMPLATE_PROTO
class meshXBaseGenericServerModel : private meshXBaseServerModel<meshXBaseGenericServerModel, meshx_gen_server_send_params_t, meshx_gen_server_restore_params_t>
{
private:
    meshx_err_t plat_model_init(void) override;
    meshx_err_t validate_server_status_opcode(uint16_t opcode) override;
public:
    meshx_err_t server_state_restore(meshx_gen_server_restore_params_t* param) override;
    meshx_err_t plat_send_msg(meshx_gen_server_send_params_t *params) override;
    meshXBaseGenericServerModel(uint32_t model_id, const control_msg_cb &from_ble_cb);
    meshXBaseGenericServerModel() = delete;
    ~meshXBaseGenericServerModel() final = default;
};
#endif /* CONFIG_ENABLE_GEN_SERVER */

#endif /* _MESHX_BASE_MODEL_GENERIC_H_ */
