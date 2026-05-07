/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_c_header.h
 * @brief This file includes all required header file encapslated under C env
 *
 * @author Pranjal Chanda
 */
#ifndef __MESHX_C_HEADER_H__
#define __MESHX_C_HEADER_H__

#ifdef __cplusplus
extern "C" {
#endif

/*************************************************************
 * Common Header
 *************************************************************/
#include "meshx_common.h"
#include "meshx_txcm.h"
#include "meshx_control_task.h"
#include "meshx_api.h"
#include "meshx_nvs.h"
#include "meshx_platform_ble_mesh.h"
#include "meshx_relay_msg_defs.h"

/*************************************************************
 * MESHX Model Common Header
 *************************************************************/
#include "meshx_gen_client.h"

#include "meshx_gen_light_cli.h"

#include "meshx_light_server.h"
#include "meshx_light_ctl_srv.h"

#include "meshx_config_server.h"
#include "meshx_prov_srv.h"
/*************************************************************
 * Interface Header
 *************************************************************/
#include <interface/ble_mesh/meshx_ble_mesh_cmn.h>
#include <interface/ble_mesh/client/meshx_ble_mesh_gen_cli.h>
#include <interface/ble_mesh/client/meshx_ble_mesh_light_cli.h>

#include <interface/ble_mesh/server/meshx_ble_mesh_config_srv.h>
#include <interface/ble_mesh/server/meshx_ble_mesh_gen_srv.h>
#include <interface/ble_mesh/server/meshx_ble_mesh_light_srv.h>
#include <interface/ble_mesh/server/meshx_ble_mesh_sensor_srv.h>
#include <interface/ble_mesh/server/meshx_ble_mesh_prov_srv.h>
#include <interface/logging/meshx_log.h>

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_C_HEADER_H__ */
