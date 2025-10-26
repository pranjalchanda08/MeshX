/**
 * @file meshx_model_power_level.hpp
 * @brief Implementation of Generic Power Level Model for MeshX
 *
 * This file contains the implementation of the Generic Power Level model,
 * which provides standard Power Level model functionality in the MeshX BLE mesh framework.
 *
 * Key Features:
 * - Implements Bluetooth SIG-defined Generic Power Level model
 * - Inherits from meshXClientModel, meshXServerModel templates
 * - Provides standard Power Level control operations (GET, SET, LAST, DEFAULT, RANGE)
 * - Integrates with MeshX transmission control
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */
#include <meshx_model_class.hpp>
#include <meshx_base_model_generic.hpp>

#define MESHX_GEN_POWER_LEVEL_CLIENT_MODEL_TEMPLATE_PROTO
#define MESHX_GEN_POWER_LEVEL_CLIENT_MODEL_TEMPLATE_PARAMS

#define MESHX_GEN_POWER_LEVEL_SERVER_MODEL_TEMPLATE_PROTO
#define MESHX_GEN_POWER_LEVEL_SERVER_MODEL_TEMPLATE_PARAMS

#define MESHX_GEN_POWER_LEVEL_SETUP_SERVER_MODEL_TEMPLATE_PROTO
#define MESHX_GEN_POWER_LEVEL_SETUP_SERVER_MODEL_TEMPLATE_PARAMS

/**
 * @brief Structure to hold the parameters for sending a Generic Power Level message.
 */
struct meshx_gen_power_level_send_params
{
    meshx_model_t *model;     /**< Pointer to the Power Level client model. */
    meshx_ctx_t *ctx;         /**< The context of the message. */
    uint16_t power_level;     /**< The Power Level value. */
    uint16_t power_default;   /**< The Power Default value (for setup). */
    uint16_t power_range_min; /**< The Power Range Min value (for setup). */
    uint16_t power_range_max; /**< The Power Range Max value (for setup). */
    uint8_t tid;              /**< The transaction ID of the message. Only used by Client*/
    uint8_t transition_time;  /**< Transition time (optional). */
    uint8_t delay;            /**< Delay (optional). */
};

using meshx_gen_power_level_send_params_t = struct meshx_gen_power_level_send_params;

#if CONFIG_ENABLE_GEN_POWER_LEVEL_CLIENT
/**
 * @brief Structure to hold the Power Level Server to parent element message.
 *        The structure is used by the on_model_cb function to send the Power Level state
 *        change notification to the parent element.
 */
struct meshx_power_level_cli_el_msg
{
    uint8_t err_code;       /**< Error code */
    meshx_model_t model;    /**< Generic Power Level Server model */
    meshx_ctx_t ctx;        /**< Context of the message */
    uint16_t present_power; /**< The present value of Generic Power Level state */
    uint16_t target_power;  /**< The target value of Generic Power Level state (optional) */
    uint8_t remain_time;    /**< Time to complete state transition (C.1) */
};

using meshx_power_level_cli_el_msg_t = struct meshx_power_level_cli_el_msg;
/**
 * @class meshXGenericPowerLevelClientModel
 * @brief A template class for creating Generic Power Level Client models.
 *
 * This class is derived from meshXClientModel and provides a convenient interface for
 * creating Generic Power Level Client models. It handles the Generic Power Level state change
 * notifications from the MeshX stack and publishes the state change event to the
 * element layer.
 */
MESHX_GEN_POWER_LEVEL_CLIENT_MODEL_TEMPLATE_PROTO
class meshXGenericPowerLevelClientModel : public meshXClientModel<meshXBaseGenericClientModel, meshx_gen_power_level_send_params_t>
{
private:
    meshx_err_t meshx_state_change_notify(const meshx_gen_cli_cb_param_t *param, uint8_t status) const;

public:
    meshx_err_t model_send(meshx_gen_power_level_send_params_t *params) override;
    meshx_err_t model_from_ble_cb(dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;

    meshXGenericPowerLevelClientModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element = nullptr);
    ~meshXGenericPowerLevelClientModel() = default;
};

#endif /* CONFIG_ENABLE_GEN_POWER_LEVEL_CLIENT */

#if CONFIG_ENABLE_GEN_POWER_LEVEL_SERVER

/**
 * @brief Structure to hold the Power Level Server to parent element message.
 *        The structure is used by the on_model_cb function to send the Power Level state
 *        change notification to the parent element.
 */
struct meshx_power_level_srv_el_msg
{
    meshx_model_t *model;   /**< Generic Power Level Server model */
    uint16_t power_default; /**< The default power level */
    struct
    {
        uint16_t range_min; /**< Minimum value of Generic Power Level state */
        uint16_t range_max; /**< Maximum value of Generic Power Level state */
    } range;                /**< Power level range parameters */
};

using meshx_power_level_srv_el_msg_t = struct meshx_power_level_srv_el_msg;

/**
 * @class meshXGenericPowerLevelServerModel
 * @brief A template class for creating Generic Power Level Server models.
 *
 * This class is derived from meshXServerModel and provides a convenient interface for
 * creating Generic Power Level Server models. It handles the Generic Power Level state change
 * notifications from the MeshX stack and publishes the state change event to the
 * element layer.
 */
MESHX_GEN_POWER_LEVEL_SERVER_MODEL_TEMPLATE_PROTO
class meshXGenericPowerLevelServerModel : public meshXServerModel<meshXBaseGenericServerModel, meshx_gen_power_level_send_params_t>
{
public:
    meshx_err_t model_send(meshx_gen_power_level_send_params_t *params) override;
    meshx_err_t model_from_ble_cb(dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;

    meshXGenericPowerLevelServerModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element = nullptr);
    ~meshXGenericPowerLevelServerModel() = default;
};
#endif /* CONFIG_ENABLE_GEN_POWER_LEVEL_SERVER */

#if CONFIG_ENABLE_GEN_POWER_LEVEL_SETUP_SERVER
/**
 * @class meshXGenericPowerLevelSetupServerModel
 * @brief A template class for creating Generic Power Level Setup Server models.
 *
 * This class is derived from meshXServerModel and provides a convenient interface for
 * creating Generic Power Level Setup Server models. It handles the Generic Power Level setup
 * operations from the MeshX stack.
 */
MESHX_GEN_POWER_LEVEL_SETUP_SERVER_MODEL_TEMPLATE_PROTO
class meshXGenericPowerLevelSetupServerModel : public meshXServerModel<meshXBaseGenericServerModel, meshx_gen_power_level_send_params_t>
{
public:
    meshx_err_t model_send(meshx_gen_power_level_send_params_t *params) override;
    meshx_err_t model_from_ble_cb(dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;

    meshXGenericPowerLevelSetupServerModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element = nullptr);
    ~meshXGenericPowerLevelSetupServerModel() = default;
};
#endif /* CONFIG_ENABLE_GEN_POWER_LEVEL_SETUP_SERVER */
