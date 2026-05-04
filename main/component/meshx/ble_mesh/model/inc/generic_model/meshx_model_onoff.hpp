/**
 * @file meshx_model_onoff.hpp
 * @brief Implementation of Generic OnOff Client Model for MeshX
 *
 * This file contains the implementation of the Generic OnOff client model,
 * which provides standard OnOff model functionality in the MeshX BLE mesh framework.
 *
 * Key Features:
 * - Implements Bluetooth SIG-defined Generic OnOff model
 * - Inherits from meshXClientModel template
 * - Provides standard OnOff control operations
 * - Integrates with MeshX transmission control
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#ifndef _MESHX_MODEL_ONOFF_HPP_
#define _MESHX_MODEL_ONOFF_HPP_

#include <meshx_model_class.hpp>
#include <meshx_base_model_generic.hpp>

#define MESHX_GEN_ONOFF_CLIENT_MODEL_TEMPLATE_PROTO
#define MESHX_GEN_ONOFF_CLIENT_MODEL_TEMPLATE_PARAMS

#define MESHX_GEN_ONOFF_SERVER_MODEL_TEMPLATE_PROTO
#define MESHX_GEN_ONOFF_SERVER_MODEL_TEMPLATE_PARAMS

struct meshx_gen_onoff_model_state
{
    uint8_t on_off;        /**< The onoff state of the message. */
};

using meshx_gen_onoff_model_state_t = struct meshx_gen_onoff_model_state;

/**
 * @brief Structure to hold the parameters for sending a Generic OnOff message.
 */
struct meshx_gen_onoff_send_params
{
    meshx_model_t                  *model;  /**< Pointer to the On/Off client model. */
    meshx_ctx_t                    *ctx;    /**< The context of the message. */
    meshx_gen_onoff_model_state_t   state;  /**< The state of the message. */
    uint8_t                         tid;    /**< The transaction ID of the message. Only sed by Client*/
};

using meshx_gen_onoff_send_params_t = struct meshx_gen_onoff_send_params;

#if CONFIG_ENABLE_GEN_ONOFF_CLIENT
/**
 * @brief Structure to hold the On/Off Server to parent element message.
 *        The structure is used by the on_model_cb function to send the On/Off state
 *        change notification to the parent element.
 */
struct meshx_on_off_cli_el_msg
{
    meshx_cli_model_send_param_header_t header; /**< Client model send param header */
    meshx_gen_onoff_model_state_t       state;  /**< The present value of Generic OnOff state */
};

using meshx_on_off_cli_el_msg_t = struct meshx_on_off_cli_el_msg;
/**
 * @class meshXGenericOnOffClientModel
 * @brief A template class for creating Generic OnOff Client models.
 *
 * This class is derived from meshXClientModel and provides a convenient interface for
 * creating Generic OnOff Client models. It handles the Generic OnOff state change
 * notifications from the MeshX stack and publishes the state change event to the
 * element layer.
 */
MESHX_GEN_ONOFF_CLIENT_MODEL_TEMPLATE_PROTO
class meshXGenericOnOffClientModel : public meshXClientModel<meshXBaseGenericClientModel, meshx_gen_onoff_send_params_t>
{
private:
    /* New or updated model state from BLE layer */
    meshx_gen_onoff_model_state_t model_state;

    /* Message to send to parent element - stored as member to persist */
    meshx_on_off_cli_el_msg_t element_msg;

    meshx_err_t meshx_state_change_notify   (const meshx_gen_cli_cb_param_t *param, uint8_t status);
    meshx_err_t element_state_change_handle (void) override;
public:
    meshx_err_t model_send          (meshx_gen_onoff_send_params_t *params) override;
    meshx_err_t model_from_ble_cb   (dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;
    meshx_err_t prepare_element_msg (meshx_ptr_t *msg_ptr, size_t *msg_size) override;

    explicit meshXGenericOnOffClientModel(
        meshXElementIF *parent_element = nullptr,
        meshx_ptr_t     parent_element_state = nullptr,
        uint16_t        model_func_id = 0
    );
    ~meshXGenericOnOffClientModel() override = default;
};

#endif /* CONFIG_ENABLE_GEN_ONOFF_CLIENT */

/********************************************************************************************************************************** */
#if CONFIG_ENABLE_GEN_ONOFF_SERVER
/**
 * @brief Structure to hold the On/Off Server to element message.
 */
struct meshx_on_off_srv_el_msg
{
    meshx_srv_model_send_param_header_t header; /**< Server model send param header */
    meshx_gen_onoff_model_state_t       state;  /**< The present value of Generic OnOff state */
};

using meshx_on_off_srv_el_msg_t = struct meshx_on_off_srv_el_msg;
/**
 * @class meshXGenericOnOffServerModel
 * @brief A template class for creating Generic OnOff Server models.
 *
 * This class is derived from meshXServerModel and provides a convenient interface for
 * creating Generic OnOff Server models. It handles the Generic OnOff state change
 * notifications from the MeshX stack and publishes the state change event to the
 * element layer.
 */
MESHX_GEN_ONOFF_SERVER_MODEL_TEMPLATE_PROTO
class meshXGenericOnOffServerModel : public meshXServerModel<meshXBaseGenericServerModel, meshx_gen_onoff_send_params_t>
{
private:
    /* New or updated model state from BLE layer */
    meshx_gen_onoff_model_state_t model_state;

    /* Message to send to parent element - stored as member to persist */
    meshx_on_off_srv_el_msg_t element_msg;

    /* Flag to indicate if message was prepared for element notification */
    bool element_msg_prepared;

    meshx_err_t plat_model_create   (MESHX_MODEL* p_plat_model_ptr = nullptr) override;
    meshx_err_t plat_model_delete   (void) override;

public:
    meshx_err_t model_send          (meshx_gen_onoff_send_params_t *params) override;
    meshx_err_t model_from_ble_cb   (dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;
    meshx_err_t prepare_element_msg (meshx_ptr_t *msg_ptr, size_t *msg_size) override;

    // Virtual method implementation from meshXModelIF
    meshx_err_t element_state_change_handle (void) override;

    explicit meshXGenericOnOffServerModel(
        meshXElementIF *parent_element = nullptr,
        meshx_ptr_t     parent_element_state = nullptr,
        uint16_t        model_func_id = 0
    );

    ~meshXGenericOnOffServerModel() override = default;
};
#endif /* CONFIG_ENABLE_GEN_ONOFF_SERVER */

#endif /* _MESHX_MODEL_ONOFF_HPP_ */
