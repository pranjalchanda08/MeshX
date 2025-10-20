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

class meshXGenericOnOffClientModel : public meshXClientModel<meshXBaseClientModel, ble_mesh_send_msg_params>
{
public:
    meshXGenericOnOffClientModel(uint32_t model_id, const control_msg_cb& from_ble_cb);
    ~meshXGenericOnOffClientModel() = default;
};
