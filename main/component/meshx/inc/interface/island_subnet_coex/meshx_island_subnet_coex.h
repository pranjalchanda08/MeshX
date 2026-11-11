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
 * @brief Initialize the island subnet coexistence feature.
 *
 * @return MESHX_SUCCESS on success (or clean no-op if disabled), error code otherwise.
 */
meshx_err_t meshx_island_subnet_coex_init(void);

/**
 * @brief De-initialize the island subnet coexistence subsystem (clean reset for teardown/tests).
 */
void meshx_island_subnet_coex_deinit(void);

/**
 * @brief Check if island subnet coexistence is enabled.
 *
 * @return true if the feature is enabled, false otherwise.
 */
bool meshx_is_island_subnet_coex_enabled(void);

/**
 * @brief Enable or disable cross-subnet bridging rules synced via PAL.
 *
 * @param[in] enable true to enable forwarding across configured rules, false to pause.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_coex_pal_bridge_set(bool enable);

/**
 * @brief Register a cross-subnet bridging rule from the Platform Abstraction Layer.
 *
 * @param[in] rule Pointer to bridging rule descriptor.
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
 * @param[in] opcode      UVP opcode (0xD1..0xDE).
 * @param[in] src_addr    Source unicast address.
 * @param[in] dst_addr    Destination unicast/group/virtual address.
 * @param[in] net_idx     Ingress BLE Mesh NetKey Index.
 * @param[in] payload     Raw UVP payload starting with opcode byte.
 * @param[in] payload_len Length in bytes.
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
void meshx_subnet_coex_record_rx_mic_fail(uint32_t now_ms);

/**
 * @brief Reset the sliding MIC failure counter upon valid local packet reception.
 */
void meshx_subnet_coex_record_rx_success(void);

/**
 * @brief Notify the coexistence witness engine of a raw scanned BLE manufacturer beacon.
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
 * @brief Summary record returned by meshx_subnet_coex_bridge_list() for MXCP_CMD_SUBNET_LIST.
 */
typedef struct {
    uint32_t global_subnet_id;
    uint16_t local_net_idx;
    uint8_t  slot_n;
    uint8_t  kr_phase;
    uint8_t  role;
    uint32_t key_epoch;
    uint32_t boundary_id;
    uint16_t active_bridge_addr;
} meshx_subnet_info_t;

/**
 * @brief Transport TX hook registered by BLE Mesh UVP layer (or test harness) for 0xD1..0xDE frames.
 */
typedef meshx_err_t (*meshx_subnet_uvp_tx_fn_t)(uint16_t net_idx,
                                              uint16_t dst_addr,
                                              uint8_t  opcode,
                                              const uint8_t *payload,
                                              uint16_t payload_len,
                                              bool     ack_req,
                                              void    *user_ctx);

void meshx_subnet_coex_register_uvp_tx(meshx_subnet_uvp_tx_fn_t tx_cb, void *user_ctx);

/**
 * @brief Periodic coexistence scheduler tick (heartbeat, watchdog failover, Stage 2 pump, Tier-2 GC).
 */
meshx_err_t meshx_subnet_coex_tick(uint32_t now_ms);

/**
 * @brief Notify coexistence module that a local subnet Key Rotation transitioned phase.
 */
meshx_err_t meshx_subnet_coex_on_kr_transition(uint16_t net_idx,
                                               uint8_t new_phase,
                                               const uint8_t new_key[16],
                                               uint32_t new_epoch);

/**
 * @brief MXCP runtime control C APIs (TASK-18).
 */
meshx_err_t meshx_subnet_coex_bridge_list(meshx_subnet_info_t *out_items,
                                          size_t max_items,
                                          size_t *out_count,
                                          uint32_t *out_cluster_epoch,
                                          uint32_t *out_active_term);

meshx_err_t meshx_subnet_coex_bridge_add(uint16_t net_idx,
                                         const uint8_t net_key[16],
                                         uint32_t global_subnet_id,
                                         uint8_t role);

meshx_err_t meshx_subnet_coex_bridge_del(uint16_t net_idx, bool force_purge);

meshx_err_t meshx_subnet_coex_rule_set(const meshx_bridge_rule_t *rule,
                                       bool enable,
                                       uint32_t group_forward_bitmap);

meshx_err_t meshx_subnet_coex_cluster_key_update(const uint8_t k_bridge[16],
                                                 uint32_t new_cluster_epoch,
                                                 uint32_t new_term);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* MESHX_ISLAND_SUBNET_COEX_H */
