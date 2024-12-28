/**
 * @file prod_light_server.h
 * @brief Header file for the production lighting server.
 *
 * This file contains the declarations and definitions for the production
 * lighting server, including callback registration and initialization functions.
 */

#ifndef __PROD_LIGHT_SERVER_H__
#define __PROD_LIGHT_SERVER_H__

#include "server_common.h"
#include "esp_ble_mesh_lighting_model_api.h"
#include "sys/queue.h"

#ifndef CONFIG_MAX_PROD_LIGHTING_SRV_CB
#define CONFIG_MAX_PROD_LIGHTING_SRV_CB   3
#endif

/**
 * @brief Type definition for the production lighting server callback function.
 *
 * @param param Pointer to the callback parameter structure.
 *
 * @return ESP_OK on success, or an appropriate error code on failure.
 */
typedef esp_err_t (* prod_lighting_server_cb) (esp_ble_mesh_lighting_server_cb_param_t *param);

/**
 * @brief Structure to register a production lighting server callback.
 */
typedef struct prod_lighting_server_cb_reg
{
    uint32_t model_id; /**< Model ID for the lighting server. */
    prod_lighting_server_cb cb; /**< Callback function for the lighting server. */
    SLIST_ENTRY(prod_lighting_server_cb_reg) entries; /**< Singly linked list entry. */
} prod_lighting_server_cb_reg_t;
/**
 * @brief Head for the singly linked list of production lighting server callbacks.
 */
SLIST_HEAD(prod_lighting_server_cb_list, prod_lighting_server_cb_reg);

/**
 * @brief Register a callback function for a specific lighting server model.
 *
 * @param model_id Model ID for the lighting server.
 * @param cb Callback function to be registered.
 *
 * @return ESP_OK on success, or an appropriate error code on failure.
 */
esp_err_t prod_lighting_reg_cb(uint32_t model_id, prod_lighting_server_cb cb);

/**
 * @brief Initialize the production lighting server.
 *
 * @return ESP_OK on success, or an appropriate error code on failure.
 */
esp_err_t prod_lighting_srv_init(void);

#endif /* __PROD_LIGHT_SERVER_H__ */
