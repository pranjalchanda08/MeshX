/**
 * Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_zgb.h
 * @brief Public interface and contracts for Zero-Gateway Bootstrapping (ZGB).
 *
 * @author Pranjal Chanda
 * @copyright Copyright (c) 2024 - 2026 MeshX
 */

#ifndef __MESHX_ZGB_H__
#define __MESHX_ZGB_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "meshx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MESHX_ERR_PROVISIONING_FAILED
#define MESHX_ERR_PROVISIONING_FAILED ((meshx_err_t)0x02)
#endif

typedef enum {
    MESHX_ZGB_STATE_UNPROVISIONED   = 0x00,
    MESHX_ZGB_STATE_BEACONING       = 0x01,
    MESHX_ZGB_STATE_PROVISIONING    = 0x02,
    MESHX_ZGB_STATE_PROVISIONED     = 0x03,
    MESHX_ZGB_STATE_COMMISSIONING   = 0x04,
    MESHX_ZGB_STATE_SUPPRESSED      = 0x05,
    MESHX_ZGB_STATE_SPLIT_BRAIN     = 0x06,
    MESHX_ZGB_STATE_RESETTING       = 0x07,
} meshx_zgb_state_t;

/* Arbitration Beacon (AB) wire payload (22 bytes payload + 2 bytes CID = 24 bytes mfg data) */
#define MESHX_ZGB_AB_VERSION         0x01
#define MESHX_ZGB_AB_FLAG_ELIGIBLE   0x01
#define MESHX_ZGB_AB_FLAG_COMM       0x02
#define MESHX_ZGB_AB_FLAG_PROV       0x04

typedef struct {
    uint8_t  version;
    uint8_t  flags;
    uint8_t  uuid[16];
    uint8_t  netkey_fp[4];
} __attribute__((packed)) meshx_zgb_ab_payload_t;

/**
 * @brief Initialize Zero-Gateway Bootstrapping subsystem.
 * Arms post-boot jittered timer if node is unprovisioned.
 */
meshx_err_t meshx_zgb_init(void);

/**
 * @brief Check if node is actively in the commission window.
 */
bool meshx_zgb_is_commissioning(void);

/**
 * @brief Get current ZGB state.
 */
meshx_zgb_state_t meshx_zgb_get_state(void);

/**
 * @brief Open self-commissioning window (60s).
 */
meshx_err_t meshx_zgb_open_commission_window(void);

/**
 * @brief Start self-commissioning (opens commission window and stages keys).
 */
meshx_err_t meshx_zgb_start_commissioning(void);

/**
 * @brief Perform atomic self-provisioning as Anchor Master (addr 0x0001).
 */
meshx_err_t meshx_zgb_perform_self_provision(void);

/**
 * @brief Abort active self-commissioning window.
 */
meshx_err_t meshx_zgb_abort_commissioning(void);

/**
 * @brief External Provisioning Priority Guard callback.
 * Invoked on MESHX_NODE_PROV_LINK_OPEN_EVT / CONTROL_TASK_MSG_EVT_IDENTIFY_START.
 * Immediately aborts self-commissioning, discards generated keys, cancels timers,
 * and accepts external credentials.
 */
meshx_err_t meshx_zgb_on_external_prov_start(void);

/**
 * @brief Notify ZGB that provisioning completed.
 */
meshx_err_t meshx_zgb_on_provision_complete(uint16_t assigned_addr);

/**
 * @brief Absorb a passive provisionee node via its native Prov Server.
 * Direct in-range: drives PB-ADV PDUs to target Prov Server.
 * Out-of-range: tunnels via existing UVP RPR opcodes (0xC0..0xC3).
 * Allocates from unified pool 0x0002..0x7FFF. Zero new UVP opcodes.
 *
 * @param[in]  target_uuid   Pointer to 16-byte UUID of target node.
 * @param[in]  trait_mask    Target semantic trait mask.
 * @param[in]  element_count Number of elements on target.
 * @param[out] out_addr      Assigned unicast address from pool (0x0002..0x7FFF).
 */
meshx_err_t meshx_zgb_absorb_case1_target(const meshx_uuid_t *target_uuid,
                                          uint16_t trait_mask,
                                          uint8_t element_count,
                                          uint16_t *out_addr);

/**
 * @brief Case 3: Process incoming ZGB Arbitration Beacon (AB) or commissioning beacon.
 * Performs min-UUID listen-before-founding arbitration and split-brain detection.
 *
 * @param[in] peer_uuid         Pointer to 16-byte UUID of reporting peer.
 * @param[in] flags             AB flags (MESHX_ZGB_AB_FLAG_*)
 * @param[in] peer_netkey_fp    4-byte NetKey fingerprint (or NULL if unprovisioned)
 * @return MESHX_SUCCESS on normal processing, or error code.
 */
meshx_err_t meshx_zgb_process_arbitration_beacon(const meshx_uuid_t *peer_uuid,
                                                 uint8_t flags,
                                                 const uint8_t peer_netkey_fp[4]);

/**
 * @brief Split-brain detection & healing handler.
 * Called when an active foreign anchor is confirmed.
 *
 * @param[in] peer_uuid  Pointer to 16-byte UUID of colliding anchor.
 * @return MESHX_SUCCESS if handled (survived or reset triggered).
 */
meshx_err_t meshx_zgb_handle_split_brain(const meshx_uuid_t *peer_uuid);

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_ZGB_H__ */

