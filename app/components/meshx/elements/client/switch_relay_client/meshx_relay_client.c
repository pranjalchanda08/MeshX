/**
 * Copyright (c) 2024 - 2025 MeshX
 *
 * @file meshx_relay_client.c
 * @brief Implementation of the relay client model for BLE Mesh.
 *
 * This file contains the implementation of the relay client model for BLE Mesh,
 * including initialization, configuration, and event handling.
 *
 * @author Pranjal Chanda
 *
 */

#include "app_common.h"
#include "meshx_control_task.h"
#include "meshx_nvs.h"
#include "meshx_api.h"

#if CONFIG_RELAY_CLIENT_COUNT
#include "meshx_relay_client_element.h"

#if CONFIG_ENABLE_CONFIG_SERVER
#include "meshx_config_server.h"
#define CONFIG_SERVER_CB_MASK       \
      CONTROL_TASK_MSG_EVT_PUB_ADD  \
    | CONTROL_TASK_MSG_EVT_SUB_ADD  \
    | CONTROL_TASK_MSG_EVT_APP_KEY_BIND
#define CONTROL_TASK_MSG_CODE_EVT_MASK CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF
#endif /* CONFIG_ENABLE_CONFIG_SERVER */

#define MOD_SRC                             MODULE_ID_ELEMENT_SWITCH_RELAY_CLIENT
#define CONFIG_RELAY_MESHX_ONOFF_SET_ACK    true
#define RELAY_CLI_EL_STATE_CH_EVT_MASK      CONTROL_TASK_MSG_EVT_EL_STATE_CH_SET_ON_OFF

#define GET_RELATIVE_EL_IDX(_element_id)    _element_id - relay_element_init_ctrl.element_id_start
#define IS_EL_IN_RANGE(_element_id)         (_element_id >= relay_element_init_ctrl.element_id_start && _element_id < relay_element_init_ctrl.element_id_end)
#define RELAY_CLI_EL(_rel_el_id)            relay_element_init_ctrl.el_list[_rel_el_id]

static relay_client_element_ctrl_t relay_element_init_ctrl;

/**
 * @brief Initializes the mesh element structure.
 *
 * This function initializes the mesh element structure with the specified maximum number of elements.
 *
 * @param n_max The maximum number of elements to initialize.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_INVALID_ARG: Invalid argument
 */
static meshx_err_t meshx_element_struct_init(uint16_t n_max)
{
    if (!n_max)
        return MESHX_INVALID_ARG;

    if (relay_element_init_ctrl.el_list)
    {
        MESHX_LOGW(MOD_SRC, "Relay element list already initialized");
        return MESHX_INVALID_STATE;
    }

    meshx_err_t err = MESHX_SUCCESS;

    relay_element_init_ctrl.element_cnt      = n_max;
    relay_element_init_ctrl.element_id_end   = 0;
    relay_element_init_ctrl.element_id_start = 0;

    relay_element_init_ctrl.el_list =
        (relay_client_elements_t *)MESHX_CALOC(relay_element_init_ctrl.element_cnt, sizeof(relay_client_elements_t));

    if (!relay_element_init_ctrl.el_list)
        return MESHX_NO_MEM;

    for (size_t i = 0; i < relay_element_init_ctrl.element_cnt; i++)
    {
        RELAY_CLI_EL(i).cli_ctx =
            (meshx_relay_client_model_ctx_t *)MESHX_CALOC(1, sizeof(meshx_relay_client_model_ctx_t));

        if (!RELAY_CLI_EL(i).cli_ctx)
            return MESHX_NO_MEM;

        err = meshx_on_off_client_create(&RELAY_CLI_EL(i).onoff_cli_model,
                                         &RELAY_CLI_EL(i).relay_cli_sig_model_list[RELAY_CLI_SIG_ONOFF_MODEL_ID]);
        if (err)
        {
            MESHX_LOGE(MOD_SRC, "Meshx On Off Client create failed (Err : 0x%x)", err);
            return err;
        }
        RELAY_CLI_EL(i).onoff_cli_model->meshx_onoff_client_sig_model
            = &RELAY_CLI_EL(i).relay_cli_sig_model_list[RELAY_CLI_SIG_ONOFF_MODEL_ID];
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Deinitializes the mesh element structure.
 *
 * This function deinitializes the mesh element structure by freeing the allocated memory.
 *
 * @param n_max The maximum number of elements to deinitialize.
 */
static meshx_err_t meshx_element_struct_deinit(void)
{
    if (!relay_element_init_ctrl.element_cnt || !relay_element_init_ctrl.el_list)
    {
        MESHX_LOGE(MOD_SRC, "Relay element list not initialized");
        return MESHX_INVALID_STATE;
    }

    meshx_err_t err;

    for (size_t i = 0; i < relay_element_init_ctrl.element_cnt; i++)
    {
        if (RELAY_CLI_EL(i).cli_ctx)
        {
            MESHX_FREE(RELAY_CLI_EL(i).cli_ctx);
            RELAY_CLI_EL(i).cli_ctx = NULL;
        }
        err = meshx_on_off_client_delete(&RELAY_CLI_EL(i).onoff_cli_model);
        if (err)
            MESHX_LOGE(MOD_SRC, "Meshx On Off Server delete failed (Err : 0x%x)", err);
    }

    if (relay_element_init_ctrl.el_list)
    {
        MESHX_FREE(relay_element_init_ctrl.el_list);
        relay_element_init_ctrl.el_list = NULL;
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Create Dynamic Relay Model Elements
 *
 * @param[in] pdev  Pointer to device structure
 * @param[in] n_max Maximum number of relay models
 *
 * @return meshx_err_t
 */
static meshx_err_t meshx_dev_create_relay_model_space(dev_struct_t const *pdev, uint16_t n_max)
{
    if (!pdev)
        return MESHX_INVALID_STATE;

    /* Assign Spaces for Model List, Publish List and onoff gen list */
    meshx_err_t err = meshx_element_struct_init(n_max);
    if (err)
    {
        MESHX_LOGE(MOD_SRC, "Failed to initialize relay element structures: (%d)", err);
        meshx_element_struct_deinit();
        return err;
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Add relay client models to the element list.
 *
 * Registers the relay client models to the BLE Mesh element list.
 *
 * @param[in]       pdev        Pointer to the device structure.
 * @param[in,out]   start_idx   Pointer to the start index of elements.
 * @param[in]       n_max       Maximum number of elements to add.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
static meshx_err_t meshx_add_relay_cli_model_to_element_list(
    dev_struct_t *pdev, uint16_t *start_idx, uint16_t n_max)
{
    if (!pdev || !start_idx || !n_max)
        return MESHX_INVALID_STATE;

    if ((n_max + *start_idx) > CONFIG_MAX_ELEMENT_COUNT)
    {
        MESHX_LOGE(MOD_SRC, "No of elements limit reached");
        return MESHX_NO_MEM;
    }

    meshx_err_t err = MESHX_SUCCESS;
    relay_element_init_ctrl.element_id_start = *start_idx;

    for (uint16_t i = *start_idx; i < (n_max + *start_idx); i++)
    {
        if (i == 0)
            continue;
        err = meshx_plat_add_element_to_composition(
            i,
            pdev->elements,
            RELAY_CLI_EL(i - *start_idx).relay_cli_sig_model_list,
            NULL,
            RELAY_CLI_MODEL_SIG_CNT,
            RELAY_CLI_MODEL_VEN_CNT);
        if (err)
        {
            MESHX_LOGE(MOD_SRC, "Failed to add element to composition: (%d)", err);
            return err;
        }
        err = meshx_nvs_element_ctx_get(
            i,
            &(RELAY_CLI_EL(i - *start_idx).cli_ctx),
            sizeof(RELAY_CLI_EL(i - *start_idx).cli_ctx));
        if (err != MESHX_SUCCESS)
        {
            MESHX_LOGW(MOD_SRC, "Failed to add element to composition: (%d)", err);
        }
    }
    /* Increment the index for further registrations */
    relay_element_init_ctrl.element_id_end = *start_idx += n_max;
    return MESHX_SUCCESS;
}

#if CONFIG_ENABLE_CONFIG_SERVER

/**
 * @brief Callback function for configuration client events.
 *
 * This function handles events from the configuration client, such as model publication
 * and application binding events.
 *
 * @param[in] param Pointer to the callback parameter structure.
 * @param[in] evt Configuration event type.
 * @return meshx_err_t
 */
static meshx_err_t relay_client_config_cli_cb(
    const dev_struct_t *pdev,
    control_task_msg_evt_t evt,
    const meshx_config_srv_cb_param_t *params)
{
    MESHX_UNUSED(pdev);
    meshx_relay_client_model_ctx_t *el_ctx = NULL;
    size_t rel_el_id = 0;
    uint16_t element_id = 0;
    bool nvs_save = false;

    MESHX_LOGD(MODULE_ID_MODEL_CLIENT, "EVT: %p", (void *)evt);
    switch (evt)
    {
    case CONTROL_TASK_MSG_EVT_APP_KEY_BIND:
        element_id = params->model.el_id;
        if (!IS_EL_IN_RANGE(element_id))
            break;
        rel_el_id = GET_RELATIVE_EL_IDX(element_id);
        el_ctx = RELAY_CLI_EL(rel_el_id).cli_ctx;
        el_ctx->app_id = params->state_change.mod_app_bind.app_idx;
        nvs_save = true;
        break;
    case CONTROL_TASK_MSG_EVT_PUB_ADD:
    case CONTROL_TASK_MSG_EVT_PUB_DEL:
        element_id = params->model.el_id;
        if (!IS_EL_IN_RANGE(element_id))
            break;
        rel_el_id = GET_RELATIVE_EL_IDX(element_id);
        el_ctx = RELAY_CLI_EL(rel_el_id).cli_ctx;
        el_ctx->pub_addr = evt == CONTROL_TASK_MSG_EVT_PUB_ADD ? params->state_change.mod_pub_set.pub_addr
                                                           : MESHX_ADDR_UNASSIGNED;
        el_ctx->app_id = params->state_change.mod_pub_set.app_idx;
        nvs_save = true;
        ESP_LOGI(TAG, "PUB_ADD: %d, %d, 0x%x, 0x%x", element_id, rel_el_id, el_ctx->pub_addr, el_ctx->app_id);
        break;
    default:
        break;
    }
    if (nvs_save)
    {
        meshx_err_t err = meshx_nvs_element_ctx_set(element_id, el_ctx, sizeof(meshx_relay_client_model_ctx_t));
        if (err != MESHX_SUCCESS)
        {
            MESHX_LOGE(MOD_SRC, "Failed to set relay element context: (%d)", err);
        }
    }
    return MESHX_SUCCESS;
}
#endif /* #if CONFIG_ENABLE_CONFIG_SERVER */

/**
 * @brief Relay Client Freshboot Control Task Message Handler
 *
 * This function handles the CW-WW client control task messages.
 *
 * @param[in] pdev   Pointer to the device structure.
 * @param[in] evt    Event type of the control task message.
 * @param[in] params Pointer to the parameters of the control task message.
 * @return meshx_err_t
 */
static meshx_err_t relay_cli_freshboot_msg_handle(const dev_struct_t *pdev, control_task_msg_evt_t evt, const void *params)
{
    if(!pdev)
        return MESHX_INVALID_ARG;

    MESHX_UNUSED(params);
    MESHX_UNUSED(evt);
    size_t rel_el_id = 0;
    for (size_t i = relay_element_init_ctrl.element_id_start; i < relay_element_init_ctrl.element_id_end; i++)
    {
        rel_el_id = GET_RELATIVE_EL_IDX(i);
        if(false == (RELAY_CLI_EL(rel_el_id).element_model_init
                    & MESHX_BIT(RELAY_CLI_SIG_ONOFF_MODEL_ID)))
        {
            MESHX_LOGE(MOD_SRC, "Sending GET for model: 0");
            return meshx_relay_el_get_state((uint16_t) i);
        }
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Sends a relay message over BLE mesh.
 *
 * This function sends a relay message to a specified element in the BLE mesh network.
 *
 * @param[in] pdev          Pointer to the device structure.
 * @param[in] element_id    The ID of the element to which the message is sent.
 * @param[in] set_get       Indicates whether the message is a set (0) or get (1) operation.
 * @param[in] ack           Indicates whether an acknowledgment is required (1) or not (0).
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_INVALID_ARG: Invalid argument
 *     - MESHX_FAIL: Sending message failed
 */
static meshx_err_t meshx_relay_cli_send_onoff_msg(
    const dev_struct_t *pdev,
    uint16_t element_id,
    uint8_t set_get,
    uint8_t ack)
{
    if (!pdev || !IS_EL_IN_RANGE(element_id))
        return MESHX_INVALID_ARG;

    meshx_err_t err = MESHX_SUCCESS;
    size_t rel_el_id = GET_RELATIVE_EL_IDX(element_id);
    meshx_ptr_t model = RELAY_CLI_EL(rel_el_id).\
        onoff_cli_model[RELAY_CLI_SIG_ONOFF_MODEL_ID].meshx_onoff_client_sig_model;

    meshx_relay_client_model_ctx_t *el_ctx = RELAY_CLI_EL(rel_el_id).cli_ctx;
    uint16_t opcode = MESHX_MODEL_OP_GEN_ONOFF_GET;

    if (MESHX_GEN_ON_OFF_CLI_MSG_SET == set_get)
    {
        opcode = ack ? MESHX_MODEL_OP_GEN_ONOFF_SET : MESHX_MODEL_OP_GEN_ONOFF_SET_UNACK;
    }

    MESHX_LOGE(MOD_SRC, "OPCODE: %p", (void *)(uint32_t)opcode);

    /* Send message to the relay client */
    err = meshx_onoff_client_send_msg(
            model,
            opcode,
            el_ctx->pub_addr,
            pdev->meshx_store.net_key_id,
            el_ctx->app_id,
            el_ctx->state.on_off,
            el_ctx->tid
    );
    if (err)
    {
        MESHX_LOGE(MOD_SRC, "Relay Client Send Message failed: (%d)", err);
    }
    else
    {
        el_ctx->tid++;
        if(opcode == MESHX_MODEL_OP_GEN_ONOFF_SET_UNACK)
        {
            el_ctx->state.prev_on_off = el_ctx->state.on_off;
            el_ctx->state.on_off = !el_ctx->state.on_off;
        }
    }
    return err;
}

/**
 * @brief Handles the relay client element message.
 * @note CONTROL_TASK_MSG_CODE_EL_STATE_CH
 *
 * This function processes the relay client element message and updates the state
 * of the relay client model accordingly.
 *
 * @param[in] pdev Pointer to the device structure.
 * @param[in] evt Event type of the control task message.
 * @param[in] param Pointer to the parameters of the control task message.
 * @return meshx_err_t
 */
static meshx_err_t meshx_handle_rel_el_msg(
    const dev_struct_t *pdev,
    control_task_msg_evt_t evt,
    const meshx_on_off_cli_el_msg_t *param
)
{
    MESHX_UNUSED(pdev);
    MESHX_UNUSED(evt);
    uint16_t element_id = param->model.el_id;
    if (!IS_EL_IN_RANGE(element_id))
    {
        return MESHX_SUCCESS;
    }

    size_t rel_el_id = GET_RELATIVE_EL_IDX(element_id);
    meshx_relay_client_model_ctx_t *el_ctx = RELAY_CLI_EL(rel_el_id).cli_ctx;
    meshx_el_relay_client_evt_t app_notify;
    meshx_err_t err;
    if(param->err_code == MESHX_SUCCESS)
    {

        RELAY_CLI_EL(rel_el_id).element_model_init |= MESHX_BIT(RELAY_CLI_SIG_ONOFF_MODEL_ID);
        if (el_ctx->state.prev_on_off != param->on_off_state)
        {
            el_ctx->state.prev_on_off = param->on_off_state;
            app_notify.err_code = 0;
            app_notify.on_off = el_ctx->state.prev_on_off;

            err = meshx_send_msg_to_app(element_id,
                                        MESHX_ELEMENT_TYPE_RELAY_CLIENT,
                                        MESHX_ELEMENT_FUNC_ID_RELAY_SERVER_ONN_OFF,
                                        sizeof(meshx_el_relay_client_evt_t),
                                        &app_notify);
            if (err != MESHX_SUCCESS)
            {
                MESHX_LOGE(MOD_SRC, "Failed to send relay state change message: (%d)", err);
            }

            el_ctx->state.on_off = !param->on_off_state;
            el_ctx->tid++;
            MESHX_LOGE(MOD_SRC, "SET|PUBLISH: %d", param->on_off_state);
            MESHX_LOGE(MOD_SRC, "Next state: %d", el_ctx->state.on_off);
        }
    }
    else
    {
        MESHX_LOGE(MOD_SRC, "Relay Client Element Message: Error (%d)", param->err_code);
        /* Retry sending the failed packet. Do not notify App */
        /* Please note that the failed packets gets sent endlessly. Hence, a loop condition */
        el_ctx->tid++;
        err = meshx_relay_cli_send_onoff_msg(pdev,
                                             element_id,
                                             MESHX_GEN_ON_OFF_CLI_MSG_SET,
                                             CONFIG_RELAY_MESHX_ONOFF_SET_ACK);
        if (err)
        {
            MESHX_LOGE(MOD_SRC, "Relay Client Element Message: Retry failed (%d)", err);
        }
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Relay Client Element Application Request Handler
 *
 * This function handles the relay client application requests for setting or getting
 * the On/Off state of the relay element.
 *
 * @param[in] pdev   Pointer to the device structure.
 * @param[in] evt    Event type of the control task message.
 * @param[in] params Pointer to the parameters of the control task message.
 * @return meshx_err_t
 */
static meshx_err_t meshx_relay_cli_el_app_req_handler(
    const dev_struct_t *pdev,
    control_task_msg_evt_t evt,
    const void *params
)
{
    const meshx_gen_on_off_cli_msg_t *msg = (const meshx_gen_on_off_cli_msg_t *)params;
    meshx_err_t err = MESHX_SUCCESS;

    if (!pdev || !IS_EL_IN_RANGE(msg->element_id))
        return MESHX_INVALID_ARG;

    if (evt == CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF)
    {
        err = meshx_relay_cli_send_onoff_msg(pdev,
                                      msg->element_id,
                                      msg->set_get,
                                      msg->ack);
        if (err)
        {
            MESHX_LOGE(
                MOD_SRC,
                "Relay Client Control Task: Set OnOff failed (%p)", (void *)err);
        }
    }
    return err;
}

#if CONFIG_ENABLE_UNIT_TEST
typedef enum
{
    RELAY_CLI_CMD_GET = 0x00,
    RELAY_CLI_CMD_SET = 0x01,
    RELAY_CLI_CMD_SET_UNACK = 0x02,
    RELAY_CLI_MAX_CMD
} relay_cli_cmd_t;

/**
 * @brief Callback handler for the Relay client unit test command.
 *
 * This function handles the Relay client unit test command by processing the
 * provided command ID and arguments.
 *
 * @param[in] cmd_id The command ID to be processed.
 * @param[in] argc The number of arguments provided.
 * @param[in] argv The array of arguments.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_INVALID_ARG: Invalid arguments
 *     - Other error codes depending on the implementation
 */
static meshx_err_t relay_cli_unit_test_cb_handler(int cmd_id, int argc, char **argv)
{
    meshx_err_t err = MESHX_SUCCESS;
    MESHX_LOGE(MOD_SRC, "argc|cmd_id: %d|%d", argc, cmd_id);
    if (argc < 1 || cmd_id >= RELAY_CLI_MAX_CMD)
    {
        MESHX_LOGE(
            MOD_SRC, "Relay Client Unit Test: Invalid number of arguments");
        return MESHX_INVALID_ARG;
    }

    relay_cli_cmd_t cmd = (relay_cli_cmd_t)cmd_id;
    uint8_t set_get = (cmd == RELAY_CLI_CMD_GET) ? MESHX_GEN_ON_OFF_CLI_MSG_GET : MESHX_GEN_ON_OFF_CLI_MSG_SET;
    uint16_t el_id  = UT_GET_ARG(0, uint16_t, argv);

    err = set_get ? meshx_relay_el_set_state(el_id, (cmd == RELAY_CLI_CMD_SET_UNACK)) :
                    meshx_relay_el_get_state(el_id);
    if (err)
    {
        MESHX_LOGE(MOD_SRC, "Relay Client Unit Test: Command %d failed", cmd);
    }
    return err;
}
#endif /* CONFIG_ENABLE_UNIT_TEST */


/**
 * @brief Registers a callback handler for relay application requests.
 *
 * This function subscribes the provided callback to control task messages
 * related to BLE events. It ensures the callback is valid before subscribing.
 *
 * @return
 *     - Result of control_task_msg_subscribe() otherwise.
 */
static meshx_err_t meshx_relay_cli_reg_app_req_cb()
{
    return control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_TO_BLE,
        CONTROL_TASK_MSG_CODE_EVT_MASK,
        (control_task_msg_handle_t)&meshx_relay_cli_el_app_req_handler
    );
}

/**
 * @brief Registers a callback handler for fresh boot events.
 *
 * This function subscribes the provided callback to control task messages
 * related to element state changes. It ensures the callback is valid before subscribing.
 *
 * @return
 *     - Result of control_task_msg_subscribe() otherwise.
 */
static meshx_err_t meshx_relay_cli_reg_freshboot_cb()
{
    return control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_SYSTEM,
        CONTROL_TASK_MSG_EVT_SYSTEM_FRESH_BOOT,
        (control_task_msg_handle_t)&relay_cli_freshboot_msg_handle
    );
}

/**
 * @brief Registers a callback for relay element state change events.
 *
 * This function subscribes the provided callback to control task messages
 * related to relay element state changes. It ensures the callback is valid before subscribing.
 *
 * @return
 *     - Result of control_task_msg_subscribe() otherwise.
 */
static meshx_err_t meshx_relay_cli_el_state_change_reg_cb()
{
    return control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_EL_STATE_CH,
        RELAY_CLI_EL_STATE_CH_EVT_MASK,
        (control_task_msg_handle_t)&meshx_handle_rel_el_msg
    );
}

/**
 * @brief Sets the state of the relay element.
 *
 * This function constructs a generic On/Off client message to set the state of a relay
 * element identified by the given element ID. It then publishes this message to the
 * control task for BLE communication.
 *
 * @param[in] el_id The element ID of the relay whose state is to be set.
 * @param[in] ack Indicates whether an acknowledgment is required (true) or not (false).
 * @return meshx_err_t Returns the result of the message publish operation.
 */
meshx_err_t meshx_relay_el_set_state(uint16_t el_id, bool ack)
{
    meshx_gen_on_off_cli_msg_t msg = {
        .ack        = ack == MESHX_GEN_ON_OFF_CLI_MSG_ACK,
        .set_get    = MESHX_GEN_ON_OFF_CLI_MSG_SET,
        .element_id = el_id
    };
    return control_task_msg_publish(
            CONTROL_TASK_MSG_CODE_TO_BLE,
            CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF,
            &msg,
            sizeof(msg));
}

/**
 * @brief Retrieves the current state of the relay element.
 *
 * This function constructs a generic On/Off client message to request the current
 * state of a relay element identified by the given element ID. It then publishes
 * this message to the control task for BLE communication.
 *
 * @param[in] el_id The element ID of the relay whose state is to be retrieved.
 * @return meshx_err_t Returns the result of the message publish operation.
 */
meshx_err_t meshx_relay_el_get_state(uint16_t el_id)
{
    meshx_gen_on_off_cli_msg_t msg = {
        .ack        = MESHX_GEN_ON_OFF_CLI_MSG_ACK,
        .set_get    = MESHX_GEN_ON_OFF_CLI_MSG_GET,
        .element_id = el_id
    };
    return control_task_msg_publish(
            CONTROL_TASK_MSG_CODE_TO_BLE,
            CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF,
            &msg,
            sizeof(msg));
}

/**
 * @brief Create Dynamic Relay Model Elements
 *
 * @param[in]       pdev            Pointer to device structure
 * @param[in]       element_cnt     Maximum number of relay models
 *
 * @return meshx_err_t
 */
meshx_err_t create_relay_client_elements(dev_struct_t *pdev, uint16_t element_cnt)
{
    meshx_err_t err;

    err = meshx_dev_create_relay_model_space(pdev, element_cnt);
    if (err)
    {
        MESHX_LOGE(MOD_SRC, "Relay Model space create failed: (%d)", err);
        return err;
    }

    err = meshx_add_relay_cli_model_to_element_list(pdev, (uint16_t *)&pdev->element_idx, element_cnt);
    if (err)
    {
        MESHX_LOGE(MOD_SRC, "Relay Model add to element create failed: (%d)", err);
        return err;
    }

#if CONFIG_ENABLE_CONFIG_SERVER
    err = meshx_config_server_cb_reg(
        (config_srv_cb_t)&relay_client_config_cli_cb,
        CONFIG_SERVER_CB_MASK);
    if (err)
    {
        MESHX_LOGE(MOD_SRC, "Relay Model config client callback reg failed: (%d)", err);
        return err;
    }
#endif /* CONFIG_ENABLE_CONFIG_SERVER */
    err = meshx_relay_cli_reg_app_req_cb();
    if (err)
    {
        MESHX_LOGE(MOD_SRC, "Relay Client app req callback reg failed: (%d)", err);
        return err;
    }
    err = meshx_relay_cli_reg_freshboot_cb();
    if (err)
    {
        MESHX_LOGE(MOD_SRC, "Relay Client freshboot callback reg failed: (%d)", err);
        return err;
    }
    err = meshx_relay_cli_el_state_change_reg_cb();
    if (err)
    {
        MESHX_LOGE(MOD_SRC, "Relay Client element state change callback reg failed: (%d)", err);
        return err;
    }
#if CONFIG_ENABLE_UNIT_TEST
    err = register_unit_test(MOD_SRC, &relay_cli_unit_test_cb_handler);
    if (err)
    {
        MESHX_LOGE(MOD_SRC, "unit_test reg failed: (%d)", err);
        return err;
    }
#endif /* CONFIG_ENABLE_UNIT_TEST */

    err = meshx_on_off_client_init();
    if (err)
    {
        MESHX_LOGE(MOD_SRC, "meshx_onoff_client_init failed: (%d)", err);
        return err;
    }

    return MESHX_SUCCESS;
}

REG_MESHX_ELEMENT_FN(relay_cli_el, MESHX_ELEMENT_TYPE_RELAY_CLIENT, create_relay_client_elements);

#endif /* CONFIG_RELAY_CLIENT_COUNT > 0*/
