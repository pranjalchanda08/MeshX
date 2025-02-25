/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_provisioning_server.h
 * @brief Header file for provisioning related definitions and functions.
 *
 * This file contains the definitions and function declarations for provisioning
 * in the BLE mesh node application.
 */

#ifndef __MESHX_PROV__
#define __MESHX_PROV__

#include <esp_ble_mesh_defs.h>
#include <esp_ble_mesh_common_api.h>
#include <esp_ble_mesh_networking_api.h>
#include <esp_ble_mesh_local_data_operation_api.h>
#include <esp_ble_mesh_provisioning_api.h>

#define MESHX_PROV_INSTANCE g_meshx_prov

/**
 * @brief Structure to hold provisioning parameters.
 */
typedef struct prov_params
{
    uint8_t uuid[16]; /**< UUID for the provisioning device */
} prov_params_t;

extern esp_ble_mesh_prov_t g_meshx_prov;

/**
 * @brief Initialize the provisioning with the given parameters.
 *
 * @param[in] svr_cfg Pointer to the provisioning parameters.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t meshx_init_prov(const prov_params_t * svr_cfg);

#endif /* __MESHX_PROV__ */
