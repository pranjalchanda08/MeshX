/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_gen_server.c
 * @brief Implementation of the BLE Mesh Generic Server for the meshxuct.
 *
 * This file contains the implementation of the BLE Mesh Generic Server
 * for handling various server events and registering callbacks.
 *
 *
 */

#include "meshx_gen_server.h"

#define TAG __func__

#define MESHX_SERVER_INIT_MAGIC_NO 0x1121

/**
 * @brief String representation of the server state change events.
 */
static const char *server_state_str[] = {
    [ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT] = "SRV_STATE_CH",
    [ESP_BLE_MESH_GENERIC_SERVER_RECV_GET_MSG_EVT] = "SRV_RECV_GET",
    [ESP_BLE_MESH_GENERIC_SERVER_RECV_SET_MSG_EVT] = "SRV_RECV_SET"
};

static uint16_t meshx_server_init = 0;

#if !CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE
static struct meshx_server_cb_reg_head meshx_server_cb_reg_list = SLIST_HEAD_INITIALIZER(meshx_server_cb_reg_list);
static SemaphoreHandle_t meshx_server_mutex;
#endif /* CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE */

/**
 * @brief Callback function for BLE Mesh Generic Server events.
 *
 * This function is called whenever a BLE Mesh Generic Server event occurs.
 *
 * @param event The event type for the BLE Mesh Generic Server.
 * @param param Parameters associated with the event.
 */
static void meshx_ble_mesh_generic_server_cb(esp_ble_mesh_generic_server_cb_event_t event,
                                            const esp_ble_mesh_generic_server_cb_param_t *param)
{
    ESP_LOGD(TAG, "%s, op|src|dst:%04" PRIx32 "|%04x|%04x",
             server_state_str[event], param->ctx.recv_op, param->ctx.addr, param->ctx.recv_dst);

    if (event != ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT)
        return;
#if !CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE
    xSemaphoreTake(meshx_server_mutex, portMAX_DELAY);
    struct meshx_server_cb_reg *item;
    SLIST_FOREACH(item, &meshx_server_cb_reg_list, next)
    {
        if ((item->model_id == param->model->model_id || item->model_id == param->model->vnd.model_id) && item->cb)
        {
            /* Dispatch callback to generic model type */
            item->cb(param);
            break;
        }
    }
    xSemaphoreGive(meshx_server_mutex);
#else
    meshx_err_t err = control_task_msg_publish(
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
 * @brief Register a callback function for the meshxuction server model.
 *
 * This function registers a callback function that will be called when
 * specific events related to the meshxuction server model occur.
 *
 * @param[in] model_id  The ID of the model for which the callback is being registered.
 * @param[in] cb        The callback function to be registered.
 *
 * @return
 *     - MESHX_SUCCESS: Callback registered successfully.
 *     - MESHX_INVALID_ARG: Invalid arguments.
 *     - ESP_FAIL: Failed to register the callback.
 */
meshx_err_t meshx_gen_srv_reg_cb(uint32_t model_id, meshx_server_cb cb)
{
    meshx_err_t err = MESHX_SUCCESS;
#if !CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE
    xSemaphoreTake(meshx_server_mutex, portMAX_DELAY);
    struct meshx_server_cb_reg *item;
    SLIST_FOREACH(item, &meshx_server_cb_reg_list, next)
    {
        if (item->model_id == model_id)
        {
            /* If already registered over-write */
            item->cb = cb;
            xSemaphoreGive(meshx_server_mutex);
            return MESHX_SUCCESS;
        }
    }

    item = malloc(sizeof(struct meshx_server_cb_reg));
    if (item == NULL)
    {
        xSemaphoreGive(meshx_server_mutex);
        return MESHX_NO_MEM;
    }

    item->model_id = model_id;
    item->cb = cb;
    SLIST_INSERT_HEAD(&meshx_server_cb_reg_list, item, next);
    xSemaphoreGive(meshx_server_mutex);

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
 * @param[in] model_id  The ID of the model to be deregistered.
 * @param[in] cb        The callback function to be deregistered.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_INVALID_ARG: Invalid argument
 *     - ESP_FAIL: Other failures
 */
meshx_err_t meshx_gen_srv_dereg_cb(uint32_t model_id, meshx_server_cb cb)
{
#if !CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE
    xSemaphoreTake(meshx_server_mutex, portMAX_DELAY);
    struct meshx_server_cb_reg *item;
    struct meshx_server_cb_reg *tmp;
    SLIST_FOREACH_SAFE(item, &meshx_server_cb_reg_list, next, tmp)
    {
        if (item->model_id == model_id && item->cb == cb)
        {
            SLIST_REMOVE(&meshx_server_cb_reg_list, item, meshx_server_cb_reg, next);
            free(item);
            xSemaphoreGive(meshx_server_mutex);
            return MESHX_SUCCESS;
        }
    }
    xSemaphoreGive(meshx_server_mutex);
    return MESHX_NOT_FOUND;
#else
    return control_task_msg_unsubscribe(
        CONTROL_TASK_MSG_CODE_FRM_BLE,
        model_id,
        (control_task_msg_handle_t)cb);
    return MESHX_SUCCESS;
#endif /* CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE */
}

/**
 * @brief Initialize the meshxuction generic server.
 *
 * This function sets up the necessary configurations and initializes the
 * meshxuction generic server for the BLE mesh node.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - ESP_FAIL: Failed to initialize the server
 */
meshx_err_t meshx_gen_srv_init(void)
{
    if (meshx_server_init == MESHX_SERVER_INIT_MAGIC_NO)
        return MESHX_SUCCESS;
    meshx_server_init = MESHX_SERVER_INIT_MAGIC_NO;
#if !CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE
    meshx_server_mutex = xSemaphoreCreateMutex();
    if (meshx_server_mutex == NULL)
    {
        return MESHX_NO_MEM;
    }
#endif /* CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE */
    return esp_ble_mesh_register_generic_server_callback((esp_ble_mesh_generic_server_cb_t)&meshx_ble_mesh_generic_server_cb);
}
