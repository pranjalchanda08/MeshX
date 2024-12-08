#ifndef __PROD_PROV__
#define __PROD_PROV__

#include <esp_ble_mesh_defs.h>
#include <esp_ble_mesh_common_api.h>
#include <esp_ble_mesh_networking_api.h>
#include <esp_ble_mesh_local_data_operation_api.h>
#include <esp_ble_mesh_provisioning_api.h>

#define PROD_PROV_INSTANCE g_prod_prov

typedef enum{
    PROD_PROV_EVT_IDENTIFY,
    PROD_PROV_EVT_PROV_COMPLETE,
    PROD_PROV_EVT_NODE_RESET,
}prod_prov_evt_t;

typedef void (*prod_prov_cb)(esp_ble_mesh_prov_cb_param_t *param, prod_prov_evt_t evt);
typedef struct prov_params
{
    const uint8_t *uuid;
    prod_prov_cb cb_reg;
}prov_params_t;

extern esp_ble_mesh_prov_t g_prod_prov;

esp_err_t prod_init_prov(prov_params_t * svr_cfg);

#endif /* __PROD_PROV__ */
