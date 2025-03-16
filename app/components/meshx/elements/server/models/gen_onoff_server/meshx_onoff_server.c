/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_onoff_server.c
 * @brief Header file for the On/Off Server model in the BLE Mesh Node application.
 *
 * This file contains the function defination for the
 * On/Off Server model used in the BLE Mesh Node application.
 *
 *
 */
#include "meshx_onoff_server.h"

/**
 * @brief Perform hardware change based on the BLE Mesh generic server callback parameter.
 *
 * This function is responsible for executing the necessary hardware changes
 * when a BLE Mesh generic server event occurs.
 *
 * @param param Pointer to the BLE Mesh generic server callback parameter structure.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_FAIL: Failure
 */
static meshx_err_t meshx_state_change_notify(meshx_gen_srv_cb_param_t *param)
{
    if (!param)
        return MESHX_INVALID_ARG;

    meshx_on_off_srv_t srv_onoff_param = {
        .model = param->model,
        .on_off_state = param->state_change.onoff_set.onoff};

    if (MESHX_ADDR_IS_UNICAST(param->ctx.dst_addr) || (MESHX_ADDR_BROADCAST(param->ctx.dst_addr)) || (MESHX_ADDR_IS_GROUP(param->ctx.dst_addr) && (MESHX_SUCCESS == meshx_is_group_subscribed(param->model.p_model, param->ctx.dst_addr))))
    {
        meshx_err_t err = control_task_msg_publish(
            CONTROL_TASK_MSG_CODE_EL_STATE_CH,
            CONTROL_TASK_MSG_EVT_EL_STATE_CH_SET_ON_OFF,
            &srv_onoff_param,
            sizeof(meshx_on_off_srv_t));
        return err ? err : MESHX_SUCCESS;
    }
    return MESHX_NOT_SUPPORTED;
}

#if !CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE
/**
 * @brief Handle Generic OnOff messages for the server model.
 *
 * This function processes the received Generic OnOff messages and performs
 * the necessary actions based on the message type and content.
 *
 * @param param Pointer to the callback parameter structure containing the
 *              details of the received message.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_INVALID_ARG: Invalid argument
 *    - MESHX_FAIL: Other failures
 */
static meshx_err_t meshx_handle_gen_onoff_msg(MESHX_GEN_SRV_CB_PARAM *param)
{
    MESHX_GEN_ONOFF_SRV *srv = (MESHX_GEN_ONOFF_SRV *)param->model->user_data;
    bool send_reply = (param->ctx.opcode != MESHX_MODEL_OP_GEN_ONOFF_SET_UNACK);
    switch (param->ctx.recv_op)
    {
    case MESHX_MODEL_OP_GEN_ONOFF_GET:
        break;
    case MESHX_MODEL_OP_GEN_ONOFF_SET:
    case MESHX_MODEL_OP_GEN_ONOFF_SET_UNACK:
        meshx_state_change_notify(param);
        break;
    default:
        break;
    }
    if (send_reply
        /* This is meant to notify the respective publish client */
        || param->ctx.dst_addr != param->model.pub_addr)
    {
        /* Here the message was received from unregistered source and mention the state to the respective client */
        MESHX_LOGD(MODULE_ID_MODEL_SERVER, "PUB: src|pub %x|%x", param->ctx.dst_addr, param->model.pub_addr);
        param->ctx.dst_addr = param->model.pub_addr;

        control_task_msg_publish(CONTROL_TASK_MSG_CODE_TO_BLE,
                                 CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF_SRV,
                                 param,
                                 sizeof(meshx_gen_srv_cb_param_t));
    }
    return MESHX_SUCCESS;
}

#else
/**
 * @brief Handle Generic OnOff messages for the server model.
 *
 * This function processes the received Generic OnOff messages and performs
 * the necessary actions based on the message type and content.
 *
 * @param param Pointer to the callback parameter structure containing the
 *              details of the received message.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_INVALID_ARG: Invalid argument
 *    - MESHX_FAIL: Other failures
 */
static meshx_err_t meshx_handle_gen_onoff_msg(const dev_struct_t *pdev, control_task_msg_evt_t model_id, meshx_gen_srv_cb_param_t *param)
{
    MESHX_UNUSED(pdev);
    MESHX_LOGD(MODULE_ID_MODEL_SERVER, "op|src|dst:%04" PRIx32 "|%04x|%04x",
               param->ctx.opcode, param->ctx.src_addr, param->ctx.dst_addr);
    if (model_id != MESHX_MODEL_ID_GEN_ONOFF_SRV)
        return MESHX_INVALID_ARG;

    bool send_reply = (param->ctx.opcode != MESHX_MODEL_OP_GEN_ONOFF_SET_UNACK);
    switch (param->ctx.opcode)
    {
    case MESHX_MODEL_OP_GEN_ONOFF_GET:
        break;
    case MESHX_MODEL_OP_GEN_ONOFF_SET:
    case MESHX_MODEL_OP_GEN_ONOFF_SET_UNACK:
        meshx_state_change_notify(param);
        break;
    default:
        break;
    }
    if (send_reply
        /* This is meant to notify the respective publish client */
        || param->ctx.dst_addr != param->model.pub_addr)
    {
        /* Here the message was received from unregistered source and mention the state to the respective client */
        MESHX_LOGD(MODULE_ID_MODEL_SERVER, "PUB: src|pub %x|%x", param->ctx.dst_addr, param->model.pub_addr);
        param->model.model_id = ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS;
        param->ctx.dst_addr = param->model.pub_addr;

        return control_task_msg_publish(CONTROL_TASK_MSG_CODE_TO_BLE,
                                        CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF_SRV,
                                        param,
                                        sizeof(meshx_gen_srv_cb_param_t));
    }
    return MESHX_SUCCESS;
}
#endif /* !CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE */

/**
 * @brief Initialize the On/Off server model.
 *
 * This function initializes the On/Off server model for the BLE mesh node.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_FAIL: Failure
 */
meshx_err_t meshx_on_off_server_init(void)
{
    meshx_err_t err = MESHX_SUCCESS;
#if CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE
    /* Protect only one registration*/
    static uint8_t init_cnt = 0;
    if (init_cnt)
        return MESHX_SUCCESS;
    init_cnt++;
#endif
#if CONFIG_ENABLE_SERVER_COMMON
    err = meshx_gen_srv_init();
    if (err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to initialize meshx server");
    }
#endif /* CONFIG_ENABLE_SERVER_COMMON */
    err = meshx_gen_srv_reg_cb(MESHX_MODEL_ID_GEN_ONOFF_SRV, (meshx_server_cb)&meshx_handle_gen_onoff_msg);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to initialize meshx_gen_srv_reg_cb (Err: %d)", err);
    }

    return err;
}

meshx_err_t meshx_on_off_server_create(meshx_onoff_server_model_t **p_model)
{
    if (!p_model)
    {
        return MESHX_INVALID_ARG;
    }

    *p_model = (meshx_onoff_server_model_t *)MESHX_CALOC(1, sizeof(meshx_onoff_server_model_t));
    if (!*p_model)
    {
        return MESHX_NO_MEM;
    }

    return meshx_plat_on_off_gen_srv_create(
        &((*p_model)->meshx_server_sig_model),
        &((*p_model)->meshx_server_pub),
        &((*p_model)->meshx_server_onoff_gen_srv));
}

meshx_err_t meshx_on_off_server_delete(meshx_onoff_server_model_t **p_model)
{
    if (p_model == NULL || *p_model == NULL)
    {
        return MESHX_INVALID_ARG;
    }

    meshx_plat_on_off_gen_srv_delete(
        &((*p_model)->meshx_server_sig_model),
        &((*p_model)->meshx_server_pub),
        &((*p_model)->meshx_server_onoff_gen_srv)
    );

    MESHX_FREE(*p_model);
    *p_model = NULL;

    return MESHX_SUCCESS; // Assuming MESHX_SUCCESS is a valid success code
}

meshx_err_t meshx_on_off_gen_srv_state_restore(meshx_onoff_server_model_t *p_model, uint8_t onoff_state)
{
    if(!p_model)
        return MESHX_INVALID_STATE;

    return meshx_plat_on_off_gen_srv_restore(p_model->meshx_server_sig_model, onoff_state);
}
