/**
 * Copyright © 2024 - 2025 MeshX
 * @file meshx_config_server.h
 * @brief Header for Config Server (migrated for C++ architecture support)
 */

#ifndef __MESHX_CONFIG_SERVER_H__
#define __MESHX_CONFIG_SERVER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <meshx_api.h>
#include <meshx_common.h>
#include <interface/ble_mesh/server/meshx_ble_mesh_config_srv.h>

meshx_err_t meshx_init_config_server  (void);
meshx_err_t meshx_get_config_srv_model(meshx_ptr_t p_model);
meshx_err_t meshx_config_server_cb_reg(config_srv_cb_t cb, uint32_t config_evt_bmap);

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_CONFIG_SERVER_H__ */
