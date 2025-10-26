/**
 * @file meshx_model_battery.hpp
 * @brief Implementation of Generic Battery Model for MeshX
 *
 * This file contains the implementation of the Generic Battery model,
 * which provides standard Battery model functionality in the MeshX BLE mesh framework.
 *
 * Key Features:
 * - Implements Bluetooth SIG-defined Generic Battery model
 * - Inherits from meshXClientModel and meshXServerModel templates
 * - Provides standard Battery status operations
 * - Integrates with MeshX transmission control
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */
#include <meshx_model_class.hpp>
#include <meshx_base_model_generic.hpp>

#define MESHX_GEN_BATTERY_CLIENT_MODEL_TEMPLATE_PROTO
#define MESHX_GEN_BATTERY_CLIENT_MODEL_TEMPLATE_PARAMS

#define MESHX_GEN_BATTERY_SERVER_MODEL_TEMPLATE_PROTO
#define MESHX_GEN_BATTERY_SERVER_MODEL_TEMPLATE_PARAMS

/**
 * @brief Structure to hold the parameters for sending a Generic Battery message.
 */
struct meshx_gen_battery_send_params
{
    meshx_model_t *model;       /**< Pointer to the Battery client model. */
    meshx_ctx_t *ctx;           /**< The context of the message. */
    uint8_t battery_level;      /**< The battery level (0-100%). */
    uint32_t time_to_discharge; /**< Time to discharge in minutes. */
    uint32_t time_to_charge;    /**< Time to charge in minutes. */
    uint8_t presence;           /**< Presence of battery. */
    uint8_t charge_level;       /**< Charge level indicators. */
    uint8_t charge_type;        /**< Charge type indicators. */
};

using meshx_gen_battery_send_params_t = struct meshx_gen_battery_send_params;

#if CONFIG_ENABLE_GEN_BATTERY_CLIENT
/**
 * @brief Structure to hold the Battery Server to parent element message.
 *        The structure is used by the on_model_cb function to send the Battery state
 *        change notification to the parent element.
 */
struct meshx_battery_cli_el_msg
{
    uint8_t err_code;                /**< Error code */
    meshx_model_t model;             /**< Generic Battery Server model */
    meshx_ctx_t ctx;                 /**< Context of the message */
    uint32_t battery_level : 8;      /*!< Value of Generic Battery Level state */
    uint32_t time_to_discharge : 24; /*!< Value of Generic Battery Time to Discharge state */
    uint32_t time_to_charge : 24;    /*!< Value of Generic Battery Time to Charge state */
    uint32_t flags : 8;              /*!< Value of Generic Battery Flags state */
};

using meshx_battery_cli_el_msg_t = struct meshx_battery_cli_el_msg;
/**
 * @class meshXGenericBatteryClientModel
 * @brief A template class for creating Generic Battery Client models.
 *
 * This class is derived from meshXClientModel and provides a convenient interface for
 * creating Generic Battery Client models. It handles the Generic Battery state change
 * notifications from the MeshX stack and publishes the state change event to the
 * element layer.
 */
MESHX_GEN_BATTERY_CLIENT_MODEL_TEMPLATE_PROTO
class meshXGenericBatteryClientModel : public meshXClientModel<meshXBaseGenericClientModel, meshx_gen_battery_send_params_t>
{
private:
    meshx_err_t meshx_state_change_notify(const meshx_gen_cli_cb_param_t *param, uint8_t status) const;

public:
    meshx_err_t model_send(meshx_gen_battery_send_params_t *params) override;
    meshx_err_t model_from_ble_cb(dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;

    meshXGenericBatteryClientModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element = nullptr);
    ~meshXGenericBatteryClientModel() = default;
};

#endif /* CONFIG_ENABLE_GEN_BATTERY_CLIENT */

#if CONFIG_ENABLE_GEN_BATTERY_SERVER

using meshx_battery_srv_el_msg_t = struct meshx_battery_srv_el_msg
{
    meshx_model_t *model;       /**< Generic Battery Server model */
    meshx_ctx_t *ctx;           /**< Context of the message */
    uint8_t battery_level;      /**< The battery level (0-100%). */
    uint32_t time_to_discharge; /**< Time to discharge in minutes. */
    uint32_t time_to_charge;    /**< Time to charge in minutes. */
    uint8_t presence;           /**< Presence of battery. */
    uint8_t charge_level;       /**< Charge level indicators. */
    uint8_t charge_type;        /**< Charge type indicators. */
};

/**
 * @class meshXGenericBatteryServerModel
 * @brief A template class for creating Generic Battery Server models.
 *
 * This class is derived from meshXServerModel and provides a convenient interface for
 * creating Generic Battery Server models. It handles the Generic Battery state change
 * notifications from the MeshX stack and publishes the state change event to the
 * element layer.
 */
MESHX_GEN_BATTERY_SERVER_MODEL_TEMPLATE_PROTO
class meshXGenericBatteryServerModel : public meshXServerModel<meshXBaseGenericServerModel, meshx_gen_battery_send_params_t>
{
public:
    meshx_err_t model_send(meshx_gen_battery_send_params_t *params) override;
    meshx_err_t model_from_ble_cb(dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;

    meshXGenericBatteryServerModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element = nullptr);
    ~meshXGenericBatteryServerModel() = default;
};
#endif /* CONFIG_ENABLE_GEN_BATTERY_SERVER */
