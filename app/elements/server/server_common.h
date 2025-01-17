/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file server_common.h
 * @brief Common definitions and includes for the BLE Mesh server.
 *
 * This header file contains common definitions and includes necessary for
 * the BLE Mesh server functionality.
 */

#ifndef __SERVER_COMMON_H__
#define __SERVER_COMMON_H__

#include <esp_ble_mesh_defs.h>
#include <esp_ble_mesh_common_api.h>
#include <esp_ble_mesh_networking_api.h>
#include <esp_ble_mesh_local_data_operation_api.h>

/**
 * @brief Macro to check if the address is a broadcast address.
 *
 * This macro checks if the given address is the broadcast address for all nodes.
 *
 * @param _x Address to check.
 * @return True if the address is the broadcast address, false otherwise.
 */
#define ESP_BLE_MESH_ADDR_BROADCAST(_x) _x == ESP_BLE_MESH_ADDR_ALL_NODES

#endif /*__SERVER_COMMON_H__*/
