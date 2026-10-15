/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_ro_cfg.c
 * @brief Persistent Serialized Configuration Loader (Read-Only)
 */

#include "meshx_ro_cfg.h"
#include "interface/utils/meshx_fal_interface.h"
#include "interface/logging/meshx_log.h"
#include "meshx_config.pb.h"
#include "pb_decode.h"
#include "meshx_builder_api.h"
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#define MODULE_ID_RO_CFG MODULE_ID_COMMON

#pragma pack(push, 1)
/**
 * @brief Binary configuration container header stored at offset 0 of meshx_cfg partition.
 *
 * Layout (15 bytes total):
 * - magic:       4-byte 'MXC\0' magic identifier (0x0043584D)
 * - version:     1-byte container layout format version (0x01)
 * - schema_id:   4-byte CRC32 fingerprint computed from proto schema and options
 * - total_len:   2-byte total length of container including this 15-byte header
 * - header_crc:  2-byte CRC-16-CCITT covering the first 11 bytes of this header
 * - payload_crc: 2-byte CRC-16-CCITT covering the serialized Protobuf payload
 */
typedef struct
{
    uint32_t magic;       /**< Magic bytes: 'MXC\0' (0x0043584D) */
    uint8_t version;      /**< Container structure version (0x01) */
    uint32_t schema_id;   /**< Fingerprint of .proto + .options (Schema ID) */
    uint16_t total_len;   /**< Total container length in bytes (15 + payload_len) */
    uint16_t header_crc;  /**< CRC-16-CCITT checksum over first 11 header bytes */
    uint16_t payload_crc; /**< CRC-16-CCITT checksum over the Protobuf payload */
} meshx_cfg_header_t;
#pragma pack(pop)

#define MESHX_CFG_MAGIC 0x0043584D
#define MESHX_CFG_VERSION 0x01
#define MESHX_HDR_SIZE sizeof(meshx_cfg_header_t)

// Ensure ProductInfo is using static buffers (char arrays) rather than callbacks.
// If the .options file is ignored, this will fail at compile-time.
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
static_assert(sizeof(ProductInfo) == 54,
              "ProductInfo size mismatch! Ensure Nanopb is using meshx_config.options");
static_assert(offsetof(ProductInfo, pid) == 2,
              "CID/PID alignment mismatch! Expected PID at offset 2.");
#endif

/**
 * @brief Calculate standard CRC-16-CCITT checksum.
 *
 * Uses polynomial 0x1021 with initial seed 0xFFFF without post-inversion.
 *
 * @param[in] data Pointer to the input byte buffer.
 * @param[in] len  Number of bytes to calculate the checksum over.
 * @return uint16_t Calculated 16-bit CRC-16-CCITT value.
 */
static uint16_t calc_crc16_ccitt(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++)
    {
        crc ^= ((uint16_t)data[i] << 8);
        for (int j = 0; j < 8; j++)
        {
            if (crc & 0x8000)
            {
                crc = (crc << 1) ^ 0x1021;
            }
            else
            {
                crc <<= 1;
            }
        }
    }
    return crc;
}

/**
 * @brief Nanopb repeated field decoding callback for element compositions.
 *
 * Decodes each ElementConfig message from the protobuf stream and registers the
 * element type and instance count with the MeshX composition builder.
 *
 * @param[in,out] stream Pointer to the Nanopb input stream.
 * @param[in]     field  Pointer to the active protobuf field iterator.
 * @param[in,out] arg    Custom user argument pointer (unused).
 * @return true if decoding and registration succeeded, false on stream decode error.
 */
static bool elements_decode_cb(pb_istream_t *stream, const pb_field_iter_t *field, void **arg)
{
    ElementConfig element = ElementConfig_init_zero;
    if (!pb_decode(stream, ElementConfig_fields, &element))
    {
        return false;
    }
    meshx_builder_add_element(element.type, element.count);
    return true;
}

/**
 * @brief Nanopb repeated field decoding callback for GPIO pin configuration.
 *
 * Decodes GpioConfig entries from the serialized configuration.
 *
 * @param[in,out] stream Pointer to the Nanopb input stream.
 * @param[in]     field  Pointer to the active protobuf field iterator.
 * @param[in,out] arg    Custom user argument pointer (unused).
 * @return true on successful decode, false on stream decode error.
 */
static bool gpio_decode_cb(pb_istream_t *stream, const pb_field_iter_t *field, void **arg)
{
    GpioConfig gpio = GpioConfig_init_zero;
    if (!pb_decode(stream, GpioConfig_fields, &gpio))
    {
        return false;
    }
    // Implementation for GPIO hardware init would go here
    return true;
}

/**
 * @brief Nanopb repeated field decoding callback for IO bindings.
 *
 * Decodes IoBinding entries from the serialized configuration.
 *
 * @param[in,out] stream Pointer to the Nanopb input stream.
 * @param[in]     field  Pointer to the active protobuf field iterator.
 * @param[in,out] arg    Custom user argument pointer (unused).
 * @return true on successful decode, false on stream decode error.
 */
static bool bindings_decode_cb(pb_istream_t *stream, const pb_field_iter_t *field, void **arg)
{
    IoBinding binding = IoBinding_init_zero;
    if (!pb_decode(stream, IoBinding_fields, &binding))
    {
        return false;
    }
    // Implementation for mapping IO bindings would go here
    return true;
}

/**
 * @brief Initialize and load read-only configuration from the dedicated flash partition.
 *
 * Locates the `meshx_cfg` partition, validates the header magic ('MXC\0'), version,
 * Schema ID fingerprint, and header/payload CRC-16 checksums. Upon successful
 * validation, decodes the Protobuf configuration payload via Nanopb, drives
 * dynamic element baking via the composition builder, and populates output parameters.
 *
 * @param[out] cid          Pointer to store the decoded Company ID (optional, nullable).
 * @param[out] pid          Pointer to store the decoded Product ID (optional, nullable).
 * @param[out] product_name Buffer to store the null-terminated product name (optional, nullable).
 * @param[in]  name_max_len Maximum capacity of the product_name buffer.
 * @param[out] uuid         Buffer to store the 16-byte device UUID if present (optional, nullable).
 * @return
 *     - MESHX_SUCCESS: Configuration successfully loaded and applied
 *     - MESHX_NOT_FOUND: 'meshx_cfg' partition not found
 *     - MESHX_ERR_PLAT: Flash read operation failed
 *     - MESHX_ERR_RO_CFG_FORMAT: Invalid magic identifier or Protobuf decoding error
 *     - MESHX_ERR_RO_CFG_VERSION: Unsupported container version or Schema ID mismatch
 *     - MESHX_ERR_RO_CFG_CRC: Header or payload CRC-16 validation failure
 *     - MESHX_NO_MEM: Memory allocation failed for payload buffer
 */
meshx_err_t meshx_ro_cfg_init(uint16_t *cid, uint16_t *pid, char *product_name, size_t name_max_len, uint8_t *uuid)
{
    meshx_fal_partition_t part;
    if (meshx_fal_find_partition("meshx_cfg", &part) != MESHX_SUCCESS)
    {
        MESHX_LOGI(MODULE_ID_RO_CFG, "No meshx_cfg partition found");
        return MESHX_NOT_FOUND;
    }

    meshx_cfg_header_t header;
    if (meshx_fal_read(&part, 0, (uint8_t *)&header, MESHX_HDR_SIZE) != MESHX_SUCCESS)
    {
        return MESHX_ERR_PLAT;
    }

    if (header.magic != MESHX_CFG_MAGIC)
    {
        MESHX_LOGE(MODULE_ID_RO_CFG, "Invalid config magic (0x%08X, exp: 0x%08X)", header.magic, MESHX_CFG_MAGIC);
        return MESHX_ERR_RO_CFG_FORMAT;
    }

    if (header.version != MESHX_CFG_VERSION)
    {
        MESHX_LOGE(MODULE_ID_RO_CFG, "Unsupported config version (0x%02X)", header.version);
        return MESHX_ERR_RO_CFG_VERSION;
    }

    if (header.schema_id != MESHX_SCHEMA_ID)
    {
        MESHX_LOGE(MODULE_ID_RO_CFG, "Schema mismatch! Binary ID: 0x%08X, Firmware ID: 0x%08X",
                   header.schema_id, MESHX_SCHEMA_ID);
        return MESHX_ERR_RO_CFG_VERSION;
    }

    // Validate header CRC (all fields except header_crc and payload_crc)
    uint16_t expected_hdr_crc = calc_crc16_ccitt((uint8_t *)&header, MESHX_HDR_SIZE - 4);
    if (header.header_crc != expected_hdr_crc)
    {
        MESHX_LOGE(MODULE_ID_RO_CFG, "Config header CRC mismatch (calc: 0x%04X, exp: 0x%04X)", expected_hdr_crc, header.header_crc);
        return MESHX_ERR_RO_CFG_CRC;
    }

    size_t payload_len = header.total_len - MESHX_HDR_SIZE;
    uint8_t *payload = malloc(payload_len);
    if (!payload)
        return MESHX_NO_MEM;

    if (meshx_fal_read(&part, MESHX_HDR_SIZE, payload, payload_len) != MESHX_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_RO_CFG, "Failed to read config payload");
        free(payload);
        return MESHX_ERR_PLAT;
    }

    uint16_t expected_pld_crc = calc_crc16_ccitt(payload, payload_len);
    if (header.payload_crc != expected_pld_crc)
    {
        MESHX_LOGE(MODULE_ID_RO_CFG, "Config payload CRC mismatch (calc: 0x%04X, exp: 0x%04X)", expected_pld_crc, header.payload_crc);
        free(payload);
        return MESHX_ERR_RO_CFG_CRC;
    }

    MeshXConfig config = MeshXConfig_init_zero;
    config.elements.funcs.decode = elements_decode_cb;
    config.gpio.funcs.decode = gpio_decode_cb;
    config.bindings.funcs.decode = bindings_decode_cb;

    pb_istream_t stream = pb_istream_from_buffer(payload, payload_len);

    meshx_builder_begin();

    if (!pb_decode(&stream, MeshXConfig_fields, &config))
    {
        MESHX_LOGE(MODULE_ID_RO_CFG, "Config protobuf decode failed");
        free(payload);
        return MESHX_ERR_RO_CFG_FORMAT;
    }

    meshx_builder_commit();

    if (config.has_product)
    {
        if (cid)
            *cid = config.product.cid;
        if (pid)
            *pid = config.product.pid;
        if (product_name)
        {
            strncpy(product_name, config.product.name, name_max_len - 1);
            product_name[name_max_len - 1] = '\0';
        }
        if (uuid && config.product.has_uuid)
        {
            memcpy(uuid, config.product.uuid, 16);
            MESHX_LOGD(MODULE_ID_RO_CFG, "Loaded UUID: %02X%02X%02X%02X...", uuid[0], uuid[1], uuid[2], uuid[3]);
        }
        MESHX_LOGI(MODULE_ID_RO_CFG, "Loaded Config: CID 0x%04X, PID 0x%04X, Name: %s",
                   config.product.cid, config.product.pid, config.product.name);
    }

    free(payload);
    return MESHX_SUCCESS;
}
