/**
 * Copyright © 2024 - 2026 MeshX
 *
 * @file meshx_virtual_topo.h
 * @brief Dynamic Software-Defined Virtual Topology (SDVT) Switch for OTA Certification & Testing.
 *
 * Provides a runtime-configurable link-layer filter to simulate multi-hop topologies,
 * network partitions, dense contention, lossy links, and path delays over 100% pure Over-The-Air (OTA)
 * hardware benches without requiring physical relocation, RF cables, or external attenuators.
 */

#ifndef __MESHX_VIRTUAL_TOPO_H__
#define __MESHX_VIRTUAL_TOPO_H__

#include <stdint.h>
#include <stdbool.h>
#include "meshx_err.h"
#include "meshx_config_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MESHX_VIRTUAL_TOPO_MAX_PEERS 16U

/**
 * @brief Standard Topological Presets
 */
typedef enum {
    MESHX_TOPO_PRESET_NONE     = 0, /**< Passthrough mode: No filtering, raw OTA behavior */
    MESHX_TOPO_PRESET_LINEAR   = 1, /**< Linear Daisy-Chain (T1): N-hop chain */
    MESHX_TOPO_PRESET_CLIQUE   = 2, /**< High-Density Clique (T2): Mutual visibility with RSSI offsets */
    MESHX_TOPO_PRESET_TREE     = 3, /**< Hierarchical Multi-Branch Tree (T3) */
    MESHX_TOPO_PRESET_DUMBBELL = 4, /**< Dumbbell Dual-Cluster with Bridge Relay (T4) */
    MESHX_TOPO_PRESET_DIAMOND  = 5, /**< Cyclic / Diamond Multi-Path (T5) */
    MESHX_TOPO_PRESET_CUSTOM   = 6  /**< OEM Custom Adjacency Configuration */
} meshx_topo_preset_t;

/**
 * @brief Per-Peer Link Configuration in Virtual Topology Matrix
 */
typedef struct {
    uint16_t peer_addr;       /**< Peer Unicast Address (e.g. 0x0001) */
    int8_t   rssi_offset_db;  /**< Injected virtual attenuation (dB), e.g. -35 dB */
    uint8_t  drop_rate_pct;   /**< Injected probabilistic drop rate (0-100%) */
    uint16_t delay_ms;        /**< Artificial forwarding delay (ms) */
    bool     blocked;         /**< Hard severance flag (true = link severed / partition cut) */
} meshx_topo_peer_cfg_t;

/**
 * @brief Complete Virtual Topology Matrix Configuration
 */
typedef struct {
    bool                  enabled;              /**< Global switch state (true = filtering active) */
    meshx_topo_preset_t   preset;               /**< Active preset identifier */
    uint16_t              own_addr;             /**< Local device unicast address */
    bool                  default_policy_drop;  /**< true = Whitelist mode (drop unlisted peers) */
    uint8_t               peer_count;           /**< Number of configured peers */
    meshx_topo_peer_cfg_t peers[MESHX_VIRTUAL_TOPO_MAX_PEERS]; /**< Peer configuration array */
} meshx_virtual_topo_config_t;

#if CONFIG_MESHX_ENABLE_VIRTUAL_TOPO

/**
 * @brief Initialize the Virtual Topology Switch.
 *
 * Automatically registers the 'topo' CLI command with the MeshX shell.
 *
 * @return MESHX_SUCCESS on success, or appropriate error code.
 */
meshx_err_t meshx_virtual_topo_init(void);

/**
 * @brief De-initialize the Virtual Topology Switch.
 *
 * Disables link filtering, clears peer table, and marks switch uninitialized.
 *
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t meshx_virtual_topo_deinit(void);

/**
 * @brief Query if Virtual Topology Switch has been initialized.
 *
 * @return true if initialized, false otherwise.
 */
bool meshx_virtual_topo_is_initialized(void);


/**
 * @brief Enable or disable the Virtual Topology link filter at runtime.
 *
 * @param[in] enabled Set true to activate link-layer filtering; false for raw OTA passthrough.
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t meshx_virtual_topo_set_enabled(bool enabled);

/**
 * @brief Query if the Virtual Topology Switch is currently active.
 *
 * @return true if filtering is active, false otherwise.
 */
bool meshx_virtual_topo_is_enabled(void);

/**
 * @brief Dynamically apply a standard topological preset.
 *
 * Configures the internal adjacency matrix according to the chosen preset.
 *
 * @param[in] preset    Preset identifier (LINEAR, CLIQUE, TREE, DUMBBELL, DIAMOND, etc.).
 * @param[in] own_addr  Local unicast address to compute neighbor relationships.
 * @return MESHX_SUCCESS on success, or error code on invalid parameters.
 */
meshx_err_t meshx_virtual_topo_apply_preset(meshx_topo_preset_t preset, uint16_t own_addr);

/**
 * @brief Add or update a peer rule dynamically in the virtual matrix.
 *
 * @param[in] peer_addr       Peer Unicast Address.
 * @param[in] rssi_offset_db  Virtual attenuation offset (e.g. -30 for distance simulation).
 * @param[in] drop_rate_pct   Simulated packet loss rate (0 to 100%).
 * @param[in] blocked         Hard cut flag (true = completely disconnect link).
 * @return MESHX_SUCCESS on success, MESHX_ERR_NO_MEM if table is full.
 */
meshx_err_t meshx_virtual_topo_set_peer(uint16_t peer_addr, int8_t rssi_offset_db, uint8_t drop_rate_pct, bool blocked);

/**
 * @brief Remove a peer rule from the virtual matrix.
 *
 * @param[in] peer_addr Peer Unicast Address to remove.
 * @return MESHX_SUCCESS on success, MESHX_ERR_NOT_FOUND if not present.
 */
meshx_err_t meshx_virtual_topo_remove_peer(uint16_t peer_addr);

/**
 * @brief Dynamically sever or heal a bridge link (for Split-Brain partition testing).
 *
 * @param[in] is_partitioned true to sever bridge links, false to heal bridge links.
 * @param[in] bridge_addrs   Array of bridge node addresses.
 * @param[in] bridge_count   Number of bridge addresses in the array.
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t meshx_virtual_topo_set_partition_state(bool is_partitioned, const uint16_t *bridge_addrs, uint8_t bridge_count);

/**
 * @brief Reset the virtual topology configuration to clean, unconfigured state.
 *
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t meshx_virtual_topo_clear(void);

/**
 * @brief Query current snapshot of the Virtual Topology configuration.
 *
 * @param[out] out_cfg Destination pointer to copy current configuration.
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t meshx_virtual_topo_get_config(meshx_virtual_topo_config_t *out_cfg);

/**
 * @brief Link-Layer RX Filter Evaluation Hook.
 *
 * Called synchronously in the BLE Mesh / UVP packet reception path.
 *
 * @param[in]     src_addr   Source unicast address of the incoming frame.
 * @param[in,out] inout_rssi Pointer to raw packet RSSI; modified in-place if virtual offset is set.
 * @return true if packet is allowed to pass to upper layers; false if packet should be silently dropped.
 */
bool meshx_virtual_topo_filter_rx(uint16_t src_addr, int8_t *inout_rssi);

/**
 * @brief Set the local node unicast address for virtual topology math.
 *
 * @param[in] own_addr Unicast address of local node.
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t meshx_virtual_topo_set_own_addr(uint16_t own_addr);

/**
 * @brief Transmit a binary MXCP Virtual Topology status event notification to host.
 *
 * Packs mxcp_evt_virtual_topo_status_t (and peer entries if action == GET_STATUS)
 * and sends it via mxcp_send_event(MXCP_EVT_VIRTUAL_TOPO_STATUS, ...).
 *
 * @param[in] action      The action triggering this notification (mxcp_topo_action_t).
 * @param[in] status_code Status/return code (0 = Success).
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t meshx_virtual_topo_notify_status(uint8_t action, uint8_t status_code);

#else /* !CONFIG_MESHX_ENABLE_VIRTUAL_TOPO (Production Build Stubs - Zero Footprint) */

static inline meshx_err_t meshx_virtual_topo_init(void) { return MESHX_SUCCESS; }
static inline meshx_err_t meshx_virtual_topo_deinit(void) { return MESHX_SUCCESS; }
static inline bool meshx_virtual_topo_is_initialized(void) { return false; }
static inline meshx_err_t meshx_virtual_topo_set_enabled(bool enabled) { (void)enabled; return MESHX_SUCCESS; }
static inline bool meshx_virtual_topo_is_enabled(void) { return false; }
static inline meshx_err_t meshx_virtual_topo_set_own_addr(uint16_t own_addr) { (void)own_addr; return MESHX_SUCCESS; }
static inline meshx_err_t meshx_virtual_topo_apply_preset(meshx_topo_preset_t preset, uint16_t own_addr) { (void)preset; (void)own_addr; return MESHX_SUCCESS; }
static inline meshx_err_t meshx_virtual_topo_set_peer(uint16_t peer_addr, int8_t rssi_offset_db, uint8_t drop_rate_pct, bool blocked) { (void)peer_addr; (void)rssi_offset_db; (void)drop_rate_pct; (void)blocked; return MESHX_SUCCESS; }
static inline meshx_err_t meshx_virtual_topo_remove_peer(uint16_t peer_addr) { (void)peer_addr; return MESHX_SUCCESS; }
static inline meshx_err_t meshx_virtual_topo_set_partition_state(bool is_partitioned, const uint16_t *bridge_addrs, uint8_t bridge_count) { (void)is_partitioned; (void)bridge_addrs; (void)bridge_count; return MESHX_SUCCESS; }
static inline meshx_err_t meshx_virtual_topo_clear(void) { return MESHX_SUCCESS; }
static inline meshx_err_t meshx_virtual_topo_notify_status(uint8_t action, uint8_t status_code) { (void)action; (void)status_code; return MESHX_SUCCESS; }
static inline bool meshx_virtual_topo_filter_rx(uint16_t src_addr, int8_t *inout_rssi) { (void)src_addr; (void)inout_rssi; return true; }

#endif /* CONFIG_MESHX_ENABLE_VIRTUAL_TOPO */

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_VIRTUAL_TOPO_H__ */
