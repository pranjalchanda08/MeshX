/**
 * Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_zgb.c
 * @brief Core Zero-Gateway Bootstrapping (ZGB) State Machine, Triggers & Priority Guard.
 */

#include "zgb/meshx_zgb.h"
#include "meshx_common.h"
#include "meshx_api.h"
#include "meshx_os_timer.h"
#include "meshx_mxcp.h"
#include "meshx_nvs.h"
#include "interface/crypto/meshx_crypto.h"
#include "pairing/meshx_group_allocator.h"
#include "pairing/meshx_unpair_mgr.h"
#include "interface/ble_mesh/server/meshx_ble_mesh_prov_srv.h"
#include "interface/logging/meshx_log.h"
#include "meshx_platform_ble_mesh.h"
#include "interface/meshx_platform.h"
#include <string.h>

#define MESHX_ZGB_COMMISSION_TIMEOUT_MS  60000U /* 60s commission window */
#define MESHX_ZGB_LISTEN_TIMEOUT_MS      10000U /* 10s listen phase before founding (ZGB-019) */
#define MESHX_ZGB_BOOT_JITTER_BASE_MS    25000U /* 25s base */
#define MESHX_ZGB_BOOT_JITTER_RANGE_MS   10001U /* 0..10000ms jitter (25s..35s = 30s +/- 5s) */
#define MESHX_ZGB_MAX_PEERS              8
#define MESHX_ZGB_SPLIT_BRAIN_CONFIRM_CNT 3

typedef struct {
    meshx_uuid_t  uuid;
    uint8_t       netkey_fp[4];
    uint8_t       flags;
    uint32_t      count;
    bool          active;
} meshx_zgb_peer_t;

typedef struct {
    meshx_zgb_state_t state;
    bool              commission_window_open;
    meshx_os_timer_t *commission_timer;
    meshx_os_timer_t *listen_timer;
    meshx_os_timer_t *retry_timer;
    meshx_os_timer_t *boot_jitter_timer;
    uint32_t          consecutive_suppressions;
    meshx_key_t       staged_net_key;
    meshx_key_t       staged_app_key;
    meshx_key_t       staged_dev_key;
    bool              keys_generated;
    meshx_zgb_peer_t  peers[MESHX_ZGB_MAX_PEERS];
    bool              suppressed_by_lower_uuid;
} meshx_zgb_ctx_t;

static meshx_zgb_ctx_t s_zgb_ctx;

/**
 * @brief Timer callback for the 10-second listen window before founding (ZGB-019).
 *
 * Evaluates whether any lower-UUID peer has been observed. If none was observed,
 * this node wins the min-UUID election and proceeds to commit self-provisioning
 * as Anchor Master (0x0001).
 *
 * @param[in] p_timer Pointer to timer instance.
 */
static void zgb_listen_timer_cb(const meshx_os_timer_t *p_timer)
{
    MESHX_UNUSED(p_timer);
    MESHX_LOGI(MODULE_ID_COMMON, "ZGB: Listen timer expired (10s). Evaluating min-UUID election.");

    if (!s_zgb_ctx.commission_window_open || s_zgb_ctx.state != MESHX_ZGB_STATE_COMMISSIONING)
    {
        return;
    }

    if (s_zgb_ctx.suppressed_by_lower_uuid)
    {
        MESHX_LOGI(MODULE_ID_COMMON, "ZGB: Suppressed by lower UUID peer during listen window. Backing off.");
        return;
    }

    /* No lower-UUID peer observed: win election and commit self-provisioning (ZGB-019 / ZGB-T22) */
    MESHX_LOGI(MODULE_ID_COMMON, "ZGB: Election won! Committing self-provisioning as 0x0001.");
    meshx_err_t err = meshx_zgb_perform_self_provision();
    if (err != MESHX_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "ZGB: Election win commit failed (%d), rolling back", err);
    }
}

/**
 * @brief Timer callback for retry backoff on suppression (ZGB-014 / ZGB-T12).
 *
 * Re-opens the commission window with a fresh listen phase if the node is still
 * unprovisioned.
 *
 * @param[in] p_timer Pointer to timer instance.
 */
static void zgb_retry_timer_cb(const meshx_os_timer_t *p_timer)
{
    MESHX_UNUSED(p_timer);
    MESHX_LOGI(MODULE_ID_COMMON, "ZGB: Retry-on-suppression backoff timer expired.");

    if (meshx_prov_srv_is_provisioned() || meshx_get_node_addr() != MESHX_ADDR_UNASSIGNED)
    {
        return;
    }

    /* Re-enter commission mode with fresh listen phase (ZGB-014 / ZGB-T12) */
    meshx_zgb_open_commission_window();
}

/**
 * @brief Timer callback for 60-second commission window timeout.
 *
 * Aborts the active commissioning window, purges staged keys, and stops timers.
 *
 * @param[in] p_timer Pointer to timer instance.
 */
static void zgb_commission_timer_cb(const meshx_os_timer_t *p_timer)
{
    MESHX_UNUSED(p_timer);
    MESHX_LOGI(MODULE_ID_COMMON, "ZGB: Commission window timed out (60s).");
    meshx_zgb_abort_commissioning();
}

/**
 * @brief Timer callback for post-boot jittered window (30s +/- 5s).
 *
 * Automatically opens the commission window if the node remains unprovisioned.
 *
 * @param[in] p_timer Pointer to timer instance.
 */
static void zgb_boot_jitter_timer_cb(const meshx_os_timer_t *p_timer)
{
    MESHX_UNUSED(p_timer);
    MESHX_LOGI(MODULE_ID_COMMON, "ZGB: Post-boot 30s jitter timer expired.");
    if (!meshx_prov_srv_is_provisioned() && meshx_get_node_addr() == MESHX_ADDR_UNASSIGNED)
    {
        meshx_zgb_open_commission_window();
    }
}

/**@brief BLE Manufacturing Data Observer for ZGB Arbitration Beacons.
 *
 * Filters and forwards Arbitration Beacons to the ZGB state machine for arbitration.
 *
 * @param[in] cid   Company Identifier.
 * @param[in] data  Pointer to manufacturing data.
 * @param[in] len   Length of manufacturing data.
 * @param[in] rssi  Received Signal Strength.
 *  
 */
static void zgb_ble_mfg_data_observer(uint16_t cid, const uint8_t *data, uint16_t len, int8_t rssi)
{
    (void)rssi;
    /* ZGB Arbitration Beacon: Company ID 0x02E5, Version 0x01, Len >= 22 */
    if (cid == 0x02E5 && data != NULL && len >= 22 && data[0] == MESHX_ZGB_AB_VERSION)
    {
        uint8_t flags = data[1];
        const uint8_t *peer_uuid = &data[2];
        const uint8_t *peer_netkey_fp = (len >= 26) ? &data[18] : NULL;
        meshx_zgb_process_arbitration_beacon((const meshx_uuid_t *)peer_uuid, flags, peer_netkey_fp);
    }
}

/**
 * @brief Initialize Zero-Gateway Bootstrapping subsystem.
 *
 * Sets up commissioning, listen, retry, and post-boot jitter timers.
 * If unprovisioned, arms the post-boot jitter timer.
 *
 * @return MESHX_SUCCESS on success, or error code on failure.
 */
meshx_err_t meshx_zgb_init(void)
{
    memset(&s_zgb_ctx, 0, sizeof(s_zgb_ctx));
    s_zgb_ctx.state = meshx_prov_srv_is_provisioned() ? MESHX_ZGB_STATE_PROVISIONED : MESHX_ZGB_STATE_UNPROVISIONED;

    meshx_err_t err = meshx_os_timer_create("zgb_comm",
                                            MESHX_ZGB_COMMISSION_TIMEOUT_MS,
                                            false,
                                            zgb_commission_timer_cb,
                                            &s_zgb_ctx.commission_timer);
    if (err != MESHX_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "ZGB: Failed to create commission timer: %d", err);
    }

    err = meshx_os_timer_create("zgb_listen",
                                MESHX_ZGB_LISTEN_TIMEOUT_MS,
                                false,
                                zgb_listen_timer_cb,
                                &s_zgb_ctx.listen_timer);
    if (err != MESHX_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "ZGB: Failed to create listen timer: %d", err);
    }

    err = meshx_os_timer_create("zgb_retry",
                                60000U,
                                false,
                                zgb_retry_timer_cb,
                                &s_zgb_ctx.retry_timer);
    if (err != MESHX_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "ZGB: Failed to create retry timer: %d", err);
    }

    /* Post-boot 30s (+/- 5s jitter) timer for unprovisioned node */
    if (!meshx_prov_srv_is_provisioned() && meshx_get_node_addr() == MESHX_ADDR_UNASSIGNED)
    {
        uint32_t rnd_val = 0;
        meshx_crypto_random_fill((uint8_t *)&rnd_val, sizeof(rnd_val));
        uint32_t jitter_ms = MESHX_ZGB_BOOT_JITTER_BASE_MS + (rnd_val % MESHX_ZGB_BOOT_JITTER_RANGE_MS);

        err = meshx_os_timer_create("zgb_boot",
                                    jitter_ms,
                                    false,
                                    zgb_boot_jitter_timer_cb,
                                    &s_zgb_ctx.boot_jitter_timer);
        if (err == MESHX_SUCCESS && s_zgb_ctx.boot_jitter_timer != NULL)
        {
            meshx_os_timer_start(s_zgb_ctx.boot_jitter_timer);
            MESHX_LOGI(MODULE_ID_COMMON, "ZGB: Armed post-boot unprovisioned timer (%u ms)", (unsigned int)jitter_ms);
        }
        else
        {
            MESHX_LOGE(MODULE_ID_COMMON, "ZGB: Failed to create/start boot jitter timer: %d", err);
        }
    }

    /* Register generic BLE manufacturer data observer via platform abstraction interface */
    meshx_plat_ble_register_mfg_data_cb(zgb_ble_mfg_data_observer);

    return MESHX_SUCCESS;
}

/**
 * @brief Check if node is actively in the commission window.
 *
 * @return true if commission window is open, false otherwise.
 */
bool meshx_zgb_is_commissioning(void)
{
    return s_zgb_ctx.commission_window_open;
}

/**
 * @brief Get current ZGB state.
 *
 * @return Current meshx_zgb_state_t state enum value.
 */
meshx_zgb_state_t meshx_zgb_get_state(void)
{
    return s_zgb_ctx.state;
}

/**
 * @brief Open self-commissioning window (60s total, with 10s listen phase).
 *
 * Verifies that the node is unprovisioned, cancels jitter/retry timers,
 * broadcasts Arbitration Beacons, and starts commission and listen timers.
 *
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_STATE if already provisioned.
 */
meshx_err_t meshx_zgb_open_commission_window(void)
{
    /* Pre-trigger guard: reject if already provisioned or external provisioning is in progress */
    if (meshx_prov_srv_is_provisioned() ||
        meshx_get_node_addr() != MESHX_ADDR_UNASSIGNED ||
        s_zgb_ctx.state == MESHX_ZGB_STATE_PROVISIONING)
    {
        MESHX_LOGW(MODULE_ID_COMMON, "ZGB: Cannot commission: node already provisioned or provisioning in progress (addr=0x%04X, state=%d)",
                   meshx_get_node_addr(), (int)s_zgb_ctx.state);
        return MESHX_INVALID_STATE;
    }

    if (s_zgb_ctx.boot_jitter_timer != NULL)
    {
        meshx_os_timer_stop(s_zgb_ctx.boot_jitter_timer);
    }
    if (s_zgb_ctx.retry_timer != NULL)
    {
        meshx_os_timer_stop(s_zgb_ctx.retry_timer);
    }

    s_zgb_ctx.commission_window_open = true;
    s_zgb_ctx.state = MESHX_ZGB_STATE_COMMISSIONING;
    s_zgb_ctx.suppressed_by_lower_uuid = false;

    /* Start 60s total window timer */
    if (s_zgb_ctx.commission_timer != NULL)
    {
        meshx_os_timer_start(s_zgb_ctx.commission_timer);
    }

    /* Start 10s listen phase before founding (ZGB-019 / ZGB-T22) */
    if (s_zgb_ctx.listen_timer != NULL)
    {
        meshx_os_timer_start(s_zgb_ctx.listen_timer);
    }

    mxcp_evt_prov_state_ind_t ind;
    memset(&ind, 0, sizeof(ind));
    ind.unicast_addr = MESHX_ADDR_UNASSIGNED;
    ind.net_idx = 0;
    ind.state = 0x04; /* MXCP_PROV_STATE_COMMISSIONING */
    mxcp_send_event(MXCP_EVT_PROV_STATE_IND, (const uint8_t *)&ind, sizeof(ind));

    /* Broadcast Arbitration Beacon: flags = eligible | commissioning (ZGB-018) */
    meshx_uuid_t self_uuid;
    if (meshx_get_device_uuid(&self_uuid) == MESHX_SUCCESS)
    {
        uint8_t ab_packet[24];
        ab_packet[0] = 0xE5; /* Company ID 0x02E5 low byte */
        ab_packet[1] = 0x02; /* Company ID 0x02E5 high byte */
        ab_packet[2] = MESHX_ZGB_AB_VERSION;
        ab_packet[3] = MESHX_ZGB_AB_FLAG_ELIGIBLE | MESHX_ZGB_AB_FLAG_COMM;
        memcpy(&ab_packet[4], self_uuid, 16);
        memset(&ab_packet[20], 0, 4);
        meshx_plat_ble_adv_mfg_data(ab_packet, sizeof(ab_packet));
    }

    MESHX_LOGI(MODULE_ID_COMMON, "ZGB: Commission window opened (60s total, 10s listen)");
    return MESHX_SUCCESS;
}

/**
 * @brief Start self-commissioning (alias for opening commission window).
 *
 * @return MESHX_SUCCESS on success, or error code on failure.
 */
meshx_err_t meshx_zgb_start_commissioning(void)
{
    return meshx_zgb_open_commission_window();
}

/**
 * @brief Perform atomic self-provisioning as Anchor Master (0x0001).
 *
 * Generates keys via hardware RNG, commits credentials atomically to NVS,
 * provisions the platform mesh stack, adds AppKey, and cleans staged keys from RAM.
 *
 * @return MESHX_SUCCESS on success, or error code on failure.
 */
meshx_err_t meshx_zgb_perform_self_provision(void)
{
    if (meshx_prov_srv_is_provisioned() ||
        meshx_get_node_addr() != MESHX_ADDR_UNASSIGNED ||
        s_zgb_ctx.state == MESHX_ZGB_STATE_PROVISIONING)
    {
        return MESHX_INVALID_STATE;
    }

    /* 1. On-device HWRNG key birth */
    meshx_err_t err = meshx_crypto_random_fill(s_zgb_ctx.staged_net_key, MESHX_KEY_LEN);
    if (err != MESHX_SUCCESS)
    {
        return err;
    }
    err = meshx_crypto_random_fill(s_zgb_ctx.staged_app_key, MESHX_KEY_LEN);
    if (err != MESHX_SUCCESS)
    {
        return err;
    }
    err = meshx_crypto_random_fill(s_zgb_ctx.staged_dev_key, MESHX_KEY_LEN);
    if (err != MESHX_SUCCESS)
    {
        return err;
    }
    s_zgb_ctx.keys_generated = true;

    /* Pre-commit guard: ensure external provisioner did not race and complete */
    if (meshx_prov_srv_is_provisioned() || s_zgb_ctx.state == MESHX_ZGB_STATE_PROVISIONING)
    {
        meshx_zgb_abort_commissioning();
        return MESHX_INVALID_STATE;
    }

    meshx_uuid_t self_uuid;
    err = meshx_get_device_uuid(&self_uuid);
    if (err != MESHX_SUCCESS)
    {
        memset(self_uuid, 0, sizeof(self_uuid));
    }

    /* 2. Atomic NVS commit */
    err = meshx_nvs_self_provision_commit(s_zgb_ctx.staged_net_key,
                                          s_zgb_ctx.staged_app_key,
                                          s_zgb_ctx.staged_dev_key,
                                          &self_uuid,
                                          0x0001,
                                          1);
    if (err != MESHX_SUCCESS)
    {
        meshx_nvs_self_provision_rollback(&self_uuid);
        meshx_zgb_abort_commissioning();
        return err;
    }

    /* 3. Platform stack self-provisioning */
    err = meshx_plat_self_provision(s_zgb_ctx.staged_net_key, 0, 0x0001, s_zgb_ctx.staged_dev_key);
    if (err != MESHX_SUCCESS)
    {
        meshx_nvs_self_provision_rollback(&self_uuid);
        meshx_zgb_abort_commissioning();
        return err;
    }

    /* 4. Platform stack AppKey add */
    err = meshx_plat_app_key_add(s_zgb_ctx.staged_app_key, 0, 0);
    if (err != MESHX_SUCCESS)
    {
        meshx_nvs_self_provision_rollback(&self_uuid);
        meshx_zgb_abort_commissioning();
        return err;
    }

    /* 5. Purge staged keys from RAM immediately */
    memset(s_zgb_ctx.staged_net_key, 0, sizeof(s_zgb_ctx.staged_net_key));
    memset(s_zgb_ctx.staged_app_key, 0, sizeof(s_zgb_ctx.staged_app_key));
    memset(s_zgb_ctx.staged_dev_key, 0, sizeof(s_zgb_ctx.staged_dev_key));
    s_zgb_ctx.keys_generated = false;
    s_zgb_ctx.commission_window_open = false;
    s_zgb_ctx.state = MESHX_ZGB_STATE_PROVISIONED;
    meshx_set_node_addr(0x0001);

    if (s_zgb_ctx.commission_timer != NULL)
    {
        meshx_os_timer_stop(s_zgb_ctx.commission_timer);
    }
    if (s_zgb_ctx.boot_jitter_timer != NULL)
    {
        meshx_os_timer_stop(s_zgb_ctx.boot_jitter_timer);
    }

    /* Emit completion events */
    mxcp_evt_prov_comp_t comp_evt;
    memset(&comp_evt, 0, sizeof(comp_evt));
    comp_evt.addr = 0x0001;
    mxcp_send_event(MXCP_EVT_PROV_COMP, (const uint8_t *)&comp_evt, sizeof(comp_evt));

    mxcp_evt_prov_state_ind_t ind;
    memset(&ind, 0, sizeof(ind));
    ind.unicast_addr = 0x0001;
    ind.net_idx = 0;
    ind.state = 0x03; /* MXCP_PROV_STATE_PROVISIONED */
    mxcp_send_event(MXCP_EVT_PROV_STATE_IND, (const uint8_t *)&ind, sizeof(ind));

    MESHX_LOGI(MODULE_ID_COMMON, "ZGB: Case-2 self-provisioning successfully committed (addr=0x0001)");
    return MESHX_SUCCESS;
}

/**
 * @brief Abort active self-commissioning window.
 *
 * Stops active commission, listen, retry, and boot jitter timers,
 * clears staged keys from RAM, and emits MXCP_EVT_PROV_STATE_IND.
 *
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t meshx_zgb_abort_commissioning(void)
{
    if (s_zgb_ctx.commission_timer != NULL)
    {
        meshx_os_timer_stop(s_zgb_ctx.commission_timer);
    }
    if (s_zgb_ctx.listen_timer != NULL)
    {
        meshx_os_timer_stop(s_zgb_ctx.listen_timer);
    }
    if (s_zgb_ctx.retry_timer != NULL)
    {
        meshx_os_timer_stop(s_zgb_ctx.retry_timer);
    }
    if (s_zgb_ctx.boot_jitter_timer != NULL)
    {
        meshx_os_timer_stop(s_zgb_ctx.boot_jitter_timer);
    }

    /* Purge staged key material */
    memset(s_zgb_ctx.staged_net_key, 0, sizeof(s_zgb_ctx.staged_net_key));
    memset(s_zgb_ctx.staged_app_key, 0, sizeof(s_zgb_ctx.staged_app_key));
    memset(s_zgb_ctx.staged_dev_key, 0, sizeof(s_zgb_ctx.staged_dev_key));
    s_zgb_ctx.keys_generated = false;
    s_zgb_ctx.commission_window_open = false;
    s_zgb_ctx.state = meshx_prov_srv_is_provisioned() ? MESHX_ZGB_STATE_PROVISIONED : MESHX_ZGB_STATE_UNPROVISIONED;

    mxcp_evt_prov_state_ind_t ind;
    memset(&ind, 0, sizeof(ind));
    ind.unicast_addr = meshx_get_node_addr();
    ind.net_idx = meshx_get_net_key_id();
    ind.state = (ind.unicast_addr != MESHX_ADDR_UNASSIGNED) ? 0x03 : 0x00;
    mxcp_send_event(MXCP_EVT_PROV_STATE_IND, (const uint8_t *)&ind, sizeof(ind));

    MESHX_LOGI(MODULE_ID_COMMON, "ZGB: Commission window aborted cleanly.");
    return MESHX_SUCCESS;
}

/**
 * @brief External Provisioning Priority Guard callback.
 *
 * Invoked on link open / identify start events. Immediately aborts self-commissioning,
 * stops boot timers, and transitions state to MESHX_ZGB_STATE_PROVISIONING to yield
 * to incoming external provisioners.
 *
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t meshx_zgb_on_external_prov_start(void)
{
    /* ZGB-T09 / ZGB-011: External Provisioning Priority Guard.
     * Always abort self-commissioning if window was open, stop boot jitter timer,
     * and transition state to PROVISIONING regardless of whether
     * the commission window was open or pending. */
    if (s_zgb_ctx.commission_window_open)
    {
        MESHX_LOGI(MODULE_ID_COMMON, "ZGB Priority Guard: External provisioner arrived during commission window. Aborting self-commissioning.");
        meshx_zgb_abort_commissioning();
    }
    if (s_zgb_ctx.boot_jitter_timer != NULL)
    {
        meshx_os_timer_stop(s_zgb_ctx.boot_jitter_timer);
    }
    s_zgb_ctx.state = MESHX_ZGB_STATE_PROVISIONING;
    MESHX_LOGI(MODULE_ID_COMMON, "ZGB Priority Guard: External provisioner arrived. State set to PROVISIONING.");
    return MESHX_SUCCESS;
}

/**
 * @brief Provisioning completion notification for ZGB.
 *
 * Stops active commission and boot timers, clears window open flag, and transitions
 * state to MESHX_ZGB_STATE_PROVISIONED.
 *
 * @param[in] assigned_addr Unicast address assigned to the node.
 *
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t meshx_zgb_on_provision_complete(uint16_t assigned_addr)
{
    if (s_zgb_ctx.commission_timer != NULL)
    {
        meshx_os_timer_stop(s_zgb_ctx.commission_timer);
    }
    if (s_zgb_ctx.boot_jitter_timer != NULL)
    {
        meshx_os_timer_stop(s_zgb_ctx.boot_jitter_timer);
    }
    s_zgb_ctx.commission_window_open = false;
    s_zgb_ctx.state = MESHX_ZGB_STATE_PROVISIONED;
    MESHX_LOGI(MODULE_ID_COMMON, "ZGB: Provisioning complete, node addr: 0x%04X", assigned_addr);
    return MESHX_SUCCESS;
}

/**
 * @brief Absorb a Case-1 passive provisionee node via its native Prov Server.
 *
 * Validates capability integrity against trait mask, allocates contiguous unicast
 * address block from pool, commits lease, and stores generated target DevKey.
 *
 * @param[in]  target_uuid   16-byte UUID of target node.
 * @param[in]  trait_mask    Target semantic trait mask.
 * @param[in]  element_count Number of elements on target.
 * @param[out] out_addr      Pointer to store assigned base unicast address.
 *
 * @return MESHX_SUCCESS on success, or error code on failure.
 */
meshx_err_t meshx_zgb_absorb_case1_target(const meshx_uuid_t *target_uuid,
                                          uint16_t trait_mask,
                                          uint8_t element_count,
                                          uint16_t *out_addr)
{
    if (!target_uuid || !out_addr)
    {
        return MESHX_INVALID_ARG;
    }
    if (!meshx_prov_srv_is_provisioned() || meshx_get_node_addr() != 0x0001)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "ZGB Absorb: Node must be provisioned Master (0x0001) to absorb targets");
        return MESHX_INVALID_STATE;
    }

    /* 1. ZGB-005: Validate capability integrity */
    bool is_multi = (trait_mask & 0x0020) != 0;
    bool match = is_multi ? (element_count > 1) : (element_count == 1);
    if (!match)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "ZGB Absorb: Trait mask 0x%04X mismatch with element count %u",
                   trait_mask, element_count);
        return MESHX_ERR_PROVISIONING_FAILED;
    }

    /* 2. ZGB-009: Allocate contiguous unicast address block from unified pool 0x0002..0x7FFF */
    uint16_t base_addr = 0;
    uint32_t lease_id = 0;
    meshx_err_t err = meshx_lease_allocate_block(element_count,
                                                 meshx_get_node_addr(),
                                                 target_uuid,
                                                 &base_addr,
                                                 &lease_id);
    if (err != MESHX_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "ZGB Absorb: Address allocation failed: %d", err);
        return err;
    }

    /* 3. Commit lease */
    err = meshx_lease_commit(lease_id);
    if (err != MESHX_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "ZGB Absorb: Lease commit failed: %d", err);
        return err;
    }

    /* 4. Generate target DevKey from HWRNG and store into vault */
    uint8_t target_dev_key[MESHX_KEY_LEN];
    meshx_crypto_random_fill(target_dev_key, sizeof(target_dev_key));
    meshx_nvs_set_device_key(target_uuid, target_dev_key);
    memset(target_dev_key, 0, sizeof(target_dev_key));

    *out_addr = base_addr;

    MESHX_LOGI(MODULE_ID_COMMON, "ZGB Absorb: Successfully allocated addr 0x%04X for target node (elements=%u)",
               base_addr, element_count);
    return MESHX_SUCCESS;
}

/**
 * @brief Case 3: Process incoming ZGB Arbitration Beacon (AB) or commissioning beacon.
 *
 * Performs min-UUID listen-before-founding arbitration, suppression monitoring,
 * and multi-beacon split-brain confirmation.
 *
 * @param[in] peer_uuid      16-byte UUID of reporting peer.
 * @param[in] flags          AB flags (MESHX_ZGB_AB_FLAG_*).
 * @param[in] peer_netkey_fp 4-byte NetKey fingerprint (or NULL if unprovisioned).
 *
 * @return MESHX_SUCCESS on normal processing, or error code on failure.
 */
meshx_err_t meshx_zgb_process_arbitration_beacon(const meshx_uuid_t *peer_uuid,
                                                 uint8_t flags,
                                                 const uint8_t peer_netkey_fp[4])
{
    if (!peer_uuid)
    {
        return MESHX_INVALID_ARG;
    }

    meshx_uuid_t self_uuid;
    if (meshx_get_device_uuid(&self_uuid) != MESHX_SUCCESS)
    {
        return MESHX_INVALID_STATE;
    }

    /* Ignore self */
    if (memcmp(*peer_uuid, self_uuid, sizeof(meshx_uuid_t)) == 0)
    {
        return MESHX_SUCCESS;
    }

    /* 1. Case 3 Election: min-UUID listen-before-founding (ZGB-013 / ZGB-T11) */
    if (s_zgb_ctx.commission_window_open && (flags & MESHX_ZGB_AB_FLAG_COMM))
    {
        int cmp = memcmp(*peer_uuid, self_uuid, sizeof(meshx_uuid_t));
        if (cmp < 0)
        {
            /* Lower UUID observed -> we lose election, enter SUPPRESSED */
            MESHX_LOGI(MODULE_ID_COMMON, "ZGB: Observed lower-UUID peer in commissioning. Suppressing founding.");
            s_zgb_ctx.suppressed_by_lower_uuid = true;
            s_zgb_ctx.commission_window_open = false;
            s_zgb_ctx.state = MESHX_ZGB_STATE_SUPPRESSED;

            if (s_zgb_ctx.listen_timer != NULL)
            {
                meshx_os_timer_stop(s_zgb_ctx.listen_timer);
            }
            if (s_zgb_ctx.commission_timer != NULL)
            {
                meshx_os_timer_stop(s_zgb_ctx.commission_timer);
            }

            /* Arm retry timer with exponential backoff (60s, 120s, 240s, 600s max) (ZGB-014 / ZGB-T12) */
            s_zgb_ctx.consecutive_suppressions++;
            uint32_t backoff_sec = 60U * (1U << (s_zgb_ctx.consecutive_suppressions > 4 ? 4 : (s_zgb_ctx.consecutive_suppressions - 1)));
            if (backoff_sec > 600U)
            {
                backoff_sec = 600U;
            }
            if (s_zgb_ctx.retry_timer != NULL)
            {
                meshx_os_timer_set_period(s_zgb_ctx.retry_timer, backoff_sec * 1000U);
                meshx_os_timer_start(s_zgb_ctx.retry_timer);
                MESHX_LOGI(MODULE_ID_COMMON, "ZGB: Armed retry backoff timer for %u s", (unsigned int)backoff_sec);
            }

            mxcp_evt_prov_state_ind_t ind;
            memset(&ind, 0, sizeof(ind));
            ind.unicast_addr = MESHX_ADDR_UNASSIGNED;
            ind.net_idx = 0;
            ind.state = 0x05; /* MXCP_PROV_STATE_SUPPRESSED */
            mxcp_send_event(MXCP_EVT_PROV_STATE_IND, (const uint8_t *)&ind, sizeof(ind));
            return MESHX_SUCCESS;
        }
        else if (cmp == 0)
        {
            /* Hardware UUID collision: ZGB-013 / ZGB-T26 duplicate UUID hard stop */
            MESHX_LOGE(MODULE_ID_COMMON, "ZGB_DUPLICATE_UUID: Peer has identical UUID. Hard stop.");
            meshx_zgb_abort_commissioning();
            return MESHX_INVALID_STATE;
        }
    }

    /* 2. Suppression state monitoring: if provisioned network observed, cancel retries and wait as passive Case-1 */
    if (s_zgb_ctx.state == MESHX_ZGB_STATE_SUPPRESSED && (flags & MESHX_ZGB_AB_FLAG_PROV))
    {
        MESHX_LOGI(MODULE_ID_COMMON, "ZGB: Provisioned network detected while suppressed. Canceling retries, entering passive provisionee mode.");
        if (s_zgb_ctx.retry_timer != NULL)
        {
            meshx_os_timer_stop(s_zgb_ctx.retry_timer);
        }
        s_zgb_ctx.state = MESHX_ZGB_STATE_UNPROVISIONED;
        return MESHX_SUCCESS;
    }

    /* 3. Split-brain detection: if both self and peer are provisioned (ZGB-015 / ZGB-T13) */
    if (meshx_prov_srv_is_provisioned() && meshx_get_node_addr() == 0x0001 &&
        (flags & MESHX_ZGB_AB_FLAG_PROV) && peer_netkey_fp != NULL)
    {

        /* Calculate self NetKey fingerprint (first 4 bytes of SHA-256 of net_key) */
        uint8_t self_netkey[MESHX_KEY_LEN];
        if (meshx_plat_net_key_get(meshx_get_net_key_id(), self_netkey) == MESHX_SUCCESS)
        {
            uint8_t self_hash[32];
            meshx_crypto_hmac_sha256(self_netkey, MESHX_KEY_LEN, self_netkey, MESHX_KEY_LEN, self_hash);

            if (memcmp(self_hash, peer_netkey_fp, 4) != 0)
            {
                /* NetKey fingerprint mismatch! Track peer confirmation count */
                int free_idx = -1;
                int match_idx = -1;
                for (int i = 0; i < MESHX_ZGB_MAX_PEERS; i++)
                {
                    if (s_zgb_ctx.peers[i].active && memcmp(s_zgb_ctx.peers[i].uuid, peer_uuid, 16) == 0)
                    {
                        match_idx = i;
                        break;
                    }
                    if (!s_zgb_ctx.peers[i].active && free_idx < 0)
                    {
                        free_idx = i;
                    }
                }

                if (match_idx >= 0)
                {
                    s_zgb_ctx.peers[match_idx].count++;
                    if (s_zgb_ctx.peers[match_idx].count >= MESHX_ZGB_SPLIT_BRAIN_CONFIRM_CNT)
                    {
                        MESHX_LOGW(MODULE_ID_COMMON, "ZGB: Split-brain confirmed over 3 stable ABs! Triggering healing.");
                        return meshx_zgb_handle_split_brain((const meshx_uuid_t *)peer_uuid);
                    }
                }
                else if (free_idx >= 0)
                {
                    s_zgb_ctx.peers[free_idx].active = true;
                    memcpy(s_zgb_ctx.peers[free_idx].uuid, peer_uuid, 16);
                    memcpy(s_zgb_ctx.peers[free_idx].netkey_fp, peer_netkey_fp, 4);
                    s_zgb_ctx.peers[free_idx].count = 1;
                }
            }
        }
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Split-brain detection & healing handler (ZGB-015 / ZGB-T14).
 *
 * Compares self UUID with colliding anchor peer UUID:
 * - If local UUID is lower: this node survives as Master 0x0001.
 * - If local UUID is higher: resets and re-joins winner network as Case-1.
 *
 * @param[in] peer_uuid 16-byte UUID of colliding foreign anchor.
 *
 * @return MESHX_SUCCESS if handled, or error code on invalid state/UUID.
 */
meshx_err_t meshx_zgb_handle_split_brain(const meshx_uuid_t *peer_uuid)
{
    if (!peer_uuid)
    {
        return MESHX_INVALID_ARG;
    }

    meshx_uuid_t self_uuid;
    if (meshx_get_device_uuid(&self_uuid) != MESHX_SUCCESS)
    {
        return MESHX_INVALID_STATE;
    }

    int cmp = memcmp(self_uuid, *peer_uuid, sizeof(meshx_uuid_t));
    if (cmp > 0)
    {
        /* Local node has higher UUID -> Loser!
         * Broadcast MESHX_SYS_NOTIFY_NODE_RESET (0x09) to children, wait 2s, factory-reset and rejoin as Case-1 (ZGB-015 / ZGB-T14) */
        MESHX_LOGW(MODULE_ID_COMMON, "ZGB Split-Brain Healing: Higher UUID detected. Executing graceful reset and rejoining winner network.");
        s_zgb_ctx.state = MESHX_ZGB_STATE_RESETTING;

        /* Execute pre-reset teardown notice to children and purge NVS */
        meshx_unpair_pre_reset_teardown();
        meshx_nvs_comp_schema_erase();
        meshx_platform_reset();
        return MESHX_SUCCESS;
    }
    else if (cmp < 0)
    {
        /* Local node has lower UUID -> Winner!
         * NetKey and address space survive; wait for loser anchor and its children to re-beacon and absorb them */
        MESHX_LOGI(MODULE_ID_COMMON, "ZGB Split-Brain Healing: Lower UUID. Local anchor survives as Master 0x0001.");
        return MESHX_SUCCESS;
    }
    else
    {
        MESHX_LOGE(MODULE_ID_COMMON, "ZGB_DUPLICATE_UUID: Split-brain with identical UUID node. Hard stop.");
        return MESHX_INVALID_STATE;
    }
}
