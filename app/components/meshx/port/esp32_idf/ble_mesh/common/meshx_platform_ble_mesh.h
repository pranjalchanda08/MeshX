#ifndef __MESHX_BLE_MESH__
#define __MESHX_BLE_MESH__

#include "stdio.h"
#include "stdint.h"

#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_local_data_operation_api.h"
#include "esp_ble_mesh_provisioning_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_generic_model_api.h"
#include "esp_ble_mesh_lighting_model_api.h"

#define MESHX_MODEL_PUB         esp_ble_mesh_model_pub_t
#define MESHX_CLIENT            esp_ble_mesh_client_t
#define MESHX_MODEL             esp_ble_mesh_model_t

#define MESHX_PROV              esp_ble_mesh_prov_t

#define MESHX_CFG_SRV           esp_ble_mesh_cfg_srv_t

#define MESHX_GEN_SRV_CB        esp_ble_mesh_generic_server_cb_t
#define MESHX_GEN_SRV_CB_EVT    esp_ble_mesh_generic_server_cb_event_t
#define MESHX_GEN_SRV_CB_PARAM  esp_ble_mesh_generic_server_cb_param_t
#define MESHX_GEN_ONOFF_SRV     esp_ble_mesh_gen_onoff_srv_t

#define MESHX_LIGHT_CTL_SRV     esp_ble_mesh_light_ctl_srv_t
#define MESHX_LIGHT_CTL_STATE   esp_ble_mesh_light_ctl_state_t

#endif /* __MESHX_BLE_MESH__ */
