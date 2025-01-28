/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file app_common.h
 * @brief Common application definitions and includes for BLE Mesh Node.
 *
 * This header file contains common definitions, includes, and data structures
 * used across the BLE Mesh Node application.
 *
 */

#pragma once

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <sdkconfig.h>
#include <codegen.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <esp_ble_mesh_defs.h>
#include <ble_mesh_example_init.h>

#if CONFIG_ENABLE_UNIT_TEST
#include <unit_test.h>
#endif /* CONFIG_ENABLE_UNIT_TEST */

#define TAG __func__

#define MAX_ELE_CNT CONFIG_MAX_ELEMENT_COUNT
#define MESHX_NVS_STORE "meshx_store"

/**
 * @brief Structure to store mesh application data.
 */
typedef struct meshx_app_store
{
    uint16_t net_key_id; /**< Network key identifier */
    uint16_t node_addr;  /**< Node address */
} meshx_app_store_t;

/**
 * @brief Structure representing the device composition and elements.
 */
typedef struct dev_struct
{
    uint8_t uuid[16];               /**< Device UUID */
    size_t element_idx;             /**< Index of the current element */
    meshx_app_store_t meshx_store;  /**< Mesh application store */
    esp_ble_mesh_comp_t composition; /**< Device composition */
    esp_ble_mesh_elem_t elements[MAX_ELE_CNT]; /**< Array of elements */
} dev_struct_t;
