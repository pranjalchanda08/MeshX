/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_uvp.h
 * @brief MeshX Unified Vendor Protocol (UVP) Definitions.
 *
 * This file defines the UVP frame structure, opcodes, wire protocol structs,
 * and constants used for communication via the single MeshX Vendor Model.
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 */

#ifndef __MESHX_UVP_H__
#define __MESHX_UVP_H__

#include <stdint.h>
#include <stdbool.h>
#include "meshx_common.h"
#include "meshx_manifest.h"
 
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Forward declaration of CID query from product profile.
 */
uint16_t meshx_get_cid(void);

/**
 * @brief Dedicated System Management Function IDs
 */
#define MESHX_FUNC_ID_SYS_DISCOVERY     0xFFFE  /**< Capability manifest queries, enumeration, epoch beacons */
#define MESHX_FUNC_ID_SYS_PAIRING       0xFFFF  /**< Over-the-air pairing requests, link establishment, unpair */

/**
 * @brief UVP Constants & Dynamic Opcode Generation
 */
#define MESHX_MODEL_ID_UVP              0x0001  /**< UVP Vendor Model ID (0x0001) */
#define MESHX_UVP_OPCODE_BASE           0x01    /**< Opcode base identifier within vendor CID namespace */

#define MESHX_UVP_CALC_OPCODE(cid)      ((0xC00000 | (MESHX_UVP_OPCODE_BASE << 16)) | ((cid) & 0xFFFF))
#define MESHX_CALC_VND_MODEL_ID(cid, model_id) (((uint32_t)(model_id) << 16) | ((cid) & 0xFFFF))

/**
 * @brief Compile-time defaults based on Product Profile (CONFIG_CID_ID)
 */
#ifdef CONFIG_CID_ID
#define MESHX_COMPANY_ID_UVP            CONFIG_CID_ID
#else
#define MESHX_COMPANY_ID_UVP            0x7908
#endif

#define MESHX_VND_MODEL_ID_UVP          MESHX_CALC_VND_MODEL_ID(MESHX_COMPANY_ID_UVP, MESHX_MODEL_ID_UVP)
#define MESHX_UVP_OPCODE                MESHX_UVP_CALC_OPCODE(MESHX_COMPANY_ID_UVP)

#define MESHX_UVP_HEADER_SIZE           sizeof(meshx_uvp_header_t)
#define MESHX_UVP_MAX_PAYLOAD           377     /**< Max total TLV payload bytes (single Segmented Access PDU) */
#define MESHX_UVP_FUNC_ID_PREFIX_SZ     2       /**< Bytes reserved for func_id wire prefix (REQ-004) */
#define MESHX_UVP_MAX_APP_PAYLOAD       (MESHX_UVP_MAX_PAYLOAD - MESHX_UVP_FUNC_ID_PREFIX_SZ) /**< 375 B app payload budget */

/**
 * @brief System Management Message Types
 */
typedef enum {
    MESHX_SYS_CMD_DISCOVERY_QUERY            = 0x01, /**< Query peer capabilities */
    MESHX_SYS_RSP_DISCOVERY_REPORT           = 0x02, /**< Peer manifest report */
    MESHX_SYS_CMD_PAIR_REQUEST               = 0x03, /**< Initiate element pairing handshake */
    MESHX_SYS_RSP_PAIR_RESPONSE              = 0x04, /**< Acknowledge / confirm pairing */
    MESHX_SYS_CMD_UNPAIR_REQUEST             = 0x05, /**< Unpair specific element link */
    MESHX_SYS_RSP_UNPAIR_RESPONSE            = 0x06, /**< Acknowledge unpair */
    MESHX_SYS_CMD_EPOCH_PING                 = 0x07, /**< Provisioner epoch audit ping */
    MESHX_SYS_RSP_EPOCH_PONG                 = 0x08, /**< Node epoch reply */
    MESHX_SYS_NOTIFY_NODE_RESET              = 0x09, /**< Pre-reset teardown broadcast */
    MESHX_SYS_NOTIFY_COMP_CHANGE             = 0x0A, /**< Dynamic composition change notice */
    MESHX_SYS_NOTIFY_GROUP_MEMBERSHIP_CHANGE = 0x0B, /**< Real-time group join/leave notice */
    MESHX_SYS_CMD_TOPOLOGY_UPGRADE           = 0x0C, /**< Upgrade 1:1 unicast link to group */
    MESHX_SYS_RSP_UPGRADE_ACK                = 0x0D, /**< Upgrade confirmation */
} meshx_sys_msg_type_t;

/**
 * @brief System Management Status Codes
 */
typedef enum {
    MESHX_SYS_STATUS_SUCCESS               = 0x00,
    MESHX_SYS_STATUS_ERR_INCOMPATIBLE      = 0x01,
    MESHX_SYS_STATUS_ERR_PERSONALITY_DIFF  = 0x02,
    MESHX_SYS_STATUS_ERR_ELEMENT_MISSING   = 0x03,
    MESHX_SYS_STATUS_ERR_TABLE_FULL        = 0x04,
    MESHX_SYS_STATUS_ERR_TIMEOUT           = 0x05,
} meshx_sys_status_t;

/**
 * @brief Wire format for discovery query (Client -> Target Node)
 */
typedef struct {
    uint8_t msg_type;                           /**< MESHX_SYS_CMD_DISCOVERY_QUERY */
    uint8_t filter_role;                        /**< 0x00=All, 0x01=Consumers Only, 0x02=Producers Only */
} MESHX_PACKED meshx_uvp_discovery_query_t;

/**
 * @brief Wire format for discovery response (Target Node -> Client)
 */
typedef struct {
    uint8_t                     msg_type;       /**< MESHX_SYS_RSP_DISCOVERY_REPORT */
    meshx_capability_manifest_t manifest;
} MESHX_PACKED meshx_uvp_discovery_report_t;

/**
 * @brief Wire format for pair request (Initiator -> Target)
 */
typedef struct {
    uint8_t  msg_type;                          /**< MESHX_SYS_CMD_PAIR_REQUEST */
    uint8_t  src_elem_idx;                      /**< Initiator element index */
    uint8_t  dst_elem_idx;                      /**< Target element index */
    uint16_t func_id;                           /**< Function ID to bind */
    uint16_t target_group_addr;                 /**< 0x0000 if 1:1 unicast, or 0xC000-0xC3FF */
    uint16_t sender_unicast;                    /**< Unicast address of sender */
} MESHX_PACKED meshx_uvp_pair_req_t;

/**
 * @brief Wire format for pair response (Target -> Initiator)
 */
typedef struct {
    uint8_t  msg_type;                          /**< MESHX_SYS_RSP_PAIR_RESPONSE */
    uint8_t  status;                            /**< meshx_sys_status_t */
    uint16_t target_group_addr;                 /**< Confirmed group address, or 0x0000 */
} MESHX_PACKED meshx_uvp_pair_rsp_t;

/**
 * @brief Wire format for topology upgrade notice (Switch -> Existing Peer)
 */
typedef struct {
    uint8_t  msg_type;                          /**< MESHX_SYS_CMD_TOPOLOGY_UPGRADE */
    uint8_t  dst_elem_idx;                      /**< Target element index */
    uint16_t func_id;                           /**< Function ID */
    uint16_t group_addr;                        /**< New group address to subscribe to */
} MESHX_PACKED meshx_uvp_upgrade_req_t;

/**
 * @brief Wire format for unpair request
 */
typedef struct {
    uint8_t  msg_type;                          /**< MESHX_SYS_CMD_UNPAIR_REQUEST */
    uint8_t  src_elem_idx;
    uint8_t  dst_elem_idx;
    uint16_t func_id;
    uint16_t target_group_addr;                 /**< 0x0000 if unicast link, or group address */
} MESHX_PACKED meshx_uvp_unpair_req_t;

/**
 * @brief Wire format for unpair response
 */
typedef struct {
    uint8_t msg_type;                           /**< MESHX_SYS_RSP_UNPAIR_RESPONSE */
    uint8_t status;                             /**< meshx_sys_status_t */
} MESHX_PACKED meshx_uvp_unpair_rsp_t;

/**
 * @brief Wire format for epoch ping
 */
typedef struct {
    uint8_t msg_type;                           /**< MESHX_SYS_CMD_EPOCH_PING */
} MESHX_PACKED meshx_uvp_epoch_ping_t;

/**
 * @brief Wire format for epoch pong response
 */
typedef struct {
    uint8_t  msg_type;                          /**< MESHX_SYS_RSP_EPOCH_PONG */
    uint16_t link_epoch;                        /**< Current link epoch */
} MESHX_PACKED meshx_uvp_epoch_pong_t;

/**
 * @brief UVP Routing Context (Application Layer)
 */
typedef struct {
    uint16_t src_addr;                          /**< Source unicast address */
    uint16_t dst_addr;                          /**< Destination unicast or group address */
    uint8_t  tid;                               /**< Transaction ID */
    uint8_t  ack_req;                           /**< 1 if client requested an ACK */
    uint16_t func_id;                           /**< Function ID within the element (REQ-003) */
} meshx_uvp_ctx_t;

/**
 * @brief MeshX Unified Vendor Protocol (UVP) Fixed Header (4 Bytes)
 */
typedef struct {
    uint8_t  tid;                               /**< Transaction ID (0-255) */
    uint8_t  ack_req : 1;                       /**< ACK Requested Flag (1 = true) */
    uint8_t  rfu     : 7;                       /**< Reserved for Future Use */
    uint16_t type_id;                           /**< Function/Type ID (0-65535) */
} MESHX_PACKED meshx_uvp_header_t;

/**
 * @brief Get the active Company ID (CID) for UVP communication.
 *
 * @return 16-bit active Company Identifier from the runtime product profile.
 */
static inline uint16_t meshx_uvp_get_company_id(void)
{
    return meshx_get_cid();
}

/**
 * @brief Get the active UVP 3-octet Vendor Opcode.
 *
 * Calculates the 24-bit Bluetooth Mesh vendor opcode dynamically
 * from the active Company ID: ((0xC00000 | (0x01 << 16)) | CID).
 *
 * @return 24-bit calculated Vendor Opcode.
 */
static inline uint32_t meshx_uvp_get_opcode(void)
{
    return MESHX_UVP_CALC_OPCODE(meshx_get_cid());
}

/**
 * @brief Get the active 32-bit Vendor Model Identifier.
 *
 * Combines the UVP Model ID (0x0001) and the active Company ID:
 * ((0x0001 << 16) | CID).
 *
 * @return 32-bit composite Vendor Model ID.
 */
static inline uint32_t meshx_uvp_get_vnd_model_id(void)
{
    return MESHX_CALC_VND_MODEL_ID(meshx_get_cid(), MESHX_MODEL_ID_UVP);
}

/**
 * @brief Send a UVP message.
 *
 * @param[in] p_model     Pointer to the UVP Vendor Model instance (meshx_model_t*).
 * @param[in] dst_addr    Destination unicast or group address.
 * @param[in] type_id     Target element Type ID.
 * @param[in] payload     Pointer to the TLV payload.
 * @param[in] payload_len Length of the TLV payload.
 * @param[in] ack_req     Whether ACK is requested.
 * @param[in] app_idx     Application key index.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_uvp_send(void       *p_model,
                           uint16_t    dst_addr,
                           uint16_t    type_id,
                           const void *payload,
                           uint16_t    payload_len,
                           bool        ack_req,
                           uint16_t    app_idx);

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_UVP_H__ */
