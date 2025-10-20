/**
 * @file meshx_model_class.hpp
 * @brief Template declarations for MeshX model wrapper classes
 *
 * This file contains the template declarations for the wrapper classes that
 * provide a convenient interface around the MeshX base model classes. It includes
 * the base wrapper (meshXModel) and specialized wrappers for server and client models.
 *
 * Key Features:
 * - Template-based wrapper architecture
 * - Unified interface for both client and server models
 * - Type-safe model creation and management
 * - Simplified integration with platform-specific implementations
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 * @project MeshX
 * @version 1.0
 * @license MIT License
 * @note This file is part of the MeshX project.
 * @warning This file is subject to change without notice.
 */

#include <meshx_base_model_class.hpp>

#define MESHX_MODEL_TEMPLATE_PROTO          template <typename meshxBaseModel_t, typename meshx_send_packet_params_t>
#define MESHX_MODEL_TEMPLATE_PARAMS                  <meshxBaseModel_t, meshx_send_packet_params_t>

#define MESHX_SERVER_MODEL_TEMPLATE_PROTO   template <typename meshxBaseServerModel_t, typename meshx_send_packet_params_t>
#define MESHX_SERVER_MODEL_TEMPLATE_PARAMS           <meshxBaseServerModel_t, meshx_send_packet_params_t>

#define MESHX_CLIENT_MODEL_TEMPLATE_PROTO   template <typename meshxBaseClientModel_t, typename meshx_send_packet_params_t>
#define MESHX_CLIENT_MODEL_TEMPLATE_PARAMS           <meshxBaseClientModel_t, meshx_send_packet_params_t>

/*********************************************************************************
 * meshXModel
 *********************************************************************************/
/**
 * @brief meshXModel class
 * @details This is a base class for both client and server models.
 */
MESHX_MODEL_TEMPLATE_PROTO
class meshXModel
{
private:

    meshxBaseModel_t *base_model;
    meshx_model_interface_t model;

public:

    virtual meshx_err_t plat_model_create(void) = 0;
    virtual meshx_err_t send_packet(meshx_send_packet_params_t *params) = 0;

    meshx_err_t set_base_model(meshxBaseModel_t *p_base_model);
    meshXModel(uint32_t model_id, const control_msg_cb& from_ble_cb);
    ~meshXModel() = default;
};

/*********************************************************************************
 * meshXServerModel
 *********************************************************************************/
/**
 * @brief meshXServerModel class
 * @details This is a base class for server models.
 * @params meshxBaseServerModel_t is a meshXBaseServerModel type Class for server models
 * @params meshx_send_packet_params_t is a meshx_send_packet_params_t structure
 */
MESHX_SERVER_MODEL_TEMPLATE_PROTO
class meshXServerModel : public meshXModel<meshxBaseServerModel_t, meshx_send_packet_params_t>
{
public:

    meshXServerModel(uint32_t model_id, const control_msg_cb& from_ble_cb);
    ~meshXServerModel() = default;
};

/*********************************************************************************
 * meshXClientModel
 *********************************************************************************/
/**
 * @brief meshXClientModel class
 * @details This is a base class for client models.
 * @params meshxBaseClientModel_t is a meshXBaseClientModel type Class for client models
 * @params meshx_send_packet_params_t is a meshx_send_packet_params_t structure
 */
MESHX_CLIENT_MODEL_TEMPLATE_PROTO
class meshXClientModel : public meshXModel<meshxBaseClientModel_t, meshx_send_packet_params_t>
{
public:
    meshXClientModel(uint32_t model_id, const control_msg_cb& from_ble_cb);
    ~meshXClientModel() = default;
};
