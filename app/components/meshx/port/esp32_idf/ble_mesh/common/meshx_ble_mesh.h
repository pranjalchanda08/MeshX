
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

typedef esp_ble_mesh_model_pub_t        meshx_model_pub_t
typedef esp_ble_mesh_client_t           meshx_client_t
typedef esp_ble_mesh_model_t            meshx_model_t

typedef esp_ble_mesh_prov_t             meshx_prov_t

typedef esp_ble_mesh_cfg_srv_t          meshx_cfg_srv_t

typedef esp_ble_mesh_generic_server_cb_t        meshx_gen_srv_cb_t
typedef esp_ble_mesh_generic_server_cb_event_t  meshx_gen_srv_cb_evt_t
typedef esp_ble_mesh_generic_server_cb_param_t  meshx_gen_srv_cb_param_t
typedef esp_ble_mesh_gen_onoff_srv_t            meshx_gen_onoff_srv_t

typedef esp_ble_mesh_light_ctl_srv_t    meshx_light_ctl_srv_t
typedef esp_ble_mesh_light_ctl_state_t  meshx_light_ctl_state_t

#endif /* __MESHX_BLE_MESH__ */
