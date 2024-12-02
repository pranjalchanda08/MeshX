#pragma once

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <sdkconfig.h>
#include <codegen.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <esp_ble_mesh_defs.h>
#include <esp_ble_mesh_common_api.h>
#include <esp_ble_mesh_networking_api.h>
#include <esp_ble_mesh_generic_model_api.h>
#include <esp_ble_mesh_local_data_operation_api.h>
#include <esp_ble_mesh_config_model_api.h>
#include <esp_ble_mesh_provisioning_api.h>
#include <ble_mesh_example_init.h>

#if CONFIG_PROD_ENABLE_SERVER_ELEMENT
    #include <prod_onoff_server.h>
#endif /* CONFIG_PROD_ENABLE_SERVER_ELEMENT */

#if CONFIG_PROD_ENABLE_CLIENT_ELEMENT
    #include <prod_onoff_client.h>
#endif /* CONFIG_PROD_ENABLE_CLIENT_ELEMENT */

#define MAX_ELE_CNT CONFIG_MAX_ELEMENT_COUNT

typedef struct dev_struct
{
    /* Represents the device composition */
    esp_ble_mesh_comp_t composition;
    /**
     * Pointer to Elements list
     * Shall be initialised with max number of elements in the app layer
     */
    esp_ble_mesh_elem_t elements[MAX_ELE_CNT];
    size_t element_idx;
    uint8_t uuid[16];
}dev_struct_t;
