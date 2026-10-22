/**
 * Copyright (c) 2024 - 2026 MeshX
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file meshx_plat_prov_srv.c
 * @brief Implementation of BLE Mesh provisioning server model for Zephyr.
 */

#include "interface/ble_mesh/server/meshx_ble_mesh_prov_srv.h"
#include "meshx_platform_ble_mesh.h"

#include <string.h>
#include "interface/logging/meshx_log.h"

static struct bt_mesh_prov g_zephyr_prov;

static void zephyr_prov_complete(uint16_t net_idx, uint16_t addr)
{
    MESHX_LOGI(MODULE_ID_MODEL_SERVER, "Zephyr provisioning complete: net_idx 0x%04x, addr 0x%04x", net_idx, addr);
    meshx_prov_srv_param_t prov_srv_param;
    memset(&prov_srv_param, 0, sizeof(meshx_prov_srv_param_t));
    prov_srv_param.prov_evt = MESHX_NODE_PROV_COMPLETE_EVT;
    prov_srv_param.param.node_prov_complete.net_idx = net_idx;
    prov_srv_param.param.node_prov_complete.addr = addr;

    if (meshx_prov_srv_notify_plat_event(&prov_srv_param) != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to notify MeshX provisioning complete event");
    }
}

static void zephyr_prov_reset(void)
{
    MESHX_LOGW(MODULE_ID_MODEL_SERVER, "Zephyr provisioning reset requested");
    meshx_prov_srv_param_t prov_srv_param;
    memset(&prov_srv_param, 0, sizeof(meshx_prov_srv_param_t));
    prov_srv_param.prov_evt = MESHX_NODE_PROV_RESET_EVT;

    if (meshx_prov_srv_notify_plat_event(&prov_srv_param) != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to notify MeshX provisioning reset event");
    }
}

meshx_err_t meshx_plat_init_prov(const uint8_t *uuid)
{
    if (!uuid) {
        return MESHX_INVALID_ARG;
    }
    g_zephyr_prov.uuid = uuid;
    g_zephyr_prov.complete = zephyr_prov_complete;
    g_zephyr_prov.reset = zephyr_prov_reset;
    return MESHX_SUCCESS;
}

meshx_ptr_t meshx_plat_get_prov(void)
{
    return (meshx_ptr_t)&g_zephyr_prov;
}
