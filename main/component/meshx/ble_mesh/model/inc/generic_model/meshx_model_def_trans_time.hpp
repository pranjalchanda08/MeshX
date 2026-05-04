/**
 * @file meshx_model_def_trans_time.hpp
 * @brief Implementation of Generic Default Transition Time Model for MeshX
 *
 * This file contains the implementation of the Generic Default Transition Time model,
 * which provides standard Default Transition Time model functionality in the MeshX BLE mesh framework.
 *
 * Key Features:
 * - Implements Bluetooth SIG-defined Generic Default Transition Time model
 * - Inherits from meshXClientModel and meshXServerModel templates
 * - Provides standard Default Transition Time control operations
 * - Integrates with MeshX transmission control
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */
#ifndef _MESHX_MODEL_DEF_TRANS_TIME_HPP_
#define _MESHX_MODEL_DEF_TRANS_TIME_HPP_

#include <meshx_model_class.hpp>
#include <meshx_base_model_generic.hpp>

#define MESHX_GEN_DEF_TRANS_TIME_CLIENT_MODEL_TEMPLATE_PROTO
#define MESHX_GEN_DEF_TRANS_TIME_CLIENT_MODEL_TEMPLATE_PARAMS

#define MESHX_GEN_DEF_TRANS_TIME_SERVER_MODEL_TEMPLATE_PROTO
#define MESHX_GEN_DEF_TRANS_TIME_SERVER_MODEL_TEMPLATE_PARAMS

/**
 * @brief Structure to hold the Default Transition Time model state.
 */
struct meshx_gen_def_trans_time_model_state
{
    uint8_t trans_time;   /**< The Default Transition Time value. */
};
using meshx_gen_def_trans_time_model_state_t = struct meshx_gen_def_trans_time_model_state;

/**
 * @brief Structure to hold the parameters for sending a Generic Default Transition Time message.
 */
struct meshx_gen_def_trans_time_send_params
{
    meshx_model_t                          *model;  /**< Pointer to the Default Transition Time client model. */
    meshx_ctx_t                            *ctx;    /**< The context of the message. */
    uint8_t                                 tid;    /**< The transaction ID of the message. Only used by Client*/
    meshx_gen_def_trans_time_model_state_t state;  /**< The state of the message. */
};

using meshx_gen_def_trans_time_send_params_t = struct meshx_gen_def_trans_time_send_params;

#if CONFIG_ENABLE_GEN_DEF_TRANS_TIME_CLIENT
/**
 * @brief Structure to hold the Default Transition Time Server to parent element message.
 *        The structure is used by the on_model_cb function to send the Default Transition Time state
 *        change notification to the parent element.
 */
struct meshx_def_trans_time_cli_el_msg
{
    meshx_cli_model_send_param_header_t header; /**< Client model send param header */
    meshx_gen_def_trans_time_model_state_t state;  /**< The state of the message. */
};

using meshx_def_trans_time_cli_el_msg_t = struct meshx_def_trans_time_cli_el_msg;
/**
 * @class meshXGenericDefTransTimeClientModel
 * @brief A template class for creating Generic Default Transition Time Client models.
 *
 * This class is derived from meshXClientModel and provides a convenient interface for
 * creating Generic Default Transition Time Client models. It handles the Generic Default Transition Time state change
 * notifications from the MeshX stack and publishes the state change event to the
 * element layer.
 */
MESHX_GEN_DEF_TRANS_TIME_CLIENT_MODEL_TEMPLATE_PROTO
class meshXGenericDefTransTimeClientModel : public meshXClientModel<meshXBaseGenericClientModel, meshx_gen_def_trans_time_send_params_t>
{
private:
    /* New or updated model state from BLE layer */
    meshx_gen_def_trans_time_model_state_t model_state;

    /* Message to be sent to parent element */
    meshx_def_trans_time_cli_el_msg_t element_msg;

    meshx_err_t meshx_state_change_notify   (const meshx_gen_cli_cb_param_t *param, uint8_t status);
    meshx_err_t element_state_change_handle (void) override;

public:
    meshx_err_t model_send          (meshx_gen_def_trans_time_send_params_t *params) override;
    meshx_err_t model_from_ble_cb   (dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;
    meshx_err_t prepare_element_msg (meshx_ptr_t *msg_ptr, size_t *msg_size) override;

    meshXGenericDefTransTimeClientModel(
        meshXElementIF *parent_element = nullptr,
        meshx_ptr_t     parent_element_state = nullptr,
        uint16_t        model_func_id = 0
    );
    ~meshXGenericDefTransTimeClientModel() = default;
};

#endif /* CONFIG_ENABLE_GEN_DEF_TRANS_TIME_CLIENT */

#if CONFIG_ENABLE_GEN_DEF_TRANS_TIME_SERVER
/**
 * @brief Structure to hold the Default Transition Time Server to parent element message.
 *        The structure is used by the on_model_cb function to send the Default Transition Time state
 *        change notification to the parent element.
 */
struct meshx_def_trans_time_srv_el_msg
{
    meshx_srv_model_send_param_header_t header; /**< Server model send param header */
    meshx_gen_def_trans_time_model_state_t state;  /**< The state of the message. */
};

using meshx_def_trans_time_srv_el_msg_t = struct meshx_def_trans_time_srv_el_msg;

/**
 * @class meshXGenericDefTransTimeServerModel
 * @brief A template class for creating Generic Default Transition Time Server models.
 *
 * This class is derived from meshXServerModel and provides a convenient interface for
 * creating Generic Default Transition Time Server models. It handles the Generic Default Transition Time state change
 * notifications from the MeshX stack and publishes the state change event to the
 * element layer.
 */
MESHX_GEN_DEF_TRANS_TIME_SERVER_MODEL_TEMPLATE_PROTO
class meshXGenericDefTransTimeServerModel : public meshXServerModel<meshXBaseGenericServerModel, meshx_gen_def_trans_time_send_params_t>
{
private:
    /* New or updated model state from BLE layer */
    meshx_gen_def_trans_time_model_state_t model_state;

    /* Message to be sent to parent element */
    meshx_def_trans_time_srv_el_msg_t element_msg;

    /* Flag to indicate if element message has been prepared */
    bool element_msg_prepared = false;

    meshx_err_t plat_model_create   (MESHX_MODEL* p_plat_model_ptr = nullptr) override;
    meshx_err_t plat_model_delete   (void) override;

public:
    meshx_err_t model_send          (meshx_gen_def_trans_time_send_params_t *params) override;
    meshx_err_t model_from_ble_cb   (dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;
    meshx_err_t prepare_element_msg (meshx_ptr_t *msg_ptr, size_t *msg_size) override;

    // Virtual method implementation from meshXModelIF
    meshx_err_t element_state_change_handle (void) override;

    meshXGenericDefTransTimeServerModel(
        meshXElementIF *parent_element = nullptr,
        meshx_ptr_t     parent_element_state = nullptr,
        uint16_t        model_func_id = 0
    );
    ~meshXGenericDefTransTimeServerModel() = default;
};
#endif /* CONFIG_ENABLE_GEN_DEF_TRANS_TIME_SERVER */

#endif /* _MESHX_MODEL_DEF_TRANS_TIME_HPP_ */
