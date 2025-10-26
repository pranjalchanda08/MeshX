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
#include <meshx_model_class.hpp>
#include <meshx_base_model_generic.hpp>

#define MESHX_GEN_DEF_TRANS_TIME_CLIENT_MODEL_TEMPLATE_PROTO
#define MESHX_GEN_DEF_TRANS_TIME_CLIENT_MODEL_TEMPLATE_PARAMS

#define MESHX_GEN_DEF_TRANS_TIME_SERVER_MODEL_TEMPLATE_PROTO
#define MESHX_GEN_DEF_TRANS_TIME_SERVER_MODEL_TEMPLATE_PARAMS

/**
 * @brief Structure to hold the parameters for sending a Generic Default Transition Time message.
 */
struct meshx_gen_def_trans_time_send_params
{
    meshx_model_t *model; /**< Pointer to the Default Transition Time client model. */
    meshx_ctx_t *ctx;     /**< The context of the message. */
    uint8_t trans_time;   /**< The Default Transition Time value. */
    uint8_t tid;          /**< The transaction ID of the message. Only used by Client*/
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
    uint8_t err_code;    /**< Error code */
    meshx_model_t model; /**< Generic Default Transition Time Server model */
    meshx_ctx_t ctx;     /**< Context of the message */
    uint8_t trans_time;  /**< The present value of Generic Default Transition Time state */
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
    meshx_err_t meshx_state_change_notify(const meshx_gen_cli_cb_param_t *param, uint8_t status) const;

public:
    meshx_err_t model_send(meshx_gen_def_trans_time_send_params_t *params) override;
    meshx_err_t model_from_ble_cb(dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;

    meshXGenericDefTransTimeClientModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element = nullptr);
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
    meshx_model_t *model; /**< Generic Default Transition Time Server model */
    uint8_t trans_time;   /**< The present value of Generic Default Transition Time state */
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
public:
    meshx_err_t model_send(meshx_gen_def_trans_time_send_params_t *params) override;
    meshx_err_t model_from_ble_cb(dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;

    meshXGenericDefTransTimeServerModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element = nullptr);
    ~meshXGenericDefTransTimeServerModel() = default;
};
#endif /* CONFIG_ENABLE_GEN_DEF_TRANS_TIME_SERVER */
