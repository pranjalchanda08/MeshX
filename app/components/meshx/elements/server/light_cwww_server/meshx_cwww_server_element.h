/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_cwww_server_element.h
 * @brief Header file for CWWW Server Model
 *
 * This file contains the definitions and function prototypes for the CWWW Server Model.
 * It includes the necessary includes, macros, and data structures required for the model.
 *
 * @author Pranjal Chanda
 */

#ifndef __CWWW_SERVER_ELEMENT_H__
#define __CWWW_SERVER_ELEMENT_H__

#include <string.h>
#include <app_common.h>
#include <meshx_onoff_server.h>
#include <meshx_light_ctl_srv.h>

#define CWWW_SERVER_ELEMENT_NOS_DEF 1

#ifndef CONFIG_LIGHT_CWWW_SRV_COUNT
#define CONFIG_LIGHT_CWWW_SRV_COUNT CWWW_SERVER_ELEMENT_NOS_DEF
#endif

#define CWWW_SRV_MODEL_SIG_CNT CWWW_SIG_ID_MAX // No of SIG models in a cwww model element
#define CWWW_SRV_MODEL_VEN_CNT 0               // No of VEN models in a cwww model element

/**
 * @brief Enumeration of CW-WW SIG model IDs.
 */
typedef enum
{
    CWWW_SIG_ONOFF_MODEL_ID, /**< On/Off model ID */
    CWWW_SIG_L_CTL_MODEL_ID, /**< Light CTL model ID */
    CWWW_SIG_ID_MAX          /**< Maximum number of model IDs */
} cwww_sig_id_t;

/**
 * @brief Structure to hold the context of the cwww client.
 */
typedef struct cwww_srv_ctx
{
    uint8_t tid;                                /**< Transaction ID */
    uint16_t app_id;                            /**< Application ID */
    uint16_t pub_addr;                          /**< Publish address */
    meshx_on_off_srv_state_t state;             /**< State of the cwww client */
    meshx_on_off_srv_state_t prev_state;        /**< State of the cwww client */
    meshx_light_ctl_srv_state_t ctl_state;      /**< State of the cwww client */
    meshx_light_ctl_srv_state_t prev_ctl_state; /**< State of the cwww client */
} meshx_cwww_server_ctx_t;

/**
 * @brief Structure to manage CWWW element initialization.
 */
typedef struct meshx_cwww_element
{
    meshx_cwww_server_ctx_t *srv_ctx;            /**< Context of the relay server */
    meshx_ctl_server_model_t *ctl_srv_model;     /**< CTL Server model */
    meshx_onoff_server_model_t *onoff_srv_model; /**< On Off Server model */
} meshx_cwww_element_t;

/**
 * @brief Structure representing a CW-WW element in the BLE mesh network.
 *
 * This structure contains all the necessary context and configuration for
 * controlling a CW-WW (Cool White - Warm White) light element in a BLE mesh network.
 */
typedef struct cwww_element
{
    size_t element_cnt;            /**< Number of relay elements */
    size_t element_id_end;         /**< Ending ID of the element */
    size_t element_id_start;       /**< Starting ID of the element */
    meshx_cwww_element_t *el_list; /**< Pointer to element list */
} meshx_cwww_elements_ctrl_t;

/**
 * @brief Create Dynamic CWWW Server Model Elements
 *
 * This function creates dynamic CWWW server model elements for the given device structure.
 *
 * @param[in] pdev          Pointer to device structure
 * @param[in] element_cnt   Maximum number of CWWW server models
 *
 * @return meshx_err_t Returns MESHX_SUCCESS on success or an error code on failure
 */
meshx_err_t meshx_create_cwww_elements(dev_struct_t *pdev, uint16_t element_cnt);

#endif /*__CWWW_SERVER_ELEMENT_H__*/
