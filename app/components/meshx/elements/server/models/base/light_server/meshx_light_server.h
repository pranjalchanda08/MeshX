/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_light_server.h
 * @brief Header file for the production lighting server.
 *
 * This file contains the declarations and definitions for the production
 * lighting server, including callback registration and initialization functions.
 *
 * @author Pranjal Chanda
 */

#ifndef __MESHX_LIGHT_SERVER_H__
#define __MESHX_LIGHT_SERVER_H__

#include "server_common.h"
#include "esp_ble_mesh_lighting_model_api.h"
#include "sys/queue.h"
#include "meshx_control_task.h"

#ifndef CONFIG_MAX_MESHX_LIGHTING_SRV_CB
#define CONFIG_MAX_MESHX_LIGHTING_SRV_CB   3
#endif

#if !CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE

/**
 * @brief Type definition for the production lighting server callback function.
 *
 * @param param Pointer to the callback parameter structure.
 *
 * @return ESP_OK on success, or an appropriate error code on failure.
 */
typedef esp_err_t (* meshx_lighting_server_cb) (esp_ble_mesh_lighting_server_cb_param_t *param);
#else
typedef control_task_msg_handle_t meshx_lighting_server_cb;
#endif /* !CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE */

/**
 * @brief Structure to register a production lighting server callback.
 */
typedef struct meshx_lighting_server_cb_reg
{
    uint32_t model_id; /**< Model ID for the lighting server. */
    meshx_lighting_server_cb cb; /**< Callback function for the lighting server. */
    SLIST_ENTRY(meshx_lighting_server_cb_reg) entries; /**< Singly linked list entry. */
} meshx_lighting_server_cb_reg_t;
/**
 * @brief Head for the singly linked list of production lighting server callbacks.
 */
SLIST_HEAD(meshx_lighting_server_cb_list, meshx_lighting_server_cb_reg);

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
esp_err_t meshx_lighting_reg_cb(uint32_t model_id, meshx_lighting_server_cb cb);

/**
 * @brief Initialize the production lighting server.
 *
 * @return ESP_OK on success, or an appropriate error code on failure.
 */
esp_err_t meshx_lighting_srv_init(void);

#endif /* __MESHX_LIGHT_SERVER_H__ */
