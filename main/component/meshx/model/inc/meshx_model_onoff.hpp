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

#if CONFIG_ENABLE_GEN_ONOFF_CLIENT

#define MESHX_GEN_ONOFF_CLIENT_MODEL_TEMPLATE_PROTO
#define MESHX_GEN_ONOFF_CLIENT_MODEL_TEMPLATE_PARAMS

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
    meshx_err_t meshx_state_change_notify(const meshx_gen_cli_cb_param_t *param, uint8_t status);
public:
    meshx_err_t model_from_ble_cb(dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;
    meshXGenericOnOffClientModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElement *parent_element = nullptr);
    ~meshXGenericOnOffClientModel() = default;
};

#endif /* CONFIG_ENABLE_GEN_ONOFF_CLIENT */
