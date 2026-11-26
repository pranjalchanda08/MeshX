/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_subnet_crc.c
 * @brief CRC-16-CCITT and IEEE 802.3 CRC-32 utilities for Island Subnet Coexistence.
 */

#include "island/meshx_subnet_types.h"
#include <stdint.h>

/**
 * @brief Calculate CRC-16-CCITT (poly 0x1021, init 0xFFFF) over a byte buffer.
 *
 * @param[in]  data    Pointer to input data buffer.
 * @param[in]  len     Number of bytes to process.
 * @param[out] out_crc Destination for computed 16-bit CRC value.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG if @p data or @p out_crc is NULL.
 */
meshx_err_t meshx_subnet_crc16(const uint8_t *data, size_t len, uint16_t *out_crc)
{
    if (!data || !out_crc)
    {
        return MESHX_INVALID_ARG;
    }
    uint16_t crc = MESHX_CRC16_INIT;
    for (size_t i = 0; i < len; i++)
    {
        crc ^= (uint16_t)data[i] << MESHX_BITS_PER_BYTE;
        for (int b = 0; b < (int)MESHX_BITS_PER_BYTE; b++)
        {
            crc = (crc & MESHX_CRC16_HIGH_BIT) ? (uint16_t)((crc << 1) ^ MESHX_CRC16_POLY_CCITT) : (uint16_t)(crc << 1);
        }
    }
    *out_crc = crc;
    return MESHX_SUCCESS;
}

/**
 * @brief Calculate IEEE 802.3 CRC-32 (poly 0xEDB88320, init 0xFFFFFFFF) over a byte buffer.
 *
 * @param[in]  data    Pointer to input data buffer.
 * @param[in]  len     Number of bytes to process.
 * @param[out] out_crc Destination for computed 32-bit CRC value.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG if @p data or @p out_crc is NULL.
 */
meshx_err_t meshx_subnet_crc32(const uint8_t *data, size_t len, uint32_t *out_crc)
{
    if (!data || !out_crc)
    {
        return MESHX_INVALID_ARG;
    }
    uint32_t crc = MESHX_CRC32_INIT;
    for (size_t i = 0; i < len; i++)
    {
        crc ^= (uint32_t)data[i];
        for (int b = 0; b < (int)MESHX_BITS_PER_BYTE; b++)
        {
            uint32_t mask = (crc & MESHX_CRC32_LSB_MASK) ? UINT32_MAX : 0UL;
            crc = (crc >> 1) ^ (MESHX_CRC32_POLY_IEEE8023 & mask);
        }
    }
    *out_crc = ~crc;
    return MESHX_SUCCESS;
}
