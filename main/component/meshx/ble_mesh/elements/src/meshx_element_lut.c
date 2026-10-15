/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_element_lut.c
 * @brief Authoritative Look-Up Tables for Element capabilities and compatibility.
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 */

#include "meshx_element_lut.h"

static const meshx_variant_lut_entry_t s_variant_lut[] = {
    { MESHX_ELEMENT_TYPE_RELAY_SERVER,      MESHX_ROLE_CONSUMER, MESHX_TRAIT_RELAY_POWER },
    { MESHX_ELEMENT_TYPE_RELAY_CLIENT,      MESHX_ROLE_PRODUCER, MESHX_TRAIT_RELAY_POWER },
    { MESHX_ELEMENT_TYPE_LIGHT_CWWW_SERVER, MESHX_ROLE_CONSUMER, MESHX_TRAIT_LIGHT_CWWW },
    { MESHX_ELEMENT_TYPE_LIGHT_CWWW_CLIENT, MESHX_ROLE_PRODUCER, MESHX_TRAIT_LIGHT_CWWW },
    { MESHX_ELEMENT_TYPE_LIGHT_HSL_SERVER,  MESHX_ROLE_CONSUMER, MESHX_TRAIT_LIGHT_HSL },
    { MESHX_ELEMENT_TYPE_LIGHT_HSL_CLIENT,  MESHX_ROLE_PRODUCER, MESHX_TRAIT_LIGHT_HSL },
    { MESHX_ELEMENT_TYPE_SENSOR_SERVER,     MESHX_ROLE_CONSUMER, MESHX_TRAIT_SENSOR_DATA },
    { MESHX_ELEMENT_TYPE_SENSOR_CLIENT,     MESHX_ROLE_PRODUCER, MESHX_TRAIT_SENSOR_DATA },
};

static const meshx_variant_compat_entry_t s_variant_compat_lut[] = {
    { MESHX_ELEMENT_TYPE_RELAY_CLIENT,      MESHX_ELEMENT_TYPE_RELAY_SERVER },
    { MESHX_ELEMENT_TYPE_LIGHT_CWWW_CLIENT, MESHX_ELEMENT_TYPE_LIGHT_CWWW_SERVER },
    { MESHX_ELEMENT_TYPE_LIGHT_HSL_CLIENT,  MESHX_ELEMENT_TYPE_LIGHT_HSL_SERVER },
    { MESHX_ELEMENT_TYPE_SENSOR_CLIENT,     MESHX_ELEMENT_TYPE_SENSOR_SERVER },
};

static const meshx_trait_compat_entry_t s_trait_compat_lut[] = {
    { MESHX_TRAIT_RELAY_POWER, MESHX_TRAIT_RELAY_POWER, 0x0000, 0x0000 },
    { MESHX_TRAIT_LIGHT_CWWW,  MESHX_TRAIT_LIGHT_CWWW,  0x0000, 0x0001 },
    { MESHX_TRAIT_LIGHT_HSL,   MESHX_TRAIT_LIGHT_HSL,   0x0000, 0x0001 },
    { MESHX_TRAIT_SENSOR_DATA, MESHX_TRAIT_SENSOR_DATA, 0x0000, 0x0000 },
};

/**
 * @brief Get the address of the variant-to-role/trait Look-Up Table.
 *
 * @param[out] out_count Pointer to receive the number of entries in the table.
 * @return Const pointer to the static LUT array.
 */
const meshx_variant_lut_entry_t* meshx_element_get_variant_lut(size_t *out_count)
{
    if (out_count) {
        *out_count = sizeof(s_variant_lut) / sizeof(s_variant_lut[0]);
    }
    return s_variant_lut;
}

/**
 * @brief Get the address of the 1:1 variant compatibility Look-Up Table.
 *
 * @param[out] out_count Pointer to receive the number of entries in the table.
 * @return Const pointer to the static LUT array.
 */
const meshx_variant_compat_entry_t* meshx_element_get_variant_compat_lut(size_t *out_count)
{
    if (out_count) {
        *out_count = sizeof(s_variant_compat_lut) / sizeof(s_variant_compat_lut[0]);
    }
    return s_variant_compat_lut;
}

/**
 * @brief Get the address of the 1:1 trait compatibility Look-Up Table.
 *
 * @param[out] out_count Pointer to receive the number of entries in the table.
 * @return Const pointer to the static LUT array.
 */
const meshx_trait_compat_entry_t* meshx_element_get_trait_compat_lut(size_t *out_count)
{
    if (out_count) {
        *out_count = sizeof(s_trait_compat_lut) / sizeof(s_trait_compat_lut[0]);
    }
    return s_trait_compat_lut;
}
