/**
 * @file meshx_base_model_common.hpp
 * @brief Header file for MeshX Config Server base class implementation.
 *
 * This header file contains the declarations for Config Server BLE mesh model classes
 * that extend the template-based meshXBaseServerModel. It defines the specific
 * implementation for Config Server models including message contexts, callback structures,
 * and the main Config Server model class.
 *
 * Key Components:
 * - Config Server message context structures for TXCM integration
 * - Config Server parameter structures for state management
 * - meshXBaseConfigServerModel class with Config-specific functionality
 * - Platform-specific message sending and state restoration interfaces
 * - Opcode validation for Config Server model operations
 *
 * Template Specialization:
 * This file provides concrete implementations of the template-based base classes
 * specifically tailored for Config BLE mesh server models, ensuring type safety and
 * optimal performance for configuration management operations.
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright © 2024 - 2025 MeshX
 */
#ifndef _MESHX_BASE_MODE_COMMON_H_
#define _MESHX_BASE_MODE_COMMON_H_

#include <meshx_base_model_class.hpp>

#define MESHX_BASE_CONFIG_SERVER_TEMPLATE_PROTO
#define MESHX_BASE_CONFIG_SERVER_TEMPLATE_PARAMS

#if CONFIG_ENABLE_CONFIG_SERVER
/*********************************************************************************************************
 * meshXBaseConfigServerModel
 ********************************************************************************************************/

/**
 * @struct meshx_config_server_send_params
 * @brief Structure containing the parameters for config server model message sending.
 *
 * This structure is used to store the message parameters for config server model messages,
 * including the model pointer, context pointer, state change information, and message length.
 */
using meshx_config_server_send_params_t = struct meshx_config_server_send_params
{
    meshx_model_t *p_model;         /**< Pointer to the server model. */
    meshx_ctx_t *p_ctx;             /**< Pointer to the context. */
    uint8_t *p_data;                /**< Pointer to the message data. */
    size_t data_len;                /**< Length of the data. */
};

/**
 * @struct meshx_config_server_restore_params
 * @brief Structure containing the parameters for config server model state restoration.
 *
 * This structure is used to store the state restoration parameters for config server models,
 * including the model pointer and state change information.
 */
using meshx_config_server_restore_params_t = struct meshx_config_server_restore_params
{
    meshx_model_t *p_model;         /**< Pointer to the server model. */
    uint8_t *p_data;                /**< Pointer to the state data. */
    size_t data_len;                /**< Length of the state data. */
};

/**
 * @class meshXBaseConfigServerModel
 * @brief Template specialization of meshXBaseServerModel for Config BLE mesh server models.
 *
 * This class provides a concrete implementation of the template-based meshXBaseServerModel
 * specifically designed for Config BLE mesh server models. It inherits all the template
 * benefits including type safety, static callback dispatching, and enhanced debugging
 * while providing Config-specific functionality.
 *
 * Key Features:
 * - Inherits template-based architecture from meshXBaseServerModel
 * - Provides Config-specific opcode validation and message handling
 * - Implements platform-specific message sending
 * - Supports configuration and provisioning operations
 * - Enhanced error handling and debugging with template type identification
 * - Static wrapper functions for C-style callback compatibility
 *
 * Template Parameters:
 * - baseServerModelDerived_t: meshXBaseConfigServerModel (CRTP pattern)
 * - ble_mesh_send_msg_params_t: meshx_config_server_send_params_t
 * - ble_mesh_restore_params_t: meshx_config_server_restore_params_t
 *
 * Supported Operations:
 * - Node identity operations
 * - Model composition operations
 * - Configuration status updates
 * - Provisioning-related configurations
 * - Device property operations
 * - State restoration from persistent storage
 *
 * @note This class uses private inheritance to maintain encapsulation while
 *       providing access to base functionality through friendship.
 * @see meshXBaseServerModel for base template functionality.
 * @see meshx_config_server_send_params_t for send parameter structure.
 * @see meshx_config_server_restore_params_t for restore parameter structure.
 */
MESHX_BASE_CONFIG_SERVER_TEMPLATE_PROTO
class meshXBaseConfigServerModel : public meshXBaseServerModel<meshXBaseConfigServerModel, meshx_config_server_send_params_t, meshx_config_server_restore_params_t, meshx_config_srv_cb_param_t>
{
public:
    using meshXBaseServerModel<meshXBaseConfigServerModel, meshx_config_server_send_params_t, meshx_config_server_restore_params_t, meshx_config_srv_cb_param_t>::from_ble_reg_cb;
    /**
     * @brief Initialize platform-specific config server model
     * @return MESHX_SUCCESS on successful initialization, error code otherwise
     */
    meshx_err_t plat_model_init(void) override;

    /**
     * @brief Validate config server model status opcode
     * @param[in] opcode The opcode to validate
     * @return MESHX_SUCCESS if opcode is valid, error code otherwise
     */
    meshx_err_t validate_server_status_opcode(uint16_t opcode) override;

public:
    /**
     * @brief Restore server state from persistent storage
     * @param[in] param Pointer to restoration parameters containing state data
     * @return MESHX_SUCCESS on successful restoration, error code otherwise
     */
    meshx_err_t server_state_restore(meshx_config_server_restore_params_t* param) override;

    /**
     * @brief Send message through the config server model
     * @param[in] params Pointer to send parameters structure
     * @return MESHX_SUCCESS if message sent successfully, error code otherwise
     */
    meshx_err_t plat_send_msg(meshx_config_server_send_params_t *params) override;

    /**
     * @brief Construct a new meshXBaseConfigServerModel object
     * @param[in] model_id Model identifier for the config server
     * @param[in] from_ble_cb Callback function for handling BLE messages
     */
    meshXBaseConfigServerModel(uint32_t model_id, meshx_ptr_t p_plat_model, const control_msg_cb &from_ble_cb);

    /**
     * @brief Delete default constructor
     */
    meshXBaseConfigServerModel() = delete;

    /**
     * @brief Virtual destructor for meshXBaseConfigServerModel
     */
    ~meshXBaseConfigServerModel() final = default;
};

#endif /* CONFIG_ENABLE_CONFIG_SERVER */

#endif /* _MESHX_BASE_MODE_COMMON_H_ */
