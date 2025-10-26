/**
 * @file meshx_model_onoff.cpp
 * @brief Implementation of Generic OnOff Model classes for MeshX.
 *        This file contains the implementation of the Generic OnOff Server and Client models
 *        for the MeshX BLE mesh framework.
 *
 * Key Features:
 *  - Implements Bluetooth SIG-defined Generic OnOff model
 *  - Inherits from meshXServerModel and meshXClientModel templates
 *  - Provides standard OnOff control operations
 *  - Integrates with MeshX transmission control
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#include <generic_model/meshx_model_onoff.hpp>
#include <meshx_element_class.hpp>

#if CONFIG_ENABLE_GEN_ONOFF_CLIENT
/**
 * @brief Handle Generic OnOff state change notifications from the MeshX stack.
 *
 * This function is responsible for handling Generic OnOff state change notifications
 * from the MeshX stack. It is called when the MeshX stack receives a state
 * change event from the Generic OnOff server model. The function publishes the
 * state change event to the control task framework, which in turn notifies the
 * application about the state change.
 *
 * @param[in] param  Pointer to the Generic OnOff client callback parameter structure.
 * @param[in] status Status of the state change event (success or timeout).
 *
 * @return
 *     - MESHX_SUCCESS: Successfully handled the state change notification.
 *     - MESHX_INVALID_ARG: One or more arguments are invalid.
 */
MESHX_GEN_ONOFF_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericOnOffClientModel MESHX_GEN_ONOFF_CLIENT_MODEL_TEMPLATE_PARAMS
    :: meshx_state_change_notify(const meshx_gen_cli_cb_param_t *param, uint8_t status) const
{
    if (!param){
        return MESHX_INVALID_ARG;
    }

    meshx_on_off_cli_el_msg_t cli_onoff_param = {
        .err_code = status,
        .model = param->model,
        .ctx = param->ctx,
        .on_off_state = param->status.onoff_status.present_onoff
    };
    /* Send the state change event to the respective Element */
    if (this->get_parent_element()) {
        return this->get_parent_element()->on_model_cb(&cli_onoff_param);
    } else {
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Parent element is null");
    }

    return MESHX_INVALID_STATE;
}
/**
 * @brief Creates a meshXGenericOnOffClientModel instance based on a BLE device
 *
 * This function is used to create a meshXGenericOnOffClientModel instance based on a BLE device.
 * It takes the device structure, model ID, and parameters as input and returns a pointer to the created instance.
 *
 * @param[in] p_dev     Pointer to the device structure
 * @param[in] model_id  Model ID associated with the device
 * @param[in] params    Pointer to the parameters associated with the device
 *
 * @return Pointer to the created meshXGenericOnOffClientModel instance
 */
MESHX_GEN_ONOFF_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericOnOffClientModel MESHX_GEN_ONOFF_CLIENT_MODEL_TEMPLATE_PARAMS
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
    if(model_id != MESHX_MODEL_ID_GEN_ONOFF_CLI)
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
 * This function takes a pointer to a meshx_gen_onoff_send_params_t structure as input
 * and sends the corresponding packet to the MeshX stack for processing.
 *
 * @param[in] params Pointer to the meshx_gen_onoff_send_params_t structure containing the parameters
 * @return MESHX_SUCCESS on success, or an error code on failure
 */
MESHX_GEN_ONOFF_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericOnOffClientModel MESHX_GEN_ONOFF_CLIENT_MODEL_TEMPLATE_PARAMS
    :: model_send(meshx_gen_onoff_send_params_t *params)
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

    if (params->ctx->opcode == MESHX_MODEL_OP_GEN_ONOFF_GET)
    {
        err = this->get_base_model()->plat_send_msg(&send_params);
    }

    else if (params->ctx->opcode == MESHX_MODEL_OP_GEN_ONOFF_SET ||
             params->ctx->opcode == MESHX_MODEL_OP_GEN_ONOFF_SET_UNACK)
    {
        set.onoff_set.tid = params->tid;
        set.onoff_set.onoff = params->state;
        set.onoff_set.op_en = false;

        err = this->get_base_model()->plat_send_msg(&send_params);
    }
    else
    {
        err = MESHX_INVALID_ARG; // Invalid opcode
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Invalid opcode for Generic OnOff Client: %04x", params->ctx->opcode);
    }
    return err;
}
/**
 * @brief A template class for creating Generic OnOff Client models.
 *
 * This class is derived from meshXClientModel and provides a convenient interface for
 * creating Generic OnOff Client models. It handles the Generic OnOff state change
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
MESHX_GEN_ONOFF_CLIENT_MODEL_TEMPLATE_PROTO
meshXGenericOnOffClientModel MESHX_GEN_ONOFF_CLIENT_MODEL_TEMPLATE_PARAMS
    ::meshXGenericOnOffClientModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element)
    : meshXClientModel(p_plat_model, model_id, parent_element) {/* Used only for initialization of Parent Class */}

#endif /* CONFIG_ENABLE_GEN_ONOFF_CLIENT */
/*******************************************************************************************************************/
#if CONFIG_ENABLE_GEN_ONOFF_SERVER
/**
 * @brief Send a packet to the MeshX stack based on the given parameters
 *
 * This function takes a pointer to a meshx_gen_onoff_send_params_t structure as input
 * and sends the corresponding packet to the MeshX stack for processing.
 *
 * @param[in] params Pointer to the meshx_gen_onoff_send_params_t structure containing the parameters
 * @return MESHX_SUCCESS on success, or an error code on failure
 */
MESHX_GEN_ONOFF_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericOnOffServerModel MESHX_GEN_ONOFF_SERVER_MODEL_TEMPLATE_PARAMS
    :: model_send(meshx_gen_onoff_send_params_t *params)
{
if (!params|| !params->model || !params->ctx)
    {
        return MESHX_INVALID_ARG;
    }
    params->ctx->opcode = MESHX_MODEL_OP_GEN_ONOFF_STATUS;
    meshx_gen_srv_state_change_t state_change = {
        .onoff_set = {
            .onoff = params->state
        }
    };
    meshx_gen_server_send_params_t send_params = {
        .p_model = params->model,
        .p_ctx = params->ctx,
        .state_change = state_change,
        .data_len = sizeof(meshx_state_change_gen_onoff_set_t)
    };
    return this->get_base_model()->plat_send_msg(&send_params);
}

/**
 * @brief This function is the callback for the Generic OnOff Server model.
 *
 * This function is triggered whenever a Generic OnOff message is received from the MeshX stack.
 * It handles the Generic OnOff state change notifications from the MeshX stack and publishes
 * the state change event to the element layer.
 *
 * @param[in] p_dev       A pointer to the device structure.
 * @param[in] model_id    The unique identifier of the BLE mesh model.
 * @param[in] params      A pointer to the callback parameter structure containing the details of the
 *                        received message.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_INVALID_ARG: Invalid argument
 *    - MESHX_FAIL: Other failures
 */
meshx_err_t meshXGenericOnOffServerModel MESHX_GEN_ONOFF_SERVER_MODEL_TEMPLATE_PARAMS
    :: model_from_ble_cb(
        dev_struct_t *p_dev,
        control_task_msg_evt_t model_id,
        meshx_ptr_t params)
{
    if(!params || !p_dev)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid parameters");
        return MESHX_INVALID_ARG;
    }
    if(model_id != MESHX_MODEL_ID_GEN_ONOFF_SRV)
    {
        /* Callback triggered not for this model */
        return MESHX_SUCCESS;
    }

    auto *param = static_cast<meshx_gen_srv_cb_param_t *>(params);

    MESHX_LOGD(MODULE_ID_MODEL_SERVER, "op|src|dst:%04" PRIx32 "|%04x|%04x",
               param->ctx.opcode, param->ctx.src_addr, param->ctx.dst_addr);

    meshx_on_off_srv_el_msg_t srv_onoff_param = {
        .model = param->model,
        .on_off_state = param->state_change.onoff_set.onoff
    };
    bool send_reply = (param->ctx.opcode != MESHX_MODEL_OP_GEN_ONOFF_SET_UNACK);
    switch (param->ctx.opcode)
    {
        case MESHX_MODEL_OP_GEN_ONOFF_GET:
            break;
        case MESHX_MODEL_OP_GEN_ONOFF_SET:
        case MESHX_MODEL_OP_GEN_ONOFF_SET_UNACK:
        {
            if (MESHX_ADDR_IS_UNICAST(param->ctx.dst_addr)
            || (MESHX_ADDR_BROADCAST(param->ctx.dst_addr))
            || (MESHX_ADDR_IS_GROUP(param->ctx.dst_addr)
            && (MESHX_SUCCESS == meshx_is_group_subscribed(&param->model, param->ctx.dst_addr))))
            {
                if (this->get_parent_element())
                {
                    return this->get_parent_element()->on_model_cb(&srv_onoff_param);
                }
                else
                {
                    MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Parent element is null");
                }
            }
            break;
        }
        default:
            break;
    }
    if (send_reply
        /* This is meant to notify the respective publish client */
        || param->ctx.src_addr != param->model.pub_addr)
    {
        /* Here the message was received from unregistered source and mention the state to the respective client */
        MESHX_LOGD(MODULE_ID_MODEL_SERVER, "PUB: src|pub %x|%x", param->ctx.src_addr, param->model.pub_addr);
        param->ctx.dst_addr = param->model.pub_addr;

        // Create a parameter structure for sending the response
        meshx_gen_onoff_send_params_t send_params = {
            .model = &param->model,
            .ctx = &param->ctx,
            .state = param->state_change.onoff_set.onoff,
            .tid = 0  // TID not used in server response
        };

        return this->model_send(&send_params);
    }
    return MESHX_SUCCESS;
}
#endif /* CONFIG_ENABLE_GEN_ONOFF_SERVER */
