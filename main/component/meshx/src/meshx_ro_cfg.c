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

#define MODULE_ID_RO_CFG MODULE_ID_COMMON

#pragma pack(push, 1)
typedef struct {
    uint16_t magic;      // 0x4D58
    uint8_t  version;    // 0x01
    uint16_t total_len;  // 9 + payload_len
    uint16_t header_crc;
    uint16_t payload_crc;
} meshx_cfg_header_t;
#pragma pack(pop)

typedef struct {
    char *buffer;
    size_t max_len;
} string_decode_ctx_t;

static bool string_decode_cb(pb_istream_t *stream, const pb_field_iter_t *field, void **arg) {
    string_decode_ctx_t *ctx = (string_decode_ctx_t *)*arg;
    size_t len = stream->bytes_left;
    if (len >= ctx->max_len) len = ctx->max_len - 1;

    if (!pb_read(stream, (pb_byte_t *)ctx->buffer, len)) return false;
    ctx->buffer[len] = '\0';
    return true;
}

// CRC-16-CCITT (Polynomial 0x1021, Initial 0xFFFF)
static uint16_t calc_crc16_ccitt(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= ((uint16_t)data[i] << 8);
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

// Nanopb callback for elements
static bool elements_decode_cb(pb_istream_t *stream, const pb_field_iter_t *field, void **arg) {
    ElementConfig element = ElementConfig_init_zero;
    if (!pb_decode(stream, ElementConfig_fields, &element)) {
        return false;
    }
    meshx_builder_add_element(element.type, element.count);
    return true;
}

// Nanopb callback for gpio
static bool gpio_decode_cb(pb_istream_t *stream, const pb_field_iter_t *field, void **arg) {
    GpioConfig gpio = GpioConfig_init_zero;
    if (!pb_decode(stream, GpioConfig_fields, &gpio)) {
        return false;
    }
    // Implementation for GPIO hardware init would go here
    return true;
}

// Nanopb callback for bindings
static bool bindings_decode_cb(pb_istream_t *stream, const pb_field_iter_t *field, void **arg) {
    IoBinding binding = IoBinding_init_zero;
    if (!pb_decode(stream, IoBinding_fields, &binding)) {
        return false;
    }
    // Implementation for mapping IO bindings would go here
    return true;
}

meshx_err_t meshx_ro_cfg_init(uint16_t *cid, uint16_t *pid, char *product_name, size_t name_max_len) {
    meshx_fal_partition_t part;
    if (meshx_fal_find_partition("meshx_cfg", &part) != MESHX_SUCCESS) {
        MESHX_LOGI(MODULE_ID_RO_CFG, "No meshx_cfg partition found");
        return MESHX_NOT_FOUND;
    }

    meshx_cfg_header_t header;
    if (meshx_fal_read(&part, 0, &header, sizeof(header)) != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_RO_CFG, "Failed to read config header");
        return MESHX_ERR_PLAT;
    }

    if (header.magic != 0x4D58) {
        // Not provisioned or empty
        return MESHX_ERR_FORMAT;
    }

    uint16_t expected_hdr_crc = calc_crc16_ccitt((const uint8_t*)&header, 5);
    if (header.header_crc != expected_hdr_crc) {
        MESHX_LOGE(MODULE_ID_RO_CFG, "Config header CRC mismatch (calc: 0x%04X, exp: 0x%04X)", expected_hdr_crc, header.header_crc);
        return MESHX_ERR_CRC;
    }

    size_t payload_len = header.total_len - 9;
    if (payload_len == 0 || payload_len > part.size - 9) {
        MESHX_LOGE(MODULE_ID_RO_CFG, "Invalid config payload length: %d", payload_len);
        return MESHX_ERR_FORMAT;
    }

    uint8_t *payload = malloc(payload_len);
    if (!payload) {
        return MESHX_NO_MEM;
    }

    if (meshx_fal_read(&part, 9, payload, payload_len) != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_RO_CFG, "Failed to read config payload");
        free(payload);
        return MESHX_ERR_PLAT;
    }

    uint16_t expected_pld_crc = calc_crc16_ccitt(payload, payload_len);
    if (header.payload_crc != expected_pld_crc) {
        MESHX_LOGE(MODULE_ID_RO_CFG, "Config payload CRC mismatch (calc: 0x%04X, exp: 0x%04X)", expected_pld_crc, header.payload_crc);
        free(payload);
        return MESHX_ERR_CRC;
    }

    MeshXConfig config = MeshXConfig_init_zero;
    config.elements.funcs.decode = elements_decode_cb;
    config.gpio.funcs.decode = gpio_decode_cb;
    config.bindings.funcs.decode = bindings_decode_cb;

    string_decode_ctx_t name_ctx = {product_name, name_max_len};
    if (product_name && name_max_len > 0) {
        config.product.name.funcs.decode = string_decode_cb;
        config.product.name.arg = &name_ctx;
    }

    pb_istream_t stream = pb_istream_from_buffer(payload, payload_len);

    meshx_builder_begin();

    if (!pb_decode(&stream, MeshXConfig_fields, &config)) {
        MESHX_LOGE(MODULE_ID_RO_CFG, "Config protobuf decode failed");
        free(payload);
        return MESHX_ERR_FORMAT;
    }

    meshx_builder_commit();

    if (config.has_product) {
        if (cid) *cid = config.product.cid;
        if (pid) *pid = config.product.pid;
        MESHX_LOGI(MODULE_ID_RO_CFG, "Loaded Config: CID 0x%04X, PID 0x%04X, Name: %s", 
                 config.product.cid, config.product.pid, product_name ? product_name : "N/A");
    }

    free(payload);
    return MESHX_SUCCESS;
}
