#include <meshx_model_onoff.hpp>

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
meshx_err_t meshXGenericOnOffClientModel MESHX_GEN_ONOFF_CLIENT_MODEL_TEMPLATE_PARAMS :: meshx_state_change_notify(
    const meshx_gen_cli_cb_param_t *param,
    uint8_t status)
{
    if (!param)
        return MESHX_INVALID_ARG;

    meshx_on_off_cli_el_msg_t cli_onoff_param = {
        .err_code = status,
        .model = param->model,
        .ctx = param->ctx,
        .on_off_state = param->status.onoff_status.present_onoff
    };

    /**
     * Publish the state change event to element layer
     * @todo to implement Element layer interface instead of control task
     */
    return control_task_msg_publish(
            CONTROL_TASK_MSG_CODE_EL_STATE_CH,
            CONTROL_TASK_MSG_EVT_EL_STATE_CH_SET_ON_OFF,
            &cli_onoff_param,
            sizeof(meshx_on_off_cli_el_msg_t));
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
meshx_err_t meshXGenericOnOffClientModel MESHX_GEN_ONOFF_CLIENT_MODEL_TEMPLATE_PARAMS :: model_from_ble_cb(
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
 */
MESHX_GEN_ONOFF_CLIENT_MODEL_TEMPLATE_PROTO
meshXGenericOnOffClientModel MESHX_GEN_ONOFF_CLIENT_MODEL_TEMPLATE_PARAMS
    ::meshXGenericOnOffClientModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElement *parent_element)
    : meshXClientModel(p_plat_model, model_id, parent_element) {}

#endif /* CONFIG_ENABLE_GEN_ONOFF_CLIENT */
/*******************************************************************************************************************/
#if CONFIG_ENABLE_GEN_ONOFF_SERVER

#endif /* CONFIG_ENABLE_GEN_ONOFF_SERVER */
