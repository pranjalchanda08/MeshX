#pragma once

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <sdkconfig.h>
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

#define MAX_ELE_CNT (1 + CONFIG_RELAY_SERVER_ELEMENT_NOS)

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
