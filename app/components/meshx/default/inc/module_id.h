/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file module_id.h
 * @brief Defines module IDs for different elements in the BLE mesh node application.
 *
 * This header file contains the enumeration of module IDs used to identify
 * different elements in the BLE mesh node application. Each module ID is
 * represented by a unique value.
 */

#ifndef MODULE_ID_H
#define MODULE_ID_H

/**
 * @enum module_id_t
 * @brief Enumeration of module IDs.
 *
 * This enumeration defines the module IDs for various elements in the BLE mesh
 * node application.
 *
 */
typedef enum {
    MODULE_ID_ELEMENT_SWITCH_RELAY_CLIENT   = 0x00,
    MODULE_ID_ELEMENT_LIGHT_CWWWW_CLIENT    = 0x01,
    MODULE_ID_COMPONENT_OS_TIMER            = 0x02,
    MODULE_ID_COMPONENT_MESHX_NVS           = 0x03,
    MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER   = 0x04,
    MODULE_ID_ELEMENT_LIGHT_CWWW_SERVER     = 0x05,
    MODULE_ID_MODEL_SERVER                  = 0x06,
    MODULE_ID_MODEL_CLIENT                  = 0x07,
    MODULE_ID_COMMON                        = 0x08,
    MODULE_ID_MAX
} module_id_t;

#endif // MODULE_ID_H

