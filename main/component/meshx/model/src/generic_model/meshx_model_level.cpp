/**
 * @file meshx_model_level.cpp
 * @brief Implementation of Generic Level Model classes for MeshX.
 *        This file contains the implementation of the Generic Level Server and Client models
 *        for the MeshX BLE mesh framework.
 *
 * Key Features:
 *  - Implements Bluetooth SIG-defined Generic Level model
 *  - Inherits from meshXServerModel and meshXClientModel templates
 *  - Provides standard Level control operations (SET, GET, DELTA, MOVE)
 *  - Integrates with MeshX transmission control
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#include <generic_model/meshx_model_level.hpp>
#include <meshx_element_class.hpp>

#if CONFIG_ENABLE_GEN_LEVEL_CLIENT
/**
 * @brief Handle Generic Level state change notifications from the MeshX stack.
 *
 * This function is responsible for handling Generic Level state change notifications
 * from the MeshX stack. It is called when the MeshX stack receives a state
 * change event from the Generic Level server model. The function publishes the
 * state change event to the control task framework, which in turn notifies the
 * application about the state change.
 *
 * @param[in] param  Pointer to the Generic Level client callback parameter structure.
 * @param[in] status Status of the state change event (success or timeout).
 *
 * @return
 *     - MESHX_SUCCESS: Successfully handled the state change notification.
 *     - MESHX_INVALID_ARG: One or more arguments are invalid.
 */
MESHX_GEN_LEVEL_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericLevelClientModel MESHX_GEN_LEVEL_CLIENT_MODEL_TEMPLATE_PARAMS
    :: meshx_state_change_notify(const meshx_gen_cli_cb_param_t *param, uint8_t status) const
{
    if (!param){
        return MESHX_INVALID_ARG;
    }

    meshx_level_cli_el_msg_t level_param = {
        .err_code = status,
        .model = param->model,
        .ctx = param->ctx,
        .present_level = param->status.level_status.present_level,
        .target_level = param->status.level_status.target_level,
        .remaining_time = param->status.level_status.remain_time
    };
    /* Send the state change event to the respective Element */
    if (this->get_parent_element()) {
        return this->get_parent_element()->on_model_cb(&level_param);
    } else {
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Parent element is null");
    }

    return MESHX_INVALID_STATE;
}
/**
 * @brief Creates a meshXGenericLevelClientModel instance based on a BLE device
 *
 * This function is used to create a meshXGenericLevelClientModel instance based on a BLE device.
 * It takes the device structure, model ID, and parameters as input and returns a pointer to the created instance.
 *
 * @param[in] p_dev     Pointer to the device structure
 * @param[in] model_id  Model ID associated with the device
 * @param[in] params    Pointer to the parameters associated with the device
 *
 * @return Pointer to the created meshXGenericLevelClientModel instance
 */
MESHX_GEN_LEVEL_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericLevelClientModel MESHX_GEN_LEVEL_CLIENT_MODEL_TEMPLATE_PARAMS
    :: model_from_ble_cb(
        dev_struct_t *p_dev,
        control_task_msg_evt_t model_id,
        meshx_ptr_t params)
{
    if(!params || !p_dev)
    {
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Invalid parameters");
        return MESHX_INVALID_ARG;
    }
    if(model_id != MESHX_MODEL_ID_GEN_LEVEL_CLI)
    {
        /* Callback triggered not for this model */
        return MESHX_SUCCESS;
    }
    const auto *param = static_cast<const meshx_gen_cli_cb_param_t *>(params);

    return std::to_underlying(param->evt) == std::to_underlying(meshx_base_cli_evt::MESHX_BASE_CLI_TIMEOUT) ?
        meshx_state_change_notify(param, MESHX_TIMEOUT) :
        meshx_state_change_notify(param, MESHX_SUCCESS);
}

/**
 * @brief Send a packet to the MeshX stack based on the given parameters
 *
 * This function takes a pointer to a meshx_gen_level_send_params_t structure as input
 * and sends the corresponding packet to the MeshX stack for processing.
 *
 * @param[in] params Pointer to the meshx_gen_level_send_params_t structure containing the parameters
 * @return MESHX_SUCCESS on success, or an error code on failure
 */
MESHX_GEN_LEVEL_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericLevelClientModel MESHX_GEN_LEVEL_CLIENT_MODEL_TEMPLATE_PARAMS
    :: model_send(meshx_gen_level_send_params_t *params)
{
    meshx_err_t err;
    meshx_gen_cli_set_t set;
    if (!params || !params->model || !params->model->p_model)
    {
        return MESHX_INVALID_ARG;
    }

    meshx_gen_client_send_params_t send_params;

    send_params.state   = &set;
    send_params.opcode  = static_cast<uint16_t>(params->ctx->opcode);
    send_params.net_idx = params->ctx->net_idx;
    send_params.app_idx = params->ctx->app_idx;
    send_params.addr    = params->model->pub_addr;
    send_params.model   = params->model->p_model;

    if (params->ctx->opcode == MESHX_MODEL_OP_GEN_LEVEL_GET)
    {
        err = this->get_base_model()->plat_send_msg(&send_params);
    }

    else if (params->ctx->opcode == MESHX_MODEL_OP_GEN_LEVEL_SET ||
             params->ctx->opcode == MESHX_MODEL_OP_GEN_LEVEL_SET_UNACK)
    {
        set.level_set.level = params->level;
        set.level_set.tid = params->tid;
        set.level_set.trans_time = params->transition_time;
        set.level_set.delay = params->delay;

        err = this->get_base_model()->plat_send_msg(&send_params);
    }
    else if (params->ctx->opcode == MESHX_MODEL_OP_GEN_DELTA_SET ||
             params->ctx->opcode == MESHX_MODEL_OP_GEN_DELTA_SET_UNACK)
    {
        set.delta_set.level = params->level; // Using level field for delta value
        set.delta_set.tid = params->tid;
        set.delta_set.trans_time = params->transition_time;
        set.delta_set.delay = params->delay;

        err = this->get_base_model()->plat_send_msg(&send_params);
    }
    else if (params->ctx->opcode == MESHX_MODEL_OP_GEN_MOVE_SET ||
             params->ctx->opcode == MESHX_MODEL_OP_GEN_MOVE_SET_UNACK)
    {
        set.move_set.delta_level = params->level; // Using level field for move delta
        set.move_set.tid = params->tid;
        set.move_set.trans_time = params->transition_time;
        set.move_set.delay = params->delay;

        err = this->get_base_model()->plat_send_msg(&send_params);
    }
    else
    {
        err = MESHX_INVALID_ARG; // Invalid opcode
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Invalid opcode for Generic Level Client: %04x", params->ctx->opcode);
    }
    return err;
}
/**
 * @brief A template class for creating Generic Level Client models.
 *
 * This class is derived from meshXClientModel and provides a convenient interface for
 * creating Generic Level Client models. It handles the Generic Level state change
 * notifications from the MeshX stack and publishes the state change event to the
 * element layer.
 *
 * @tparam meshxBaseClientModel_t The type of the meshXBaseClientModel class to be used.
 * @tparam meshx_send_packet_params_t The type of the meshXSendPacketParams structure used
 * for sending packets.
 *
 * @param[in] p_plat_model  A pointer to the platform model (MESHX_MODEL).
 * @param[in] model_id      The unique identifier of the BLE mesh model.
 * @param[in] parent_element A pointer to the parent element (meshXElementIF).
 */
MESHX_GEN_LEVEL_CLIENT_MODEL_TEMPLATE_PROTO
meshXGenericLevelClientModel MESHX_GEN_LEVEL_CLIENT_MODEL_TEMPLATE_PARAMS
    ::meshXGenericLevelClientModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element)
    : meshXClientModel(p_plat_model, model_id, parent_element) {/* Used only for initialization of Parent Class */}

#endif /* CONFIG_ENABLE_GEN_LEVEL_CLIENT */
/*******************************************************************************************************************/
#if CONFIG_ENABLE_GEN_LEVEL_SERVER
/**
 * @brief Send a packet to the MeshX stack based on the given parameters
 *
 * This function takes a pointer to a meshx_gen_level_send_params_t structure as input
 * and sends the corresponding packet to the MeshX stack for processing.
 *
 * @param[in] params Pointer to the meshx_gen_level_send_params_t structure containing the parameters
 * @return MESHX_SUCCESS on success, or an error code on failure
 */
MESHX_GEN_LEVEL_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericLevelServerModel MESHX_GEN_LEVEL_SERVER_MODEL_TEMPLATE_PARAMS
    :: model_send(meshx_gen_level_send_params_t *params)
{
if (!params|| !params->model || !params->ctx)
    {
        return MESHX_INVALID_ARG;
    }
    params->ctx->opcode = MESHX_MODEL_OP_GEN_LEVEL_STATUS;
    meshx_gen_srv_state_change_t state_change = {
        .level_set = {
            .level = params->level
        }
    };
    meshx_gen_server_send_params_t send_params = {
        .p_model = params->model,
        .p_ctx = params->ctx,
        .state_change = state_change,
        .data_len = sizeof(meshx_state_change_gen_level_set_t)
    };
    return this->get_base_model()->plat_send_msg(&send_params);
}

/**
 * @brief Callback function for handling BLE mesh events for Generic Level Server Model
 *
 * @param[in] p_dev     Pointer to the device structure
 * @param[in] model_id  Model ID associated with the event
 * @param[in] params    Pointer to the event parameters
 * @return MESHX_SUCCESS if successful, error code otherwise
 */
MESHX_GEN_LEVEL_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericLevelServerModel MESHX_GEN_LEVEL_SERVER_MODEL_TEMPLATE_PARAMS
    :: model_from_ble_cb(dev_struct_t *p_dev, control_task_msg_evt_t model_id, meshx_ptr_t params)
{
    if(!params || !p_dev)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid parameters");
        return MESHX_INVALID_ARG;
    }
    if(model_id != MESHX_MODEL_ID_GEN_LEVEL_SRV)
    {
        /* Callback triggered not for this model */
        return MESHX_SUCCESS;
    }

    auto *param = static_cast<meshx_gen_srv_cb_param_t *>(params);
    meshx_level_srv_el_msg_t msg = {
        .model = &param->model,
        .level = param->state_change.level_set.level
    };

    if (this->get_parent_element()) {
        return this->get_parent_element()->on_model_cb(&msg);
    }

    MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Parent element is null");
    return MESHX_INVALID_STATE;
}

/**
 * @brief Create a platform model for the Generic Level Server
 *
 * This function creates a platform model for the Generic Level Server and initializes
 * the necessary parameters.
 *
 * @return MESHX_SUCCESS on success, or an error code on failure
 */
MESHX_GEN_LEVEL_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericLevelServerModel MESHX_GEN_LEVEL_SERVER_MODEL_TEMPLATE_PARAMS
    :: plat_model_create(void)
{
    meshx_ptr_t p_pub = this->get_pub_struct();
    meshx_ptr_t p_gen = this->get_gen_struct();
    meshx_err_t err = MESHX_SUCCESS;

    err = meshx_plat_level_gen_srv_create(this->get_plat_model(), &p_pub, &p_gen);
    if(err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to create Generic Level Server Model");
    }
    else
    {
        this->set_pub_struct(p_pub);
        this->set_gen_struct(p_gen);
    }
    return err;
}

/**
 * @brief Delete a platform model for the Generic Level Server
 *
 * This function deletes a platform model for the Generic Level Server and releases
 * the necessary resources.
 *
 * @return MESHX_SUCCESS on success, or an error code on failure
 */
MESHX_GEN_LEVEL_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericLevelServerModel MESHX_GEN_LEVEL_SERVER_MODEL_TEMPLATE_PARAMS
    :: plat_model_delete(void)
{
    meshx_ptr_t p_pub = this->get_pub_struct();
    meshx_ptr_t p_gen = this->get_gen_struct();

    meshx_err_t err = meshx_plat_gen_srv_delete(&p_pub, &p_gen);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to delete Generic Level Server Model");
    }
    else
    {
        this->set_pub_struct(nullptr);
        this->set_gen_struct(nullptr);
    }
    return err;
}

#endif /* CONFIG_ENABLE_GEN_LEVEL_SERVER */
