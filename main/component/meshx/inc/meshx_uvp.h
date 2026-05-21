/**
 * @file meshx_uvp.h
 * @brief MeshX Unified Vendor Protocol (UVP) Definitions.
 *
 * This file defines the UVP frame structure, opcodes, and constants used for
 * communication via the single MeshX Vendor Model.
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 */

#ifndef __MESHX_UVP_H__
#define __MESHX_UVP_H__

#include <stdint.h>
#include <meshx_common.h>   /* meshx_err_t */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief MeshX Unified Vendor Protocol (UVP) Fixed Header (4 Bytes)
 */
/**
 * @brief UVP Routing Context (Application Layer)
 */
typedef struct {
    uint16_t src_addr;     /**< Source unicast address.
                           *   0x0001 = host command, MESHX_ADDR_UNASSIGNED = TXCM timeout */
    uint16_t dst_addr;     /**< Destination unicast or group address */
    uint8_t  tid;          /**< Transaction ID */
    uint8_t  ack_req;      /**< 1 if client requested an ACK */
    uint16_t func_id;      /**< Function ID within the element (REQ-003).
                           *   0xFFFF = broadcast sentinel for TXCM timeout (all models). */
} meshx_uvp_ctx_t;

/**
 * @brief MeshX Unified Vendor Protocol (UVP) Fixed Header (4 Bytes)
 */
typedef struct {
    uint8_t tid;           /**< Transaction ID (0-255) */
    uint8_t ack_req     : 1; /**< ACK Requested Flag (1 = true) */
    uint8_t rfu         : 7; /**< Reserved for Future Use */
    uint16_t type_id;      /**< Function/Type ID (0-65535) */
} __attribute__((packed)) meshx_uvp_header_t;

/**
 * @brief UVP Constants
 */
#define MESHX_COMPANY_ID_UVP    0x7908      /**< MeshX Company ID (0x7908) */
#define MESHX_MODEL_ID_UVP      0x0001      /**< UVP Vendor Model ID (0x0001) */
#define MESHX_VND_MODEL_ID_UVP  ((MESHX_MODEL_ID_UVP << 16) | MESHX_COMPANY_ID_UVP)

#define MESHX_UVP_OPCODE_BASE   0x01
#define MESHX_UVP_OPCODE        ((0xC0 | MESHX_UVP_OPCODE_BASE) | (MESHX_COMPANY_ID_UVP << 8))
#define MESHX_UVP_HEADER_SIZE       sizeof(meshx_uvp_header_t)
#define MESHX_UVP_MAX_PAYLOAD       377  /**< Max total TLV payload bytes (single Segmented Access PDU) */
#define MESHX_UVP_FUNC_ID_PREFIX_SZ 2    /**< Bytes reserved for func_id wire prefix (REQ-004) */
#define MESHX_UVP_MAX_APP_PAYLOAD   (MESHX_UVP_MAX_PAYLOAD - MESHX_UVP_FUNC_ID_PREFIX_SZ) /**< 375 B app payload budget */

/**
 * @brief Send a UVP message.
 * 
 * @param p_model       Pointer to the UVP Vendor Model instance (meshx_model_t*).
 * @param dst_addr      Destination unicast or group address.
 * @param el_idx        Target element index on the destination node.
 * @param type_id       Target element Type ID.
 * @param payload       Pointer to the TLV payload.
 * @param payload_len   Length of the TLV payload.
 * 
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_uvp_send(void *p_model,
                           uint16_t dst_addr,
                           uint16_t type_id,
                           const void *payload,
                           uint16_t payload_len,
                           bool ack_req);

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_UVP_H__ */
