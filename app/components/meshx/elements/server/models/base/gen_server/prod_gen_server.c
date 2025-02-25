/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file prod_gen_server.c
 * @brief Implementation of the BLE Mesh Generic Server for the product.
 *
 * This file contains the implementation of the BLE Mesh Generic Server
 * for handling various server events and registering callbacks.
 *
 *
 */

#include "prod_gen_server.h"

#define TAG __func__

#define PROD_SERVER_INIT_MAGIC_NO 0x1121

static const char *server_state_str[] =
    {
        [ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT] = "SRV_STATE_CH",
        [ESP_BLE_MESH_GENERIC_SERVER_RECV_GET_MSG_EVT] = "SRV_RECV_GET",
        [ESP_BLE_MESH_GENERIC_SERVER_RECV_SET_MSG_EVT] = "SRV_RECV_SET"};

static uint16_t prod_server_init = 0;

#if !CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE
static struct prod_server_cb_reg_head prod_server_cb_reg_list = SLIST_HEAD_INITIALIZER(prod_server_cb_reg_list);
static SemaphoreHandle_t prod_server_mutex;
#endif /* CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE */

/**
 * @brief Callback function for BLE Mesh Generic Server events.
 *
 * This function is called whenever a BLE Mesh Generic Server event occurs.
 *
 * @param event The event type for the BLE Mesh Generic Server.
 * @param param Parameters associated with the event.
 */
static void prod_ble_mesh_generic_server_cb(esp_ble_mesh_generic_server_cb_event_t event,
                                            const esp_ble_mesh_generic_server_cb_param_t *param)
{
    ESP_LOGD(TAG, "%s, op|src|dst:%04" PRIx32 "|%04x|%04x",
             server_state_str[event], param->ctx.recv_op, param->ctx.addr, param->ctx.recv_dst);

    if (event != ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT)
        return;
#if !CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE
    xSemaphoreTake(prod_server_mutex, portMAX_DELAY);
    struct prod_server_cb_reg *item;
    SLIST_FOREACH(item, &prod_server_cb_reg_list, next)
    {
        if ((item->model_id == param->model->model_id || item->model_id == param->model->vnd.model_id) && item->cb)
        {
            /* Dispatch callback to generic model type */
            item->cb(param);
            break;
        }
    }
    xSemaphoreGive(prod_server_mutex);
#else
    esp_err_t err = control_task_publish(
        CONTROL_TASK_MSG_CODE_FRM_BLE,
        param->model->model_id,
        param,
        sizeof(esp_ble_mesh_generic_server_cb_param_t));
    if (err)
    {
        ESP_LOGE(TAG, "Failed to publish to control task");
    }
#endif /* !CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE */
}

/**
 * @brief Register a callback function for the production server model.
 *
 * This function registers a callback function that will be called when
 * specific events related to the production server model occur.
 *
 * @param model_id The ID of the model for which the callback is being registered.
 * @param cb The callback function to be registered.
 *
 * @return
 *     - ESP_OK: Callback registered successfully.
 *     - ESP_ERR_INVALID_ARG: Invalid arguments.
 *     - ESP_FAIL: Failed to register the callback.
 */
esp_err_t prod_gen_srv_reg_cb(uint32_t model_id, prod_server_cb cb)
{
    esp_err_t err = ESP_OK;
#if !CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE
    xSemaphoreTake(prod_server_mutex, portMAX_DELAY);
    struct prod_server_cb_reg *item;
    SLIST_FOREACH(item, &prod_server_cb_reg_list, next)
    {
        if (item->model_id == model_id)
        {
            /* If already registered over-write */
            item->cb = cb;
            xSemaphoreGive(prod_server_mutex);
            return ESP_OK;
        }
    }

    item = malloc(sizeof(struct prod_server_cb_reg));
    if (item == NULL)
    {
        xSemaphoreGive(prod_server_mutex);
        return ESP_ERR_NO_MEM;
    }

    item->model_id = model_id;
    item->cb = cb;
    SLIST_INSERT_HEAD(&prod_server_cb_reg_list, item, next);
    xSemaphoreGive(prod_server_mutex);

#else
    err = control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_FRM_BLE,
        model_id,
        (control_task_msg_handle_t)cb);
#endif /* CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE */

    return err;
}

/**
 * @brief Callback function to deregister a generic server model.
 *
 * This function is called to deregister a generic server model identified by the given model ID.
 *
 * @param model_id The ID of the model to be deregistered.
 * @param cb The callback function to be deregistered.
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid argument
 *     - ESP_FAIL: Other failures
 */
esp_err_t prod_gen_srv_dereg_cb(uint32_t model_id, prod_server_cb cb)
{
#if !CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE
    xSemaphoreTake(prod_server_mutex, portMAX_DELAY);
    struct prod_server_cb_reg *item;
    struct prod_server_cb_reg *tmp;
    SLIST_FOREACH_SAFE(item, &prod_server_cb_reg_list, next, tmp)
    {
        if (item->model_id == model_id && item->cb == cb)
        {
            SLIST_REMOVE(&prod_server_cb_reg_list, item, prod_server_cb_reg, next);
            free(item);
            xSemaphoreGive(prod_server_mutex);
            return ESP_OK;
        }
    }
    xSemaphoreGive(prod_server_mutex);
    return ESP_ERR_NOT_FOUND;
#else
    return control_task_msg_unsubscribe(
        CONTROL_TASK_MSG_CODE_FRM_BLE,
        model_id,
        (control_task_msg_handle_t)cb);
    return ESP_OK;
#endif /* CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE */
}

/**
 * @brief Initialize the production generic server.
 *
 * This function sets up the necessary configurations and initializes the
 * production generic server for the BLE mesh node.
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_FAIL: Failed to initialize the server
 */
esp_err_t prod_gen_srv_init(void)
{
    if (prod_server_init == PROD_SERVER_INIT_MAGIC_NO)
        return ESP_OK;
    prod_server_init = PROD_SERVER_INIT_MAGIC_NO;
#if !CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE
    prod_server_mutex = xSemaphoreCreateMutex();
    if (prod_server_mutex == NULL)
    {
        return ESP_ERR_NO_MEM;
    }
#endif /* CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE */
    return esp_ble_mesh_register_generic_server_callback((esp_ble_mesh_generic_server_cb_t)&prod_ble_mesh_generic_server_cb);
}
