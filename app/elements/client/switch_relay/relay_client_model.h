#pragma once

#include "app_common.h"
#include "prod_onoff_server.h"

#define RELAY_SERVER_ELEMENT_NOS_DEF 1

#ifndef CONFIG_RELAY_SERVER_ELEMENT_NOS
#define CONFIG_RELAY_SERVER_ELEMENT_NOS RELAY_SERVER_ELEMENT_NOS_DEF
#endif


#define RELAY_SRV_MODEL_SIG_CNT 1 // No of SIG models in a relay model element
#define RELAY_SRV_MODEL_VEN_CNT 0 // No of VEN models in a relay model element

typedef struct relay_element
{
    size_t model_cnt;
    esp_ble_mesh_model_t relay_server_sig_model_list[CONFIG_RELAY_SERVER_ELEMENT_NOS][RELAY_SRV_MODEL_SIG_CNT];
    esp_ble_mesh_model_pub_t relay_server_pub_list[CONFIG_RELAY_SERVER_ELEMENT_NOS];
    esp_ble_mesh_gen_onoff_srv_t relay_server_onoff_gen_list[CONFIG_RELAY_SERVER_ELEMENT_NOS];
} relay_elements_t;

/**
 * @brief Create Dynamic Relay Model Elements
 *
 * @param[in]       pdev    Pointer to device structure
 *
 * @return esp_err_t
 */
esp_err_t create_relay_elements(dev_struct_t *pdev);

