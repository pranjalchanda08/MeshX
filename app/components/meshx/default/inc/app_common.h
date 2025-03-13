/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file app_common.h
 * @brief Common application definitions and includes for BLE Mesh Node.
 *
 * This header file contains common definitions, includes, and data structures
 * used across the BLE Mesh Node application.
 *
 * @author Pranjal Chanda
 *
 */

#pragma once

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <sdkconfig.h>
#include <meshx_config.h>
#include <esp_log.h>
#include <meshx_log.h>
#include <meshx_err.h>
#include <meshx_platform.h>

#if CONFIG_ENABLE_UNIT_TEST
#include <unit_test.h>
#endif /* CONFIG_ENABLE_UNIT_TEST */

/*********************************************************************
 *      FEATURE CONFIGURATION
 * *******************************************************************/

/**
 * @brief Enable Element Table registration using Linker script.
 * @note This feature is disabled by default.
 */
#ifndef CONFIG_SECTION_ENABLE_ELEMENT_TABLE
#define CONFIG_SECTION_ENABLE_ELEMENT_TABLE 0
#endif /* CONFIG_SECTION_ENABLE_ELEMENT_TABLE */

/**
 * @brief Ofload BLE Events to MeshX Control Task.
 */
#ifndef CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE
#define CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE 1
#endif /* CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE */

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
    uint8_t uuid[16];                          /**< Device UUID */
    size_t element_idx;                        /**< Index of the current element */
    meshx_app_store_t meshx_store;             /**< Mesh application store */
    esp_ble_mesh_comp_t composition;           /**< Device composition */
    esp_ble_mesh_elem_t elements[MAX_ELE_CNT]; /**< Array of elements */
} dev_struct_t;

/**
 * @brief MeshX Compostion init Function Pointer
 * @typedef element_comp_fn_t
 *
 * @param[in] pdev          Pointer to the device structure.
 * @param[in] element_cnt   Number of elements.
 *
 * @return meshx_err_t
 */
typedef meshx_err_t (*element_comp_fn_t)(dev_struct_t *pdev, uint16_t element_cnt);

/**
 * @struct element_comp_table
 * @brief Structure to store element composition functions.
 */
typedef struct element_comp_table
{
    uint8_t idx;                       /**< Index of the element type */
    element_comp_fn_t element_comp_fn; /**< Element composition function */
} element_comp_table_t;

#if CONFIG_SECTION_ENABLE_ELEMENT_TABLE

/**
 * @brief Register an element composition function.
 *
 * This macro registers an element composition function to the element table.
 *
 * @param _name Name of the element composition function.
 * @param _type Type of the element.
 * @param _fn Element composition function.
 */
#define REG_MESHX_ELEMENT_FN(_name, _type, _fn)                      \
    __section(".element_table") const element_comp_table_t _name = { \
        .idx = _type,                                                \
        .element_comp_fn = &_fn,                                     \
    }

#else

/**
 * @brief Register an element composition function.
 * @note Currently feature is disabled using CONFIG_SECTION_ENABLE_ELEMENT_TABLE
 *
 * This macro registers an element composition function to the element table.
 *
 * @param _name Name of the element composition function.
 * @param _type Type of the element.
 * @param _fn Element composition function.
 */
#define REG_MESHX_ELEMENT_FN(_name, _type, _fn)

#endif /* CONFIG_SECTION_ENABLE_ELEMENT_TABLE */
