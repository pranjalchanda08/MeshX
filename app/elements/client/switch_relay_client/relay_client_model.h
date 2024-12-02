#pragma once

#include "app_common.h"
#include "prod_onoff_client.h"

#define RELAY_CLIENT_ELEMENT_NOS_DEF 3

#ifndef CONFIG_RELAY_CLIENT_ELEMENT_NOS
#define CONFIG_RELAY_CLIENT_ELEMENT_NOS RELAY_CLIENT_ELEMENT_NOS_DEF
#endif


#define RELAY_CLI_MODEL_SIG_CNT 1 // No of SIG models in a relay model element
#define RELAY_CLI_MODEL_VEN_CNT 0 // No of VEN models in a relay model element

typedef struct relay_client_element
{
    size_t model_cnt;
    esp_ble_mesh_model_t relay_cli_sig_model_list[CONFIG_RELAY_CLIENT_ELEMENT_NOS][RELAY_CLI_MODEL_SIG_CNT];
    esp_ble_mesh_client_t relay_cli_onoff_gen_list[CONFIG_RELAY_CLIENT_ELEMENT_NOS];
    esp_ble_mesh_model_pub_t relay_cli_pub_list[CONFIG_RELAY_CLIENT_ELEMENT_NOS];
} relay_client_elements_t;

/**
 * @brief Create Dynamic Relay Model Elements
 *
 * @param[in]       pdev    Pointer to device structure
 *
 * @return esp_err_t
 */
esp_err_t create_relay_client_elements(dev_struct_t *pdev);

