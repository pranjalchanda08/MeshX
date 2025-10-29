/**
 * @file meshx_model_power_onoff.cpp
 * @brief Implementation of Generic Power OnOff Model classes for MeshX.
 *        This file contains the implementation of the Generic Power OnOff Server and Client models
 *        for the MeshX BLE mesh framework.
 *
 * Key Features:
 *  - Implements Bluetooth SIG-defined Generic Power OnOff model
 *  - Inherits from meshXServerModel and meshXClientModel templates
 *  - Provides standard Power OnOff control operations
 *  - Integrates with MeshX transmission control
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#include <generic_model/meshx_model_power_onoff.hpp>
#include <meshx_element_class.hpp>

#if CONFIG_ENABLE_GEN_POWER_ONOFF_CLIENT
/**
 * @brief Handle Generic Power OnOff state change notifications from the MeshX stack.
 *
 * This function is responsible for handling Generic Power OnOff state change notifications
 * from the MeshX stack. It is called when the MeshX stack receives a state
 * change event from the Generic Power OnOff server model. The function publishes the
 * state change event to the control task framework, which in turn notifies the
 * application about the state change.
 *
 * @param[in] param  Pointer to the Generic Power OnOff client callback parameter structure.
 * @param[in] status Status of the state change event (success or timeout).
 *
 * @return
 *     - MESHX_SUCCESS: Successfully handled the state change notification.
 *     - MESHX_INVALID_ARG: One or more arguments are invalid.
 */
MESHX_GEN_POWER_ONOFF_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPowerOnOffClientModel MESHX_GEN_POWER_ONOFF_CLIENT_MODEL_TEMPLATE_PARAMS
    :: meshx_state_change_notify(const meshx_gen_cli_cb_param_t *param, uint8_t status) const
{
    if (!param){
        return MESHX_INVALID_ARG;
    }

    meshx_power_onoff_cli_el_msg_t power_onoff_param = {
        .err_code = status,
        .model = param->model,
        .ctx = param->ctx,
        .on_power_up = param->status.onpowerup_status.onpowerup
    };
    /* Send the state change event to the respective Element */
    if (this->get_parent_element()) {
        return this->get_parent_element()->on_model_cb(&power_onoff_param);
    } else {
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Parent element is null");
    }

    return MESHX_INVALID_STATE;
}
/**
 * @brief Creates a meshXGenericPowerOnOffClientModel instance based on a BLE device
 *
 * This function is used to create a meshXGenericPowerOnOffClientModel instance based on a BLE device.
 * It takes the device structure, model ID, and parameters as input and returns a pointer to the created instance.
 *
 * @param[in] p_dev     Pointer to the device structure
 * @param[in] model_id  Model ID associated with the device
 * @param[in] params    Pointer to the parameters associated with the device
 *
 * @return Pointer to the created meshXGenericPowerOnOffClientModel instance
 */
MESHX_GEN_POWER_ONOFF_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPowerOnOffClientModel MESHX_GEN_POWER_ONOFF_CLIENT_MODEL_TEMPLATE_PARAMS
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
    if(model_id != MESHX_MODEL_ID_GEN_POWER_ONOFF_CLI)
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
 * This function takes a pointer to a meshx_gen_power_onoff_send_params_t structure as input
 * and sends the corresponding packet to the MeshX stack for processing.
 *
 * @param[in] params Pointer to the meshx_gen_power_onoff_send_params_t structure containing the parameters
 * @return MESHX_SUCCESS on success, or an error code on failure
 */
MESHX_GEN_POWER_ONOFF_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPowerOnOffClientModel MESHX_GEN_POWER_ONOFF_CLIENT_MODEL_TEMPLATE_PARAMS
    :: model_send(meshx_gen_power_onoff_send_params_t *params)
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

    if (params->ctx->opcode == MESHX_MODEL_OP_GEN_ONPOWERUP_GET)
    {
        err = this->get_base_model()->plat_send_msg(&send_params);
    }

    else if (params->ctx->opcode == MESHX_MODEL_OP_GEN_ONPOWERUP_SET ||
             params->ctx->opcode == MESHX_MODEL_OP_GEN_ONPOWERUP_SET_UNACK)
    {
        set.power_set.onpowerup = params->on_power_up;
        // Note: Power OnOff SET operations don't use TID

        err = this->get_base_model()->plat_send_msg(&send_params);
    }
    else
    {
        err = MESHX_INVALID_ARG; // Invalid opcode
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Invalid opcode for Generic Power OnOff Client: %04x", params->ctx->opcode);
    }
    return err;
}
/**
 * @brief A template class for creating Generic Power OnOff Client models.
 *
 * This class is derived from meshXClientModel and provides a convenient interface for
 * creating Generic Power OnOff Client models. It handles the Generic Power OnOff state change
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
MESHX_GEN_POWER_ONOFF_CLIENT_MODEL_TEMPLATE_PROTO
meshXGenericPowerOnOffClientModel MESHX_GEN_POWER_ONOFF_CLIENT_MODEL_TEMPLATE_PARAMS
    ::meshXGenericPowerOnOffClientModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element)
    : meshXClientModel(p_plat_model, model_id, parent_element) {/* Used only for initialization of Parent Class */}

#endif /* CONFIG_ENABLE_GEN_POWER_ONOFF_CLIENT */
/*******************************************************************************************************************/
#if CONFIG_ENABLE_GEN_POWER_ONOFF_SERVER
/**
 * @brief Send a packet to the MeshX stack based on the given parameters
 *
 * This function takes a pointer to a meshx_gen_power_onoff_send_params_t structure as input
 * and sends the corresponding packet to the MeshX stack for processing.
 *
 * @param[in] params Pointer to the meshx_gen_power_onoff_send_params_t structure containing the parameters
 * @return MESHX_SUCCESS on success, or an error code on failure
 */
MESHX_GEN_POWER_ONOFF_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPowerOnOffServerModel MESHX_GEN_POWER_ONOFF_SERVER_MODEL_TEMPLATE_PARAMS
    :: model_send(meshx_gen_power_onoff_send_params_t *params)
{
if (!params|| !params->model || !params->ctx)
    {
        return MESHX_INVALID_ARG;
    }
    params->ctx->opcode = MESHX_MODEL_OP_GEN_ONPOWERUP_STATUS;
    meshx_gen_srv_state_change_t state_change = {
        .onpowerup_set = {
            .onpowerup = params->on_power_up
        }
    };
    meshx_gen_server_send_params_t send_params = {
        .p_model = params->model,
        .p_ctx = params->ctx,
        .state_change = state_change,
        .data_len = sizeof(meshx_state_change_gen_onpowerup_set_t)
    };
    return this->get_base_model()->plat_send_msg(&send_params);
}

MESHX_GEN_POWER_ONOFF_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPowerOnOffServerModel MESHX_GEN_POWER_ONOFF_SERVER_MODEL_TEMPLATE_PARAMS
    :: model_from_ble_cb(dev_struct_t *p_dev, control_task_msg_evt_t model_id, meshx_ptr_t params)
{
    if(!params || !p_dev)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid parameters");
        return MESHX_INVALID_ARG;
    }
    if(model_id != MESHX_MODEL_ID_GEN_POWER_ONOFF_SRV)
    {
        /* Callback triggered not for this model */
        return MESHX_SUCCESS;
    }

    auto *param = static_cast<meshx_gen_srv_cb_param_t *>(params);
    meshx_power_onoff_srv_el_msg_t msg = {
        .model = &param->model,
        .on_power_up = param->state_change.onpowerup_set.onpowerup
    };

    if (this->get_parent_element()) {
        return this->get_parent_element()->on_model_cb(&msg);
    }

    MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Parent element is null");
    return MESHX_INVALID_STATE;
}

MESHX_GEN_POWER_ONOFF_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPowerOnOffServerModel MESHX_GEN_POWER_ONOFF_SERVER_MODEL_TEMPLATE_PARAMS
    :: plat_model_create(void)
{
    meshx_ptr_t p_pub = this->get_pub_struct();
    meshx_ptr_t p_gen = this->get_gen_struct();
    meshx_err_t err = MESHX_SUCCESS;

    err = meshx_plat_power_onoff_gen_srv_create(this->get_plat_model(), &p_pub, &p_gen);
    if(err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to create Generic Power OnOff Server Model");
    }
    else
    {
        this->set_pub_struct(p_pub);
        this->set_gen_struct(p_gen);
    }
    return err;
}

MESHX_GEN_POWER_ONOFF_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPowerOnOffServerModel MESHX_GEN_POWER_ONOFF_SERVER_MODEL_TEMPLATE_PARAMS
    :: plat_model_delete(void)
{
    meshx_ptr_t p_pub = this->get_pub_struct();
    meshx_ptr_t p_gen = this->get_gen_struct();

    meshx_err_t err = meshx_plat_gen_srv_delete(&p_pub, &p_gen);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to delete Generic Power OnOff Server Model");
    }
    else
    {
        this->set_pub_struct(nullptr);
        this->set_gen_struct(nullptr);
    }
    return err;
}
#endif /* CONFIG_ENABLE_GEN_POWER_ONOFF_SERVER */

#if CONFIG_ENABLE_GEN_POWER_ONOFF_SETUP_SERVER
/**
 * @brief Send a packet to the MeshX stack based on the given parameters
 *
 * This function takes a pointer to a meshx_gen_power_onoff_send_params_t structure as input
 * and sends the corresponding packet to the MeshX stack for processing.
 *
 * @param[in] params Pointer to the meshx_gen_power_onoff_send_params_t structure containing the parameters
 * @return MESHX_SUCCESS on success, or an error code on failure
 */
MESHX_GEN_POWER_ONOFF_SETUP_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPowerOnOffSetupServerModel MESHX_GEN_POWER_ONOFF_SETUP_SERVER_MODEL_TEMPLATE_PARAMS
    :: model_send(meshx_gen_power_onoff_send_params_t *params)
{
if (!params|| !params->model || !params->ctx)
    {
        return MESHX_INVALID_ARG;
    }
    params->ctx->opcode = MESHX_MODEL_OP_GEN_ONPOWERUP_STATUS;
    meshx_gen_srv_state_change_t state_change = {
        .onpowerup_set = {
            .onpowerup = params->on_power_up
        }
    };
    meshx_gen_server_send_params_t send_params = {
        .p_model = params->model,
        .p_ctx = params->ctx,
        .state_change = state_change,
        .data_len = sizeof(meshx_state_change_gen_onpowerup_set_t)
    };
    return this->get_base_model()->plat_send_msg(&send_params);
}

MESHX_GEN_POWER_ONOFF_SETUP_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXGenericPowerOnOffSetupServerModel MESHX_GEN_POWER_ONOFF_SETUP_SERVER_MODEL_TEMPLATE_PARAMS
    :: model_from_ble_cb(dev_struct_t *p_dev, control_task_msg_evt_t model_id, meshx_ptr_t params)
{
    if(!params || !p_dev)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid parameters");
        return MESHX_INVALID_ARG;
    }
    if(model_id != MESHX_MODEL_ID_GEN_POWER_ONOFF_SETUP_SRV)
    {
        /* Callback triggered not for this model */
        return MESHX_SUCCESS;
    }

    auto *param = static_cast<meshx_gen_srv_cb_param_t *>(params);
    // Setup server forwards state changes to its base server model
    meshx_power_onoff_srv_el_msg_t msg = {
        .model = &param->model,
        .on_power_up = param->state_change.onpowerup_set.onpowerup
    };

    if (this->get_parent_element()) {
        return this->get_parent_element()->on_model_cb(&msg);
    }

    MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Parent element is null");
    return MESHX_INVALID_STATE;
}
#endif /* CONFIG_ENABLE_GEN_POWER_ONOFF_SETUP_SERVER */
