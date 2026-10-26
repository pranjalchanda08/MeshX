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
#include "pairing/meshx_group_allocator.h"

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

#define MESHX_UVP_HMAC_LEN              16      /**< HMAC-SHA256 truncated to 16 bytes */
#define MESHX_KEY_LEN                   16      /**< Network Key length */

/**
 * @brief Re-Healing & Redundancy Protocol Constants
 */
#define MESHX_UVP_MASTER_ADDR           0x0001  /**< Master Provisioner unicast address */
#define MESHX_CCM_MIC_LEN               4       /**< AES-128-CCM MIC/tag length (bytes) */
#define MESHX_CCM_NONCE_LEN             13      /**< AES-128-CCM nonce length (bytes) */
#define MESHX_HMAC_SHA256_FULL_LEN      32      /**< Full HMAC-SHA256 output length (bytes) */
#define MESHX_UVP_REHEAL_BEACON_PAYLOAD_LEN 39  /**< 0xCE beacon payload: msg_type(1)+UUID(16)+addr(2)+nonce(4)+HMAC(16) */

/* Inverse-RSSI Backoff Constants (TRD Page 7 Section 3 & TASK-BPROV-004) */
#define MESHX_BACKOFF_MIN_DELAY_MS      20U
#define MESHX_BACKOFF_RSSI_REF_DBM      (-50)
#define MESHX_BACKOFF_SLOPE_MULTIPLIER  8U
#define MESHX_BACKOFF_JITTER_MAX_MS     40U

/* Key Rotation RFC 1982 Constants (RFC 1982 Section 3.2) */
#define MESHX_KR_RFC1982_MODULO     256U
#define MESHX_KR_RFC1982_HALF_SPACE 128U

/**
 * @brief Calculate proxy inverse-RSSI backoff delay (TRD Page 7 Section 3).
 *
 * Formula: T_backoff = max(20 ms, ((-50 - RSSI) * 8) + rand(0, 40) ms).
 * Ensures proxy physically closest to the unprovisioned device responds first.
 *
 * @param[in] rssi Received signal strength indicator of the unprovisioned beacon.
 * @param[in] jitter_ms Random jitter in range [0, 40] ms.
 * @return Delay in milliseconds before proxy transmits lease request or beacon report.
 */
static inline uint32_t meshx_uvp_calc_proxy_backoff_ms(int8_t rssi, uint8_t jitter_ms)
{
    int32_t diff = MESHX_BACKOFF_RSSI_REF_DBM - (int32_t)rssi;
    int32_t delay = (diff > 0) ? (diff * (int32_t)MESHX_BACKOFF_SLOPE_MULTIPLIER) : 0;
    delay += (int32_t)(jitter_ms % (MESHX_BACKOFF_JITTER_MAX_MS + 1U));
    return (delay < (int32_t)MESHX_BACKOFF_MIN_DELAY_MS) ? MESHX_BACKOFF_MIN_DELAY_MS : (uint32_t)delay;
}

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
    MESHX_SYS_CMD_PAIR_ANNOUNCE              = 0x0E, /**< Autonomous pairing beacon announcement */

    /* UVP Remote Provisioning (RPR) & Advanced Security Management (0xC0..0xC9) */
    MESHX_UVP_OP_RPR_SCAN_REQ                = 0xC0, /**< Master -> Mesh: scan for unprov beacons */
    MESHX_UVP_OP_RPR_BEACON_REPORT           = 0xC1, /**< Proxy -> Master: report unprov device beacon */
    MESHX_UVP_OP_RPR_TUNNEL_DOWN             = 0xC2, /**< Master -> Proxy: downlink provisioning PDU to device */
    MESHX_UVP_OP_RPR_TUNNEL_UP               = 0xC3, /**< Proxy -> Master: uplink device reply PDU */
    MESHX_UVP_OP_KR_DISTRIBUTE               = 0xC4, /**< Master -> Mesh: KR Phase 1 distribute new key */
    MESHX_UVP_OP_KR_SWITCHOVER               = 0xC5, /**< Master -> Mesh: KR Phase 2 TX switchover */
    MESHX_UVP_OP_KR_REVOKE_OLD               = 0xC6, /**< Master -> Mesh: KR Phase 3 revoke old key */
    MESHX_UVP_OP_REDUNDANCY_SYNC             = 0xC7, /**< Master <-> Standby: heartbeat & state replication */
    MESHX_UVP_OP_INFILL_LEASE_REQ            = 0xC8, /**< Saturated Proxy -> Master: request 10% reserve address lease */
    MESHX_UVP_OP_FAST_PROV_REPORT            = 0xC9, /**< Proxy -> Master: fast prov progress / quota retirement report */
    MESHX_UVP_OP_KR_KEY_REQ                  = 0xCA, /**< Straggler -> Master: orphan unicast pull request */
    MESHX_UVP_OP_KR_KEY_CONFIRM              = 0xCB, /**< Straggler -> Master: orphan key confirm */
    MESHX_UVP_OP_INFILL_LEASE_RSP            = 0xCC, /**< Master -> Saturated Proxy: grant 10% reserve address lease */
    MESHX_UVP_OP_REDUNDANCY_CLAIM            = 0xCD, /**< Master -> Broadcast: claim standby candidate */
    MESHX_UVP_OP_REHEAL_BEACON_REQ           = 0xCE, /**< Evicted Node -> BLE Adv: DevKey-signed re-healing beacon */
    MESHX_UVP_OP_REHEAL_ADMIT_RSP            = 0xCF, /**< Master -> Evicted Node: DevKey-encrypted re-admission */
    MESHX_UVP_OP_REDUNDANCY_CLAIM_ACK        = 0xD0, /**< Candidate -> Master: accept standby duty */
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
    MESHX_SYS_STATUS_ERR_BUSY              = 0x06,
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
 * @brief Wire format for topology upgrade acknowledgement
 */
typedef struct {
    uint8_t msg_type;                           /**< MESHX_SYS_RSP_UPGRADE_ACK */
    uint8_t status;                             /**< meshx_sys_status_t */
} MESHX_PACKED meshx_uvp_upgrade_rsp_t;

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
 * @brief Wire format for Key Rotation Phase 1 (0xC4) - Distribute New NetKey.
 * Broadcast to company secret virtual address MESHX_MGMT_VIRTUAL_ADDR (0x8F42).
 */
typedef struct {
    uint32_t epoch;                         /**< Monotonically increasing epoch (RFC 1982) */
    uint8_t  new_key[MESHX_KEY_LEN];        /**< 16-byte K_new NetKey payload */
} MESHX_PACKED uvp_key_phase1_prepare_t;

/**
 * @brief Wire format for Key Rotation Phase 2 (0xC5) - Switchover Primary TX Key.
 */
typedef struct {
    uint32_t epoch;                         /**< Target rotation epoch to activate */
} MESHX_PACKED uvp_key_phase2_activate_t;

/**
 * @brief Wire format for Key Rotation Phase 3 (0xC6) - Revoke Old NetKey.
 */
typedef struct {
    uint32_t epoch;                         /**< Target rotation epoch to finalize */
} MESHX_PACKED uvp_key_phase3_revoke_t;


/**
 * @brief Compare Key Rotation epochs using RFC 1982 serial number arithmetic (TRD Page 4 Section 3).
 *
 * Formula: ((new_epoch - curr_epoch) % 256) < 128.
 * Protects against delayed or replayed packets while allowing safe wrap-around.
 *
 * @param[in] new_epoch  Candidate epoch received over the air.
 * @param[in] curr_epoch Current locally committed epoch from NVS.
 * @return true if new_epoch is forward in time relative to curr_epoch, false otherwise.
 */
static inline bool meshx_kr_epoch_is_newer(uint32_t new_epoch, uint32_t curr_epoch)
{
    uint32_t diff = (new_epoch - curr_epoch) % MESHX_KR_RFC1982_MODULO;
    return (diff > 0U && diff < MESHX_KR_RFC1982_HALF_SPACE);
}

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
 * @brief Wire format for node reset teardown broadcast notice
 */
typedef struct {
    uint8_t  msg_type;                          /**< MESHX_SYS_NOTIFY_NODE_RESET */
    uint16_t reset_node_addr;                   /**< Primary unicast address of resetting node */
} MESHX_PACKED meshx_uvp_notify_node_reset_t;

/**
 * @brief Wire format for dynamic composition change broadcast notice
 */
typedef struct {
    uint8_t  msg_type;                          /**< MESHX_SYS_NOTIFY_COMP_CHANGE */
    uint16_t node_addr;                         /**< Primary unicast address of mutated node */
} MESHX_PACKED meshx_uvp_notify_comp_change_t;

/**
 * @brief Wire format for autonomous pairing beacon announcement
 */
typedef struct {
    uint8_t  msg_type;                          /**< MESHX_SYS_CMD_PAIR_ANNOUNCE */
    uint16_t unicast_addr;                      /**< Announcing node primary unicast address */
    uint8_t  elem_idx;                          /**< Announcing element index */
    uint16_t variant;                           /**< Element variant */
    uint8_t  role;                              /**< Role: PRODUCER (0x02) or CONSUMER (0x01) */
    int8_t   min_rssi;                          /**< Minimum RSSI threshold (e.g. -50 dBm) */
    uint8_t  seq_num;                           /**< Sequence number for deduplication */
} MESHX_PACKED meshx_uvp_pair_announce_t;

/**
 * @brief Payload for MESHX_UVP_OP_REHEAL_BEACON_REQ (0xCE)
 */
typedef struct {
    meshx_uuid_t target_uuid;                   /**< 16-byte Device UUID */
    uint16_t previous_addr;                      /**< Last known 16-bit unicast address */
    uint32_t timestamp_nonce;                    /**< Monotonic counter / timestamp preventing replay */
    uint8_t  hmac_signature[MESHX_UVP_HMAC_LEN]; /**< HMAC-SHA256 truncated to 16 bytes over [UUID || Addr || Nonce] */
} MESHX_PACKED meshx_uvp_reheal_req_t;

/**
 * @brief Plaintext payload for MESHX_UVP_OP_REHEAL_ADMIT_RSP (0xCF) prior to DevKey encryption
 */
typedef struct {
    uint8_t  active_netkey[MESHX_KEY_LEN]; /**< Current active Network Key */
    uint32_t active_epoch;                 /**< Current key rotation epoch */
    uint16_t assigned_addr;                /**< Assigned or restored unicast address */
} MESHX_PACKED meshx_uvp_reheal_rsp_plain_t;

/**
 * @brief Subtypes for MESHX_UVP_OP_REDUNDANCY_SYNC (0xC7).
 */
typedef enum {
    MESHX_REDUND_SYNC_HEARTBEAT_FV = 0x01, /**< Master heartbeat & State Freshness Vector */
    MESHX_REDUND_SYNC_ADDR_DELTA   = 0x02, /**< Single 32-byte chunk address delta update */
    MESHX_REDUND_SYNC_DEVKEY_MIRROR= 0x03, /**< Node DevKey mirror envelope */
    MESHX_REDUND_SYNC_BULK_SNAPSHOT= 0x04, /**< Full state snapshot request / transfer */
} meshx_uvp_redund_sync_subtype_t;

/**
 * @brief Wire format for MESHX_UVP_OP_REDUNDANCY_SYNC (0xC7) - Subtype 0x01 (Heartbeat / Freshness Vector).
 */
typedef struct {
    uint8_t                  subtype;            /**< MESHX_REDUND_SYNC_HEARTBEAT_FV (0x01) */
    uint16_t                 sender_addr;        /**< Master or Standby sender unicast address */
    meshx_uuid_t             sender_uuid;        /**< Sender UUID for arbitration */
    meshx_freshness_vector_t freshness_vector;   /**< Current local state freshness vector */
} MESHX_PACKED meshx_uvp_redund_sync_heartbeat_t;

/**
 * @brief Wire format for MESHX_UVP_OP_REDUNDANCY_SYNC (0xC7) - Subtype 0x02 (Addr Map Delta).
 */
typedef struct {
    uint8_t  subtype;            /**< MESHX_REDUND_SYNC_ADDR_DELTA (0x02) */
    uint16_t chunk_idx;          /**< Chunk index (0..255) */
    uint8_t  chunk_data[MESHX_CHUNK_BYTE_SIZE]; /**< 32-byte chunk bitmap (2 bits per address) */
} MESHX_PACKED meshx_uvp_redund_sync_delta_t;

/**
 * @brief Payload for MESHX_UVP_OP_REDUNDANCY_CLAIM (0xCD)
 */
typedef struct {
    uint32_t claim_epoch;       /**< Master claim epoch */
    uint16_t master_addr;       /**< Master unicast address */
    uint16_t next_free_addr;    /**< Monotonic allocation high-water mark */
    uint16_t reserved;          /**< Reserved alignment padding */
} MESHX_PACKED meshx_uvp_redundancy_claim_t;

/**
 * @brief Payload for MESHX_UVP_OP_REDUNDANCY_CLAIM_ACK (0xD0)
 */
typedef struct {
    meshx_uuid_t candidate_uuid;/**< Candidate node UUID accepting Standby */
    uint16_t candidate_addr;    /**< Candidate unicast address */
} MESHX_PACKED meshx_uvp_redundancy_claim_ack_t;

/**
 * @brief Payload for MESHX_UVP_OP_INFILL_LEASE_REQ (0xC8).
 * Saturated Proxy -> Master provisioner requesting contiguous address block.
 */
typedef struct {
    uint8_t      requested_count; /**< Number of contiguous addresses requested (e.g. 1-16) */
    uint16_t     proxy_addr;      /**< Unicast address of requesting proxy relay */
    meshx_uuid_t target_uuid;     /**< Unprovisioned device UUID */
} MESHX_PACKED meshx_uvp_infill_lease_req_t;

/**
 * @brief Payload for MESHX_UVP_OP_INFILL_LEASE_RSP (0xCC).
 * Master provisioner -> Proxy granting contiguous block lease.
 */
typedef struct {
    uint32_t     lease_id;        /**< Unique lease identifier for commit/rollback */
    uint16_t     allocated_base;  /**< Starting unicast address of granted block */
    uint8_t      allocated_count; /**< Number of addresses granted (0 if rejected) */
    meshx_uuid_t target_uuid;     /**< UUID confirmed for this lease */
} MESHX_PACKED meshx_uvp_infill_lease_rsp_t;

/**
 * @brief Payload for MESHX_UVP_OP_FAST_PROV_REPORT (0xC9).
 * Proxy -> Master provisioner reporting completion of node provisioning to commit lease.
 */
typedef struct {
    uint32_t     lease_id;        /**< Lease identifier to commit */
    uint16_t     assigned_addr;   /**< Primary unicast address assigned */
    uint8_t      element_count;   /**< Actual element count assigned */
    meshx_uuid_t device_uuid;     /**< UUID of successfully onboarded node */
} MESHX_PACKED meshx_uvp_fast_prov_report_t;


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

/**
 * @brief Start 15s eviction/blindness detection timer (Evicted Node side).
 *
 * Called on node boot or after provisioning to arm the blindness detection
 * watchdog. On expiry, the node transitions to ISOLATED_HEALING and begins
 * broadcasting 0xCE re-healing beacons.
 *
 * @param[in] pdev Pointer to device structure (for UUID and address).
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_reheal_start_eviction_detection(dev_struct_t *pdev);

/**
 * @brief Broadcast 0xCD Redundancy Claim with exponential backoff (Master side).
 *
 * Invites candidate nodes to assume Standby duty. Retries at 2s, 4s, 8s intervals.
 *
 * @param[in] claim_epoch     Monotonic claim epoch.
 * @param[in] next_free_addr  Local allocation high-water mark across unified address space.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_redundancy_claim_start(uint32_t claim_epoch,
                                         uint16_t next_free_addr);

/**
 * @brief Relay-side courier forward of raw 0xCE beacon to Master (TASK-HEAL-005).
 *
 * Called by the platform layer when a raw 0xCE BLE advertisement is received
 * over the air. Applies zero-trust (no local HMAC auth) and rate limiting
 * (1 forward per unique UUID per 30s).
 *
 * @param[in] beacon_payload    Raw 0xCE beacon payload from BLE advertisement.
 * @param[in] beacon_payload_len Length of beacon payload.
 * @param[in] relay_addr        Relay node unicast address (used for CCM nonce).
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_reheal_courier_forward_to_master(const uint8_t *beacon_payload,
                                                   uint16_t beacon_payload_len,
                                                   uint16_t relay_addr);

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_UVP_H__ */
