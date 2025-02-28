/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_gen_server.h
 * @brief Header file for the generic server model in the BLE mesh node application.
 *
 * This file contains the function declarations and data structures for registering,
 * deregistering, and initializing the generic server model callbacks in the BLE mesh node application.
 *
 *
 */

#ifndef __MESHX_GEN_SERVER_H__
#define __MESHX_GEN_SERVER_H__

#include <server_common.h>
#include <esp_ble_mesh_generic_model_api.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sys/queue.h"
#include "meshx_control_task.h"

#if !CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE
/**
 * @typedef meshx_server_cb
 * @brief Callback function type for the generic server model.
 *
 * @param param Pointer to the callback parameter structure.
 * @return ESP_OK on success, or an appropriate error code on failure.
 */
typedef esp_err_t (* meshx_server_cb) (esp_ble_mesh_generic_server_cb_param_t *param);
#else

typedef control_task_msg_handle_t meshx_server_cb;

#endif /* CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE */
/**
 * @struct meshx_server_cb_reg
 * @brief Structure to register a callback for a specific model ID.
 */
typedef struct meshx_server_cb_reg
{
    uint32_t model_id;                      /**< Model ID */
    meshx_server_cb cb;                     /**< Callback function */
    SLIST_ENTRY(meshx_server_cb_reg) next;  /**< Pointer to the next callback registration */
} meshx_server_cb_reg_t;

/**
 * @struct meshx_server_cb_reg_head
 * @brief Head of the list of registered callbacks.
 */
SLIST_HEAD(meshx_server_cb_reg_head, meshx_server_cb_reg);

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
 *     - ESP_OK: Callback registered successfully.
 *     - ESP_ERR_INVALID_ARG: Invalid arguments.
 *     - ESP_FAIL: Failed to register the callback.
 */
esp_err_t meshx_gen_srv_reg_cb(uint32_t model_id, meshx_server_cb cb);

/**
 * @brief Callback function to deregister a generic server model.
 *
 * This function is called to deregister a generic server model identified by the given model ID.
 *
 * @param[in] model_id  The ID of the model to be deregistered.
 * @param[in] cb        The callback function to be deregistered.
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid argument
 *     - ESP_FAIL: Other failures
 */
esp_err_t meshx_gen_srv_dereg_cb(uint32_t model_id, meshx_server_cb cb);

/**
 * @brief Initialize the generic server model.
 *
 * @return ESP_OK on success, or an appropriate error code on failure.
 */
esp_err_t meshx_gen_srv_init(void);

#endif /* __MESHX_GEN_SERVER_H__ */
