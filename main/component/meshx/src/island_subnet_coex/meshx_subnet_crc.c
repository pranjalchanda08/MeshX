/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_subnet_crc.c
 * @brief CRC-16-CCITT and IEEE 802.3 CRC-32 utilities for Island Subnet Coexistence.
 */

#include "island_subnet_coex/meshx_subnet_types.h"

uint16_t meshx_subnet_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFU;
    if (!data) {
        return crc;
    }
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x8000U) ? (uint16_t)((crc << 1) ^ 0x1021U) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

uint32_t meshx_subnet_crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;
    if (!data) {
        return 0U;
    }
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint32_t)data[i];
        for (int b = 0; b < 8; b++) {
            uint32_t mask = -(crc & 1UL);
            crc = (crc >> 1) ^ (0xEDB88320UL & mask);
        }
    }
    return ~crc;
}

