/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_config_server.h
 * @brief Header file for the meshxuction configuration server model.
 *
 * This file contains the definitions and function declarations for the
 * meshxuction configuration server model used in the ESP32 BLE Mesh Node.
 *
 *
 */

#ifndef __MESHX_CONFIG_SERVER__
#define __MESHX_CONFIG_SERVER__

#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_generic_model_api.h"
#include "esp_ble_mesh_local_data_operation_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sys/queue.h"

#define MESHX_CONFIG_SERVER_INSTANCE g_meshx_config_server

/**
 * @brief Enumeration of configuration events.
 */
typedef enum
{
    CONFIG_EVT_MODEL_APP_KEY_ADD        = BIT0, /**< Event for adding an application key. */
    CONFIG_EVT_MODEL_APP_KEY_DEL        = BIT1, /**< Event for deleting an application key. */
    CONFIG_EVT_MODEL_APP_KEY_BIND       = BIT2, /**< Event for binding an application key. */
    CONFIG_EVT_MODEL_APP_KEY_UNBIND     = BIT3, /**< Event for unbinding an application key. */
    CONFIG_EVT_MODEL_SUB_ADD            = BIT4, /**< Event for adding a subscription. */
    CONFIG_EVT_MODEL_SUB_DEL            = BIT5, /**< Event for deleting a subscription. */
    CONFIG_EVT_MODEL_PUB_ADD            = BIT6, /**< Event for adding a publication. */
    CONFIG_EVT_MODEL_PUB_DEL            = BIT7, /**< Event for deleting a publication. */
    CONFIG_EVT_MODEL_NET_KEY_ADD        = BIT8, /**< Event for adding a network key. */
    CONFIG_EVT_MODEL_NET_KEY_DEL        = BIT9, /**< Event for deleting a network key. */
    CONFIG_EVT_ALL                      = 0xFFFFFFFF, /**< Event for all configuration events. */
} config_evt_t;

/**
 * @brief Callback function type for configuration server events.
 *
 * @param param Pointer to the callback parameter structure.
 * @param evt The configuration event that occurred.
 */
typedef void (*config_srv_cb)(const esp_ble_mesh_cfg_server_cb_param_t *param, config_evt_t evt);

extern esp_ble_mesh_cfg_srv_t g_meshx_config_server;

/**
 * @brief Initialize the meshxuction configuration server.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t meshx_init_config_server();

/**
 * @brief Registers a configuration server callback for specific events.
 *
 * Adds a new callback registration to the linked list for dispatching events.
 *
 * @param[in] cb Callback function to register.
 * @param[in] config_evt_bmap Bitmap of events the callback is interested in.
 *
 * @return ESP_OK on success, an error code otherwise.
 */
esp_err_t meshx_config_server_cb_reg(config_srv_cb cb, uint32_t config_evt_bmap);

#endif /* __MESHX_CONFIG_SERVER__ */
