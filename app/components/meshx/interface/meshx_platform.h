/**
 * @file meshx_platform.h
 * @brief Platform abstraction layer for MeshX.
 *
 * This header provides initialization functions for the MeshX platform
 * and its Bluetooth subsystem.
 */

#ifndef __MESHX_PLATFORM_H__
#define __MESHX_PLATFORM_H__

#include "meshx_err.h"
#include "meshx_platform_ble_mesh.h"
#include "interface/ble_mesh/meshx_ble_mesh_cmn_def.h"

/**
 * @brief Structure to hold provisioning parameters.
 */
typedef struct meshx_prov_params
{
    meshx_uuid_addr_t uuid; /**< UUID for the provisioning device */
    uint8_t *node_name;     /**< Node name for the provisioning device */
} meshx_prov_params_t;

/**
 * @brief Initializes the MeshX platform.
 *
 * This function sets up the necessary hardware and software components
 * required for the MeshX platform to function correctly.
 *
 * @return meshx_err_t Returns MESHX_OK on success, or an appropriate error code.
 */
meshx_err_t meshx_platform_init(void);

/**
 * @brief Initializes the Bluetooth subsystem of the MeshX platform.
 *
 * This function sets up the Bluetooth-related components necessary for
 * MeshX operation, such as BLE Mesh provisioning and communication.
 *
 * @return meshx_err_t Returns MESHX_OK on success, or an appropriate error code.
 */
meshx_err_t meshx_platform_bt_init(void);

/**
 * @brief Initializes the BLE Mesh stack with the given provisioning parameters.
 *
 * This function sets up the BLE Mesh stack and initializes it with the
 * provided provisioning parameters.
 *
 * @param[in] prov_cfg   Pointer to the provisioning parameters structure.
 * @param[in] comp      Pointer to the composition data.
 *
 * @return meshx_err_t Returns MESHX_OK on success, or an appropriate error code.
 */
meshx_err_t meshx_plat_ble_mesh_init(const meshx_prov_params_t *prov_cfg, void* comp);

#endif /* __MESHX_PLATFORM_H__ */
