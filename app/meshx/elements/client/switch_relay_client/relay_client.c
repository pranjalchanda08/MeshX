/*
 * Copyright (c) 2025 Pranjal Chanda
 *
 * @brief: Relay Client Model
 * @file: relay_client.c
 *
 * This file contains the implementation of the relay client model for BLE mesh.
 * It includes functions for creating relay model space, adding relay client models
 * to the element list, handling control task messages, and sending relay messages.
 *
 * The relay client model is responsible for controlling the on/off state of relay
 * devices in the BLE mesh network.
 *
 * This implementation supports configuration server callbacks, control task message
 * handling, and unit testing.
 *
 * @auther: Pranjal Chanda
 */
#include "app_common.h"
#include "control_task.h"
#include "meshx_nvs.h"

#if CONFIG_RELAY_CLIENT_COUNT
#include "relay_client_element.h"

#if CONFIG_ENABLE_CONFIG_SERVER
#include "config_server.h"
#define CONFIG_SERVER_CB_MASK CONFIG_EVT_MODEL_PUB_ADD | CONFIG_EVT_MODEL_SUB_ADD | CONFIG_EVT_MODEL_APP_KEY_BIND
#endif /* CONFIG_ENABLE_CONFIG_SERVER */

#if defined(__CONTROL_TASK_H__)
#define CONTROL_TASK_MSG_CODE_EVT_MASK CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF
#endif /* __CONTROL_TASK_H__ */

#define RELAY_CLI_PROD_ONOFF_ENABLE_CB true
#define CONFIG_RELAY_PROD_ONOFF_SET_ACK true
#define RELAY_CLI_PROD_ONOFF_CLI_CB_EVT_BMAP PROD_ONOFF_CLI_EVT_ALL // PROD_ONOFF_CLI_EVT_GET | PROD_ONOFF_CLI_EVT_SET

#define GET_RELATIVE_EL_IDX(_element_id) _element_id - relay_element_init_ctrl.element_id_start
#define IS_EL_IN_RANGE(_element_id) (_element_id >= relay_element_init_ctrl.element_id_start && _element_id < relay_element_init_ctrl.element_id_end)

static relay_client_elements_t relay_element_init_ctrl;

static const esp_ble_mesh_model_t relay_sig_template = ESP_BLE_MESH_SIG_MODEL(
    ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_CLI,
    NULL, NULL, NULL);

/**
 * @brief Initializes the mesh element structure.
 *
 * This function initializes the mesh element structure with the specified maximum number of elements.
 *
 * @param n_max The maximum number of elements to initialize.
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_NO_MEM: Memory allocation failure
 *     - ESP_ERR_INVALID_ARG: Invalid argument
 */
static esp_err_t meshx_element_struct_init(uint16_t n_max)
{

    relay_element_init_ctrl.model_cnt = n_max;
    relay_element_init_ctrl.element_id_end = 0;
    relay_element_init_ctrl.element_id_start = 0;

    relay_element_init_ctrl.rel_cli_ctx = (rel_cli_ctx_t *)calloc(n_max, sizeof(rel_cli_ctx_t));
    if (!relay_element_init_ctrl.rel_cli_ctx)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for relay client context");
        return ESP_ERR_NO_MEM;
    }
    relay_element_init_ctrl.relay_cli_pub_list = (esp_ble_mesh_model_pub_t *)calloc(n_max, sizeof(esp_ble_mesh_model_pub_t));
    if (!relay_element_init_ctrl.relay_cli_pub_list)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for relay client pub list");
        return ESP_ERR_NO_MEM;
    }
    relay_element_init_ctrl.relay_cli_onoff_gen_list = (esp_ble_mesh_client_t *)calloc(n_max, sizeof(esp_ble_mesh_client_t));
    if (!relay_element_init_ctrl.relay_cli_onoff_gen_list)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for relay client onoff gen list");
        return ESP_ERR_NO_MEM;
    }
    relay_element_init_ctrl.relay_cli_sig_model_list = (esp_ble_mesh_model_t **)calloc(n_max, sizeof(esp_ble_mesh_model_t *));
    if (!relay_element_init_ctrl.relay_cli_sig_model_list)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for relay client sig model list");
        return ESP_ERR_NO_MEM;
    }
    else
    {
        for (size_t i = 0; i < n_max; i++)
        {
            relay_element_init_ctrl.relay_cli_sig_model_list[i] = (esp_ble_mesh_model_t *)calloc(RELAY_CLI_MODEL_SIG_CNT, sizeof(esp_ble_mesh_model_t));
            if (!relay_element_init_ctrl.relay_cli_sig_model_list[i])
            {
                ESP_LOGE(TAG, "Failed to allocate memory for relay client sig model list");
                return ESP_ERR_NO_MEM;
            }
        }
    }
    return ESP_OK;
}

/**
 * @brief Deinitializes the mesh element structure.
 *
 * This function deinitializes the mesh element structure by freeing the allocated memory.
 *
 * @param n_max The maximum number of elements to deinitialize.
 */
static void meshx_element_struct_deinit(uint16_t n_max)
{
    if (relay_element_init_ctrl.rel_cli_ctx)
    {
        free(relay_element_init_ctrl.rel_cli_ctx);
        relay_element_init_ctrl.rel_cli_ctx = NULL;
    }
    if (relay_element_init_ctrl.relay_cli_pub_list)
    {
        free(relay_element_init_ctrl.relay_cli_pub_list);
        relay_element_init_ctrl.relay_cli_pub_list = NULL;
    }
    if (relay_element_init_ctrl.relay_cli_onoff_gen_list)
    {
        free(relay_element_init_ctrl.relay_cli_onoff_gen_list);
        relay_element_init_ctrl.relay_cli_onoff_gen_list = NULL;
    }
    if (relay_element_init_ctrl.relay_cli_sig_model_list)
    {
        for (size_t i = 0; i < n_max; i++)
        {
            if (relay_element_init_ctrl.relay_cli_sig_model_list[i])
            {
                free(relay_element_init_ctrl.relay_cli_sig_model_list[i]);
                relay_element_init_ctrl.relay_cli_sig_model_list[i] = NULL;
            }
        }
        free(relay_element_init_ctrl.relay_cli_sig_model_list);
        relay_element_init_ctrl.relay_cli_sig_model_list = NULL;
    }
}

/**
 * @brief Create Dynamic Relay Model Elements
 *
 * @param[in] pdev  Pointer to device structure
 * @param[in] n_max Maximum number of relay models
 *
 * @return esp_err_t
 */
static esp_err_t dev_create_relay_model_space(dev_struct_t const *pdev, uint16_t n_max)
{
    if (!pdev)
        return ESP_ERR_INVALID_STATE;

    /* Assign Spaces for Model List, Publish List and onoff gen list */
    esp_err_t err = meshx_element_struct_init(n_max);
    if (err)
    {
        ESP_LOGE(TAG, "Failed to initialize relay element structures: (%d)", err);
        meshx_element_struct_deinit(n_max);
        return err;
    }
#if CONFIG_GEN_ONOFF_CLIENT_COUNT
    for (size_t relay_model_id = 0; relay_model_id < n_max; relay_model_id++)
    {
        /* Perform memcpy to setup the constants */
        memcpy(&relay_element_init_ctrl.relay_cli_sig_model_list[relay_model_id][0],
               &relay_sig_template,
               sizeof(esp_ble_mesh_model_t));
        /* Set the dynamic spaces for the model */
        void **temp = (void **)&relay_element_init_ctrl.relay_cli_sig_model_list[relay_model_id][0].pub;
        *temp = relay_element_init_ctrl.relay_cli_pub_list + relay_model_id;
        relay_element_init_ctrl.relay_cli_sig_model_list[relay_model_id][0].user_data =
            relay_element_init_ctrl.relay_cli_onoff_gen_list + relay_model_id;
    }
#endif /* CONFIG_GEN_ONOFF_CLIENT_COUNT */
    return ESP_OK;
}

/**
 * @brief Add relay client models to the element list.
 *
 * Registers the relay client models to the BLE Mesh element list.
 *
 * @param[in]       pdev        Pointer to the device structure.
 * @param[inout]    start_idx   Pointer to the start index of elements.
 * @param[in]       n_max       Maximum number of elements to add.
 *
 * @return ESP_OK on success, error code otherwise.
 */
static esp_err_t dev_add_relay_cli_model_to_element_list(dev_struct_t *pdev, uint16_t *start_idx, uint16_t n_max)
{
    if (!pdev)
        return ESP_ERR_INVALID_STATE;

    if (!start_idx)
        return ESP_ERR_INVALID_ARG;

    if ((n_max + *start_idx) > CONFIG_MAX_ELEMENT_COUNT)
    {
        ESP_LOGE(TAG, "No of elements limit reached namx|start_idx|config_max: %d|%d|%d", n_max, *start_idx, CONFIG_MAX_ELEMENT_COUNT);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = ESP_OK;
    esp_ble_mesh_elem_t *elements = pdev->elements;
    relay_element_init_ctrl.element_id_start = *start_idx;

    for (uint16_t i = *start_idx; i < (n_max + *start_idx); i++)
    {
        if (i == 0)
        {
            /* Insert the first SIG model in root model to save element virtual addr space */
            memcpy(&elements[i].sig_models[1],
                   relay_element_init_ctrl.relay_cli_sig_model_list[i - *start_idx],
                   sizeof(esp_ble_mesh_model_t));
            uint8_t *ref_ptr = (uint8_t *)&elements[i].sig_model_count;
            (*ref_ptr)++;
        }
        else
        {
            ESP_LOGD(TAG, "Relay Client Element: %d", i);
            elements[i].sig_models = relay_element_init_ctrl.relay_cli_sig_model_list[i - *start_idx];
            elements[i].vnd_models = ESP_BLE_MESH_MODEL_NONE;
            uint8_t *ref_ptr = (uint8_t *)&elements[i].sig_model_count;
            *ref_ptr = RELAY_CLI_MODEL_SIG_CNT;
            ref_ptr = (uint8_t *)&elements[i].vnd_model_count;
            *ref_ptr = RELAY_CLI_MODEL_VEN_CNT;
        }
        err = meshx_nvs_elemnt_ctx_get(i, &(relay_element_init_ctrl.rel_cli_ctx[i - *start_idx]), sizeof(rel_cli_ctx_t));
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to get cwww cli element context: (0x%x)", err);
        }
    }
    /* Increment the index for further registrations */
    relay_element_init_ctrl.element_id_end = *start_idx += n_max;
    return ESP_OK;
}

#if RELAY_CLI_PROD_ONOFF_ENABLE_CB
/**
 * @brief Relay Client Generic Client Callback
 *
 * This function handles the relay client generic client callback events.
 *
 * @param[in] param Pointer to the BLE Mesh generic client callback parameter structure.
 * @param[in] evt   Event type of the callback.
 * @return void
 */
void relay_el_generic_client_cb(const esp_ble_mesh_generic_client_cb_param_t *param, prod_onoff_cli_evt_t evt)
{
    uint8_t element_id = param->params->model->element_idx;
    if (!IS_EL_IN_RANGE(element_id))
    {
        return;
    }

    size_t rel_el_id = GET_RELATIVE_EL_IDX(element_id);
    rel_cli_ctx_t *el_ctx = &relay_element_init_ctrl.rel_cli_ctx[rel_el_id];
    relay_client_msg_t msg = {0};
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
        msg.set_get = RELAY_CLI_MSG_SET;
        msg.ack = RELAY_CLI_MSG_ACK;
        /*
         * @warning: Possible loop case:
         * 1. Relay Client sends a message to the server
         * 2. Timeout occurs
         * 3. #1 and #2 are repeated with no break in stetes.
         */
        control_task_publish(CONTROL_TASK_MSG_CODE_TO_BLE, CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF, &msg, sizeof(msg));
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

#if CONFIG_ENABLE_CONFIG_SERVER

/**
 * @brief Callback function for configuration server events.
 *
 * This function handles events from the configuration server, such as model publication
 * and application binding events.
 *
 * @param[in] param Pointer to the callback parameter structure.
 * @param[in] evt Configuration event type.
 * @return void
 */
static void relay_client_config_srv_cb(const esp_ble_mesh_cfg_server_cb_param_t *param, config_evt_t evt)
{
    rel_cli_ctx_t *el_ctx = NULL;
    size_t rel_el_id = 0;
    uint16_t element_id = 0;
    bool nvs_save = false;

    ESP_LOGD(TAG, "EVT: %p", (void *)evt);
    switch (evt)
    {
    case CONFIG_EVT_MODEL_APP_KEY_BIND:
        element_id = param->value.state_change.mod_app_bind.element_addr - esp_ble_mesh_get_primary_element_address();
        if (!IS_EL_IN_RANGE(element_id))
            return;
        rel_el_id = GET_RELATIVE_EL_IDX(element_id);
        el_ctx = &relay_element_init_ctrl.rel_cli_ctx[rel_el_id];
        el_ctx->app_id = param->value.state_change.mod_app_bind.app_idx;
        nvs_save = true;
        break;
    case CONFIG_EVT_MODEL_PUB_ADD:
    case CONFIG_EVT_MODEL_PUB_DEL:
        element_id = param->value.state_change.mod_pub_set.element_addr - esp_ble_mesh_get_primary_element_address();
        if (!IS_EL_IN_RANGE(element_id))
            return;
        rel_el_id = GET_RELATIVE_EL_IDX(element_id);
        el_ctx = &relay_element_init_ctrl.rel_cli_ctx[rel_el_id];
        el_ctx->pub_addr = evt == CONFIG_EVT_MODEL_PUB_ADD ? param->value.state_change.mod_pub_set.pub_addr
                                                           : ESP_BLE_MESH_ADDR_UNASSIGNED;
        el_ctx->app_id = param->value.state_change.mod_pub_set.app_idx;
        nvs_save = true;
        ESP_LOGI(TAG, "PUB_ADD: %d, %d, 0x%x, 0x%x", element_id, rel_el_id, el_ctx->pub_addr, el_ctx->app_id);
        break;
    default:
        break;
    }
    if(nvs_save)
    {
        esp_err_t err = meshx_nvs_elemnt_ctx_set(element_id, el_ctx, sizeof(rel_cli_ctx_t));
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to set relay element context: (%d)", err);
        }
    }
}
#endif /* #if CONFIG_ENABLE_CONFIG_SERVER */

#if defined(__CONTROL_TASK_H__)
/**
 * @brief Relay Client Control Task Message Handler
 *
 * This function handles the relay client control task messages.
 *
 * @param[in] pdev   Pointer to the device structure.
 * @param[in] evt    Event type of the control task message.
 * @param[in] params Pointer to the parameters of the control task message.
 * @return esp_err_t
 */
static esp_err_t relay_cli_control_task_msg_handle(dev_struct_t *pdev, control_task_msg_evt_t evt, const void *params)
{
    const relay_client_msg_t *msg = (const relay_client_msg_t *)params;
    esp_err_t err = ESP_OK;

    if (!pdev || !IS_EL_IN_RANGE(msg->element_id))
        return ESP_ERR_INVALID_ARG;

    if (evt == CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF)
    {
        err = ble_mesh_send_relay_msg(pdev,
                                      msg->element_id,
                                      msg->set_get,
                                      msg->ack);
        if (err)
        {
            ESP_LOGE(TAG, "Relay Client Control Task: Set OnOff failed (%p)", (void *)err);
        }
    }
    return err;
}
#endif /* __CONTROL_TASK_H__ */
#if CONFIG_ENABLE_UNIT_TEST
typedef enum
{
    RELAY_CLI_CMD_GET       = 0x00,
    RELAY_CLI_CMD_SET       = 0x01,
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
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 *     - Other error codes depending on the implementation
 */
static esp_err_t relay_cli_unit_test_cb_handler(int cmd_id, int argc, char **argv)
{
    esp_err_t err = ESP_OK;
    relay_client_msg_t msg = {0};
    ESP_LOGI(TAG, "argc|cmd_id: %d|%d", argc, cmd_id);
    if (argc < 1 || cmd_id >= RELAY_CLI_MAX_CMD)
    {
        ESP_LOGE(TAG, "Relay Client Unit Test: Invalid number of arguments");
        return ESP_ERR_INVALID_ARG;
    }
    relay_cli_cmd_t cmd = (relay_cli_cmd_t)cmd_id;
    msg.element_id = UT_GET_ARG(0, uint16_t, argv);
    msg.set_get = (cmd == RELAY_CLI_CMD_GET) ? RELAY_CLI_MSG_GET : RELAY_CLI_MSG_SET;
    msg.ack = (cmd == RELAY_CLI_CMD_SET_UNACK) ? RELAY_CLI_MSG_NO_ACK : RELAY_CLI_MSG_ACK;
    err = control_task_publish(CONTROL_TASK_MSG_CODE_TO_BLE, CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF, &msg, sizeof(msg));
    if (err)
    {
        ESP_LOGE(TAG, "Relay Client Unit Test: Command %d failed", cmd);
    }
    return err;
}
#endif /* CONFIG_ENABLE_UNIT_TEST */
#endif /* RELAY_CLI_PROD_ONOFF_ENABLE_CB */

/**
 * @brief Sends a relay message over BLE mesh.
 *
 * This function sends a relay message to a specified element in the BLE mesh network.
 *
 * @param[in] pdev Pointer to the device structure.
 * @param[in] element_id The ID of the element to which the message is sent.
 * @param[in] set_get Indicates whether the message is a set (0) or get (1) operation.
 * @param[in] ack Indicates whether an acknowledgment is required (1) or not (0).
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid argument
 *     - ESP_FAIL: Sending message failed
 */
esp_err_t ble_mesh_send_relay_msg(dev_struct_t *pdev, uint16_t element_id, uint8_t set_get, uint8_t ack)
{
    if (!pdev || !IS_EL_IN_RANGE(element_id))
        return ESP_ERR_INVALID_ARG;

    esp_err_t err = ESP_OK;
    esp_ble_mesh_elem_t *element = &pdev->elements[element_id];
    esp_ble_mesh_model_t *model  = &element->sig_models[0];

    size_t rel_el_id      = element_id - relay_element_init_ctrl.element_id_start;
    rel_cli_ctx_t *el_ctx = &relay_element_init_ctrl.rel_cli_ctx[rel_el_id];


    uint16_t opcode = ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET;
    if (RELAY_CLI_MSG_SET == set_get)
    {
        opcode = ack ? ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET : ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK;
    }

    ESP_LOGD(TAG, "OPCODE: %p", (void *)(uint32_t)opcode);

    /* Send message to the relay client */
    err = prod_onoff_client_send_msg(
            model,
            opcode,
            el_ctx->pub_addr,
            pdev->meshx_store.net_key_id,
            el_ctx->app_id,
            el_ctx->state,
            el_ctx->tid
        );
    if (err)
    {
        ESP_LOGE(TAG, "Relay Client Send Message failed: (%d)", err);
    }
    else {
        el_ctx->tid++;
        el_ctx->state = opcode == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK ? el_ctx->state : !el_ctx->state;
    }
    return err;
}

/**
 * @brief Create relay model space.
 *
 * Allocates memory and initializes space for relay models.
 *
 * @param[in] p_dev Pointer to the device structure.
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t create_relay_client_elements(dev_struct_t *pdev)
{
    esp_err_t err;

    err = dev_create_relay_model_space(pdev, CONFIG_RELAY_CLIENT_COUNT);
    if (err)
    {
        ESP_LOGE(TAG, "Relay Model space create failed: (%d)", err);
        return err;
    }

    err = dev_add_relay_cli_model_to_element_list(pdev, (uint16_t *)&pdev->element_idx, CONFIG_RELAY_CLIENT_COUNT);
    if (err)
    {
        ESP_LOGE(TAG, "Relay Model add to element create failed: (%d)", err);
        return err;
    }

    err = prod_onoff_client_init();
    if (err)
    {
        ESP_LOGE(TAG, "prod_onoff_client_init failed: (%d)", err);
        return err;
    }

#if RELAY_CLI_PROD_ONOFF_ENABLE_CB
    err = prod_onoff_reg_cb(&relay_el_generic_client_cb, RELAY_CLI_PROD_ONOFF_CLI_CB_EVT_BMAP);
    if (err)
    {
        ESP_LOGE(TAG, "Relay Model callback reg failed: (%d)", err);
        return err;
    }

#if CONFIG_ENABLE_CONFIG_SERVER
    err = prod_config_server_cb_reg(&relay_client_config_srv_cb, CONFIG_SERVER_CB_MASK);
    if (err)
    {
        ESP_LOGE(TAG, "Relay Model config server callback reg failed: (%d)", err);
        return err;
    }
#endif /* CONFIG_ENABLE_CONFIG_SERVER */
#if defined(__CONTROL_TASK_H__)
    err = control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_TO_BLE,
        CONTROL_TASK_MSG_CODE_EVT_MASK,
        (control_task_msg_handle_t)&relay_cli_control_task_msg_handle);
    if (err)
    {
        ESP_LOGE(TAG, "control task callback reg failed: (%d)", err);
        return err;
    }
#endif /* __CONTROL_TASK_H__ */
#if CONFIG_ENABLE_UNIT_TEST
    err = register_unit_test(MODULE_ID_ELEMENT_SWITCH_RELAY_CLIENT, &relay_cli_unit_test_cb_handler);
    if (err)
    {
        ESP_LOGE(TAG, "unit_test reg failed: (%d)", err);
        return err;
    }
#endif /* CONFIG_ENABLE_UNIT_TEST */
#endif /* RELAY_CLI_PROD_ONOFF_ENABLE_CB */

    return ESP_OK;
}

#endif /* CONFIG_RELAY_CLIENT_COUNT > 0*/
