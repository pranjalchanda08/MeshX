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
#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_local_data_operation_api.h"
#include "esp_ble_mesh_provisioning_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_generic_model_api.h"
#include "esp_ble_mesh_lighting_model_api.h"

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
#define MESHX_CFG_SRV           esp_ble_mesh_cfg_srv_t

#define MESHX_GEN_SRV_CB        esp_ble_mesh_generic_server_cb_t
#define MESHX_GEN_SRV_CB_EVT    esp_ble_mesh_generic_server_cb_event_t
#define MESHX_GEN_SRV_CB_PARAM  esp_ble_mesh_generic_server_cb_param_t
#define MESHX_GEN_ONOFF_SRV     esp_ble_mesh_gen_onoff_srv_t

#define MESHX_LIGHT_SRV_CB      esp_ble_mesh_lighting_server_cb_t
#define MESHX_LIGHT_SRV_CB_EVT  esp_ble_mesh_lighting_server_cb_event_t
#define MESHX_LIGHT_SRV_CB_PARAM esp_ble_mesh_lighting_server_cb_param_t

#define MESHX_LIGHT_CTL_SRV     esp_ble_mesh_light_ctl_srv_t
#define MESHX_LIGHT_CTL_STATE   esp_ble_mesh_light_ctl_state_t

/**
 * MESHX_CLIENT_MODELS
 */

#define MESHX_CLI               esp_ble_mesh_client_t
#define MESHX_GEN_CLI_CB        esp_ble_mesh_generic_client_cb_t
#define MESHX_GEN_CLI_CB_EVT    esp_ble_mesh_generic_client_cb_event_t
#define MESHX_GEN_CLI_CB_PARAM  esp_ble_mesh_generic_client_cb_param_t

#define MESHX_GEN_ONOFF_CLI             esp_ble_mesh_gen_onoff_cli_t
#define MESHX_GEN_LIGHT_CTL_CLI         esp_ble_mesh_light_ctl_cli_t
#define MESHX_GEN_LIGHT_CTL_STATE_CLI   esp_ble_mesh_light_ctl_state_cli_t
#define MESHX_GEN_LIGHT_CLI_CB          esp_ble_mesh_light_client_cb_t
#define MESHX_GEN_LIGHT_CLI_CB_EVT      esp_ble_mesh_light_client_cb_event_t
#define MESHX_GEN_LIGHT_CLI_CB_PARAM    esp_ble_mesh_light_client_cb_param_t
#endif /* __MESHX_BLE_MESH__ */
