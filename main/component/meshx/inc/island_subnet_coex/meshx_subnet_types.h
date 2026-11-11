/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_subnet_types.h
 * @brief Core constants, wire formats, and packed structs for Island Subnet Coexistence.
 */

#ifndef MESHX_SUBNET_TYPES_H
#define MESHX_SUBNET_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "meshx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MESHX_PACKED
#if defined(__GNUC__) || defined(__clang__)
#define MESHX_PACKED __attribute__((packed))
#else
#define MESHX_PACKED
#endif
#endif

/* =========================================================================
 * Addressing, Slot Assignment, & NAT Constants (TRD Pages 2, 3, 11)
 * ========================================================================= */
#define MESHX_SLOT_MAX                    14U
#define MESHX_MAX_BRIDGING_SLOTS          4U
#define MESHX_NET_IDX_INVALID             0xFFFFU
#define MESHX_GLOBAL_SUBNET_INVALID       0x00000000UL

#define MESHX_NAT_TIER1_BASE              0x4000U
#define MESHX_NAT_TIER1_STRIDE            1024U
#define MESHX_NAT_TIER1_END               0x77FFU
#define MESHX_NAT_TIER2_START             0x7800U
#define MESHX_NAT_TIER2_END               0x7FFFU
#define MESHX_NAT_TIER2_CAP               16U
#define MESHX_NAT_TEMP_TTL_MIN            5U      /**< 300s / 60s ephemeral discovery NAT TTL */

#define MESHX_GROUP_BAND_START            0xC400U /**< Band 0 start: 0xC000 + ((0 + 1) << 10) */
#define MESHX_GROUP_BAND_END              0xFBFFU /**< Band 13 end:  0xC000 + ((13 + 1) << 10) + 0x03FF */
#define MESHX_GROUP_OFFSET_MASK           0x03FFU

#define MESHX_DISCOVERY_PROXY_ADDR        0x8000U /**< Dedicated virtual discovery relay address */

#define MESHX_NAT_FLAG_VALID              0x01U
#define MESHX_NAT_FLAG_PERSISTENT         0x02U
#define MESHX_NAT_FLAG_TEMP_DISCOVERY     0x04U

#define MESHX_SUBNET_ERR_NAT_FULL         (-101)
#define MESHX_SUBNET_ERR_REPLAY           (-102)
#define MESHX_SUBNET_ERR_AUTH             (-103)
#define MESHX_SUBNET_ERR_QUARANTINE       (-104)
#define MESHX_SUBNET_ERR_WHITELIST        (-105)
#define MESHX_SUBNET_ERR_LOOP             (-106)
#define MESHX_SUBNET_ERR_MTU              (-107)

/* =========================================================================
 * 2-Stage Store-and-Forward Router & NACK Constants (TRD Pages 4, 7, 10)
 * ========================================================================= */
#define MESHX_ROUTER_QUEUE_CAPACITY       4U
#define MESHX_MAX_UPPER_PDU_LEN           108U
#define MESHX_MAX_MULTIHOP_APP_PDU_LEN    85U
#define MESHX_EMERGENCY_NACK_BUF_LEN      64U
#define MESHX_STAGE2_MAX_RETRIES          3U
#define MESHX_SUBNET_NACK_TIMEOUT_MS      3000U

#define MESHX_TRANSIT_DEFAULT_HOP_LIMIT   4U
#define MESHX_TRANSIT_ECHO_FILTER_SIZE    32U
#define MESHX_TRANSIT_ECHO_TTL_MS         5000U
#define MESHX_TRANSIT_MAGIC_VER           0x41U   /**< Version 1 Transit encapsulation */

#define MESHX_ROUTER_FLAG_PENDING_ACK     0x01U
#define MESHX_ROUTER_FLAG_TX_IN_FLIGHT    0x02U
#define MESHX_ROUTER_FLAG_IS_UNICAST      0x04U
#define MESHX_ROUTER_FLAG_NACK_PENDING    0x08U
#define MESHX_ROUTER_FLAG_MULTIHOP        0x10U
#define MESHX_ROUTER_FLAG_DIRECT_TX       0x20U

#define MESHX_NACK_REASON_QUEUE_FULL      0x01U
#define MESHX_NACK_REASON_NO_ROUTE        0x02U
#define MESHX_NACK_REASON_NAT_FULL        0x03U
#define MESHX_NACK_REASON_WHITELIST_DROP  0x04U
#define MESHX_NACK_REASON_STAGE2_FAIL     0x05U
#define MESHX_NACK_REASON_HOP_EXCEEDED    0x06U

/* =========================================================================
 * Key Rotation & Crypto Constants (TRD Pages 6, 8)
 * ========================================================================= */
typedef enum {
    MESHX_SUBNET_KR_PHASE_NORMAL    = 0,
    MESHX_SUBNET_KR_PHASE_DIST      = 1,
    MESHX_SUBNET_KR_PHASE_SWITCH    = 2,
    MESHX_SUBNET_KR_PHASE_REVOKE    = 3,
} meshx_kr_phase_t;

#define MESHX_SEQ_IV_GUARD_LIMIT          0x00F00000UL
#define MESHX_HMAC_TAG_LEN                16U
#define MESHX_ANTI_REPLAY_CACHE_SIZE      16U
#define MESHX_BRIDGE_NONCE_CHKPT_STEP     1024UL
#define MESHX_BRIDGE_NONCE_BOOT_MARGIN    10000UL
#define MESHX_BRIDGE_NONCE_WRAP_LIMIT     0xFFFF0000UL
#define MESHX_RPL_CACHE_SIZE              32U
#define MESHX_TRANSIT_WHITELIST_MAX       8U
#define MESHX_CLUSTER_AUTH_HDR_LEN        11U     /**< Opcode(1)+Term(4)+Sender(2)+Nonce(4) */
#define MESHX_CLUSTER_AUTH_OVERHEAD       (MESHX_CLUSTER_AUTH_HDR_LEN + MESHX_HMAC_TAG_LEN) /* 27B */

/* =========================================================================
 * Discovery, Witness, & Redundancy Constants (TRD Pages 5, 7, 10)
 * ========================================================================= */
#define MESHX_MIC_FAIL_WINDOW_MS          30000U
#define MESHX_MIC_FAIL_THRESHOLD          3U
#define MESHX_WITNESS_MIN_RSSI_DBM        (-82)
#define MESHX_WITNESS_MIN_HEAP_BYTES      4096U
#define MESHX_DISCOVERY_TX_PENDING_MAX    8U
#define MESHX_PAIR_RECORD_MAX             8U

#define MESHX_BRIDGE_BOOT_GRACE_MS        6000U
#define MESHX_BRIDGE_HEARTBEAT_MS         4000U
#define MESHX_BRIDGE_WATCHDOG_MS          12000U
#define MESHX_BRIDGE_DEMOTE_GRACE_MS      100U
#define MESHX_BRIDGE_RANK_BACKOFF_STEP_MS 400U
#define MESHX_ROUTE_ADV_TABLE_SIZE        8U
#define MESHX_ROUTE_ADV_INTERVAL_MS       30000U
#define MESHX_ROUTE_ADV_TTL_MS            90000U

typedef enum {
    MESHX_BRIDGE_ROLE_IDLE     = 0,
    MESHX_BRIDGE_ROLE_STANDBY  = 1,
    MESHX_BRIDGE_ROLE_ACTIVE   = 2,
    MESHX_BRIDGE_ROLE_DEMOTING = 3,
} meshx_bridge_role_t;

/* =========================================================================
 * Packed Data Structures (TRD Pages 2, 4, 5, 6, 7, 8, 10, 11)
 * ========================================================================= */

/** Tier-2 Dynamic NAT Entry: 19 Bytes (16 entries = 304 Bytes RAM) */
typedef struct MESHX_PACKED {
    uint8_t  uuid[8];           /**< Truncated 64-bit UUID prefix of remote device */
    uint32_t global_subnet_src; /**< Origin island Global Subnet ID */
    uint16_t orig_addr;         /**< True unicast address in origin island */
    uint16_t virtual_addr;      /**< Mapped virtual address 0x7800..0x7FFF (or Tier-1) */
    uint8_t  flags;             /**< MESHX_NAT_FLAG_* */
    uint16_t last_active_min;   /**< LRU activity minute stamp for temp cleanup */
} meshx_nat_entry_t;

/** Cross-Subnet Pair Record: 20 Bytes */
typedef struct MESHX_PACKED {
    uint32_t global_subnet_src;   /**< Source island ID */
    uint32_t global_subnet_dst;   /**< Destination island ID */
    uint16_t local_elem_addr;     /**< Local bound element address */
    uint16_t virtual_remote_addr; /**< Local proxy NAT address representing remote node */
    uint16_t remote_phys_addr;    /**< True physical address in remote island */
    uint16_t func_id;             /**< Bound functional ID */
    uint16_t group_addr;          /**< Band-prefixed group address (0x0000 if unicast) */
    uint8_t  role_flags;          /**< Role bitmask (0x01=Producer, 0x02=Consumer) */
    uint8_t  active;              /**< 1 = active binding, 0 = free */
} meshx_pair_record_t;

/** Multi-Island Transit Header: 23 Bytes (max 85B App PDU + 23B = 108B) */
typedef struct MESHX_PACKED {
    uint8_t  magic_ver;            /**< MESHX_TRANSIT_MAGIC_VER (0x41) */
    uint8_t  flags;                /**< bit 0: is_group, bit 1: nack_req */
    uint8_t  hop_limit;            /**< Decremented per hop; drop when 0 */
    uint8_t  payload_len;          /**< Inner App PDU length (<= 85 B) */
    uint32_t visited_subnets_mask; /**< Bitmask of traversed slot indices / CRC bits */
    uint32_t global_subnet_src;    /**< Origin island ID */
    uint32_t global_subnet_dst;    /**< Final destination island ID */
    uint16_t orig_src_addr;        /**< Origin element address */
    uint16_t orig_dst_addr;        /**< Final physical/group destination address */
    uint16_t tx_tag_crc16;         /**< Disambiguating tag CRC-16 */
    uint8_t  reserved;
} meshx_subnet_transit_hdr_t;

/** Single Route Advertisement Entry: 8 Bytes */
typedef struct MESHX_PACKED {
    uint32_t dest_subnet_id;
    uint8_t  hop_count;
    uint8_t  flags;
    uint16_t route_metric;
} meshx_route_adv_dest_t;

/** UVP 0xD6 Route Advertisement Payload: 35 Bytes */
typedef struct MESHX_PACKED {
    uint8_t                entry_count;
    uint16_t               via_bridge_addr;
    meshx_route_adv_dest_t entries[4];
} meshx_uvp_route_adv_t;

/** UVP Authenticated Envelope Prefix: 11 Bytes */
typedef struct MESHX_PACKED {
    uint8_t  opcode;
    uint32_t term;
    uint16_t sender_addr;
    uint32_t bridge_nonce;
} meshx_uvp_auth_hdr_t;

/** UVP 0xD1 Heartbeat Payload */
typedef struct MESHX_PACKED {
    uint32_t boundary_id;
    uint32_t slot_digest_crc32;
    uint32_t key_epoch;
    uint8_t  kr_phase;
    uint8_t  role;
    uint16_t score;
} meshx_uvp_heartbeat_payload_t;

/** UVP 0xD2 Takeover Claim Payload */
typedef struct MESHX_PACKED {
    uint32_t boundary_id;
    uint16_t score;
    uint16_t candidate_addr;
    uint16_t prev_active_addr;
    uint8_t  rank;
    uint8_t  reason;
} meshx_uvp_takeover_payload_t;

/** UVP 0xDB Demote Payload */
typedef struct MESHX_PACKED {
    uint32_t boundary_id;
    uint16_t winner_addr;
    uint16_t winner_score;
    uint32_t winning_term;
} meshx_uvp_demote_payload_t;

/** UVP 0xD3 Candidate Report Payload */
typedef struct MESHX_PACKED {
    uint32_t observed_subnet_id;
    uint32_t observer_subnet_id;
    int8_t   rssi_dbm;
    uint8_t  mains_powered;
    uint16_t free_heap_kb;
    uint8_t  candidate_score;
    uint8_t  uuid_prefix[4];
} meshx_uvp_candidate_report_t;

/** UVP 0xD4 DevKey-Encrypted Invite Cleartext Payload */
typedef struct MESHX_PACKED {
    uint8_t  cluster_bridge_key[16]; /**< K_bridge */
    uint32_t cluster_epoch;
    uint16_t target_net_idx;
    uint32_t target_global_subnet_id;
    uint8_t  target_subnet_key[16];
    uint8_t  assigned_slot_n;
    uint8_t  assigned_role;          /**< 1=Standby, 2=Active */
    uint32_t boundary_id;
} meshx_uvp_bridge_invite_plain_t;

/** UVP 0xDA DevKey-Encrypted Invite Ack Cleartext Payload */
typedef struct MESHX_PACKED {
    uint32_t boundary_id;
    uint16_t bridge_addr;
    uint8_t  status;
    uint32_t committed_epoch;
} meshx_uvp_bridge_invite_ack_plain_t;

/** UVP 0xD5 NAT Sync Payload */
typedef struct MESHX_PACKED {
    meshx_nat_entry_t entry;
    uint8_t           op; /**< 1 = Upsert, 2 = Delete */
} meshx_uvp_nat_sync_payload_t;

/** UVP 0xD7 Router NACK & 0xD9 NACK-ACK Payloads */
typedef struct MESHX_PACKED {
    uint32_t nack_id;
    uint32_t orig_seq_auth;
    uint32_t global_subnet_src;
    uint32_t global_subnet_dst;
    uint16_t orig_src_addr;
    uint16_t orig_dst_addr;
    uint8_t  reason_code;
} meshx_uvp_router_nack_t;

typedef struct MESHX_PACKED {
    uint32_t nack_id;
    uint16_t responder_addr;
    uint8_t  status;
} meshx_uvp_router_nack_ack_t;

/** UVP 0xD8 Epoch Sync Payload */
typedef struct MESHX_PACKED {
    uint32_t global_subnet_id;
    uint32_t iv_index;
    uint8_t  iv_update_flag;
    uint32_t link_epoch;
    uint32_t kr_epoch;
} meshx_uvp_epoch_sync_t;

/** UVP 0xDC / 0xDD / 0xDE Discovery Payloads */
typedef struct MESHX_PACKED {
    uint32_t global_subnet_src;
    uint32_t global_subnet_dst;
    uint16_t requestor_addr;
    uint16_t trans_id;
    uint16_t func_id_filter;
    uint8_t  role_filter;
} meshx_uvp_discover_req_t;

typedef struct MESHX_PACKED {
    uint32_t global_subnet_src;
    uint32_t global_subnet_dst;
    uint16_t requestor_addr;
    uint16_t trans_id;
    uint16_t remote_phys_addr;
    uint16_t virtual_nat_addr;
    uint16_t func_id;
    uint8_t  role;
    uint8_t  elem_count;
} meshx_uvp_discover_rsp_t;

typedef struct MESHX_PACKED {
    uint32_t requester_subnet_id;
    uint16_t requestor_addr;
    uint8_t  start_idx;
} meshx_uvp_dir_query_t;

/** Per-Subnet Crypto Slot (4 slots static) */
typedef struct MESHX_PACKED {
    uint16_t net_idx;
    uint32_t global_subnet_id;
    uint32_t key_epoch;
    uint8_t  kr_phase;
    uint8_t  old_net_key[16];
    uint8_t  new_net_key[16];
    uint8_t  active_net_key_flag; /**< 0 = old_net_key, 1 = new_net_key */
    uint32_t iv_index;
    uint8_t  iv_update_flag;
    uint32_t last_seq_num;
} meshx_subnet_crypto_slot_t;

/** Dynamic Local Slot Assignment Node: 13 Bytes on 32-bit MCU */
typedef struct MESHX_PACKED meshx_local_slot_entry {
    struct meshx_local_slot_entry *next;
    uint32_t global_subnet_id;
    uint16_t local_net_idx;
    uint8_t  slot_n;
    uint8_t  flags;
    uint8_t  ref_count;
} meshx_local_slot_entry_t;

/** Local Slot Registry Header: 12 Bytes on 32-bit MCU */
typedef struct MESHX_PACKED {
    meshx_local_slot_entry_t *head;
    uint8_t                   active_count;
    uint8_t                   coordinator_slot;
    uint16_t                  coordinator_addr;
    uint32_t                  table_digest_crc32;
} meshx_local_slot_registry_t;

/* Compile-time size verification of fixed-width wire & RAM records */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(sizeof(meshx_nat_entry_t) == 19, "meshx_nat_entry_t must be 19 bytes");
_Static_assert(sizeof(meshx_pair_record_t) == 20, "meshx_pair_record_t must be 20 bytes");
_Static_assert(sizeof(meshx_subnet_transit_hdr_t) == 23, "meshx_subnet_transit_hdr_t must be 23 bytes");
_Static_assert(sizeof(meshx_uvp_route_adv_t) == 35, "meshx_uvp_route_adv_t must be 35 bytes");
_Static_assert(sizeof(meshx_uvp_auth_hdr_t) == 11, "meshx_uvp_auth_hdr_t must be 11 bytes");
#endif

/** Shared CRC-16-CCITT & CRC-32 helpers */
uint16_t meshx_subnet_crc16(const uint8_t *data, size_t len);
uint32_t meshx_subnet_crc32(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* MESHX_SUBNET_TYPES_H */

