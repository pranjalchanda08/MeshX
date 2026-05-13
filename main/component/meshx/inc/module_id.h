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

#ifdef __cplusplus
extern "C" {
#endif

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
    MODULE_ID_ELEMENT_LIGHT_CWWW_CLIENT     = 0x01,
    MODULE_ID_COMPONENT_OS_TIMER            = 0x02,
    MODULE_ID_COMPONENT_MESHX_NVS           = 0x03,
    MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER   = 0x04,
    MODULE_ID_ELEMENT_LIGHT_CWWW_SERVER     = 0x05,
    MODULE_ID_MODEL_SERVER                  = 0x06,
    MODULE_ID_MODEL_CLIENT                  = 0x07,
    MODULE_ID_COMMON                        = 0x08,
    MODULE_ID_TXCM                          = 0x09,
    MODULE_ID_ELEMENT_ROOT                  = 0x0A,
    MODULE_ID_BLE_MESH_ELEMENT              = 0x0B,
    MODULE_ID_ELEMENT_SENSOR_SERVER         = 0x0C,
    MODULE_ID_ELEMENT_SENSOR_CLIENT         = 0x0D,
    MODULE_ID_ELEMENT_LIGHT_HSL_SERVER      = 0x0E,
    MODULE_ID_ELEMENT_LIGHT_HSL_CLIENT      = 0x0F,
    MODULE_ID_COMPONENT_MESHX_GPIO          = 0x10,
    MODULE_ID_GPIO_UNIT_TEST                = 0x11,
    MODULE_ID_GPIO_PROPERTY_TEST            = 0x12,
    MODULE_ID_GPIO_INTEGRATION_TEST         = 0x13,
    MODULE_ID_PWM_PROPERTY_TEST             = 0x14,
    MODULE_ID_GPIO_PLATFORM_TEST            = 0x15,
    MODULE_ID_GPIO_ELEMENT_TEST             = 0x16,
    MODULE_ID_MAX
} module_id_t;

#ifdef __cplusplus
}
#endif

#endif // MODULE_ID_H

