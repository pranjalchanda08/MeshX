/**
 * Copyright (c) 2024 - 2026 MeshX
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file meshx_plat_cfg_srv.c
 * @brief Implementation of BLE Mesh Configuration Server model for Zephyr.
 */

#include "interface/ble_mesh/server/meshx_ble_mesh_config_srv.h"
#include "meshx_platform_ble_mesh.h"
#include <zephyr/bluetooth/mesh/cfg_srv.h>
#include <string.h>

static struct bt_mesh_model g_zephyr_cfg_srv_model = BT_MESH_MODEL_CFG_SRV;

meshx_err_t meshx_plat_get_config_srv_model(meshx_ptr_t p_model)
{
    if (p_model == NULL) {
        return MESHX_INVALID_ARG;
    }
    memcpy(p_model, &g_zephyr_cfg_srv_model, sizeof(g_zephyr_cfg_srv_model));
    return MESHX_SUCCESS;
}

meshx_err_t meshx_plat_config_srv_init(void)
{
    return MESHX_SUCCESS;
}
