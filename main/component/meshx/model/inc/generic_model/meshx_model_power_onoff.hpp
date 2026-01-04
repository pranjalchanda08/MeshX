/**
 * @file meshx_model_power_onoff.hpp
 * @brief Implementation of Generic Power OnOff Model for MeshX
 *
 * This file contains the implementation of the Generic Power OnOff model,
 * which provides standard Power OnOff model functionality in the MeshX BLE mesh framework.
 *
 * Key Features:
 * - Implements Bluetooth SIG-defined Generic Power OnOff model
 * - Inherits from meshXClientModel, meshXServerModel templates
 * - Provides standard Power OnOff control operations
 * - Integrates with MeshX transmission control
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#ifndef _MESHX_MODEL_POWER_ONOFF_HPP_
#define _MESHX_MODEL_POWER_ONOFF_HPP_

#include <meshx_model_class.hpp>
#include <meshx_base_model_generic.hpp>

#define MESHX_GEN_POWER_ONOFF_CLIENT_MODEL_TEMPLATE_PROTO
#define MESHX_GEN_POWER_ONOFF_CLIENT_MODEL_TEMPLATE_PARAMS

#define MESHX_GEN_POWER_ONOFF_SERVER_MODEL_TEMPLATE_PROTO
#define MESHX_GEN_POWER_ONOFF_SERVER_MODEL_TEMPLATE_PARAMS

#define MESHX_GEN_POWER_ONOFF_SETUP_SERVER_MODEL_TEMPLATE_PROTO
#define MESHX_GEN_POWER_ONOFF_SETUP_SERVER_MODEL_TEMPLATE_PARAMS

/**
 * @brief Structure to hold the Power OnOff model state.
 */
struct meshx_gen_power_onoff_model_state
{
    uint8_t on_power_up;  /**< The OnPowerUp state value. */
};

using meshx_gen_power_onoff_model_state_t = struct meshx_gen_power_onoff_model_state;

/**
 * @brief Structure to hold the parameters for sending a Generic Power OnOff message.
 */
struct meshx_gen_power_onoff_send_params
{
    meshx_model_t                      *model;  /**< Pointer to the Power OnOff client model. */
    meshx_ctx_t                        *ctx;    /**< The context of the message. */
    uint8_t                             tid;    /**< The transaction ID of the message. Only used by Client*/
    meshx_gen_power_onoff_model_state_t state;  /**< The state of the message. */
};

using meshx_gen_power_onoff_send_params_t = struct meshx_gen_power_onoff_send_params;

#if CONFIG_ENABLE_GEN_POWER_ONOFF_CLIENT
/**
 * @brief Structure to hold the Power OnOff Server to parent element message.
 *        The structure is used by the on_model_cb function to send the Power OnOff state
 *        change notification to the parent element.
 */
struct meshx_power_onoff_cli_el_msg
{
    meshx_cli_model_send_param_header_t header; /**< Client model send param header */
    meshx_gen_power_onoff_model_state_t state;  /**< The state of the message. */
};

using meshx_power_onoff_cli_el_msg_t = struct meshx_power_onoff_cli_el_msg;
/**
 * @class meshXGenericPowerOnOffClientModel
 * @brief A template class for creating Generic Power OnOff Client models.
 *
 * This class is derived from meshXClientModel and provides a convenient interface for
 * creating Generic Power OnOff Client models. It handles the Generic Power OnOff state change
 * notifications from the MeshX stack and publishes the state change event to the
 * element layer.
 */
MESHX_GEN_POWER_ONOFF_CLIENT_MODEL_TEMPLATE_PROTO
class meshXGenericPowerOnOffClientModel : public meshXClientModel<meshXBaseGenericClientModel, meshx_gen_power_onoff_send_params_t>
{
private:
    /* New or updated model state from BLE layer */
    meshx_gen_power_onoff_model_state_t model_state;

    /* Message to be sent to parent element */
    meshx_power_onoff_cli_el_msg_t element_msg;

    meshx_err_t meshx_state_change_notify   (const meshx_gen_cli_cb_param_t *param, uint8_t status);
    meshx_err_t element_state_change_handle (void) override;

public:
    meshx_err_t model_send          (meshx_gen_power_onoff_send_params_t *params) override;
    meshx_err_t model_from_ble_cb   (dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;
    meshx_err_t prepare_element_msg (meshx_ptr_t *msg_ptr, size_t *msg_size) override;

    meshXGenericPowerOnOffClientModel(
        meshXElementIF *parent_element = nullptr,
        meshx_ptr_t     parent_element_state = nullptr
    );
    ~meshXGenericPowerOnOffClientModel() = default;
};

#endif /* CONFIG_ENABLE_GEN_POWER_ONOFF_CLIENT */

#if CONFIG_ENABLE_GEN_POWER_ONOFF_SERVER

/**
 * @brief Structure to hold the Power OnOff Server to parent element message.
 *        The structure is used by the on_model_cb function to send the Power OnOff state
 *        change notification to the parent element.
 */
struct meshx_power_onoff_srv_el_msg
{
    meshx_srv_model_send_param_header_t header; /**< Server model send param header */
    meshx_gen_power_onoff_model_state_t state;  /**< The state of the message. */
};

using meshx_power_onoff_srv_el_msg_t = struct meshx_power_onoff_srv_el_msg;

/**
 * @class meshXGenericPowerOnOffServerModel
 * @brief A template class for creating Generic Power OnOff Server models.
 *
 * This class is derived from meshXServerModel and provides a convenient interface for
 * creating Generic Power OnOff Server models. It handles the Generic Power OnOff state change
 * notifications from the MeshX stack and publishes the state change event to the
 * element layer.
 */
MESHX_GEN_POWER_ONOFF_SERVER_MODEL_TEMPLATE_PROTO
class meshXGenericPowerOnOffServerModel : public meshXServerModel<meshXBaseGenericServerModel, meshx_gen_power_onoff_send_params_t>
{
private:
    /* New or updated model state from BLE layer */
    meshx_gen_power_onoff_model_state_t model_state;

    /* Message to be sent to parent element */
    meshx_power_onoff_srv_el_msg_t element_msg;

    /* Flag to indicate if element message has been prepared */
    bool element_msg_prepared = false;

    meshx_err_t plat_model_create   (void) override;
    meshx_err_t plat_model_delete   (void) override;

public:
    meshx_err_t model_send          (meshx_gen_power_onoff_send_params_t *params) override;
    meshx_err_t model_from_ble_cb   (dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;
    meshx_err_t prepare_element_msg (meshx_ptr_t *msg_ptr, size_t *msg_size) override;

    // Virtual method implementation from meshXModelIF
    meshx_err_t element_state_change_handle (void) override;

    meshXGenericPowerOnOffServerModel(
        meshXElementIF *parent_element = nullptr,
        meshx_ptr_t     parent_element_state = nullptr
    );
    ~meshXGenericPowerOnOffServerModel() = default;
};
#endif /* CONFIG_ENABLE_GEN_POWER_ONOFF_SERVER */

#if CONFIG_ENABLE_GEN_POWER_ONOFF_SETUP_SERVER
/**
 * @class meshXGenericPowerOnOffSetupServerModel
 * @brief A template class for creating Generic Power OnOff Setup Server models.
 *
 * This class is derived from meshXServerModel and provides a convenient interface for
 * creating Generic Power OnOff Setup Server models. It handles the Generic Power OnOff setup
 * operations from the MeshX stack.
 */
MESHX_GEN_POWER_ONOFF_SETUP_SERVER_MODEL_TEMPLATE_PROTO
class meshXGenericPowerOnOffSetupServerModel : public meshXServerModel<meshXBaseGenericServerModel, meshx_gen_power_onoff_send_params_t>
{
private:
    /* Message to be sent to parent element */
    meshx_power_onoff_srv_el_msg_t element_msg;

    /* Flag to indicate if element message has been prepared */
    bool element_msg_prepared = false;

public:
    meshx_err_t model_send          (meshx_gen_power_onoff_send_params_t *params) override;
    meshx_err_t model_from_ble_cb   (dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;
    meshx_err_t prepare_element_msg (meshx_ptr_t *msg_ptr, size_t *msg_size) override;

    meshXGenericPowerOnOffSetupServerModel(
        meshXElementIF *parent_element = nullptr,
        meshx_ptr_t     parent_element_state = nullptr
    );
    ~meshXGenericPowerOnOffSetupServerModel() = default;
};
#endif /* CONFIG_ENABLE_GEN_POWER_ONOFF_SETUP_SERVER */

#endif /* CONFIG_ENABLE_GEN_POWER_ONOFF */
