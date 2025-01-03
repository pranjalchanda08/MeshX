#pragma once

#include "app_common.h"
#include "control_task.h"
#include "prod_onoff_client.h"
#include "light_ctl_client.h"

#define CWWW_CLIENT_ELEMENT_NOS_DEF 3

#ifndef CONFIG_LIGHT_CWWW_CLIENT_COUNT
#define CONFIG_LIGHT_CWWW_CLIENT_COUNT CWWW_CLIENT_ELEMENT_NOS_DEF
#endif

#define CWWW_CLI_MODEL_SIG_CNT CWWW_CLI_SIG_ID_MAX // No of SIG models in a cwww model element
#define CWWW_CLI_MODEL_VEN_CNT 0                   // No of VEN models in a cwww model element

#define CWWW_CLI_MSG_SET 0
#define CWWW_CLI_MSG_GET 1
#define CWWW_CLI_MSG_ACK 1
#define CWWW_CLI_MSG_NO_ACK 0

#define CWWW_ARG_BMAP_ONOFF_SET BIT0
#define CWWW_ARG_BMAP_LIGHTNESS_SET BIT1
#define CWWW_ARG_BMAP_TEMPERATURE_SET BIT2
#define CWWW_ARG_BMAP_DELTA_UV_SET BIT3
#define CWWW_ARG_BMAP_TEMPERATURE_RANGE_SET_MIN BIT4
#define CWWW_ARG_BMAP_TEMPERATURE_RANGE_SET_MAX BIT5

#define CWWW_ARG_BMAP_TEMPERATURE_RANGE_SET (CWWW_ARG_BMAP_TEMPERATURE_RANGE_SET_MIN | CWWW_ARG_BMAP_TEMPERATURE_RANGE_SET_MAX)
#define CWWW_ARG_BMAP_CTL_SET (CWWW_ARG_BMAP_LIGHTNESS_SET | CWWW_ARG_BMAP_TEMPERATURE_SET | CWWW_ARG_BMAP_DELTA_UV_SET)
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
 * @brief Structure to hold the context of the cwww client.
 */
typedef struct cwww_cli_ctx
{
    uint8_t tid;                  /**< Transaction ID */
    uint8_t state;                /**< State of the cwww client */
    uint16_t net_id;              /**< Network ID */
    uint16_t app_id;              /**< Application ID */
    uint16_t pub_addr;            /**< Publish address */
    uint16_t delta_uv;            /**< Delta UV value */
    uint16_t lightness;           /**< Lightness level */
    uint16_t temperature;         /**< Color temperature */
    uint16_t lightness_range_max; /**< Maximum lightness range */
    uint16_t lightness_range_min; /**< Minimum lightness range */
    uint16_t temp_def;            /**< Default temperature */
    uint16_t lightness_def;       /**< Default lightness */
    uint16_t dwlta_uv_def;        /**< Default delta UV */
} cwww_cli_ctx_t;

/**
 * @brief Structure to hold the cwww client message.
 */
typedef struct cwww_client_msg
{
    uint8_t ack;                  /**< Acknowledgment flag */
    uint8_t arg_bmap;             /**< Argument bitmap */
    uint8_t set_get;              /**< Set/Get flag */
    uint16_t element_id;          /**< Element ID */
    uint16_t temperature;         /**< Temperature */
    uint16_t lightness;           /**< Lightness */
    uint16_t delta_uv;            /**< Delta UV */
    uint16_t lightness_range_max; /**< Maximum lightness range */
    uint16_t lightness_range_min; /**< Minimum lightness range */
} cwww_client_msg_t;

/**
 * @brief Structure to hold the cwww client elements.
 */
typedef struct cwww_client_element
{
    size_t model_cnt;
    size_t element_id_end;
    size_t element_id_start;
    cwww_cli_ctx_t cwww_cli_ctx[CONFIG_LIGHT_CWWW_CLIENT_COUNT];
    esp_ble_mesh_client_t cwww_cli_list[CONFIG_LIGHT_CWWW_CLIENT_COUNT][CWWW_CLI_MODEL_SIG_CNT];
    esp_ble_mesh_model_pub_t cwww_cli_pub_list[CONFIG_LIGHT_CWWW_CLIENT_COUNT][CWWW_CLI_MODEL_SIG_CNT];
    esp_ble_mesh_model_t cwww_cli_sig_model_list[CONFIG_LIGHT_CWWW_CLIENT_COUNT][CWWW_CLI_MODEL_SIG_CNT];
} cwww_client_elements_t;

/**
 * @brief Create Dynamic Relay Model Elements
 *
 * @param[in] pdev    Pointer to device structure
 *
 * @return esp_err_t
 */
esp_err_t create_cwww_client_elements(dev_struct_t *pdev);

/**
 * @brief Send Msg to cwww node or group represented by the provisioned publish address
 *
 * @param[in] pdev          Pointer to dev_struct
 * @param[in] element_id    Element id of cwww client
 * @param[in] model_id      Model id of cwww client
 * @param[in] set_get       Message type: Set -> 1 Get -> 0
 * @param[in] ack           Set with ack, 1/0
 * @return esp_err_t
 */
esp_err_t ble_mesh_send_cwww_msg(dev_struct_t *pdev, cwww_cli_sig_id_t model_id, uint16_t element_id, uint8_t set_get, uint8_t ack);
