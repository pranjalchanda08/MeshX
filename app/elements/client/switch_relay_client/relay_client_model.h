#pragma once

#include "app_common.h"
#include "prod_onoff_client.h"

#define RELAY_CLIENT_ELEMENT_NOS_DEF 3

#ifndef CONFIG_RELAY_CLIENT_COUNT
#define CONFIG_RELAY_CLIENT_COUNT RELAY_CLIENT_ELEMENT_NOS_DEF
#endif

#define RELAY_CLI_MODEL_SIG_CNT 1 // No of SIG models in a relay model element
#define RELAY_CLI_MODEL_VEN_CNT 0 // No of VEN models in a relay model element

typedef struct relay_client_element
{
    size_t model_cnt;
    size_t element_id_end;
    size_t element_id_start;
    esp_ble_mesh_model_t relay_cli_sig_model_list   [CONFIG_RELAY_CLIENT_COUNT][RELAY_CLI_MODEL_SIG_CNT];
    esp_ble_mesh_client_t relay_cli_onoff_gen_list  [CONFIG_RELAY_CLIENT_COUNT];
    esp_ble_mesh_model_pub_t relay_cli_pub_list     [CONFIG_RELAY_CLIENT_COUNT];
    prod_onoff_ctx_t prod_onoff_ctx                 [CONFIG_RELAY_CLIENT_COUNT];
} relay_client_elements_t;

/**
 * @brief Create Dynamic Relay Model Elements
 *
 * @param[in]       pdev    Pointer to device structure
 *
 * @return esp_err_t
 */
esp_err_t create_relay_client_elements(dev_struct_t *pdev);

/**
 * @brief Send Msg to relay node or group represented by the provisioned publish address
 * 
 * @param[in] pdev          Pointer to dev_struct 
 * @param[in] element_id    Element id of relay client
 * @param[in] set_get       Message type: Set -> 1 Get -> 0
 * @param[in] ack           Set with ack, 1/0
 * @return esp_err_t 
 */
esp_err_t send_relay_msg(dev_struct_t *pdev, uint16_t element_id, uint8_t set_get, uint8_t ack);

