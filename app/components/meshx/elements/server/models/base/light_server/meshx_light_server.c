/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_light_server.c
 * @brief Implementation of the BLE Mesh Lighting Server for the ESP32.
 *
 * This file contains the implementation of the BLE Mesh Lighting Server,
 * including initialization, event handling, and callback registration.
 *
 * @author Pranjal Chanda
 */

#include "meshx_light_server.h"
#include "meshx_gen_server.h"

#define TAG __func__

#define MESHX_SERVER_INIT_MAGIC_NO   0x2483

static uint16_t meshx_lighting_server_init = 0;
#if !CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE
static struct meshx_lighting_server_cb_list meshx_lighting_server_cb_reg_table = SLIST_HEAD_INITIALIZER(meshx_lighting_server_cb_reg_table);
static SemaphoreHandle_t meshx_lighting_server_mutex;
#endif /* !CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE */

/**
 * @brief Callback function for BLE Mesh Lightness Server events.
 *
 * This function is called whenever a BLE Mesh Lightness Server event occurs.
 *
 * @param[in] event The event type for the BLE Mesh Lightness Server.
 * @param[in] param Parameters associated with the event.
 */
static void meshx_ble_lightness_server_cb(esp_ble_mesh_lighting_server_cb_event_t event,
                                         const esp_ble_mesh_lighting_server_cb_param_t *param)
{
    ESP_LOGD(TAG, "evt|op|src|dst: %02x|%04x|%04x|%04x|%04x",
            event, (unsigned)param->ctx.recv_op, param->ctx.addr, param->ctx.recv_dst,
            param->model->model_id);

#if !CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE
    if (xSemaphoreTake(meshx_lighting_server_mutex, portMAX_DELAY) == pdTRUE) {
        meshx_lighting_server_cb_reg_t *entry;
        SLIST_FOREACH(entry, &meshx_lighting_server_cb_reg_table, entries) {
            if ((entry->model_id == param->model->model_id || entry->model_id == param->model->vnd.model_id) && entry->cb) {
                /* Dispatch callback to lighting model type */
                entry->cb(param);
            }
        }
        xSemaphoreGive(meshx_lighting_server_mutex);
    }
#else
    control_task_msg_publish(CONTROL_TASK_MSG_CODE_FRM_BLE, param->model->model_id, param, sizeof(esp_ble_mesh_lighting_server_cb_param_t));
#endif /* !CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE */
}

/**
 * @brief Register a callback function for the lighting server model.
 *
 * This function registers a callback function that will be called when
 * certain events occur in the lighting server model.
 *
 * @param[in] model_id  The ID of the lighting server model.
 * @param[in] cb        The callback function to register.
 *
 * @return
 *    - ESP_OK: Success
 *    - ESP_ERR_INVALID_ARG: Invalid argument
 *    - ESP_FAIL: Other failures
 */
esp_err_t meshx_lighting_reg_cb(uint32_t model_id, meshx_lighting_server_cb cb)
{
#if !CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE
    if (xSemaphoreTake(meshx_lighting_server_mutex, portMAX_DELAY) == pdTRUE) {
        meshx_lighting_server_cb_reg_t *entry;
        SLIST_FOREACH(entry, &meshx_lighting_server_cb_reg_table, entries) {
            if (entry->model_id == model_id) {
                /* If already registered, overwrite */
                entry->cb = cb;
                xSemaphoreGive(meshx_lighting_server_mutex);
                return ESP_OK;
            }
        }

        entry = malloc(sizeof(meshx_lighting_server_cb_reg_t));
        if (!entry) {
            ESP_LOGE(TAG, "No Memory left for meshx_lighting_server_cb_reg_table");
            xSemaphoreGive(meshx_lighting_server_mutex);
            return ESP_ERR_NO_MEM;
        }

        entry->model_id = model_id;
        entry->cb = cb;
        SLIST_INSERT_HEAD(&meshx_lighting_server_cb_reg_table, entry, entries);
        xSemaphoreGive(meshx_lighting_server_mutex);
    }
#else
    return control_task_msg_subscribe(CONTROL_TASK_MSG_CODE_FRM_BLE, model_id, (control_task_msg_handle_t)cb);
#endif /* !CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE */
    return ESP_OK;
}

/**
 * @brief Initialize the meshxuction lighting server.
 *
 * This function sets up the necessary configurations and initializes the
 * meshxuction lighting server for the BLE mesh node.
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_FAIL: Failed to initialize the lighting server
 */
esp_err_t meshx_lighting_srv_init(void)
{
    if (meshx_lighting_server_init == MESHX_SERVER_INIT_MAGIC_NO)
        return ESP_OK;
#if !CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE
    meshx_lighting_server_mutex = xSemaphoreCreateMutex();
    if (meshx_lighting_server_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_FAIL;
    }
#endif /* !CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE */
    meshx_lighting_server_init = MESHX_SERVER_INIT_MAGIC_NO;
    return esp_ble_mesh_register_lighting_server_callback((esp_ble_mesh_lighting_server_cb_t)&meshx_ble_lightness_server_cb);
}
