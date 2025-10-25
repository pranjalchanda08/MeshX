/**
 * @file meshx_model_generic.hpp
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
#include <meshx_model_class.hpp>
#include <meshx_base_model_generic.hpp>


#define MESHX_GEN_ONOFF_CLIENT_MODEL_TEMPLATE_PROTO
#define MESHX_GEN_ONOFF_CLIENT_MODEL_TEMPLATE_PARAMS

#define MESHX_GEN_ONOFF_SERVER_MODEL_TEMPLATE_PROTO
#define MESHX_GEN_ONOFF_SERVER_MODEL_TEMPLATE_PARAMS
/**
 * @brief Structure to hold the parameters for sending a Generic OnOff message.
 */
struct meshx_gen_onoff_send_params
{
    meshx_model_t *model; /**< Pointer to the On/Off client model. */
    meshx_ctx_t *ctx;     /**< The context of the message. */
    uint8_t state;        /**< The state of the message. */
    uint8_t tid;          /**< The transaction ID of the message. Only sed by Client*/
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
    uint8_t err_code;           /**< Error code */
    meshx_model_t model;        /**< Generic OnOff Server model */
    meshx_ctx_t ctx;            /**< Context of the message */
    uint8_t on_off_state;       /**< The present value of Generic OnOff state */
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
class meshXGenericOnOffClientModel : public meshXClientModel <meshXBaseGenericClientModel, meshx_gen_onoff_send_params_t>
{
private:
    meshx_err_t meshx_state_change_notify(const meshx_gen_cli_cb_param_t *param, uint8_t status) const;
public:
    meshx_err_t model_send(meshx_gen_onoff_send_params_t *params) override;
    meshx_err_t model_from_ble_cb(dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;

    meshXGenericOnOffClientModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element = nullptr);
    ~meshXGenericOnOffClientModel() = default;
};

#endif /* CONFIG_ENABLE_GEN_ONOFF_CLIENT */

/********************************************************************************************************************************** */
#if CONFIG_ENABLE_GEN_ONOFF_SERVER
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
class meshXGenericOnOffServerModel : public meshXServerModel <meshXBaseGenericServerModel, meshx_gen_onoff_send_params_t>
{
public:
    meshx_err_t model_send(meshx_gen_onoff_send_params_t *params) override;
    meshx_err_t model_from_ble_cb(dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;

    meshXGenericOnOffServerModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element = nullptr);
    ~meshXGenericOnOffServerModel() = default;
};
#endif /* CONFIG_ENABLE_GEN_ONOFF_SERVER */
