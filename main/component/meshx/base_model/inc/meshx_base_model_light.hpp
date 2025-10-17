/**
 * @file meshx_base_model_light.hpp
 * @brief Header file for MeshX Light Client model declarations.
 *
 * This header file contains the declarations for Light BLE mesh model classes
 * that extend the template-based meshXBaseClientModel. It defines the specific
 * implementation for Light models including message contexts, callback structures,
 * and the main Light Client model class.
 *
 * Key Components:
 * - Light Client message context structures for TXCM integration
 * - Resend context for timeout and retry mechanisms
 * - meshXBaseLightClientModel class with Light-specific functionality
 * - Platform-specific message sending and handling interfaces
 * - Opcode validation for Light model operations
 *
 * Template Specialization:
 * This file provides concrete implementations of the template-based base classes
 * specifically tailored for Light BLE mesh models, ensuring type safety and
 * optimal performance for Light Lightness, CTL, HSL, XYL, and LC models.
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright © 2024 - 2025 MeshX
 */
#ifndef _MESHX_BASE_MODEL_LIGHT_H_
#define _MESHX_BASE_MODEL_LIGHT_H_

#include<meshx_base_model_class.hpp>

/*********************************************************************************************************
 * meshXBaseLightClientModel
 ********************************************************************************************************/
/**
 * @struct meshx_light_cli_resend_ctx
 * @brief Structure containing the model ID and parameter for light client model message re-sending.
 *
 * This structure is used to store the model ID and parameter associated with a light client model message re-sending.
 */
using meshx_light_cli_resend_ctx_t = struct meshx_light_cli_resend_ctx
{
    uint16_t model_id;                    /**< Model ID associated with the re-sending. */
    meshx_gen_light_cli_cb_param_t param; /**< Parameter associated with the re-sending. */
};

/**
 * @struct meshx_light_client_msg_ctx
 * @brief Structure containing the message context for light client model messages.
 *
 * This structure is used to store the message context for light client model messages, including the model context, state parameters, opcode, destination address, network index, and application key index.
 */
using meshx_light_client_msg_ctx_t = struct meshx_light_client_msg_ctx
{
    meshx_ptr_t model;                         /**< Model context associated with the message. */
    uint16_t opcode;                           /**< Opcode associated with the message. */
    uint16_t addr;                             /**< Destination address associated with the message. */
    uint16_t net_idx;                          /**< Network index associated with the message. */
    uint16_t app_idx;                          /**< Application key index associated with the message. */
    meshx_light_client_set_state_t state;      /**< State parameters associated with the message. */
};

/**
 * @class meshXBaseLightClientModel
 * @brief Template specialization of meshXBaseClientModel for Light BLE mesh models.
 *
 * This class provides a concrete implementation of the template-based meshXBaseClientModel
 * specifically designed for Light BLE mesh client models. It inherits all the template
 * benefits including type safety, static callback dispatching, and enhanced debugging
 * while providing Light-specific functionality.
 *
 * Key Features:
 * - Inherits template-based architecture from meshXBaseClientModel
 * - Provides Light-specific opcode validation and message handling
 * - Implements platform-specific message sending via TXCM integration
 * - Supports all Light model types (Lightness, CTL, HSL, XYL, LC)
 * - Enhanced error handling and debugging with template type identification
 * - Static wrapper functions for C-style callback compatibility
 *
 * Template Parameters:
 * - baseClientModelDerived: meshXBaseLightClientModel (CRTP pattern)
 * - ble_mesh_plat_model_cb_params: meshx_gen_light_cli_cb_param_t
 * - ble_mesh_send_msg_params: meshx_gen_light_client_send_params_t
 *
 * Supported Operations:
 * - Light Lightness SET/GET operations
 * - Light CTL SET/GET operations
 * - Light HSL SET/GET operations
 * - Light XYL SET/GET operations
 * - Light LC (Light Control) operations
 *
 * @note This class uses private inheritance to maintain encapsulation while
 *       providing access to base functionality through friendship.
 * @see meshXBaseClientModel for base template functionality.
 * @see meshx_gen_light_cli_cb_param_t for callback parameter structure.
 * @see meshx_gen_light_client_send_params_t for send parameter structure.
 */
class meshXBaseLightClientModel : private meshXBaseClientModel <meshXBaseLightClientModel, meshx_gen_light_client_send_params_t, meshx_gen_light_cli_cb_param_t>
{
private:
    meshx_err_t plat_model_init(void) override;
    meshx_err_t validate_client_model_id(uint32_t model_id) override;

    static meshx_err_t meshx_is_unack_opcode(uint32_t opcode);
    static meshx_err_t meshx_is_get_req_opcode(uint16_t opcode);
    static meshx_err_t meshx_light_client_txcm_fn_model_send(meshx_light_client_msg_ctx_t *msg_param, size_t msg_param_len);
public:
    meshx_err_t plat_send_msg(meshx_gen_light_client_send_params_t *params) override;

    meshXBaseLightClientModel() = delete;
    meshXBaseLightClientModel(uint32_t model_id, const control_msg_cb& from_ble_cb);

    ~meshXBaseLightClientModel() = default;
};

#endif /* _MESHX_BASE_MODEL_LIGHT_H_ */
