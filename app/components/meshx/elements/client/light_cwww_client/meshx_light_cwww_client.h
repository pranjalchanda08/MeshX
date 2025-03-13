/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_light_cwww_client.h
 * @brief Implementation of the CW-WW (Cool White - Warm White) client model for BLE Mesh.
 *
 * @author Pranjal Chanda
 */
#pragma once

#include "app_common.h"
#include "meshx_control_task.h"
#include "meshx_onoff_client.h"
#include "meshx_light_ctl_client.h"

/**
 * @def CWWW_CLIENT_ELEMENT_NOS_DEF
 * @brief Defines the number of CW-WW client elements.
 *
 * This macro defines the number of CW-WW (Cool White - Warm White) client elements
 * used in the MeshX application.
 */
#define CWWW_CLIENT_ELEMENT_NOS_DEF 3

/**
 * @def CONFIG_LIGHT_CWWW_CLIENT_COUNT
 * @brief Number of CW-WW client elements.
 *
 * This macro defines the number of CW-WW client elements. If it is not defined,
 * it will default to CWWW_CLIENT_ELEMENT_NOS_DEF.
 */
#ifndef CONFIG_LIGHT_CWWW_CLIENT_COUNT
#define CONFIG_LIGHT_CWWW_CLIENT_COUNT CWWW_CLIENT_ELEMENT_NOS_DEF
#endif

/**
 * @def CWWW_CLI_MODEL_SIG_CNT
 * @brief Number of SIG models in a CW-WW model element.
 */
#define CWWW_CLI_MODEL_SIG_CNT CWWW_CLI_SIG_ID_MAX // No of SIG models in a cwww model element

/**
 * @def CWWW_CLI_MODEL_VEN_CNT
 * @brief Number of Vendor models in a CW-WW model element.
 */
#define CWWW_CLI_MODEL_VEN_CNT 0 // No of VEN models in a cwww model element

/**
 * @def CWWW_CLI_MSG_SET
 * @brief Message type for setting CW-WW client state.
 */
#define CWWW_CLI_MSG_SET 0

/**
 * @def CWWW_CLI_MSG_GET
 * @brief Message type for getting CW-WW client state.
 */
#define CWWW_CLI_MSG_GET 1

/**
 * @def CWWW_CLI_MSG_ACK
 * @brief Acknowledgment message type.
 */
#define CWWW_CLI_MSG_ACK 1

/**
 * @def CWWW_CLI_MSG_NO_ACK
 * @brief Non-acknowledgment message type.
 */
#define CWWW_CLI_MSG_NO_ACK 0

#define CWWW_ARG_BMAP_ONOFF_SET BIT0
#define CWWW_ARG_BMAP_LIGHTNESS_SET BIT1
#define CWWW_ARG_BMAP_TEMPERATURE_SET BIT2
#define CWWW_ARG_BMAP_DELTA_UV_SET BIT3
#define CWWW_ARG_BMAP_TEMPERATURE_RANGE_SET_MIN BIT4
#define CWWW_ARG_BMAP_TEMPERATURE_RANGE_SET_MAX BIT5

/**
 * @def CWWW_ARG_BMAP_TEMPERATURE_RANGE_SET
 * @brief Argument bitmap for setting the temperature range.
 */
#define CWWW_ARG_BMAP_TEMPERATURE_RANGE_SET (CWWW_ARG_BMAP_TEMPERATURE_RANGE_SET_MIN | CWWW_ARG_BMAP_TEMPERATURE_RANGE_SET_MAX)

/**
 * @def CWWW_ARG_BMAP_CTL_SET
 * @brief Argument bitmap for setting the CW-WW client control state.
 */
#define CWWW_ARG_BMAP_CTL_SET (CWWW_ARG_BMAP_LIGHTNESS_SET | CWWW_ARG_BMAP_TEMPERATURE_SET | CWWW_ARG_BMAP_DELTA_UV_SET)

/**
 * @def CWWW_ARG_BMAP_ALL
 * @brief Argument bitmap for setting all CW-WW client states.
 */
#define CWWW_ARG_BMAP_ALL (CWWW_ARG_BMAP_ONOFF_SET | CWWW_ARG_BMAP_CTL_SET | CWWW_ARG_BMAP_TEMPERATURE_RANGE_SET)

/**
 * @brief Enumeration of CW-WW SIG model IDs.
 */
typedef enum
{
    CWWW_CLI_SIG_ONOFF_MODEL_ID, /**< On/Off model ID */
    CWWW_CLI_SIG_L_CTL_MODEL_ID, /**< Light CTL model ID */
    CWWW_CLI_SIG_ID_MAX          /**< Maximum number of model IDs */
} cwww_cli_sig_id_t;

/**
 * @brief Structure to hold the CW-WW client on/off state.
 */
typedef struct cwww_cli_on_off_state
{
    uint8_t on_off; /**< On/Off state */
} cwww_cli_on_off_state_t;

/**
 * @brief Structure to hold the CW-WW client control state.
 */
typedef struct cwww_cli_ctl_state
{
    uint16_t delta_uv;       /**< Delta UV value */
    uint16_t lightness;      /**< Lightness level */
    uint16_t temperature;    /**< Color temperature */
    uint16_t temp_range_max; /**< Maximum Temperature range */
    uint16_t temp_range_min; /**< Minimum Temperature range */
    uint16_t temp_def;       /**< Default temperature */
    uint16_t lightness_def;  /**< Default lightness */
    uint16_t delta_uv_def;   /**< Default delta UV */
} cwww_cli_ctl_state_t;

/**
 * @brief Structure to hold the context of the cwww client.
 */
typedef struct cwww_cli_ctx
{
    uint8_t tid;                         /**< Transaction ID */
    cwww_cli_on_off_state_t state;       /**< State of the cwww client */
    cwww_cli_on_off_state_t prev_state;  /**< State of the cwww client */
    cwww_cli_ctl_state_t ctl_state;      /**< State of the cwww client */
    cwww_cli_ctl_state_t prev_ctl_state; /**< State of the cwww client */
    uint16_t app_id;                     /**< Application ID */
    uint16_t pub_addr;                   /**< Publish address */
} cwww_cli_ctx_t;

/**
 * @brief Structure to hold the cwww client message.
 */
typedef struct cwww_client_msg
{
    uint8_t ack;             /**< Acknowledgment flag */
    uint8_t arg_bmap;        /**< Argument bitmap */
    uint8_t set_get;         /**< Set/Get flag */
    uint16_t element_id;     /**< Element ID */
    uint16_t temperature;    /**< Temperature */
    uint16_t lightness;      /**< Lightness */
    uint16_t delta_uv;       /**< Delta UV */
    uint16_t temp_range_max; /**< Maximum lightness range */
    uint16_t temp_range_min; /**< Minimum lightness range */
} cwww_client_msg_t;

/**
 * @brief Structure to hold the cwww client elements.
 */
typedef struct cwww_client_element
{
    size_t element_cnt;                             /**< Number of elements */
    size_t element_id_end;                          /**< Ending ID of the element */
    size_t element_id_start;                        /**< Starting ID of the element */
    size_t element_model_init;                      /**< Initialization status of the element model */
    cwww_cli_ctx_t *cwww_cli_ctx;                   /**< Context of the cwww client */
    esp_ble_mesh_client_t **cwww_cli_list;          /**< List of cwww client structures */
    MESHX_MODEL_PUB **cwww_cli_pub_list;   /**< List of cwww client publication structures */
    MESHX_MODEL **cwww_cli_sig_model_list; /**< List of cwww client SIG model structures */
} cwww_client_elements_t;

/**
 * @brief Create Dynamic Relay Model Elements
 *
 * @param[in] pdev          Pointer to device structure
 * @param[in] element_cnt   Maximum number of relay models
 *
 * @return meshx_err_t
 */
meshx_err_t create_cwww_client_elements(dev_struct_t *pdev, uint16_t element_cnt);

/**
 * @brief Send a CW/WW (Cool White/Warm White) message over BLE Mesh.
 *
 * @param[in] pdev          Pointer to the device structure.
 * @param[in] model_id      Model ID of the CW/WW client.
 * @param[in] element_id    Element ID to which the message is addressed.
 * @param[in] set_get       Flag indicating whether the message is a set (1) or get (0) operation.
 * @param[in] is_range      Flag indicating whether the message is a temperature range set (1) or not (0).
 * @param[in] ack           Flag indicating whether the message requires an acknowledgment (1) or not (0).
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_INVALID_ARG: Invalid argument
 *     - MESHX_FAIL: Sending message failed
 */
meshx_err_t ble_mesh_send_cwww_msg(dev_struct_t *pdev, cwww_cli_sig_id_t model_id, uint16_t element_id, uint8_t set_get, uint8_t is_range, uint8_t ack);
