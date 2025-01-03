/**
 * @file light_cwww_client.c
 * @brief Implementation of the CW-WW (Cool White - Warm White) client model for BLE Mesh.
 *
 * This file contains the implementation of the CW-WW client model, including initialization,
 * configuration, and message handling functions. The CW-WW client model is used to control
 * and manage CW-WW lighting devices in a BLE Mesh network.
 *
 * @note This implementation supports configuration server callbacks and generic client callbacks.
 *       It also includes functions to create and add CW-WW client models to the device's element list.
 *
 * @details
 * The CW-WW client model supports the following features:
 * - Initialization and allocation of resources for CW-WW models.
 * - Handling of configuration server events such as model publication and application key binding.
 * - Handling of generic client callback events for CW-WW models.
 * - Sending CW-WW messages to the server.
 *
 * @author Pranjal Chanda
 */
#include "light_cwww_client.h"

#if CONFIG_LIGHT_CWWW_CLIENT_COUNT > 0

#if CONFIG_ENABLE_CONFIG_SERVER
#include "config_server.h"

/**
 * @brief Configuration server callback event mask for cwww server.
 */
#define CONFIG_SERVER_CB_MASK \
    CONFIG_EVT_MODEL_PUB_ADD  \
    | CONFIG_EVT_MODEL_SUB_ADD | CONFIG_EVT_MODEL_APP_KEY_BIND
#endif /* CONFIG_ENABLE_CONFIG_SERVER */

#if defined(__CONTROL_TASK_H__)
#define CONTROL_TASK_MSG_CODE_EVT_MASK      CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF | CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL
#endif /* __CONTROL_TASK_H__ */

#define CWWW_CLI_PROD_ONOFF_ENABLE_CB       true
#define GET_RELATIVE_EL_IDX(_element_id)    ((_element_id) - cwww_client_element_init_ctrl.element_id_start)
#define IS_EL_IN_RANGE(_element_id)         ((_element_id) >= cwww_client_element_init_ctrl.element_id_start \
                                            && (_element_id) < cwww_client_element_init_ctrl.element_id_end)

#define CWWW_CLI_PROD_ONOFF_CLI_CB_EVT_BMAP PROD_ONOFF_CLI_EVT_ALL
#define CWWW_CLI_PROD_CTL_CLI_CB_EVT_BMAP   LIGHT_CTL_CLI_EVT_ALL

static const esp_ble_mesh_model_t cwww_cli_sig_template[CWWW_CLI_MODEL_SIG_CNT] = {
    ESP_BLE_MESH_SIG_MODEL(ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_CLI, NULL, NULL, NULL),
    ESP_BLE_MESH_SIG_MODEL(ESP_BLE_MESH_MODEL_ID_LIGHT_CTL_CLI, NULL, NULL, NULL),
};
cwww_client_elements_t cwww_client_element_init_ctrl;

#if CWWW_CLI_PROD_ONOFF_ENABLE_CB

/**
 * @brief CW-WW Client Generic Client Callback
 *
 * This function handles the CW-WW client generic client callback events.
 *
 * @param param Pointer to the BLE Mesh generic client callback parameter structure.
 * @param evt Event type of the callback.
 */
static void cwww_client_generic_client_cb(const esp_ble_mesh_generic_client_cb_param_t *param, prod_onoff_cli_evt_t evt)
{
    uint8_t element_id = param->params->model->element_idx;
    if (!IS_EL_IN_RANGE(element_id))
    {
        return;
    }

    size_t rel_el_id = GET_RELATIVE_EL_IDX(element_id);
    cwww_cli_ctx_t *el_ctx = &cwww_client_element_init_ctrl.cwww_cli_ctx[rel_el_id];
    cwww_client_msg_t msg = {0};

    switch (evt)
    {
        case PROD_ONOFF_CLI_PUBLISH:
            el_ctx->state = !param->status_cb.onoff_status.present_onoff;
            ESP_LOGD(TAG, "PUBLISH: %d", param->status_cb.onoff_status.present_onoff);
            ESP_LOGI(TAG, "Next state: %d", el_ctx->state);
            break;
        case PROD_ONOFF_CLI_TIMEOUT:
            ESP_LOGD(TAG, "Timeout");
            msg.element_id = param->params->model->element_idx;
            msg.set_get = CWWW_CLI_MSG_SET;
            msg.ack = CWWW_CLI_MSG_ACK;
            /*
            * @warning: Possible loop case:
            * 1. CWWW Client sends a message to the server
            * 2. Timeout occurs
            * 3. #1 and #2 are repeated with no break in stetes.
            */
            control_task_send_msg(CONTROL_TASK_MSG_CODE_TO_BLE, CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF, &msg, sizeof(msg));
            break;
        case PROD_ONOFF_CLI_EVT_SET:
            el_ctx->state = !param->status_cb.onoff_status.present_onoff;
            ESP_LOGD(TAG, "SET: %d", param->status_cb.onoff_status.present_onoff);
            ESP_LOGI(TAG, "Next state: %d", el_ctx->state);
            break;
        default:
            ESP_LOGW(TAG, "Unhandled event: %d", evt);
            break;
    }
}

/**
 * @brief Callback function for handling Light CTL Client events.
 *
 * This function is called whenever a Light CTL Client event occurs.
 *
 * @param param Pointer to the structure containing the event parameters.
 * @param evt The specific Light CTL Client event that occurred.
 */
static bool cwww_client_ctl_client_cb(const esp_ble_mesh_light_client_cb_param_t *param, light_ctl_cli_evt_t evt)
{
    uint8_t element_id = param->params->model->element_idx;
    if (!IS_EL_IN_RANGE(element_id))
        return false;

    size_t rel_el_id = GET_RELATIVE_EL_IDX(element_id);
    cwww_cli_ctx_t *el_ctx = &cwww_client_element_init_ctrl.cwww_cli_ctx[rel_el_id];
    cwww_client_msg_t msg = {0};

    if(param->params->opcode == ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_STATUS)
    {
        el_ctx->lightness = param->status_cb.ctl_status.present_ctl_lightness;
        el_ctx->temperature = param->status_cb.ctl_status.present_ctl_temperature;
    }
    else if (param->params->opcode == ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_STATUS)
    {
        el_ctx->delta_uv = param->status_cb.ctl_temperature_status.present_ctl_delta_uv;
        el_ctx->temperature = param->status_cb.ctl_temperature_status.present_ctl_temperature;
    }
    else if ((param->params->opcode == ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_STATUS)
          && (param->status_cb.ctl_temperature_range_status.status_code == ESP_OK))
    {
        el_ctx->lightness_range_max = param->status_cb.ctl_temperature_range_status.range_max;
        el_ctx->lightness_range_min = param->status_cb.ctl_temperature_range_status.range_min;
    }
    else if (ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_DEFAULT_STATUS == param->params->opcode)
    {
        el_ctx->temp_def = param->status_cb.ctl_default_status.temperature;
        el_ctx->dwlta_uv_def = param->status_cb.ctl_default_status.delta_uv;
        el_ctx->lightness_def = param->status_cb.ctl_default_status.lightness;
    }
    else
    {
        /* Return as No CTL related OPCode were received */
        return false;
    }

    switch (evt)
    {
        case LIGHT_CTL_CLI_PUBLISH:
            ESP_LOGI(TAG, "PUBLISH: %d %d",
                el_ctx->lightness,
                el_ctx->temperature);
            break;
        case LIGHT_CTL_CLI_TIMEOUT:
            ESP_LOGD(TAG, "Timeout");
            msg.ack = CWWW_CLI_MSG_ACK;
            msg.set_get = CWWW_CLI_MSG_SET;
            msg.element_id = param->params->model->element_idx;
            /*
            * @warning: Possible loop case:
            * 1. CWWW Client sends a message to the server
            * 2. Timeout occurs
            * 3. #1 and #2 are repeated with no break in stetes.
            */
            control_task_send_msg(CONTROL_TASK_MSG_CODE_TO_BLE, CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL, &msg, sizeof(msg));
            break;
        case LIGHT_CTL_CLI_EVT_SET:
            ESP_LOGI(TAG, "SET: %d, %d",
                     el_ctx->lightness,
                     el_ctx->temperature);
            break;
        default:
            ESP_LOGW(TAG, "Unhandled event: %d", evt);
            break;
    }
    return true;
}
#if CONFIG_ENABLE_CONFIG_SERVER
/**
 * @brief Callback function for configuration server events.
 *
 * This function handles events from the configuration server, such as model publication
 * and application binding events.
 *
 * @param param Pointer to the callback parameter structure.
 * @param evt Configuration event type.
 */
static void cwww_client_config_srv_cb(const esp_ble_mesh_cfg_server_cb_param_t *param, config_evt_t evt)
{
    cwww_cli_ctx_t *el_ctx = NULL;
    size_t rel_el_id = 0;
    uint16_t element_id = 0;

    ESP_LOGD(TAG, "EVT: %p", (void *)evt);
    switch (evt)
    {
    case CONFIG_EVT_MODEL_APP_KEY_BIND:
        element_id = param->value.state_change.mod_app_bind.element_addr - esp_ble_mesh_get_primary_element_address();
        if (!IS_EL_IN_RANGE(element_id))
            break;
        rel_el_id = GET_RELATIVE_EL_IDX(element_id);
        el_ctx = &cwww_client_element_init_ctrl.cwww_cli_ctx[rel_el_id];
        el_ctx->app_id = param->value.state_change.appkey_add.app_idx;
        break;
    case CONFIG_EVT_MODEL_PUB_ADD:
    case CONFIG_EVT_MODEL_PUB_DEL:
        element_id = param->value.state_change.mod_pub_set.element_addr - esp_ble_mesh_get_primary_element_address();
        if (!IS_EL_IN_RANGE(element_id))
            break;
        rel_el_id = GET_RELATIVE_EL_IDX(element_id);
        el_ctx = &cwww_client_element_init_ctrl.cwww_cli_ctx[rel_el_id];
        el_ctx->pub_addr = evt == CONFIG_EVT_MODEL_PUB_ADD ? param->value.state_change.mod_pub_set.pub_addr
                                                           : ESP_BLE_MESH_ADDR_UNASSIGNED;
        el_ctx->app_id = param->value.state_change.mod_pub_set.app_idx;
        ESP_LOGI(TAG, "PUB_ADD: %d, %d, 0x%x, 0x%x", element_id, rel_el_id, el_ctx->pub_addr, el_ctx->app_id);
        break;
    default:
        break;
    }
}
#endif /* CONFIG_ENABLE_CONFIG_SERVER */

#if defined(__CONTROL_TASK_H__)
/**
 * @brief CWWW Client Control Task Message Handler
 *
 * This function handles the cwww client control task messages.
 *
 * @param[in] pdev   Pointer to the device structure.
 * @param[in] evt    Event type of the control task message.
 * @param[in] params Pointer to the parameters of the control task message.
 * @return esp_err_t
 */
static esp_err_t cwww_cli_control_task_msg_handle(dev_struct_t *pdev, control_task_msg_evt_t evt, const void *params)
{
    esp_err_t err = ESP_OK;
    const cwww_client_msg_t *msg = (const cwww_client_msg_t *)params;
    cwww_cli_sig_id_t model_id = evt == CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF ?
                                        CWWW_CLI_SIG_ONOFF_MODEL_ID : CWWW_CLI_SIG_L_CTL_MODEL_ID;

    uint16_t element_id = msg->element_id;
    size_t rel_el_id = element_id - cwww_client_element_init_ctrl.element_id_start;
    cwww_cli_ctx_t *el_ctx = &cwww_client_element_init_ctrl.cwww_cli_ctx[rel_el_id];

    if (!IS_EL_IN_RANGE(element_id))
    {
        return ESP_ERR_INVALID_ARG;
    }

    if(model_id == CWWW_CLI_SIG_L_CTL_MODEL_ID)
    {
        /* Set new context value as per msg sent */
        el_ctx->delta_uv = (msg->arg_bmap & CWWW_ARG_BMAP_DELTA_UV_SET) ? msg->delta_uv : el_ctx->delta_uv;
        el_ctx->lightness = (msg->arg_bmap & CWWW_ARG_BMAP_LIGHTNESS_SET) ? msg->lightness : el_ctx->lightness;
        el_ctx->temperature = (msg->arg_bmap & CWWW_ARG_BMAP_TEMPERATURE_SET) ? msg->temperature : el_ctx->temperature;
        el_ctx->lightness_range_max = (msg->arg_bmap & CWWW_ARG_BMAP_TEMPERATURE_RANGE_SET_MAX) ? msg->lightness_range_max : el_ctx->lightness_range_max;
        el_ctx->lightness_range_min = (msg->arg_bmap & CWWW_ARG_BMAP_TEMPERATURE_RANGE_SET_MIN) ? msg->lightness_range_min : el_ctx->lightness_range_min;
    }
    /* Send message to the cwww client */
    err = ble_mesh_send_cwww_msg(pdev,
                                 model_id,
                                 msg->element_id,
                                 msg->set_get,
                                 msg->ack);
    if (err)
    {
        ESP_LOGE(TAG, "CWWW Client Control Task: Set OnOff failed (%p)", (void *)err);
    }
    return err;
}
#endif /* __CONTROL_TASK_H__ */

#if CONFIG_ENABLE_UNIT_TEST

typedef enum
{
    /* ONOFF UT COMMANDS */
    CWWW_CLI_UT_CMD_ONOFF_GET = 0,
    CWWW_CLI_UT_CMD_ONOFF_SET,
    CWWW_CLI_UT_CMD_ONOFF_SET_UNACK,
    /* CTL UT COMMANDS */
    CWWW_CLI_UT_CMD_CTL_GET,
    CWWW_CLI_UT_CMD_CTL_SET,
    CWWW_CLI_UT_CMD_CTL_SET_UNACK,
    CWWW_CLI_UT_CMD_LIGHTNESS_SET,
    CWWW_CLI_UT_CMD_LIGHTNESS_SET_UNACK,
    CWWW_CLI_UT_CMD_TEMPERATURE_SET,
    CWWW_CLI_UT_CMD_TEMPERATURE_SET_UNACK,
    CWWW_CLI_UT_CMD_DELTA_UV_SET,
    CWWW_CLI_UT_CMD_DELTA_UV_SET_UNACK,
    CWWW_CLI_UT_CMD_TEMPERATURE_RANGE_SET,
    CWWW_CLI_UT_CMD_TEMPERATURE_RANGE_SET_UNACK,
    CWWW_CLI_MAX_CMD
} cwww_cli_cmd_t;

/**
 * @brief Callback handler for the CW-WW client unit test command.
 *
 * This function handles the CW-WW client unit test command by processing the
 * provided command ID and arguments.
 *
 * @param[in] cmd_id The command ID to be processed.
 * @param[in] argc The number of arguments provided.
 * @param[in] argv The array of arguments.
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 *     - Other error codes depending on the implementation
 */
static esp_err_t cwww_cli_unit_test_cb_handler(int cmd_id, int argc, char **argv)
{
    esp_err_t err = ESP_OK;
    cwww_client_msg_t msg = {0};
    msg.element_id = UT_GET_ARG(0, uint16_t, argv);
    cwww_cli_cmd_t cmd = (cwww_cli_cmd_t)cmd_id;

    ESP_LOGI(TAG, "argc|cmd_id: %d|%d", argc, cmd_id);
    if (argc < 1 || cmd_id >= CWWW_CLI_MAX_CMD)
    {
        ESP_LOGE(TAG, "Relay Client Unit Test: Invalid number of arguments");
        return ESP_ERR_INVALID_ARG;
    }

    control_task_msg_evt_to_ble_t msg_evt = CONTROL_TASK_MSG_EVT_TO_BLE_MAX;
    switch (cmd)
    {
        case CWWW_CLI_UT_CMD_ONOFF_GET:
            /* ut 1 0 1 <el_id> */
            msg.ack = CWWW_CLI_MSG_ACK;
            msg.set_get = CWWW_CLI_MSG_GET;
            msg_evt = CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF;
            break;
        case CWWW_CLI_UT_CMD_ONOFF_SET:
            /* ut 1 1 1 <el_id> */
        case CWWW_CLI_UT_CMD_ONOFF_SET_UNACK:
            /* ut 1 2 1 <el_id> */
            msg.set_get = CWWW_CLI_MSG_SET;
            msg.arg_bmap = CWWW_ARG_BMAP_ONOFF_SET;
            msg_evt = CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF;
            msg.ack = cmd == CWWW_CLI_UT_CMD_ONOFF_SET ? CWWW_CLI_MSG_ACK : CWWW_CLI_MSG_NO_ACK;
            break;
        case CWWW_CLI_UT_CMD_CTL_GET:
            /* ut 1 3 1 <el_id> */
            msg.ack = CWWW_CLI_MSG_NO_ACK;
            msg.set_get = CWWW_CLI_MSG_GET;
            msg_evt = CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL;
            break;
        case CWWW_CLI_UT_CMD_CTL_SET:
            /* ut 1 4 4 <el_id> <temp> <light> <delta_uv> */
        case CWWW_CLI_UT_CMD_CTL_SET_UNACK:
            /* ut 1 5 4 <el_id> <temp> <light> <delta_uv> */
            if (argc >= 2) msg.temperature = UT_GET_ARG(1, uint16_t, argv);
            if (argc >= 3) msg.lightness = UT_GET_ARG(2, uint16_t, argv);
            if (argc >= 4) msg.delta_uv = UT_GET_ARG(3, uint16_t, argv);
            msg.set_get = CWWW_CLI_MSG_SET;
            msg.arg_bmap = CWWW_ARG_BMAP_CTL_SET;
            msg_evt = CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL;
            msg.ack = cmd == CWWW_CLI_UT_CMD_CTL_SET ? CWWW_CLI_MSG_ACK : CWWW_CLI_MSG_NO_ACK;
            break;
        case CWWW_CLI_UT_CMD_LIGHTNESS_SET:
            /* ut 1 6 2 <el_id> <light> */
        case CWWW_CLI_UT_CMD_LIGHTNESS_SET_UNACK:
            /* ut 1 7 2 <el_id> <light> */
            if (argc >= 2) msg.lightness = UT_GET_ARG(1, uint16_t, argv);
            msg.set_get = CWWW_CLI_MSG_SET;
            msg.arg_bmap = CWWW_ARG_BMAP_LIGHTNESS_SET;
            msg_evt = CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL;
            msg.ack = cmd == CWWW_CLI_UT_CMD_LIGHTNESS_SET ? CWWW_CLI_MSG_ACK : CWWW_CLI_MSG_NO_ACK;
            break;
        case CWWW_CLI_UT_CMD_TEMPERATURE_SET:
            /* ut 1 8 2 <el_id> <temp> */
        case CWWW_CLI_UT_CMD_TEMPERATURE_SET_UNACK:
            /* ut 1 9 2 <el_id> <temp> */
            if (argc >= 2) msg.temperature = UT_GET_ARG(1, uint16_t, argv);
            msg.set_get = CWWW_CLI_MSG_SET;
            msg.arg_bmap = CWWW_ARG_BMAP_TEMPERATURE_SET;
            msg_evt = CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL;
            msg.ack = cmd == CWWW_CLI_UT_CMD_TEMPERATURE_SET ? CWWW_CLI_MSG_ACK : CWWW_CLI_MSG_NO_ACK;
            break;
        case CWWW_CLI_UT_CMD_DELTA_UV_SET:
            /* ut 1 10 2 <el_id> <delta_uv> */
        case CWWW_CLI_UT_CMD_DELTA_UV_SET_UNACK:
            /* ut 1 11 2 <el_id> <delta_uv> */
            if (argc >= 2) msg.delta_uv = UT_GET_ARG(1, uint16_t, argv);
            msg.set_get = CWWW_CLI_MSG_SET;
            msg.arg_bmap = CWWW_ARG_BMAP_DELTA_UV_SET;
            msg_evt = CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL;
            msg.ack = cmd == CWWW_CLI_UT_CMD_DELTA_UV_SET ? CWWW_CLI_MSG_ACK : CWWW_CLI_MSG_NO_ACK;
            break;
        case CWWW_CLI_UT_CMD_TEMPERATURE_RANGE_SET:
            /* ut 1 12 3 <el_id> <min> <max> */
        case CWWW_CLI_UT_CMD_TEMPERATURE_RANGE_SET_UNACK:
            /* ut 1 13 3 <el_id> <min> <max> */
            if (argc >= 2) msg.lightness_range_min = UT_GET_ARG(1, uint16_t, argv);
            if (argc >= 3) msg.lightness_range_max = UT_GET_ARG(2, uint16_t, argv);
            msg.set_get = CWWW_CLI_MSG_SET;
            msg.arg_bmap = CWWW_ARG_BMAP_CTL_SET;
            msg_evt = CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL;
            msg.ack = cmd == CWWW_CLI_UT_CMD_TEMPERATURE_RANGE_SET ? CWWW_CLI_MSG_ACK : CWWW_CLI_MSG_NO_ACK;
            break;
        default:
            ESP_LOGE(TAG, "CWWW Client Unit Test: Invalid command");
            return ESP_ERR_INVALID_ARG;
    }

    err = control_task_send_msg(CONTROL_TASK_MSG_CODE_TO_BLE, msg_evt, &msg, sizeof(msg));
    if (err)
    {
        ESP_LOGE(TAG, "CWWW Client Unit Test: Command %d failed", cmd);
    }
    return err;
}

#endif /* CONFIG_ENABLE_UNIT_TEST */

#endif /* CWWW_CLI_PROD_ONOFF_ENABLE_CB */
/**
 * @brief Creates a CW-WW model space for the given device.
 *
 * This function initializes and allocates resources for a CW-WW (Cool White - Warm White)
 * model space for the specified device. It sets up the necessary structures and configurations
 * to manage the CW-WW model.
 *
 * @param[in] pdev Pointer to the device structure.
 * @param[in] n_max Maximum number of elements that can be created in the model space.
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_NO_MEM: Memory allocation failure
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 */
static esp_err_t dev_create_cwww_model_space(dev_struct_t const *pdev, uint16_t n_max)
{
    if (!pdev)
        return ESP_ERR_INVALID_STATE;

    /* Assign Spaces for Model List, Publish List and onoff gen list */
    cwww_client_element_init_ctrl.model_cnt = n_max;
    void **temp = NULL;

    for (size_t cwww_model_id = 0; cwww_model_id < n_max && cwww_model_id < CWWW_CLI_MODEL_SIG_CNT; cwww_model_id++)
    {
#if CONFIG_GEN_ONOFF_CLIENT_COUNT
        /* Perform memcpy to setup the constants */
        memcpy(&cwww_client_element_init_ctrl.cwww_cli_sig_model_list[cwww_model_id][CWWW_CLI_SIG_ONOFF_MODEL_ID],
               &cwww_cli_sig_template[CWWW_CLI_SIG_ONOFF_MODEL_ID],
               sizeof(esp_ble_mesh_model_t));
        /* Set the dynamic spaces for the model */
        temp = (void **)&cwww_client_element_init_ctrl.cwww_cli_sig_model_list[cwww_model_id][CWWW_CLI_SIG_ONOFF_MODEL_ID].pub;
        *temp = &cwww_client_element_init_ctrl.cwww_cli_pub_list[cwww_model_id][CWWW_CLI_SIG_ONOFF_MODEL_ID];
        cwww_client_element_init_ctrl.cwww_cli_sig_model_list[cwww_model_id][CWWW_CLI_SIG_ONOFF_MODEL_ID].user_data =
            &cwww_client_element_init_ctrl.cwww_cli_list[cwww_model_id][CWWW_CLI_SIG_ONOFF_MODEL_ID];
#endif /* CONFIG_GEN_ONOFF_CLIENT_COUNT */

#if CONFIG_LIGHT_CTL_CLIENT_COUNT
        memcpy(&cwww_client_element_init_ctrl.cwww_cli_sig_model_list[cwww_model_id][CWWW_CLI_SIG_L_CTL_MODEL_ID],
               &cwww_cli_sig_template[CWWW_CLI_SIG_L_CTL_MODEL_ID],
               sizeof(esp_ble_mesh_model_t));
        temp = (void **)&cwww_client_element_init_ctrl.cwww_cli_sig_model_list[cwww_model_id][CWWW_CLI_SIG_L_CTL_MODEL_ID].pub;
        *temp = &cwww_client_element_init_ctrl.cwww_cli_pub_list[cwww_model_id][CWWW_CLI_SIG_L_CTL_MODEL_ID];
        cwww_client_element_init_ctrl.cwww_cli_sig_model_list[cwww_model_id][CWWW_CLI_SIG_L_CTL_MODEL_ID].user_data =
            &cwww_client_element_init_ctrl.cwww_cli_list[cwww_model_id][CWWW_CLI_SIG_L_CTL_MODEL_ID];
#endif /* CONFIG_LIGHT_CTL_CLIENT_COUNT */
    }
    return ESP_OK;
}

/**
 * @brief
 * This function adds the CW-WW client models to the element list of the specified device.
 * It initializes the necessary structures and configurations for each model.
 *
 * @param pdev Pointer to the device structure.
 * @param element_idx Pointer to the element index.
 * @param n_max Maximum number of elements that can be created in the model space.
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_NO_MEM: Memory allocation failure
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 */
static esp_err_t dev_add_cwww_cli_model_to_element_list(dev_struct_t *pdev, uint16_t *start_idx, uint16_t n_max)
{
    if (!pdev || !start_idx)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if((n_max + *start_idx) > CONFIG_MAX_ELEMENT_COUNT)
    {
        ESP_LOGE(TAG, "No of elements limit reached namx|start_idx|config_max: %d|%d|%d", n_max, *start_idx, CONFIG_MAX_ELEMENT_COUNT);
        return ESP_ERR_NO_MEM;
    }
    uint8_t *ref_ptr = NULL;
    esp_ble_mesh_elem_t *elements = pdev->elements;

    cwww_client_element_init_ctrl.element_id_start = *start_idx;

    for (size_t i = *start_idx; i < (n_max + *start_idx) && (i - *start_idx) < CWWW_CLI_MODEL_SIG_CNT; i++)
    {
        if (i == 0)
        {
            /* Insert the first SIG model in root model to save element virtual addr space */
            memcpy(&elements[i].sig_models[1],
                   cwww_client_element_init_ctrl.cwww_cli_sig_model_list[i - *start_idx],
                   sizeof(esp_ble_mesh_model_t));
            ref_ptr = (uint8_t *)&elements[i].sig_model_count;
            (*ref_ptr)++;
        }
        else
        {
            ESP_LOGD(TAG, "CWWW Client Element: %d", i);
            elements[i].sig_models = cwww_client_element_init_ctrl.cwww_cli_sig_model_list[i - *start_idx];
            elements[i].vnd_models = ESP_BLE_MESH_MODEL_NONE;
            ref_ptr = (uint8_t *)&elements[i].sig_model_count;
            *ref_ptr = CWWW_CLI_MODEL_SIG_CNT;
            ref_ptr = (uint8_t *)&elements[i].vnd_model_count;
            *ref_ptr = CWWW_CLI_MODEL_VEN_CNT;
        }
    }
    /* Increment the index for further registrations */
    cwww_client_element_init_ctrl.element_id_end = *start_idx += n_max;
    return ESP_OK;
}


/**
 * @brief Create Dynamic Light CWWW Elements
 *
 * @param[in]       pdev    Pointer to device structure
 *
 * @return esp_err_t
 */
esp_err_t create_cwww_client_elements(dev_struct_t *pdev)
{
    esp_err_t err;

    err = dev_create_cwww_model_space(pdev, CONFIG_LIGHT_CWWW_CLIENT_COUNT);
    if (err)
    {
        ESP_LOGE(TAG, "CWWW Model space create failed: (%d)", err);
        return err;
    }

    err = dev_add_cwww_cli_model_to_element_list(pdev, (uint16_t *)&pdev->element_idx, CONFIG_LIGHT_CWWW_CLIENT_COUNT);
    if (err)
    {
        ESP_LOGE(TAG, "CWWW Model add to element create failed: (%d)", err);
        return err;
    }

    err = prod_light_ctl_client_init();
    if (err)
    {
        ESP_LOGE(TAG, "prod_light_ctl_client_init failed: (%d)", err);
        return err;
    }

#if CWWW_CLI_PROD_ONOFF_ENABLE_CB
#if CONFIG_ENABLE_CONFIG_SERVER
    err = prod_config_server_cb_reg(&cwww_client_config_srv_cb, CONFIG_SERVER_CB_MASK);
    if (err)
    {
        ESP_LOGE(TAG, "Light CWWW config server callback reg failed: (%d)", err);
        return err;
    }
#endif /* CONFIG_ENABLE_CONFIG_SERVER */

    err = prod_onoff_reg_cb(&cwww_client_generic_client_cb, CWWW_CLI_PROD_ONOFF_CLI_CB_EVT_BMAP);
    if (err)
    {
        ESP_LOGE(TAG, "Light CWWW ONOFF callback reg failed: (%d)", err);
        return err;
    }
    err = prod_light_ctl_cli_reg_cb(&cwww_client_ctl_client_cb, CWWW_CLI_PROD_CTL_CLI_CB_EVT_BMAP);
    if (err)
    {
        ESP_LOGE(TAG, "Light CWWW CTL Model callback reg failed: (%d)", err);
        return err;
    }
#if defined(__CONTROL_TASK_H__)
    err = control_task_reg_msg_code_handler_cb(
        CONTROL_TASK_MSG_CODE_TO_BLE,
        CONTROL_TASK_MSG_CODE_EVT_MASK,
        (control_task_msg_handle_t)&cwww_cli_control_task_msg_handle);
    if (err)
    {
        ESP_LOGE(TAG, "control task callback reg failed: (%d)", err);
        return err;
    }
#endif /* __CONTROL_TASK_H__ */
#if CONFIG_ENABLE_UNIT_TEST
    err = register_unit_test(MODULE_ID_ELEMENT_LIGHT_CWWWW_CLIENT, &cwww_cli_unit_test_cb_handler);
    if (err)
    {
        ESP_LOGE(TAG, "unit_test reg failed: (%d)", err);
        return err;
    }
#endif /* CONFIG_ENABLE_UNIT_TEST */
#endif /* CWWW_CLI_PROD_ONOFF_ENABLE_CB */

    return ESP_OK;
}

/**
 * @brief Send a CW/WW (Cool White/Warm White) message over BLE Mesh.
 *
 * @param[in] pdev Pointer to the device structure.
 * @param[in] model_id Model ID of the CW/WW client.
 * @param[in] element_id Element ID to which the message is addressed.
 * @param[in] set_get Flag indicating whether the message is a set (1) or get (0) operation.
 * @param[in] ack Flag indicating whether the message requires an acknowledgment (1) or not (0).
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid argument
 *     - ESP_FAIL: Sending message failed
 */
esp_err_t ble_mesh_send_cwww_msg(dev_struct_t *pdev, cwww_cli_sig_id_t model_id, uint16_t element_id, uint8_t set_get, uint8_t ack)
{
    if (!pdev)
    {
        ESP_LOGE(TAG, "Invalid device structure");
        return ESP_ERR_INVALID_ARG;
    }

    size_t rel_el_id = element_id - cwww_client_element_init_ctrl.element_id_start;
    if (rel_el_id >= CONFIG_LIGHT_CWWW_CLIENT_COUNT)
    {
        ESP_LOGE(TAG, "Invalid element id: %d", element_id);
        return ESP_ERR_INVALID_ARG;
    }

    esp_ble_mesh_elem_t *element = &pdev->elements[element_id];
    esp_ble_mesh_model_t *model = &element->sig_models[model_id];
    cwww_cli_ctx_t *el_ctx = &cwww_client_element_init_ctrl.cwww_cli_ctx[rel_el_id];
    esp_err_t err = ESP_OK;
    uint16_t opcode = 0;

    switch (model_id)
    {
        case CWWW_CLI_SIG_ONOFF_MODEL_ID:
            if (set_get == CWWW_CLI_MSG_SET) {
                opcode = ack ? ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET : ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK;
            } else {
                opcode = ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET;
            }
            ESP_LOGD(TAG, "OPCODE: %p", (void *)(uint32_t)opcode);
            err = prod_onoff_client_send_msg(model, opcode, el_ctx->pub_addr, el_ctx->net_id, el_ctx->app_id, el_ctx->state, el_ctx->tid);
            if (!err)
            {
                el_ctx->tid++;
                el_ctx->state = (opcode == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK) ? el_ctx->state : !el_ctx->state;
            }
            break;

        case CWWW_CLI_SIG_L_CTL_MODEL_ID:
            if (set_get == CWWW_CLI_MSG_SET) {
                opcode = ack ? ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_SET : ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_SET_UNACK;
            } else {
                opcode = ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_GET;
            }
            ESP_LOGD(TAG, "OPCODE: %p", (void *)(uint32_t)opcode);
            err = prod_light_ctl_send_msg(model, opcode, el_ctx->pub_addr, el_ctx->net_id, el_ctx->app_id,
                                          el_ctx->lightness, el_ctx->temperature, el_ctx->delta_uv, el_ctx->tid);
            if (!err)
            {
                el_ctx->tid++;
            }
            break;

        default:
            ESP_LOGE(TAG, "Invalid model id: %d", model_id);
            err = ESP_ERR_INVALID_ARG;
            break;
    }

    if (err)
    {
        ESP_LOGE(TAG, "CWWW Client Send Message failed: (%d)", err);
    }

    return err;
}

#endif /* CONFIG_LIGHT_CWWW_CLIENT_COUNT */
