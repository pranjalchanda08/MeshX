/**
 * Copyright (c) 2024 - 2025 MeshX
 *
 * @file ble_mesh_plat_init.h
 * @brief Header file for BLE Mesh example initialization functions and definitions.
 *        This file contains declarations for initializing Bluetooth and retrieving
 *        the device UUID for BLE Mesh applications.
 *
 * @author Pranjal Chanda
 *
 */

#ifndef _BLE_MESH_EXAMPLE_INIT_H_
#define _BLE_MESH_EXAMPLE_INIT_H_

#ifdef __cplusplus
extern "C" {
#endif /**< __cplusplus */

#include "esp_err.h"

/**
 * @brief Retrieve the device UUID for BLE Mesh.
 *
 * This function populates the provided buffer with the device UUID
 * used in BLE Mesh operations.
 *
 * @param[out] dev_uuid Pointer to a buffer where the device UUID will be stored.
 *                      The buffer must be large enough to hold the UUID.
 */
void ble_mesh_get_dev_uuid(uint8_t *dev_uuid);

/**
 * @brief Initialize the Bluetooth stack for BLE Mesh.
 *
 * This function sets up the necessary components and configurations
 * to initialize the Bluetooth stack for BLE Mesh functionality.
 *
 * @return
 *     - ESP_OK: Initialization successful.
 *     - Other: Appropriate error code indicating failure.
 */

esp_err_t bluetooth_init(void);

#ifdef __cplusplus
}
#endif /**< __cplusplus */

#endif /* _BLE_MESH_EXAMPLE_INIT_H_ */
