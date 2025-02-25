/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_relay_server_element.h
 * @brief Header file for the Relay Server Model.
 *
 * This file contains the definitions and function prototypes for the Relay Server Model.
 * The Relay Server Model is responsible for managing the relay elements and their states.
 *
 *
 */
#ifndef __RELAY_SERVER_MODEL_H__
#define __RELAY_SERVER_MODEL_H__

#include "app_common.h"
#include <meshx_onoff_server.h>

#define RELAY_SERVER_ELEMENT_NOS_DEF 1

#ifndef CONFIG_RELAY_SERVER_COUNT
#define CONFIG_RELAY_SERVER_COUNT RELAY_SERVER_ELEMENT_NOS_DEF
#endif

#define RELAY_SRV_MODEL_SIG_CNT RELAY_SIG_MAX_ID    // No of SIG models in a relay model element
#define RELAY_SRV_MODEL_VEN_CNT 0                   // No of VEN models in a relay model element

typedef enum{
    RELAY_SIG_ONOFF_MODEL_ID,
    RELAY_SIG_MAX_ID,
}relay_sig_id_t;

typedef struct meshx_gen_ctx
{
    uint8_t state;
    uint8_t tid;
    uint16_t pub_addr;
    uint16_t app_id;
}relay_srv_model_ctx_t;

typedef struct relay_element
{
    size_t element_cnt;
    size_t element_id_end;
    size_t element_id_start;
    relay_srv_model_ctx_t *meshx_gen_ctx;
    esp_ble_mesh_model_t **relay_server_sig_model_list;
    esp_ble_mesh_model_pub_t *relay_server_pub_list;
    esp_ble_mesh_gen_onoff_srv_t *relay_server_onoff_gen_list;
} relay_elements_t;

/**
 * @brief Create Dynamic Relay Model Elements
 *
 * @param[in] pdev    Pointer to device structure
 * @param[in] element_cnt Maximum number of relay models
 *
 * @return esp_err_t
 */
esp_err_t create_relay_elements(dev_struct_t *pdev, uint16_t element_cnt);

#endif /*__RELAY_SERVER_MODEL_H__*/
