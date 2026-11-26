/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_island_subnet_coex.h
 * @brief Island Subnet Coexistence Feature Interface.
 *
 * Non-destructive island subnet coexistence bridging and routing between
 * multiple provisioned BLE Mesh islands via virtual proxy addresses,
 * 2-stage store-and-forward routing, multi-subnet KR, and MitM security.
 */

#ifndef MESHX_ISLAND_SUBNET_COEX_H
#define MESHX_ISLAND_SUBNET_COEX_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "meshx_err.h"
#include "island/meshx_subnet_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup island_subnet_coex Island Subnet Coexistence
 * @{
 */

#ifndef MESHX_BRIDGE_RULE_T_DEFINED
#define MESHX_BRIDGE_RULE_T_DEFINED
/**
 * @brief Cross-subnet bridging rule descriptor passed through the Platform Abstraction Layer.
 */
typedef struct {
    uint8_t  directions;    /**< 0x01 = net_idx_1->net_idx_2, 0x02 = reverse, 0x03 = bidirectional */
    uint16_t net_idx_1;     /**< Primary subnet NetKey Index */
    uint16_t net_idx_2;     /**< Secondary subnet NetKey Index */
    uint16_t addr_1;        /**< Boundary endpoint address on net_idx_1 (0xFFFF = wildcard) */
    uint16_t addr_2;        /**< Boundary endpoint address on net_idx_2 (0xFFFF = wildcard) */
} meshx_bridge_rule_t;
#endif /* MESHX_BRIDGE_RULE_T_DEFINED */

/**
 * @brief Summary record returned by meshx_subnet_coex_bridge_list() for MXCP_CMD_SUBNET_LIST.
 */
typedef struct {
    uint32_t global_subnet_id;   /**< 32-bit Global Subnet ID */
    uint16_t local_net_idx;      /**< Local assigned NetKey Index */
    uint8_t  slot_n;             /**< Assigned slot index (0..13) */
    uint8_t  kr_phase;           /**< Current Key Refresh phase (0..3) */
    uint8_t  role;               /**< Bridge role (NONE, STANDBY, ACTIVE, CANDIDATE) */
    uint32_t key_epoch;          /**< Key epoch generation counter */
    uint32_t boundary_id;        /**< Associated boundary link identifier */
    uint16_t active_bridge_addr; /**< Unicast address of active bridge */
} meshx_subnet_info_t;

/**
 * @brief Transport TX hook registered by BLE Mesh UVP layer (or test harness) for 0xD1..0xDE frames.
 *
 * @param[in] net_idx     Target BLE Mesh NetKey index.
 * @param[in] dst_addr    Destination unicast, virtual or group address.
 * @param[in] opcode      UVP opcode byte.
 * @param[in] payload     Raw packet payload.
 * @param[in] payload_len Length of packet payload in bytes.
 * @param[in] ack_req     True if transport layer acknowledgement is required.
 * @param[in] user_ctx    User context pointer passed during registration.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
typedef meshx_err_t (*meshx_subnet_uvp_tx_fn_t)(uint16_t net_idx,
                                              uint16_t dst_addr,
                                              uint8_t  opcode,
                                              const uint8_t *payload,
                                              uint16_t payload_len,
                                              bool     ack_req,
                                              void    *user_ctx);

/**
 * @brief Initialize the island subnet coexistence feature and restore state from NVS.
 *
 * @return MESHX_SUCCESS on success (or clean no-op if disabled), error code otherwise.
 */
meshx_err_t meshx_island_subnet_coex_init(void);

/**
 * @brief De-initialize the island subnet coexistence subsystem (clean reset for teardown/tests).
 */
void meshx_island_subnet_coex_deinit(void);

/**
 * @brief Check if island subnet coexistence is enabled and initialized.
 *
 * @return true if the feature is enabled, false otherwise.
 */
bool meshx_is_island_subnet_coex_enabled(void);

/**
 * @brief Enable or disable cross-subnet bridging rules synced via PAL.
 *
 * @param[in] enable true to enable forwarding across configured rules, false to pause.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_coex_pal_bridge_set(bool enable);

/**
 * @brief Register a cross-subnet bridging rule from the Platform Abstraction Layer.
 *
 * @param[in] rule Pointer to bridging rule descriptor.
 *
 * @return MESHX_SUCCESS on success, MESHX_INVALID_ARG or MESHX_NO_MEM on error.
 */
meshx_err_t meshx_subnet_coex_pal_bridge_rule_add(const meshx_bridge_rule_t *rule);

/**
 * @brief Check whether Key Rotation burst opcodes (0xC4..0xC6) must be quarantined across bridges.
 *
 * @return true if a Cluster Bridge Key (K_bridge) is provisioned and bridging is active.
 */
bool meshx_subnet_coex_is_kr_quarantined(void);

/**
 * @brief Process an incoming UVP management opcode belonging to 0xD1..0xDE.
 *
 * Authenticates envelope using Cluster Bridge Key if present, verifies anti-replay,
 * and routes message body to the appropriate subsystem handler.
 *
 * @param[in] opcode      UVP opcode (0xD1..0xDE).
 * @param[in] src_addr    Source unicast address.
 * @param[in] dst_addr    Destination unicast/group/virtual address.
 * @param[in] net_idx     Ingress BLE Mesh NetKey Index.
 * @param[in] payload     Raw UVP payload starting with opcode byte.
 * @param[in] payload_len Length in bytes.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_coex_handle_uvp(uint8_t opcode,
                                         uint16_t src_addr,
                                         uint16_t dst_addr,
                                         uint16_t net_idx,
                                         const uint8_t *payload,
                                         uint16_t payload_len);

/**
 * @brief Record a local network MIC authentication failure (feeds 30s Sliding Blindness Detector).
 *
 * @param[in] now_ms Current monotonic timestamp in milliseconds.
 */
bool meshx_subnet_coex_record_rx_mic_fail(uint32_t now_ms);

/**
 * @brief Reset the sliding MIC failure counter upon valid local packet reception.
 */
void meshx_subnet_coex_record_rx_success(void);

/**
 * @brief Notify the coexistence witness engine of a raw scanned BLE manufacturer beacon.
 *
 * Evaluates candidate witness eligibility and triggers candidate report if qualified.
 *
 * @param[in] company_id  16-bit SIG Company Identifier.
 * @param[in] payload     Beacon payload bytes.
 * @param[in] payload_len Payload length.
 * @param[in] rssi        Measured RSSI in dBm.
 */
void meshx_subnet_coex_on_beacon_rx(uint16_t company_id,
                                    const uint8_t *payload,
                                    uint16_t payload_len,
                                    int8_t rssi);

/**
 * @brief Register transport callback for outbound UVP coexistence frames.
 *
 * @param[in] tx_cb    Callback function invoked when sending UVP packets.
 * @param[in] user_ctx User context pointer.
 */
void meshx_subnet_coex_register_uvp_tx(meshx_subnet_uvp_tx_fn_t tx_cb, void *user_ctx);

/**
 * @brief Periodic coexistence scheduler tick.
 *
 * Drives Stage 2 store-and-forward retransmissions, active bridge heartbeats,
 * standby watchdog checks, and Tier-2 NAT garbage collection.
 *
 * @param[in] now_ms Current monotonic timestamp in milliseconds.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_coex_tick(uint32_t now_ms);

/**
 * @brief Notify coexistence module that a local subnet Key Rotation transitioned phase.
 *
 * @param[in] net_idx   Subnet NetKey index.
 * @param[in] new_phase New Key Refresh phase (0..3).
 * @param[in] new_key   Pointer to 16-byte NetKey associated with new phase.
 * @param[in] new_epoch Epoch counter.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_coex_on_kr_transition(uint16_t net_idx,
                                               uint8_t new_phase,
                                               const meshx_subnet_key_t *new_key,
                                               uint32_t new_epoch);

/**
 * @brief Query list of configured subnets, slots, roles, and cluster state.
 *
 * @param[out] out_items         Buffer to receive subnet info records.
 * @param[in]  max_items         Capacity of out_items buffer.
 * @param[out] out_count         Pointer to store count of returned records.
 * @param[out] out_cluster_epoch Pointer to store active cluster epoch.
 * @param[out] out_active_term   Pointer to store active term counter.
 *
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG.
 */
meshx_err_t meshx_subnet_coex_bridge_list(meshx_subnet_info_t *out_items,
                                          size_t max_items,
                                          size_t *out_count,
                                          uint32_t *out_cluster_epoch,
                                          uint32_t *out_active_term);

/**
 * @brief Dynamically bind a subnet to a local slot and initialize bridging context.
 *
 * @param[in] net_idx          BLE Mesh NetKey Index.
 * @param[in] net_key          Pointer to 16-byte NetKey structure.
 * @param[in] global_subnet_id Global Subnet ID (or MESHX_GLOBAL_SUBNET_INVALID to derive).
 * @param[in] role             Initial bridge role (e.g. MESHX_BRIDGE_ROLE_ACTIVE).
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_coex_bridge_add(uint16_t net_idx,
                                         const meshx_subnet_key_t *net_key,
                                         uint32_t global_subnet_id,
                                         uint8_t role);

/**
 * @brief Unbind and remove a bridge subnet slot.
 *
 * @param[in] net_idx     BLE Mesh NetKey Index to unbind.
 * @param[in] force_purge If true, flushes queued frames for the associated boundary.
 *
 * @return MESHX_SUCCESS on success, MESHX_NOT_FOUND, or error code.
 */
meshx_err_t meshx_subnet_coex_bridge_del(uint16_t net_idx, bool force_purge);

/**
 * @brief Set or update cross-subnet bridging rule and group forwarding policy.
 *
 * @param[in] rule                  Optional bridging rule to add or update (may be NULL).
 * @param[in] enable                Enable or pause bridging.
 * @param[in] group_forward_bitmap  32-bit bitmap of group addresses allowed for forwarding.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_coex_rule_set(const meshx_bridge_rule_t *rule,
                                       bool enable,
                                       uint32_t group_forward_bitmap);

/**
 * @brief Install or update the shared Cluster Bridge Key (K_bridge).
 *
 * Persists key into NVS before activating in RAM context.
 *
 * @param[in] k_bridge          Pointer to 16-byte Cluster Bridge Key.
 * @param[in] new_cluster_epoch New cluster epoch generation counter.
 * @param[in] new_term          New bridge election term counter.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_coex_cluster_key_update(const meshx_subnet_key_t *k_bridge,
                                                 uint32_t new_cluster_epoch,
                                                 uint32_t new_term);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* MESHX_ISLAND_SUBNET_COEX_H */
