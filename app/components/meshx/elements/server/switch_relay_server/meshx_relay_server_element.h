/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_relay_server_element.h
 * @brief Header file for the Relay Server Model.
 *
 * This file contains the definitions and function prototypes for the Relay Server Model.
 * The Relay Server Model is responsible for managing the relay elements and their states.
 *
 * @author Pranjal Chanda
 */
#ifndef __RELAY_SERVER_MODEL_H__
#define __RELAY_SERVER_MODEL_H__

#include "app_common.h"
#include "meshx_onoff_server.h"

#define RELAY_SERVER_ELEMENT_NOS_DEF 1

#ifndef CONFIG_RELAY_SERVER_COUNT
#define CONFIG_RELAY_SERVER_COUNT RELAY_SERVER_ELEMENT_NOS_DEF
#endif

#define RELAY_SRV_MODEL_SIG_CNT RELAY_SIG_MAX_ID // No of SIG models in a relay model element
#define RELAY_SRV_MODEL_VEN_CNT 0                // No of VEN models in a relay model element

typedef enum
{
    RELAY_SIG_ONOFF_MODEL_ID,
    RELAY_SIG_MAX_ID,
} relay_sig_id_t;

/**
 * @brief Structure to hold the relay server save restore context
 */
typedef struct meshx_relay_srv_model_ctx
{
    meshx_on_off_srv_state_t state; /**< On/Off state */
    uint8_t tid;                    /**< Transaction ID */
    uint16_t pub_addr;              /**< Publish address */
    uint16_t app_id;                /**< Application ID */
} meshx_relay_srv_model_ctx_t;

/**
 * @brief Structure to manage relay element models
 */
typedef struct meshx_relay_element
{
    meshx_relay_srv_model_ctx_t *srv_ctx;                      /**< Context of the relay server */
    MESHX_MODEL relay_srv_model_list[RELAY_SRV_MODEL_SIG_CNT]; /**< List of Relay Server SIG Models */
    meshx_onoff_server_model_t *onoff_srv_model;               /**< On Off Server model */
} meshx_relay_element_t;

/**
 * @brief Structure to manage relay element initialization.
 */
typedef struct meshx_relay_element_ctrl
{
    size_t element_cnt;             /**< Number of relay elements */
    size_t element_id_end;          /**< Ending ID of the element */
    size_t element_id_start;        /**< Starting ID of the element */
    meshx_relay_element_t *el_list; /**< Pointer to element list */
} meshx_relay_element_ctrl_t;

/**
 * @brief Create Dynamic Relay Model Elements
 *
 * @param[in] pdev          Pointer to device structure
 * @param[in] element_cnt   Maximum number of relay models
 *
 * @return meshx_err_t
 */
meshx_err_t meshx_create_relay_elements(dev_struct_t *pdev, uint16_t element_cnt);

#endif /*__RELAY_SERVER_MODEL_H__*/
