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
typedef struct meshx_gen_ctx
{
    uint8_t state;     /**< On/Off state */
    uint8_t tid;       /**< Transaction ID */
    uint16_t pub_addr; /**< Publish address */
    uint16_t app_id;   /**< Application ID */
} relay_srv_model_ctx_t;

/**
 * @brief Structure to manage relay element initialization.
 */
typedef struct relay_element
{
    size_t element_cnt;                                        /**< Number of relay elements */
    size_t element_id_end;                                     /**< Ending ID of the element */
    size_t element_id_start;                                   /**< Starting ID of the element */
    relay_srv_model_ctx_t *meshx_gen_ctx;                      /**< Context of the relay server */
    MESHX_MODEL **relay_server_sig_model_list;        /**< List of relay server SIG model structures */
    MESHX_MODEL_PUB *relay_server_pub_list;           /**< List of relay server publication structures */
    MESHX_GEN_ONOFF_SRV *relay_server_onoff_gen_list; /**< List of relay server on/off generic structures */
} relay_elements_t;

/**
 * @brief Create Dynamic Relay Model Elements
 *
 * @param[in] pdev          Pointer to device structure
 * @param[in] element_cnt   Maximum number of relay models
 *
 * @return meshx_err_t
 */
meshx_err_t create_relay_elements(dev_struct_t *pdev, uint16_t element_cnt);

#endif /*__RELAY_SERVER_MODEL_H__*/
