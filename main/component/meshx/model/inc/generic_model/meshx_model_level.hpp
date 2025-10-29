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
#include <meshx_model_class.hpp>
#include <meshx_base_model_generic.hpp>

#define MESHX_GEN_LEVEL_CLIENT_MODEL_TEMPLATE_PROTO
#define MESHX_GEN_LEVEL_CLIENT_MODEL_TEMPLATE_PARAMS

#define MESHX_GEN_LEVEL_SERVER_MODEL_TEMPLATE_PROTO
#define MESHX_GEN_LEVEL_SERVER_MODEL_TEMPLATE_PARAMS

/**
 * @brief Structure to hold the parameters for sending a Generic Level message.
 */
struct meshx_gen_level_send_params
{
    meshx_model_t *model;    /**< Pointer to the Level client model. */
    meshx_ctx_t *ctx;        /**< The context of the message. */
    int16_t level;           /**< The level value of the message. */
    uint8_t tid;             /**< The transaction ID of the message. Only used by Client*/
    uint8_t transition_time; /**< Transition time (optional). */
    uint8_t delay;           /**< Delay (optional). */
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
    uint8_t err_code;       /**< Error code */
    meshx_model_t model;    /**< Generic Level Server model */
    meshx_ctx_t ctx;        /**< Context of the message */
    int16_t present_level;  /**< The present value of Generic Level state */
    int16_t target_level;   /**< The target value of Generic Level state (optional) */
    uint8_t remaining_time; /**< Remaining transition time (optional) */
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
    meshx_err_t meshx_state_change_notify(const meshx_gen_cli_cb_param_t *param, uint8_t status) const;

public:
    meshx_err_t model_send(meshx_gen_level_send_params_t *params) override;
    meshx_err_t model_from_ble_cb(dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;

    meshXGenericLevelClientModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element = nullptr);
    ~meshXGenericLevelClientModel() = default;
};

#endif /* CONFIG_ENABLE_GEN_LEVEL_CLIENT */

#if CONFIG_ENABLE_GEN_LEVEL_SERVER

using meshx_level_srv_el_msg_t = struct meshx_level_srv_el_msg
{
    meshx_model_t *model; /**< Generic Level Server model */
    int16_t level;        /**< The current level value */
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
    meshx_err_t plat_model_create(void) override;
    meshx_err_t plat_model_delete(void) override;
public:
    meshx_err_t model_send(meshx_gen_level_send_params_t *params) override;
    meshx_err_t model_from_ble_cb(dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;

    meshXGenericLevelServerModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element = nullptr);
    ~meshXGenericLevelServerModel() = default;
};
#endif /* CONFIG_ENABLE_GEN_LEVEL_SERVER */
