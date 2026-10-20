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
#include <meshx_common.h>
#include <meshx_txcm.h>
#include <meshx_control_task.h>
#include <meshx_api.h>
#include <meshx_nvs.h>
#include <meshx_platform_ble_mesh.h>
#include <meshx_serial.h>
#include <meshx_os_timer.h>
#include <meshx_manifest.h>
#include <meshx_element_lut.h>
#include <meshx_compat_matrix.h>
#include <meshx_group_allocator.h>
#include <pairing/meshx_pair_orchestrator.h>
#include <pairing/meshx_unpair_mgr.h>
#include <pairing/meshx_proximity_pair.h>
#include <meshx_builder_api.h>
#include <meshx_uvp.h>

/*************************************************************
 * Interface Header
 *************************************************************/
#include <interface/ble_mesh/meshx_ble_mesh_cmn.h>

/* Provisioning and Config server (still active) */
#include <interface/ble_mesh/server/meshx_ble_mesh_config_srv.h>
#include <interface/ble_mesh/server/meshx_ble_mesh_prov_srv.h>
#include <interface/rtos/meshx_rtos_utils.h>
#include <interface/rtos/meshx_sem.h>
#include <interface/logging/meshx_log.h>
#include <meshx_prov_srv.h>
#include <meshx_config_server.h>

/* NOTE: Legacy SIG model headers (gen_srv, light_srv, sensor_srv, gen_cli, light_cli)
 * were decommissioned in TASK-007 (UVP migration). Removed from this header. */


#ifdef __cplusplus
}
#endif

#endif /* __MESHX_C_HEADER_H__ */
