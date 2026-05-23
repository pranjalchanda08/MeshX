/**
 * Copyright (c) 2024 - 2025 MeshX
 *
 * @file meshx_platform_ble_mesh.h
 * @brief Header file for MeshX BLE Mesh platform abstraction layer.
 *        This file provides type definitions and macros to map MeshX
 *        BLE Mesh components to ESP-IDF BLE Mesh APIs.
 *
 * @author Pranjal Chanda
 *
 */

#ifndef __MESHX_BLE_MESH__
#define __MESHX_BLE_MESH__

#include "stdio.h"
#include "stdint.h"

#include "sdkconfig.h"
#include <meshx_err.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_local_data_operation_api.h"
#include "esp_ble_mesh_provisioning_api.h"
#include "esp_ble_mesh_config_model_api.h"
#pragma GCC diagnostic pop


/**
 * MESHX_COMMONS
 */
#define MESHX_COMPOSITION       esp_ble_mesh_comp_t
#define MESHX_ELEMENT           esp_ble_mesh_elem_t
#define MESHX_MODEL             esp_ble_mesh_model_t
#define MESHX_CLIENT            esp_ble_mesh_client_t
#define MESHX_MODEL_PUB         esp_ble_mesh_model_pub_t

/**
 * MESHX_SERVER_MODELS
 */
#define MESHX_PROV              esp_ble_mesh_prov_t
#define MESHX_PROV_CB           esp_ble_mesh_prov_cb_t
#define MESHX_PROV_CB_EVT       esp_ble_mesh_prov_cb_event_t
#define MESHX_PROV_CB_PARAM     esp_ble_mesh_prov_cb_param_t
#define MESHX_CFG_SRV           esp_ble_mesh_cfg_srv_t

#define MESHX_VND_SRV_CB        esp_ble_mesh_model_cb_t
#define MESHX_VND_SRV_CB_EVT    esp_ble_mesh_model_cb_event_t
#define MESHX_VND_SRV_CB_PARAM  esp_ble_mesh_model_cb_param_t

/**
 * @brief Initialize the vendor server model platform.
 */
meshx_err_t meshx_plat_ven_srv_init(void);

/**
 * @brief Bind the platform-specific UVP opcodes to a model instance.
 * @param p_model Pointer to the platform model structure.
 * @return MESHX_SUCCESS on success, or an error code.
 */
meshx_err_t meshx_plat_bind_uvp_opcodes(void *p_model);

#endif /* __MESHX_BLE_MESH__ */

