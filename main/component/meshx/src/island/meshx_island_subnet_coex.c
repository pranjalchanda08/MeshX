/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_island_subnet_coex.c
 * @brief Island Subnet Coexistence Facade, Lifecycle, UVP Dispatch & MXCP Control APIs.
 */

#include <string.h>
#ifndef CONFIG_MESHX_ISLAND_SUBNET_COEXISTENCE
#include "meshx_config_internal.h"
#endif
#include "island/meshx_island_subnet_coex.h"
#include "island/meshx_router_core.h"
#include "island/meshx_subnet_kr.h"
#include "island/meshx_nat_mgr.h"
#include "island/meshx_subnet_security.h"
#include "island/meshx_subnet_discovery.h"
#include "island/meshx_subnet_bridge.h"
#include "island/meshx_subnet_nvs.h"

#if defined(CONFIG_MESHX_ZGB) && CONFIG_MESHX_ZGB
#include "zgb/meshx_zgb.h"
#endif

#if defined(CONFIG_MESHX_ISLAND_SUBNET_COEXISTENCE) && CONFIG_MESHX_ISLAND_SUBNET_COEXISTENCE

#define MESHX_PAL_MAX_BRIDGE_RULES 8U

/**
 * @brief Island Subnet Coexistence Global State.
 */
typedef struct {
    bool                        initialized;
    bool                        bridge_enabled;
    meshx_bridge_rule_t         rules[MESHX_PAL_MAX_BRIDGE_RULES];
    uint8_t                     rule_count;
    meshx_router_ctx_t          router;
    meshx_subnet_kr_mgr_t       kr;
    meshx_nat_mgr_t             nat;
    meshx_subnet_security_ctx_t sec;
    meshx_subnet_discovery_t    disc;
    meshx_subnet_bridge_mgr_t   bridge;
    meshx_subnet_uvp_tx_fn_t    uvp_tx_cb;
    void                       *uvp_tx_ctx;
    uint32_t                    now_ms;
    uint32_t                    last_gc_min;
} meshx_coex_state_t;

static meshx_coex_state_t s_coex;

/**
 * @brief Callback invoked by Security Engine when anti-replay checkpoint exceeds stride.
 *
 * @param[in] ckpt Checkpoint value to persist.
 * @param[in] ctx  User context pointer.
 *
 * @return MESHX_SUCCESS on success, or NVS error.
 */
static meshx_err_t save_nonce_ckpt_cb(uint32_t ckpt, void *ctx)
{
    MESHX_UNUSED(ctx);
    return meshx_subnet_nvs_save_nonce(ckpt);
}

/**
 * @brief Wrap UVP payload with authentication header if K_bridge valid, or emit plain frame.
 *
 * @param[in] net_idx     Target NetKey index.
 * @param[in] dst_addr    Destination address.
 * @param[in] opcode      UVP opcode byte.
 * @param[in] payload     Pointer to payload data.
 * @param[in] payload_len Length of payload data.
 * @param[in] ack_req     Whether transport acknowledgement is requested.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
static meshx_err_t send_authenticated_uvp(uint16_t net_idx,
                                          uint16_t dst_addr,
                                          uint8_t opcode,
                                          const void *payload,
                                          uint16_t payload_len,
                                          bool ack_req)
{
    if (!s_coex.uvp_tx_cb)
    {
        return MESHX_SUCCESS;
    }
    if (!s_coex.sec.k_bridge_valid ||
        opcode == MESHX_UVP_OP_SUBNET_BRIDGE_INVITE ||
        opcode == MESHX_UVP_OP_SUBNET_INVITE_ACK)
    {
        /* Emit an unsigned frame whose leading byte is the opcode, matching the
         * wire convention of the authenticated envelope. */
        uint8_t plain[MESHX_SUBNET_PLAIN_TX_BUF_LEN];
        if ((uint32_t)payload_len + 1U > (uint32_t)sizeof(plain))
        {
            return MESHX_NO_MEM;
        }
        plain[0] = opcode;
        if (payload_len > 0U)
        {
            memcpy(&plain[1], payload, payload_len);
        }
        return s_coex.uvp_tx_cb(net_idx, dst_addr, opcode,
                                plain, (uint16_t)(payload_len + 1U), ack_req, s_coex.uvp_tx_ctx);
    }

    uint8_t frame[MESHX_SUBNET_FRAME_TX_BUF_LEN];
    uint16_t frame_len = 0U;
    meshx_sec_sign_req_t sreq = {
        .opcode = opcode,
        .sender_addr = s_coex.nat.local_node_addr,
        .payload = (const uint8_t *)payload,
        .payload_len = payload_len,
    };
    meshx_sec_sign_rsp_t srsp = {
        .out_frame = frame,
        .out_cap = (uint16_t)sizeof(frame),
        .out_len = &frame_len,
    };
    meshx_err_t err = meshx_subnet_sec_sign_frame(&s_coex.sec, &sreq, &srsp);
    if (err != MESHX_SUCCESS)
    {
        return err;
    }
    return s_coex.uvp_tx_cb(net_idx, dst_addr, opcode, frame, frame_len, ack_req, s_coex.uvp_tx_ctx);
}

/**
 * @brief Router emergency NACK transmission callback.
 *
 * @param[in] net_idx   Target NetKey index.
 * @param[in] dst_addr  Destination address.
 * @param[in] nack_buf  Buffer containing NACK frame.
 * @param[in] nack_len  Length of NACK frame.
 * @param[in] user_ctx  User context pointer.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
static meshx_err_t coex_router_nack_cb(uint16_t net_idx,
                                       uint16_t dst_addr,
                                       const uint8_t *nack_buf,
                                       uint8_t nack_len,
                                       void *user_ctx)
{
    MESHX_UNUSED(user_ctx);
    return send_authenticated_uvp(net_idx, dst_addr, MESHX_UVP_OP_SUBNET_ROUTER_NACK, nack_buf, nack_len, true);
}

/**
 * @brief Replicate newly allocated Tier-2 NAT entry to Standby bridges via UVP NAT Sync.
 *
 * @param[in] virtual_addr Assigned virtual address.
 * @return MESHX_SUCCESS on success (or if not a Tier-2 address), error code otherwise.
 */
static meshx_err_t emit_nat_sync_if_tier2(uint16_t virtual_addr)
{
    if (virtual_addr < MESHX_NAT_TIER2_START || virtual_addr > MESHX_NAT_TIER2_END)
    {
        return MESHX_SUCCESS;
    }
    uint32_t src_gid = MESHX_GLOBAL_SUBNET_INVALID;
    uint16_t orig_addr = 0U;
    meshx_err_t err = meshx_nat_tier2_reverse(&s_coex.nat, virtual_addr, &src_gid, &orig_addr);
    if (err != MESHX_SUCCESS)
    {
        return err;
    }

    /* TASK-12: Replication Before Forwarding - push the fresh Tier-2 alias to the
     * Standby bridge immediately so takeover can restore the reverse mapping. */
    meshx_uvp_nat_sync_payload_t sync;
    memset(&sync, 0, sizeof(sync));
    sync.entry.global_subnet_src = src_gid;
    sync.entry.orig_addr = orig_addr;
    sync.entry.virtual_addr = virtual_addr;
    sync.entry.flags = MESHX_NAT_FLAG_VALID;
    sync.entry.last_active_min = (uint16_t)(s_coex.now_ms / MESHX_MS_PER_MINUTE);
    sync.op = MESHX_NAT_SYNC_OP_UPSERT;
    return send_authenticated_uvp(MESHX_SUBNET_PRIMARY_NET_IDX, MESHX_SUBNET_ADDR_ALL_NODES,
                                  MESHX_UVP_OP_SUBNET_NAT_SYNC, &sync, sizeof(sync), false);
}

/**
 * @brief Initialize the island subnet coexistence feature and restore state from NVS.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_island_subnet_coex_init(void)
{
    if (s_coex.initialized)
    {
        return MESHX_SUCCESS;
    }

    /* Preserve any UVP TX binding registered before facade init (dispatcher init order is
     * not guaranteed relative to meshx_init). */
    meshx_subnet_uvp_tx_fn_t saved_tx_cb = s_coex.uvp_tx_cb;
    void *saved_tx_ctx = s_coex.uvp_tx_ctx;

    memset(&s_coex, 0, sizeof(s_coex));

    s_coex.uvp_tx_cb = saved_tx_cb;
    s_coex.uvp_tx_ctx = saved_tx_ctx;

    uint32_t nonce_ckpt = 0UL;
    meshx_err_t err = meshx_subnet_nvs_load_nonce(&nonce_ckpt);
    if (err != MESHX_SUCCESS && err != MESHX_NOT_FOUND)
    {
        return err;
    }

    meshx_router_core_init(&s_coex.router, NULL, coex_router_nack_cb, &s_coex);
    meshx_subnet_kr_init(&s_coex.kr, false);
    meshx_nat_mgr_init(&s_coex.nat, MESHX_COEX_DEFAULT_LOCAL_ADDR);
    err = meshx_subnet_sec_init(&s_coex.sec, nonce_ckpt, save_nonce_ckpt_cb, &s_coex);
    if (err != MESHX_SUCCESS)
    {
        return err;
    }
    meshx_subnet_disc_init(&s_coex.disc);
    meshx_subnet_bridge_init(&s_coex.bridge, MESHX_COEX_DEFAULT_LOCAL_ADDR, true, 50U, 0UL);

    meshx_nvs_kbridge_blob_t kb;
    memset(&kb, 0, sizeof(kb));
    err = meshx_subnet_nvs_load_kbridge(&kb);
    if (err == MESHX_SUCCESS && kb.valid != 0U)
    {
        err = meshx_subnet_sec_set_k_bridge(&s_coex.sec, &kb.k_bridge, kb.cluster_epoch, kb.active_term);
        if (err != MESHX_SUCCESS)
        {
            return err;
        }
        s_coex.kr.has_cluster_bridge_key = true;
    }
    else if (err != MESHX_SUCCESS && err != MESHX_NOT_FOUND)
    {
        return err;
    }

    uint8_t nat_cnt = 0U;
    err = meshx_subnet_nvs_load_nat_table(s_coex.nat.tier2, &nat_cnt);
    if (err != MESHX_SUCCESS && err != MESHX_NOT_FOUND)
    {
        return err;
    }
    err = meshx_subnet_nvs_load_pair_records(s_coex.disc.pairs, &s_coex.disc.pair_count);
    if (err != MESHX_SUCCESS && err != MESHX_NOT_FOUND)
    {
        return err;
    }

    s_coex.initialized = true;
    return MESHX_SUCCESS;
}

/**
 * @brief De-initialize the island subnet coexistence subsystem (clean reset for teardown/tests).
 */
void meshx_island_subnet_coex_deinit(void)
{
    if (!s_coex.initialized)
    {
        return;
    }
    meshx_router_core_deinit(&s_coex.router);
    meshx_nat_mgr_deinit(&s_coex.nat);
    meshx_subnet_sec_deinit(&s_coex.sec);
    meshx_subnet_disc_deinit(&s_coex.disc);

    /* Keep the UVP TX binding across teardown/re-init cycles. */
    meshx_subnet_uvp_tx_fn_t saved_tx_cb = s_coex.uvp_tx_cb;
    void *saved_tx_ctx = s_coex.uvp_tx_ctx;
    memset(&s_coex, 0, sizeof(s_coex));
    s_coex.uvp_tx_cb = saved_tx_cb;
    s_coex.uvp_tx_ctx = saved_tx_ctx;
}

/**
 * @brief Check if island subnet coexistence is enabled and initialized.
 *
 * @return true if the feature is enabled, false otherwise.
 */
bool meshx_is_island_subnet_coex_enabled(void)
{
    return s_coex.initialized;
}

/**
 * @brief Register transport callback for outbound UVP coexistence frames.
 *
 * @param[in] tx_cb    Callback function invoked when sending UVP packets.
 * @param[in] user_ctx User context pointer.
 */
void meshx_subnet_coex_register_uvp_tx(meshx_subnet_uvp_tx_fn_t tx_cb, void *user_ctx)
{
    s_coex.uvp_tx_cb = tx_cb;
    s_coex.uvp_tx_ctx = user_ctx;
}

/**
 * @brief Enable or disable cross-subnet bridging rules synced via PAL.
 *
 * @param[in] enable true to enable forwarding across configured rules, false to pause.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_coex_pal_bridge_set(bool enable)
{
    if (!s_coex.initialized)
    {
        meshx_err_t err = meshx_island_subnet_coex_init();
        if (err != MESHX_SUCCESS)
        {
            return err;
        }
    }
    s_coex.bridge_enabled = enable;
    return MESHX_SUCCESS;
}

/**
 * @brief Register a cross-subnet bridging rule from the Platform Abstraction Layer.
 *
 * @param[in] rule Pointer to bridging rule descriptor.
 *
 * @return MESHX_SUCCESS on success, MESHX_INVALID_ARG or MESHX_NO_MEM on error.
 */
meshx_err_t meshx_subnet_coex_pal_bridge_rule_add(const meshx_bridge_rule_t *rule)
{
    if (!rule)
    {
        return MESHX_INVALID_ARG;
    }
    if (!s_coex.initialized)
    {
        meshx_err_t err = meshx_island_subnet_coex_init();
        if (err != MESHX_SUCCESS)
        {
            return err;
        }
    }
    for (uint8_t i = 0U; i < s_coex.rule_count; i++)
    {
        if (s_coex.rules[i].net_idx_1 == rule->net_idx_1 &&
            s_coex.rules[i].net_idx_2 == rule->net_idx_2 &&
            s_coex.rules[i].addr_1 == rule->addr_1 &&
            s_coex.rules[i].addr_2 == rule->addr_2)
        {
            s_coex.rules[i] = *rule;
            return MESHX_SUCCESS;
        }
    }
    if (s_coex.rule_count >= MESHX_PAL_MAX_BRIDGE_RULES)
    {
        return MESHX_NO_MEM;
    }
    s_coex.rules[s_coex.rule_count++] = *rule;
    return MESHX_SUCCESS;
}

/**
 * @brief Check whether Key Rotation burst opcodes (0xC4..0xC6) must be quarantined across bridges.
 *
 * @return true if a Cluster Bridge Key (K_bridge) is provisioned and bridging is active.
 */
bool meshx_subnet_coex_is_kr_quarantined(void)
{
    return s_coex.initialized && s_coex.kr.has_cluster_bridge_key;
}

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
                                               uint32_t new_epoch)
{
    if (!s_coex.initialized)
    {
        return MESHX_INVALID_STATE;
    }
    return meshx_subnet_kr_set_phase(&s_coex.kr,
                                     net_idx,
                                     (meshx_kr_phase_t)new_phase,
                                     new_key,
                                     new_epoch);
}

/**
 * @brief Periodic coexistence scheduler tick.
 *
 * @param[in] now_ms Current monotonic timestamp in milliseconds.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_coex_tick(uint32_t now_ms)
{
    if (!s_coex.initialized)
    {
        return MESHX_INVALID_STATE;
    }

    s_coex.now_ms = now_ms;

    meshx_err_t err = meshx_router_stage2_pump(&s_coex.router, now_ms);
    if (err != MESHX_SUCCESS)
    {
        return err;
    }

    uint32_t hb_boundary = 0UL;
    uint32_t claim_boundary = 0UL;
    err = meshx_subnet_bridge_tick(&s_coex.bridge,
                                   now_ms,
                                   MESHX_WITNESS_DEFAULT_HEAP_BYTES,
                                   &hb_boundary,
                                   &claim_boundary);
    if (err != MESHX_SUCCESS)
    {
        return err;
    }

    if (hb_boundary != 0UL)
    {
        meshx_uvp_heartbeat_payload_t hb;
        memset(&hb, 0, sizeof(hb));
        hb.boundary_id = hb_boundary;
        hb.slot_digest_crc32 = s_coex.nat.slots.table_digest_crc32;
        hb.role = (uint8_t)MESHX_BRIDGE_ROLE_ACTIVE;
        err = send_authenticated_uvp(MESHX_SUBNET_PRIMARY_NET_IDX, MESHX_SUBNET_ADDR_ALL_NODES,
                                     MESHX_UVP_OP_SUBNET_HEARTBEAT, &hb, sizeof(hb), false);
        if (err != MESHX_SUCCESS)
        {
            return err;
        }
    }

    if (claim_boundary != 0UL)
    {
        meshx_uvp_takeover_payload_t tk;
        memset(&tk, 0, sizeof(tk));
        tk.boundary_id = claim_boundary;
        tk.candidate_addr = s_coex.nat.local_node_addr;
        err = send_authenticated_uvp(MESHX_SUBNET_PRIMARY_NET_IDX, MESHX_SUBNET_ADDR_ALL_NODES,
                                     MESHX_UVP_OP_SUBNET_TAKEOVER_CLAIM, &tk, sizeof(tk), false);
        if (err != MESHX_SUCCESS)
        {
            return err;
        }
    }

    uint32_t now_min = now_ms / MESHX_MS_PER_MINUTE;
    if (now_min != s_coex.last_gc_min)
    {
        s_coex.last_gc_min = now_min;
        err = meshx_nat_tier2_gc(&s_coex.nat, (uint16_t)now_min);
        if (err != MESHX_SUCCESS)
        {
            return err;
        }
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Handle an incoming UVP Heartbeat frame and emit a Demote if local bridge wins.
 *
 * @param[in] net_idx  Ingress NetKey index.
 * @param[in] rx_term  Authenticated cluster term.
 * @param[in] sender   Sender unicast address.
 * @param[in] body     Heartbeat payload bytes.
 * @param[in] body_len Payload length in bytes.
 * @return MESHX_SUCCESS on success, or error code.
 */
static meshx_err_t handle_uvp_heartbeat(uint16_t net_idx,
                                        uint32_t rx_term,
                                        uint16_t sender,
                                        const uint8_t *body,
                                        uint16_t body_len)
{
    if (body_len < sizeof(meshx_uvp_heartbeat_payload_t))
    {
        return MESHX_SUCCESS;
    }
    meshx_uvp_heartbeat_payload_t hb;
    memcpy(&hb, body, sizeof(hb));
    bool send_demote = false;
    uint16_t demote_dst = 0U;
    meshx_err_t err = meshx_subnet_bridge_on_heartbeat(&s_coex.bridge,
                                                       hb.boundary_id,
                                                       rx_term,
                                                       sender,
                                                       hb.score,
                                                       0UL,
                                                       &s_coex.router.queue,
                                                       &send_demote,
                                                       &demote_dst);
    if (err != MESHX_SUCCESS)
    {
        return err;
    }
    if (send_demote)
    {
        meshx_uvp_demote_payload_t dm = {
            .boundary_id = hb.boundary_id,
            .winner_addr = s_coex.nat.local_node_addr,
            .winner_score = s_coex.bridge.boundaries[0].local_score,
            .winning_term = s_coex.sec.active_term,
        };
        return send_authenticated_uvp(net_idx, demote_dst, MESHX_UVP_OP_SUBNET_DEMOTE, &dm, sizeof(dm), false);
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Process an incoming UVP management opcode belonging to 0xD1..0xDE.
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
                                         uint16_t payload_len)
{
    MESHX_UNUSED(dst_addr);
    if (!s_coex.initialized || !payload)
    {
        return MESHX_INVALID_ARG;
    }

    uint32_t rx_term = s_coex.sec.active_term;
    uint16_t sender = src_addr;
    const uint8_t *body = payload;
    uint16_t body_len = payload_len;

    if (opcode != MESHX_UVP_OP_SUBNET_BRIDGE_INVITE &&
        opcode != MESHX_UVP_OP_SUBNET_INVITE_ACK &&
        s_coex.sec.k_bridge_valid &&
        payload_len >= MESHX_CLUSTER_AUTH_OVERHEAD)
    {
        meshx_sec_verify_rsp_t vrsp;
        memset(&vrsp, 0, sizeof(vrsp));
        meshx_err_t verr = meshx_subnet_sec_verify_frame(&s_coex.sec,
                                                         payload,
                                                         payload_len,
                                                         &vrsp);
        if (verr != MESHX_SUCCESS)
        {
            return verr;
        }
        opcode = vrsp.opcode;
        rx_term = vrsp.term;
        sender = vrsp.sender;
        body = vrsp.payload;
        body_len = vrsp.payload_len;
    }

    switch (opcode)
    {
        case MESHX_UVP_OP_SUBNET_HEARTBEAT:
            return handle_uvp_heartbeat(net_idx, rx_term, sender, body, body_len);
        case MESHX_UVP_OP_SUBNET_TAKEOVER_CLAIM:
            if (body_len >= sizeof(meshx_uvp_takeover_payload_t))
            {
                meshx_uvp_takeover_payload_t tk;
                memcpy(&tk, body, sizeof(tk));
                bool send_demote = false;
                return meshx_subnet_bridge_on_takeover_claim(&s_coex.bridge,
                                                             tk.boundary_id,
                                                             rx_term,
                                                             tk.candidate_addr,
                                                             tk.score,
                                                             0UL,
                                                             &s_coex.router.queue,
                                                             &send_demote);
            }
            return MESHX_SUCCESS;
        case MESHX_UVP_OP_SUBNET_NAT_SYNC:
            if (body_len >= sizeof(meshx_uvp_nat_sync_payload_t))
            {
                meshx_uvp_nat_sync_payload_t sync;
                memcpy(&sync, body, sizeof(sync));
                return meshx_nat_tier2_apply_sync(&s_coex.nat, &sync.entry, sync.op);
            }
            return MESHX_INVALID_ARG;
        case MESHX_UVP_OP_SUBNET_ROUTE_ADV:
            if (body_len >= sizeof(meshx_uvp_route_adv_t))
            {
                meshx_uvp_route_adv_t adv;
                memcpy(&adv, body, sizeof(adv));
                uint32_t src_gid = meshx_nat_get_global_id_by_net_idx(&s_coex.nat, net_idx);
                return meshx_subnet_bridge_route_update(&s_coex.bridge, &adv, src_gid, 0UL);
            }
            return MESHX_INVALID_ARG;
        case MESHX_UVP_OP_SUBNET_NACK_ACK:
            if (body_len >= sizeof(meshx_uvp_router_nack_ack_t))
            {
                meshx_uvp_router_nack_ack_t ack;
                memcpy(&ack, body, sizeof(ack));
                return meshx_router_on_nack_ack(&s_coex.router, ack.nack_id);
            }
            return MESHX_INVALID_ARG;
        case MESHX_UVP_OP_SUBNET_DEMOTE:
            if (body_len >= sizeof(meshx_uvp_demote_payload_t))
            {
                meshx_uvp_demote_payload_t dm;
                memcpy(&dm, body, sizeof(dm));
                return meshx_subnet_bridge_on_demote(&s_coex.bridge,
                                                    dm.boundary_id,
                                                    dm.winning_term,
                                                    dm.winner_addr,
                                                    dm.winner_score,
                                                    0UL,
                                                    &s_coex.router.queue);
            }
            return MESHX_INVALID_ARG;
        case MESHX_UVP_OP_SUBNET_DISCOVER_RSP:
            if (body_len >= sizeof(meshx_uvp_discover_rsp_t))
            {
                meshx_uvp_discover_rsp_t rsp;
                memcpy(&rsp, body, sizeof(rsp));
                uint16_t vnat = 0U;
                meshx_err_t derr = meshx_subnet_disc_handle_rsp(&s_coex.disc,
                                                                &s_coex.nat,
                                                                &rsp,
                                                                (uint16_t)(s_coex.now_ms / MESHX_MS_PER_MINUTE),
                                                                s_coex.now_ms,
                                                                &vnat);
                if (derr == MESHX_SUCCESS)
                {
                    derr = emit_nat_sync_if_tier2(vnat);
                }
                return derr;
            }
            return MESHX_INVALID_ARG;
        default:
            return MESHX_SUCCESS;
    }
}

/**
 * @brief Record a local network MIC authentication failure (feeds 30s Sliding Blindness Detector).
 *
 * @param[in] now_ms Current monotonic timestamp in milliseconds.
 */
bool meshx_subnet_coex_record_rx_mic_fail(uint32_t now_ms)
{
    if (!s_coex.initialized)
    {
        return false;
    }
    return meshx_subnet_disc_record_mic_fail(&s_coex.disc, now_ms);
}

/**
 * @brief Reset the sliding MIC failure counter upon valid local packet reception.
 */
void meshx_subnet_coex_record_rx_success(void)
{
    if (!s_coex.initialized)
    {
        return;
    }
    meshx_subnet_disc_reset_mic_fails(&s_coex.disc);
}

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
                                    int8_t rssi)
{
    MESHX_UNUSED(company_id);
    MESHX_UNUSED(payload);
    MESHX_UNUSED(payload_len);
    if (!s_coex.initialized)
    {
        return;
    }

    /* TASK-04/05: Unrecognized network-ID beacon above the witness RSSI threshold
     * promotes this node to BRIDGE_ROLE_CANDIDATE and reports to the Master. */
    if (!meshx_subnet_disc_eval_candidate(&s_coex.disc, rssi, true, MESHX_WITNESS_DEFAULT_HEAP_BYTES))
    {
        return;
    }

    s_coex.disc.witness_eligible = true;
    s_coex.bridge.candidate_active = true;

    uint32_t observer_gid = meshx_nat_get_global_id_by_net_idx(&s_coex.nat, 0U);

    meshx_uvp_candidate_report_t rpt;
    memset(&rpt, 0, sizeof(rpt));
    rpt.observed_subnet_id = MESHX_GLOBAL_SUBNET_INVALID;
    rpt.observer_subnet_id = (observer_gid == MESHX_GLOBAL_SUBNET_INVALID) ? 0UL : observer_gid;
    rpt.rssi_dbm = rssi;
    rpt.mains_powered = 1U;
    rpt.free_heap_kb = MESHX_WITNESS_FREE_HEAP_KB;
    rpt.candidate_score = (uint8_t)(meshx_subnet_bridge_compute_score(true, s_coex.bridge.rssi_score,
                                                                      MESHX_WITNESS_DEFAULT_HEAP_BYTES,
                                                                      s_coex.nat.local_node_addr) >> 8);

    /* Report candidate availability to the cluster Master. */
    meshx_err_t err = send_authenticated_uvp(MESHX_SUBNET_PRIMARY_NET_IDX,
                                             MESHX_COEX_DEFAULT_MASTER_ADDR,
                                             MESHX_UVP_OP_SUBNET_CANDIDATE_RPT,
                                             &rpt, sizeof(rpt), false);
    if (err != MESHX_SUCCESS)
    {
        MESHX_UNUSED(err);
    }
}

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
                                          uint32_t *out_active_term)
{
    if (!out_count)
    {
        return MESHX_INVALID_ARG;
    }
    *out_count = 0U;
    if (out_cluster_epoch)
    {
        *out_cluster_epoch = s_coex.sec.cluster_epoch;
    }
    if (out_active_term)
    {
        *out_active_term = s_coex.sec.active_term;
    }
    if (!s_coex.initialized)
    {
        return MESHX_SUCCESS;
    }

    size_t idx = 0U;
    for (meshx_local_slot_entry_t *c = s_coex.nat.slots.head;
         c != NULL && idx < max_items;
         c = c->next)
    {
        meshx_subnet_info_t *info = &out_items[idx++];
        memset(info, 0, sizeof(*info));
        info->global_subnet_id = c->global_subnet_id;
        info->local_net_idx = c->local_net_idx;
        info->slot_n = c->slot_n;
        const meshx_subnet_crypto_slot_t *ks = meshx_subnet_kr_find_by_net_idx(&s_coex.kr, c->local_net_idx);
        if (ks)
        {
            info->kr_phase = ks->kr_phase;
            info->key_epoch = ks->key_epoch;
        }
        info->role = (idx <= MESHX_MAX_BRIDGING_SLOTS && s_coex.bridge.boundaries[idx - 1U].valid)
                         ? (uint8_t)s_coex.bridge.boundaries[idx - 1U].role
                         : (uint8_t)MESHX_BRIDGE_ROLE_ACTIVE;
    }
    /* A boundary witness that has not yet been bound to any subnet reports its
     * pending BRIDGE_ROLE_CANDIDATE state so the operator can authorize bridging. */
    if (idx == 0U && s_coex.bridge.candidate_active && max_items > 0U)
    {
        meshx_subnet_info_t *info = &out_items[idx++];
        memset(info, 0, sizeof(*info));
        info->global_subnet_id = MESHX_GLOBAL_SUBNET_INVALID;
        info->local_net_idx = MESHX_NET_IDX_INVALID;
        info->role = (uint8_t)MESHX_BRIDGE_ROLE_CANDIDATE;
    }

    *out_count = idx;
    return MESHX_SUCCESS;
}

/**
 * @brief Dynamically bind a subnet to a local slot and initialize bridging context.
 *
 * @param[in] net_idx          BLE Mesh NetKey Index.
 * @param[in] net_key          Pointer to 16-byte NetKey structure.
 * @param[in] global_subnet_id Global Subnet ID (or MESHX_GLOBAL_SUBNET_INVALID to derive).
 * @param[in] role             Initial bridge role.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_coex_bridge_add(uint16_t net_idx,
                                         const meshx_subnet_key_t *net_key,
                                         uint32_t global_subnet_id,
                                         uint8_t role)
{
    if (!s_coex.initialized || !net_key)
    {
        return MESHX_INVALID_ARG;
    }
    uint32_t gid = (global_subnet_id != MESHX_GLOBAL_SUBNET_INVALID)
                       ? global_subnet_id
                       : meshx_nat_derive_global_subnet_id(net_key, MESHX_VND_MODEL_CID_MESHX, NULL, 0U, NULL);

    uint8_t slot_n = 0U;
    bool is_primary = (s_coex.nat.slots.active_count == 0U);
    meshx_err_t err = meshx_nat_slot_assign(&s_coex.nat, gid, net_idx, is_primary, &slot_n);
    if (err != MESHX_SUCCESS)
    {
        return err;
    }
    err = meshx_subnet_kr_slot_bind(&s_coex.kr, net_idx, gid, net_key, 1UL, 0UL);
    if (err != MESHX_SUCCESS)
    {
        meshx_err_t rel_err = meshx_nat_slot_release(&s_coex.nat, gid);
        if (rel_err != MESHX_SUCCESS)
        {
            return rel_err;
        }
        return err;
    }
    return meshx_subnet_bridge_add_boundary(&s_coex.bridge,
                                            gid,
                                            (meshx_bridge_role_t)role,
                                            s_coex.nat.local_node_addr,
                                            slot_n,
                                            s_coex.sec.active_term,
                                            0UL);
}

/**
 * @brief Unbind and remove a bridge subnet slot.
 *
 * @param[in] net_idx     BLE Mesh NetKey Index to unbind.
 * @param[in] force_purge If true, flushes queued frames for the associated boundary.
 *
 * @return MESHX_SUCCESS on success, MESHX_NOT_FOUND, or error code.
 */
meshx_err_t meshx_subnet_coex_bridge_del(uint16_t net_idx, bool force_purge)
{
    if (!s_coex.initialized)
    {
        return MESHX_INVALID_STATE;
    }
    uint32_t gid = meshx_nat_get_global_id_by_net_idx(&s_coex.nat, net_idx);
    if (gid == MESHX_GLOBAL_SUBNET_INVALID)
    {
        return MESHX_NOT_FOUND;
    }
    if (force_purge)
    {
        (void)meshx_router_queue_purge_boundary(&s_coex.router.queue, gid);
    }
    meshx_err_t kr_err = meshx_subnet_kr_slot_unbind(&s_coex.kr, net_idx);
    if (kr_err != MESHX_SUCCESS && kr_err != MESHX_NOT_FOUND)
    {
        return kr_err;
    }
    return meshx_nat_slot_release(&s_coex.nat, gid);
}

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
                                       uint32_t group_forward_bitmap)
{
    if (!s_coex.initialized)
    {
        return MESHX_INVALID_STATE;
    }
    if (rule)
    {
        meshx_err_t err = meshx_subnet_coex_pal_bridge_rule_add(rule);
        if (err != MESHX_SUCCESS)
        {
            return err;
        }
    }
    s_coex.bridge_enabled = enable;
    s_coex.nat.group_forward_bitmap = group_forward_bitmap;
    return MESHX_SUCCESS;
}

/**
 * @brief Install or update the shared Cluster Bridge Key (K_bridge).
 *
 * @param[in] k_bridge          Pointer to 16-byte Cluster Bridge Key.
 * @param[in] new_cluster_epoch New cluster epoch generation counter.
 * @param[in] new_term          New bridge election term counter.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_coex_cluster_key_update(const meshx_subnet_key_t *k_bridge,
                                                 uint32_t new_cluster_epoch,
                                                 uint32_t new_term)
{
    if (!s_coex.initialized || !k_bridge)
    {
        return MESHX_INVALID_ARG;
    }

    /* Persist-before-activate rule (TRD Page 8) */
    meshx_nvs_kbridge_blob_t blob;
    memset(&blob, 0, sizeof(blob));
    blob.k_bridge = *k_bridge;
    blob.cluster_epoch = new_cluster_epoch;
    blob.active_term = new_term;
    blob.valid = 1U;
    meshx_err_t err = meshx_subnet_nvs_save_kbridge(&blob);
    memset(&blob, 0, sizeof(blob));
    if (err != MESHX_SUCCESS)
    {
        return err;
    }

    err = meshx_subnet_sec_set_k_bridge(&s_coex.sec, k_bridge, new_cluster_epoch, new_term);
    if (err == MESHX_SUCCESS)
    {
        s_coex.kr.has_cluster_bridge_key = true;
    }
    return err;
}

#else

meshx_err_t meshx_island_subnet_coex_init(void) { return MESHX_SUCCESS; }
void meshx_island_subnet_coex_deinit(void) {}
bool meshx_is_island_subnet_coex_enabled(void) { return false; }
void meshx_subnet_coex_register_uvp_tx(meshx_subnet_uvp_tx_fn_t tx_cb, void *user_ctx)
{
    MESHX_UNUSED(tx_cb);
    MESHX_UNUSED(user_ctx);
}
meshx_err_t meshx_subnet_coex_pal_bridge_set(bool enable)
{
    MESHX_UNUSED(enable);
    return MESHX_SUCCESS;
}
meshx_err_t meshx_subnet_coex_pal_bridge_rule_add(const meshx_bridge_rule_t *rule)
{
    MESHX_UNUSED(rule);
    return MESHX_SUCCESS;
}
bool meshx_subnet_coex_is_kr_quarantined(void) { return false; }
meshx_err_t meshx_subnet_coex_on_kr_transition(uint16_t net_idx, uint8_t new_phase, const meshx_subnet_key_t *new_key, uint32_t new_epoch)
{
    MESHX_UNUSED(net_idx);
    MESHX_UNUSED(new_phase);
    MESHX_UNUSED(new_key);
    MESHX_UNUSED(new_epoch);
    return MESHX_SUCCESS;
}
meshx_err_t meshx_subnet_coex_tick(uint32_t now_ms)
{
    MESHX_UNUSED(now_ms);
    return MESHX_SUCCESS;
}
meshx_err_t meshx_subnet_coex_handle_uvp(uint8_t opcode, uint16_t src_addr, uint16_t dst_addr, uint16_t net_idx, const uint8_t *payload, uint16_t payload_len)
{
    MESHX_UNUSED(opcode);
    MESHX_UNUSED(src_addr);
    MESHX_UNUSED(dst_addr);
    MESHX_UNUSED(net_idx);
    MESHX_UNUSED(payload);
    MESHX_UNUSED(payload_len);
    return MESHX_SUCCESS;
}
bool meshx_subnet_coex_record_rx_mic_fail(uint32_t now_ms) { MESHX_UNUSED(now_ms); return false; }
void meshx_subnet_coex_record_rx_success(void) {}
void meshx_subnet_coex_on_beacon_rx(uint16_t company_id, const uint8_t *payload, uint16_t payload_len, int8_t rssi)
{
    MESHX_UNUSED(company_id);
    MESHX_UNUSED(payload);
    MESHX_UNUSED(payload_len);
    MESHX_UNUSED(rssi);
}
meshx_err_t meshx_subnet_coex_bridge_list(meshx_subnet_info_t *out_items, size_t max_items, size_t *out_count, uint32_t *out_cluster_epoch, uint32_t *out_active_term)
{
    MESHX_UNUSED(out_items);
    MESHX_UNUSED(max_items);
    if (out_count)
        *out_count = 0U;
    if (out_cluster_epoch)
        *out_cluster_epoch = 0UL;
    if (out_active_term)
        *out_active_term = 0UL;
    return MESHX_SUCCESS;
}
meshx_err_t meshx_subnet_coex_bridge_add(uint16_t net_idx, const meshx_subnet_key_t *net_key, uint32_t global_subnet_id, uint8_t role)
{
    MESHX_UNUSED(net_idx);
    MESHX_UNUSED(net_key);
    MESHX_UNUSED(global_subnet_id);
    MESHX_UNUSED(role);
    return MESHX_SUCCESS;
}
meshx_err_t meshx_subnet_coex_bridge_del(uint16_t net_idx, bool force_purge)
{
    MESHX_UNUSED(net_idx);
    MESHX_UNUSED(force_purge);
    return MESHX_SUCCESS;
}
meshx_err_t meshx_subnet_coex_rule_set(const meshx_bridge_rule_t *rule, bool enable, uint32_t group_forward_bitmap)
{
    MESHX_UNUSED(rule);
    MESHX_UNUSED(enable);
    MESHX_UNUSED(group_forward_bitmap);
    return MESHX_SUCCESS;
}
meshx_err_t meshx_subnet_coex_cluster_key_update(const meshx_subnet_key_t *k_bridge, uint32_t new_cluster_epoch, uint32_t new_term)
{
    MESHX_UNUSED(k_bridge);
    MESHX_UNUSED(new_cluster_epoch);
    MESHX_UNUSED(new_term);
    return MESHX_SUCCESS;
}

#endif /* CONFIG_MESHX_ISLAND_SUBNET_COEXISTENCE */
