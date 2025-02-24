/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file cwww_server_element.h
 * @brief Header file for CWWW Server Model
 *
 * This file contains the definitions and function prototypes for the CWWW Server Model.
 * It includes the necessary includes, macros, and data structures required for the model.
 *
 *
 */

#ifndef __CWWW_SERVER_ELEMENT_H__
#define __CWWW_SERVER_ELEMENT_H__

#include <string.h>
#include <app_common.h>
#include <prod_onoff_server.h>
#include <prod_light_ctl_srv.h>

#define CWWW_SERVER_ELEMENT_NOS_DEF 1

#ifndef CONFIG_LIGHT_CWWW_SRV_COUNT
#define CONFIG_LIGHT_CWWW_SRV_COUNT CWWW_SERVER_ELEMENT_NOS_DEF
#endif

#define CWWW_SRV_MODEL_SIG_CNT CWWW_SIG_ID_MAX   // No of SIG models in a cwww model element
#define CWWW_SRV_MODEL_VEN_CNT 0                    // No of VEN models in a cwww model element

/**
 * @brief Enumeration of CW-WW SIG model IDs.
 */
typedef enum{
    CWWW_SIG_ONOFF_MODEL_ID, /**< On/Off model ID */
    CWWW_SIG_L_CTL_MODEL_ID, /**< Light CTL model ID */
    CWWW_SIG_ID_MAX          /**< Maximum number of model IDs */
} cwww_sig_id_t;

/**
 * @brief Context structure for the Light CTL Client model.
 */
typedef struct light_ctl_cli_ctx
{
    uint8_t tid;                  /**< Transaction ID */
    uint8_t state;                /**< Current state */
    uint16_t lightness;           /**< Lightness level */
    uint16_t temperature;         /**< Color temperature */
    uint16_t delta_uv;            /**< Delta UV value */
    uint16_t temp_range_max;      /**< Maximum temperature range */
    uint16_t temp_range_min;      /**< Minimum temperature range */
    uint16_t pub_addr;            /**< Publication address */
    uint16_t app_id;              /**< Application ID */
} cwww_server_ctx_t;

/**
 * @brief Structure representing a CW-WW element in the BLE mesh network.
 *
 * This structure contains all the necessary context and configuration for
 * controlling a CW-WW (Cool White - Warm White) light element in a BLE mesh network.
 */
typedef struct cwww_element
{
    size_t element_cnt;
    size_t element_id_end;
    size_t element_id_start;
    cwww_server_ctx_t *cwww_server_ctx;
    esp_ble_mesh_model_t **cwww_server_sig_model_list;
    esp_ble_mesh_model_pub_t **cwww_server_pub_list;
    esp_ble_mesh_gen_onoff_srv_t *cwww_server_onoff_gen_list;
    esp_ble_mesh_light_ctl_srv_t *cwww_server_light_ctl_list;
    esp_ble_mesh_light_ctl_state_t *cwww_light_ctl_state;
} cwww_elements_t;

/**
 * @brief Create Dynamic CWWW Server Model Elements
 *
 * This function creates dynamic CWWW server model elements for the given device structure.
 *
 * @param[in] pdev Pointer to device structure
 * @param[in] element_cnt Maximum number of CWWW server models
 *
 * @return esp_err_t Returns ESP_OK on success or an error code on failure
 */
esp_err_t create_cwww_elements(dev_struct_t *pdev, uint16_t element_cnt);

#endif /*__CWWW_SERVER_ELEMENT_H__*/
