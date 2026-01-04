/**
 * @file meshx_model_level.hpp
 * @brief Implementation of Generic Level Model for MeshX
 *
 * This file contains the implementation of the Generic Level model,
 * which provides standard Level model functionality in the MeshX BLE mesh framework.
 *
 * Key Features:
 * - Implements Bluetooth SIG-defined Generic Level model
 * - Inherits from meshXClientModel and meshXServerModel templates
 * - Provides standard Level control operations (SET, GET, DELTA, MOVE)
 * - Integrates with MeshX transmission control
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */
#ifndef _MESHX_MODEL_LEVEL_HPP_
#define _MESHX_MODEL_LEVEL_HPP_

#include <meshx_model_class.hpp>
#include <meshx_base_model_generic.hpp>

#define MESHX_GEN_LEVEL_CLIENT_MODEL_TEMPLATE_PROTO
#define MESHX_GEN_LEVEL_CLIENT_MODEL_TEMPLATE_PARAMS

#define MESHX_GEN_LEVEL_SERVER_MODEL_TEMPLATE_PROTO
#define MESHX_GEN_LEVEL_SERVER_MODEL_TEMPLATE_PARAMS

/**
 * @brief Structure to hold the Level model state.
 */
struct meshx_gen_level_model_state
{
    int16_t present_level;  /**< The present value of Generic Level state */
    int16_t target_level;   /**< The target value of Generic Level state (optional) */
    uint8_t remaining_time; /**< Remaining transition time (optional) */
};
using meshx_gen_level_model_state_t = struct meshx_gen_level_model_state;

/**
 * @brief Structure to hold the parameters for sending a Generic Level message.
 */
struct meshx_gen_level_send_params
{
    meshx_model_t               *model;  /**< Pointer to the Level client model. */
    meshx_ctx_t                 *ctx;    /**< The context of the message. */
    uint8_t                      tid;    /**< The transaction ID of the message. Only used by Client*/
    uint8_t                      transition_time; /**< Transition time (optional). */
    uint8_t                      delay;   /**< Delay (optional). */
    meshx_gen_level_model_state_t state;  /**< The state of the message. */
};

using meshx_gen_level_send_params_t = struct meshx_gen_level_send_params;

#if CONFIG_ENABLE_GEN_LEVEL_CLIENT
/**
 * @brief Structure to hold the Level Server to parent element message.
 *        The structure is used by the on_model_cb function to send the Level state
 *        change notification to the parent element.
 */
struct meshx_level_cli_el_msg
{
    meshx_cli_model_send_param_header_t header; /**< Client model send param header */
    meshx_gen_level_model_state_t       state;  /**< The state of the message. */
};

using meshx_level_cli_el_msg_t = struct meshx_level_cli_el_msg;
/**
 * @class meshXGenericLevelClientModel
 * @brief A template class for creating Generic Level Client models.
 *
 * This class is derived from meshXClientModel and provides a convenient interface for
 * creating Generic Level Client models. It handles the Generic Level state change
 * notifications from the MeshX stack and publishes the state change event to the
 * element layer.
 */
MESHX_GEN_LEVEL_CLIENT_MODEL_TEMPLATE_PROTO
class meshXGenericLevelClientModel : public meshXClientModel<meshXBaseGenericClientModel, meshx_gen_level_send_params_t>
{
private:
    /* New or updated model state from BLE layer */
    meshx_gen_level_model_state_t model_state;

    /* Message to send to parent element - stored as member to persist */
    meshx_level_cli_el_msg_t element_msg;

    meshx_err_t meshx_state_change_notify   (const meshx_gen_cli_cb_param_t *param, uint8_t status);
    meshx_err_t element_state_change_handle (void) override;

public:
    meshx_err_t model_send          (meshx_gen_level_send_params_t *params) override;
    meshx_err_t model_from_ble_cb   (dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;
    meshx_err_t prepare_element_msg (meshx_ptr_t *msg_ptr, size_t *msg_size) override;

    meshXGenericLevelClientModel(
        meshXElementIF *parent_element = nullptr,
        meshx_ptr_t     parent_element_state = nullptr
    );
    ~meshXGenericLevelClientModel() = default;
};

#endif /* CONFIG_ENABLE_GEN_LEVEL_CLIENT */

#if CONFIG_ENABLE_GEN_LEVEL_SERVER

using meshx_level_srv_el_msg_t = struct meshx_level_srv_el_msg
{
    meshx_srv_model_send_param_header_t header; /**< Server model send param header */
    meshx_gen_level_model_state_t       state;  /**< The state of the message. */
};

/**
 * @class meshXGenericLevelServerModel
 * @brief A template class for creating Generic Level Server models.
 *
 * This class is derived from meshXServerModel and provides a convenient interface for
 * creating Generic Level Server models. It handles the Generic Level state change
 * notifications from the MeshX stack and publishes the state change event to the
 * element layer.
 */
MESHX_GEN_LEVEL_SERVER_MODEL_TEMPLATE_PROTO
class meshXGenericLevelServerModel : public meshXServerModel<meshXBaseGenericServerModel, meshx_gen_level_send_params_t>
{
private:
    /* New or updated model state from BLE layer */
    meshx_gen_level_model_state_t model_state;

    /* Message to send to parent element - stored as member to persist */
    meshx_level_srv_el_msg_t element_msg;

    /* Flag to indicate if message was prepared for element notification */
    bool element_msg_prepared;

    meshx_err_t plat_model_create   (void) override;
    meshx_err_t plat_model_delete   (void) override;

public:
    meshx_err_t model_send          (meshx_gen_level_send_params_t *params) override;
    meshx_err_t model_from_ble_cb   (dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;
    meshx_err_t prepare_element_msg (meshx_ptr_t *msg_ptr, size_t *msg_size) override;

    // Virtual method implementation from meshXModelIF
    meshx_err_t element_state_change_handle (void) override;

    meshXGenericLevelServerModel(
        meshXElementIF *parent_element = nullptr,
        meshx_ptr_t     parent_element_state = nullptr
    );
    ~meshXGenericLevelServerModel() = default;
};
#endif /* CONFIG_ENABLE_GEN_LEVEL_SERVER */

#endif /* _MESHX_MODEL_LEVEL_HPP_ */

