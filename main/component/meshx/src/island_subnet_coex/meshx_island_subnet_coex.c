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
#include "interface/island_subnet_coex/meshx_island_subnet_coex.h"
#include "island_subnet_coex/meshx_router_core.h"
#include "island_subnet_coex/meshx_subnet_kr.h"
#include "island_subnet_coex/meshx_nat_mgr.h"
#include "island_subnet_coex/meshx_subnet_security.h"
#include "island_subnet_coex/meshx_subnet_discovery.h"
#include "island_subnet_coex/meshx_subnet_bridge.h"
#include "island_subnet_coex/meshx_subnet_nvs.h"

#if defined(CONFIG_MESHX_ZGB) && CONFIG_MESHX_ZGB
#include "zgb/meshx_zgb.h"
#endif

#if defined(CONFIG_MESHX_ISLAND_SUBNET_COEXISTENCE) && CONFIG_MESHX_ISLAND_SUBNET_COEXISTENCE

#define MESHX_PAL_MAX_BRIDGE_RULES 8U

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
    uint32_t                    last_gc_min;
} meshx_coex_state_t;

static meshx_coex_state_t s_coex;

static meshx_err_t save_nonce_ckpt_cb(uint32_t ckpt, void *ctx)
{
    (void)ctx;
    return meshx_subnet_nvs_save_nonce(ckpt);
}

static meshx_err_t send_authenticated_uvp(uint16_t net_idx,
                                          uint16_t dst_addr,
                                          uint8_t  opcode,
                                          const void *payload,
                                          uint16_t payload_len,
                                          bool     ack_req)
{
    if (!s_coex.uvp_tx_cb) {
        return MESHX_SUCCESS;
    }
    if (!s_coex.sec.k_bridge_valid || opcode == 0xD4U || opcode == 0xDAU) {
        return s_coex.uvp_tx_cb(net_idx, dst_addr, opcode,
                                (const uint8_t *)payload, payload_len, ack_req, s_coex.uvp_tx_ctx);
    }

    uint8_t frame[128];
    uint16_t frame_len = 0U;
    meshx_err_t err = meshx_subnet_sec_sign_frame(&s_coex.sec,
                                                  opcode,
                                                  s_coex.nat.local_node_addr,
                                                  (const uint8_t *)payload,
                                                  payload_len,
                                                  frame,
                                                  sizeof(frame),
                                                  &frame_len);
    if (err != MESHX_SUCCESS) {
        return err;
    }
    return s_coex.uvp_tx_cb(net_idx, dst_addr, opcode, frame, frame_len, ack_req, s_coex.uvp_tx_ctx);
}

static meshx_err_t coex_router_nack_cb(uint16_t net_idx,
                                       uint16_t dst_addr,
                                       const uint8_t *nack_buf,
                                       uint8_t nack_len,
                                       void *user_ctx)
{
    (void)user_ctx;
    return send_authenticated_uvp(net_idx, dst_addr, 0xD7U, nack_buf, nack_len, true);
}

meshx_err_t meshx_island_subnet_coex_init(void)
{
    if (s_coex.initialized) {
        return MESHX_SUCCESS;
    }

    memset(&s_coex, 0, sizeof(s_coex));

    uint32_t nonce_ckpt = 0UL;
    (void)meshx_subnet_nvs_load_nonce(&nonce_ckpt);

    meshx_router_core_init(&s_coex.router, NULL, coex_router_nack_cb, &s_coex);
    meshx_subnet_kr_init(&s_coex.kr, false);
    meshx_nat_mgr_init(&s_coex.nat, 0x0001U);
    meshx_subnet_sec_init(&s_coex.sec, nonce_ckpt, save_nonce_ckpt_cb, &s_coex);
    meshx_subnet_disc_init(&s_coex.disc);
    meshx_subnet_bridge_init(&s_coex.bridge, 0x0001U, true, 50U, 0UL);

    meshx_nvs_kbridge_blob_t kb;
    memset(&kb, 0, sizeof(kb));
    if (meshx_subnet_nvs_load_kbridge(&kb) == MESHX_SUCCESS && kb.valid != 0U) {
        (void)meshx_subnet_sec_set_k_bridge(&s_coex.sec, kb.k_bridge, kb.cluster_epoch, kb.active_term);
        s_coex.kr.has_cluster_bridge_key = true;
    }

    uint8_t nat_cnt = 0U;
    (void)meshx_subnet_nvs_load_nat_table(s_coex.nat.tier2, &nat_cnt);
    (void)meshx_subnet_nvs_load_pair_records(s_coex.disc.pairs, &s_coex.disc.pair_count);

    s_coex.initialized = true;
    return MESHX_SUCCESS;
}

void meshx_island_subnet_coex_deinit(void)
{
    if (!s_coex.initialized) {
        return;
    }
    meshx_router_core_deinit(&s_coex.router);
    meshx_nat_mgr_deinit(&s_coex.nat);
    meshx_subnet_sec_deinit(&s_coex.sec);
    meshx_subnet_disc_deinit(&s_coex.disc);
    memset(&s_coex, 0, sizeof(s_coex));
}

bool meshx_is_island_subnet_coex_enabled(void)
{
    return s_coex.initialized;
}

void meshx_subnet_coex_register_uvp_tx(meshx_subnet_uvp_tx_fn_t tx_cb, void *user_ctx)
{
    s_coex.uvp_tx_cb  = tx_cb;
    s_coex.uvp_tx_ctx = user_ctx;
}

meshx_err_t meshx_subnet_coex_pal_bridge_set(bool enable)
{
    if (!s_coex.initialized) {
        (void)meshx_island_subnet_coex_init();
    }
    s_coex.bridge_enabled = enable;
    return MESHX_SUCCESS;
}

meshx_err_t meshx_subnet_coex_pal_bridge_rule_add(const meshx_bridge_rule_t *rule)
{
    if (!rule) {
        return MESHX_INVALID_ARG;
    }
    if (!s_coex.initialized) {
        (void)meshx_island_subnet_coex_init();
    }
    for (uint8_t i = 0U; i < s_coex.rule_count; i++) {
        if (s_coex.rules[i].net_idx_1 == rule->net_idx_1 &&
            s_coex.rules[i].net_idx_2 == rule->net_idx_2 &&
            s_coex.rules[i].addr_1 == rule->addr_1 &&
            s_coex.rules[i].addr_2 == rule->addr_2) {
            s_coex.rules[i] = *rule;
            return MESHX_SUCCESS;
        }
    }
    if (s_coex.rule_count >= MESHX_PAL_MAX_BRIDGE_RULES) {
        return MESHX_NO_MEM;
    }
    s_coex.rules[s_coex.rule_count++] = *rule;
    return MESHX_SUCCESS;
}

bool meshx_subnet_coex_is_kr_quarantined(void)
{
    return s_coex.initialized && s_coex.kr.has_cluster_bridge_key;
}

meshx_err_t meshx_subnet_coex_on_kr_transition(uint16_t net_idx,
                                               uint8_t new_phase,
                                               const uint8_t new_key[16],
                                               uint32_t new_epoch)
{
    if (!s_coex.initialized) {
        return MESHX_INVALID_STATE;
    }
    return meshx_subnet_kr_set_phase(&s_coex.kr,
                                     net_idx,
                                     (meshx_kr_phase_t)new_phase,
                                     new_key,
                                     new_epoch);
}

meshx_err_t meshx_subnet_coex_tick(uint32_t now_ms)
{
    if (!s_coex.initialized) {
        return MESHX_INVALID_STATE;
    }

    (void)meshx_router_stage2_pump(&s_coex.router, now_ms);

    uint32_t hb_boundary = 0UL, claim_boundary = 0UL;
    (void)meshx_subnet_bridge_tick(&s_coex.bridge,
                                   now_ms,
                                   8192UL,
                                   &hb_boundary,
                                   &claim_boundary);

    if (hb_boundary != 0UL) {
        meshx_uvp_heartbeat_payload_t hb;
        memset(&hb, 0, sizeof(hb));
        hb.boundary_id       = hb_boundary;
        hb.slot_digest_crc32 = s_coex.nat.slots.table_digest_crc32;
        hb.role              = (uint8_t)MESHX_BRIDGE_ROLE_ACTIVE;
        (void)send_authenticated_uvp(0U, 0xFFFFU, 0xD1U, &hb, sizeof(hb), false);
    }

    if (claim_boundary != 0UL) {
        meshx_uvp_takeover_payload_t tk;
        memset(&tk, 0, sizeof(tk));
        tk.boundary_id    = claim_boundary;
        tk.candidate_addr = s_coex.nat.local_node_addr;
        (void)send_authenticated_uvp(0U, 0xFFFFU, 0xD2U, &tk, sizeof(tk), false);
    }

    uint32_t now_min = now_ms / 60000UL;
    if (now_min != s_coex.last_gc_min) {
        s_coex.last_gc_min = now_min;
        (void)meshx_nat_tier2_gc(&s_coex.nat, (uint16_t)now_min);
    }
    return MESHX_SUCCESS;
}

meshx_err_t meshx_subnet_coex_handle_uvp(uint8_t opcode,
                                         uint16_t src_addr,
                                         uint16_t dst_addr,
                                         uint16_t net_idx,
                                         const uint8_t *payload,
                                         uint16_t payload_len)
{
    (void)dst_addr;
    if (!s_coex.initialized || !payload) {
        return MESHX_INVALID_ARG;
    }

    uint32_t rx_term = s_coex.sec.active_term;
    uint16_t sender  = src_addr;
    const uint8_t *body = payload;
    uint16_t body_len   = payload_len;

    if (opcode != 0xD4U && opcode != 0xDAU && s_coex.sec.k_bridge_valid &&
        payload_len >= MESHX_CLUSTER_AUTH_OVERHEAD) {
        uint8_t auth_op = opcode;
        meshx_err_t verr = meshx_subnet_sec_verify_frame(&s_coex.sec,
                                                         payload,
                                                         payload_len,
                                                         &auth_op,
                                                         &rx_term,
                                                         &sender,
                                                         &body,
                                                         &body_len);
        if (verr != MESHX_SUCCESS) {
            return verr;
        }
        opcode = auth_op;
    }

    switch (opcode) {
    case 0xD1U:
        if (body_len >= sizeof(meshx_uvp_heartbeat_payload_t)) {
            meshx_uvp_heartbeat_payload_t hb;
            memcpy(&hb, body, sizeof(hb));
            bool send_demote = false;
            uint16_t demote_dst = 0U;
            (void)meshx_subnet_bridge_on_heartbeat(&s_coex.bridge,
                                                   hb.boundary_id,
                                                   rx_term,
                                                   sender,
                                                   hb.score,
                                                   0UL,
                                                   &s_coex.router.queue,
                                                   &send_demote,
                                                   &demote_dst);
            if (send_demote) {
                meshx_uvp_demote_payload_t dm = {
                    .boundary_id  = hb.boundary_id,
                    .winner_addr  = s_coex.nat.local_node_addr,
                    .winner_score = s_coex.bridge.boundaries[0].local_score,
                    .winning_term = s_coex.sec.active_term,
                };
                (void)send_authenticated_uvp(net_idx, demote_dst, 0xDBU, &dm, sizeof(dm), false);
            }
        }
        return MESHX_SUCCESS;
    case 0xD2U:
        if (body_len >= sizeof(meshx_uvp_takeover_payload_t)) {
            meshx_uvp_takeover_payload_t tk;
            memcpy(&tk, body, sizeof(tk));
            bool send_demote = false;
            (void)meshx_subnet_bridge_on_takeover_claim(&s_coex.bridge,
                                                        tk.boundary_id,
                                                        rx_term,
                                                        tk.candidate_addr,
                                                        tk.score,
                                                        0UL,
                                                        &s_coex.router.queue,
                                                        &send_demote);
        }
        return MESHX_SUCCESS;
    case 0xD5U:
        if (body_len >= sizeof(meshx_uvp_nat_sync_payload_t)) {
            meshx_uvp_nat_sync_payload_t sync;
            memcpy(&sync, body, sizeof(sync));
            return meshx_nat_tier2_apply_sync(&s_coex.nat, &sync.entry, sync.op);
        }
        return MESHX_INVALID_ARG;
    case 0xD6U:
        if (body_len >= sizeof(meshx_uvp_route_adv_t)) {
            meshx_uvp_route_adv_t adv;
            memcpy(&adv, body, sizeof(adv));
            uint32_t src_gid = meshx_nat_get_global_id_by_net_idx(&s_coex.nat, net_idx);
            return meshx_subnet_bridge_route_update(&s_coex.bridge, &adv, src_gid, 0UL);
        }
        return MESHX_INVALID_ARG;
    case 0xD9U:
        if (body_len >= sizeof(meshx_uvp_router_nack_ack_t)) {
            meshx_uvp_router_nack_ack_t ack;
            memcpy(&ack, body, sizeof(ack));
            return meshx_router_on_nack_ack(&s_coex.router, ack.nack_id);
        }
        return MESHX_INVALID_ARG;
    case 0xDBU:
        if (body_len >= sizeof(meshx_uvp_demote_payload_t)) {
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
    case 0xDDU:
        if (body_len >= sizeof(meshx_uvp_discover_rsp_t)) {
            meshx_uvp_discover_rsp_t rsp;
            memcpy(&rsp, body, sizeof(rsp));
            uint16_t vnat = 0U;
            return meshx_subnet_disc_handle_rsp(&s_coex.disc, &s_coex.nat, &rsp, 0U, 0UL, &vnat);
        }
        return MESHX_INVALID_ARG;
    default:
        return MESHX_SUCCESS;
    }
}

void meshx_subnet_coex_record_rx_mic_fail(uint32_t now_ms)
{
    if (!s_coex.initialized) {
        return;
    }
    (void)meshx_subnet_disc_record_mic_fail(&s_coex.disc, now_ms);
}

void meshx_subnet_coex_record_rx_success(void)
{
    if (!s_coex.initialized) {
        return;
    }
    meshx_subnet_disc_reset_mic_fails(&s_coex.disc);
}

void meshx_subnet_coex_on_beacon_rx(uint16_t company_id,
                                    const uint8_t *payload,
                                    uint16_t payload_len,
                                    int8_t rssi)
{
    (void)company_id;
    (void)payload;
    (void)payload_len;
    if (!s_coex.initialized) {
        return;
    }
    (void)meshx_subnet_disc_eval_candidate(&s_coex.disc, rssi, true, 8192U);
}

meshx_err_t meshx_subnet_coex_bridge_list(meshx_subnet_info_t *out_items,
                                          size_t max_items,
                                          size_t *out_count,
                                          uint32_t *out_cluster_epoch,
                                          uint32_t *out_active_term)
{
    if (!out_count) {
        return MESHX_INVALID_ARG;
    }
    *out_count = 0U;
    if (out_cluster_epoch) {
        *out_cluster_epoch = s_coex.sec.cluster_epoch;
    }
    if (out_active_term) {
        *out_active_term = s_coex.sec.active_term;
    }
    if (!s_coex.initialized) {
        return MESHX_SUCCESS;
    }

    size_t idx = 0U;
    for (meshx_local_slot_entry_t *c = s_coex.nat.slots.head;
         c != NULL && idx < max_items;
         c = c->next) {
        meshx_subnet_info_t *info = &out_items[idx++];
        memset(info, 0, sizeof(*info));
        info->global_subnet_id = c->global_subnet_id;
        info->local_net_idx    = c->local_net_idx;
        info->slot_n           = c->slot_n;
        meshx_subnet_crypto_slot_t *ks = meshx_subnet_kr_find_by_net_idx(&s_coex.kr, c->local_net_idx);
        if (ks) {
            info->kr_phase  = ks->kr_phase;
            info->key_epoch = ks->key_epoch;
        }
        info->role = (idx <= MESHX_MAX_BRIDGING_SLOTS && s_coex.bridge.boundaries[idx - 1U].valid)
                         ? (uint8_t)s_coex.bridge.boundaries[idx - 1U].role
                         : (uint8_t)MESHX_BRIDGE_ROLE_ACTIVE;
    }
    *out_count = idx;
    return MESHX_SUCCESS;
}

meshx_err_t meshx_subnet_coex_bridge_add(uint16_t net_idx,
                                         const uint8_t net_key[16],
                                         uint32_t global_subnet_id,
                                         uint8_t role)
{
    if (!s_coex.initialized || !net_key) {
        return MESHX_INVALID_ARG;
    }
    uint32_t gid = (global_subnet_id != MESHX_GLOBAL_SUBNET_INVALID)
                       ? global_subnet_id
                       : meshx_nat_derive_global_subnet_id(net_key, 0x02E5U, NULL, 0U, NULL);

    uint8_t slot_n = 0U;
    bool is_primary = (s_coex.nat.slots.active_count == 0U);
    meshx_err_t err = meshx_nat_slot_assign(&s_coex.nat, gid, net_idx, is_primary, &slot_n);
    if (err != MESHX_SUCCESS) {
        return err;
    }
    err = meshx_subnet_kr_slot_bind(&s_coex.kr, net_idx, gid, net_key, 1UL, 0UL);
    if (err != MESHX_SUCCESS) {
        (void)meshx_nat_slot_release(&s_coex.nat, gid);
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

meshx_err_t meshx_subnet_coex_bridge_del(uint16_t net_idx, bool force_purge)
{
    if (!s_coex.initialized) {
        return MESHX_INVALID_STATE;
    }
    uint32_t gid = meshx_nat_get_global_id_by_net_idx(&s_coex.nat, net_idx);
    if (gid == MESHX_GLOBAL_SUBNET_INVALID) {
        return MESHX_NOT_FOUND;
    }
    if (force_purge) {
        (void)meshx_router_queue_purge_boundary(&s_coex.router.queue, gid);
    }
    (void)meshx_subnet_kr_slot_unbind(&s_coex.kr, net_idx);
    return meshx_nat_slot_release(&s_coex.nat, gid);
}

meshx_err_t meshx_subnet_coex_rule_set(const meshx_bridge_rule_t *rule,
                                       bool enable,
                                       uint32_t group_forward_bitmap)
{
    if (!s_coex.initialized) {
        return MESHX_INVALID_STATE;
    }
    if (rule) {
        meshx_err_t err = meshx_subnet_coex_pal_bridge_rule_add(rule);
        if (err != MESHX_SUCCESS) {
            return err;
        }
    }
    s_coex.bridge_enabled           = enable;
    s_coex.nat.group_forward_bitmap = group_forward_bitmap;
    return MESHX_SUCCESS;
}

meshx_err_t meshx_subnet_coex_cluster_key_update(const uint8_t k_bridge[16],
                                                 uint32_t new_cluster_epoch,
                                                 uint32_t new_term)
{
    if (!s_coex.initialized || !k_bridge) {
        return MESHX_INVALID_ARG;
    }

    /* Persist-before-activate rule (TRD Page 8) */
    meshx_nvs_kbridge_blob_t blob;
    memset(&blob, 0, sizeof(blob));
    memcpy(blob.k_bridge, k_bridge, 16U);
    blob.cluster_epoch = new_cluster_epoch;
    blob.active_term   = new_term;
    blob.valid         = 1U;
    meshx_err_t err = meshx_subnet_nvs_save_kbridge(&blob);
    memset(&blob, 0, sizeof(blob));
    if (err != MESHX_SUCCESS) {
        return err;
    }

    err = meshx_subnet_sec_set_k_bridge(&s_coex.sec, k_bridge, new_cluster_epoch, new_term);
    if (err == MESHX_SUCCESS) {
        s_coex.kr.has_cluster_bridge_key = true;
    }
    return err;
}

#else

meshx_err_t meshx_island_subnet_coex_init(void) { return MESHX_SUCCESS; }
void meshx_island_subnet_coex_deinit(void) {}
bool meshx_is_island_subnet_coex_enabled(void) { return false; }
void meshx_subnet_coex_register_uvp_tx(meshx_subnet_uvp_tx_fn_t tx_cb, void *user_ctx) { (void)tx_cb; (void)user_ctx; }
meshx_err_t meshx_subnet_coex_pal_bridge_set(bool enable) { (void)enable; return MESHX_SUCCESS; }
meshx_err_t meshx_subnet_coex_pal_bridge_rule_add(const meshx_bridge_rule_t *rule) { (void)rule; return MESHX_SUCCESS; }
bool meshx_subnet_coex_is_kr_quarantined(void) { return false; }
meshx_err_t meshx_subnet_coex_on_kr_transition(uint16_t net_idx, uint8_t new_phase, const uint8_t new_key[16], uint32_t new_epoch) { (void)net_idx; (void)new_phase; (void)new_key; (void)new_epoch; return MESHX_SUCCESS; }
meshx_err_t meshx_subnet_coex_tick(uint32_t now_ms) { (void)now_ms; return MESHX_SUCCESS; }
meshx_err_t meshx_subnet_coex_handle_uvp(uint8_t opcode, uint16_t src_addr, uint16_t dst_addr, uint16_t net_idx, const uint8_t *payload, uint16_t payload_len)
{
    (void)opcode; (void)src_addr; (void)dst_addr; (void)net_idx; (void)payload; (void)payload_len;
    return MESHX_SUCCESS;
}
void meshx_subnet_coex_record_rx_mic_fail(uint32_t now_ms) { (void)now_ms; }
void meshx_subnet_coex_record_rx_success(void) {}
void meshx_subnet_coex_on_beacon_rx(uint16_t company_id, const uint8_t *payload, uint16_t payload_len, int8_t rssi)
{
    (void)company_id; (void)payload; (void)payload_len; (void)rssi;
}
meshx_err_t meshx_subnet_coex_bridge_list(meshx_subnet_info_t *out_items, size_t max_items, size_t *out_count, uint32_t *out_cluster_epoch, uint32_t *out_active_term)
{
    (void)out_items; (void)max_items;
    if (out_count) *out_count = 0U;
    if (out_cluster_epoch) *out_cluster_epoch = 0UL;
    if (out_active_term) *out_active_term = 0UL;
    return MESHX_SUCCESS;
}
meshx_err_t meshx_subnet_coex_bridge_add(uint16_t net_idx, const uint8_t net_key[16], uint32_t global_subnet_id, uint8_t role) { (void)net_idx; (void)net_key; (void)global_subnet_id; (void)role; return MESHX_SUCCESS; }
meshx_err_t meshx_subnet_coex_bridge_del(uint16_t net_idx, bool force_purge) { (void)net_idx; (void)force_purge; return MESHX_SUCCESS; }
meshx_err_t meshx_subnet_coex_rule_set(const meshx_bridge_rule_t *rule, bool enable, uint32_t group_forward_bitmap) { (void)rule; (void)enable; (void)group_forward_bitmap; return MESHX_SUCCESS; }
meshx_err_t meshx_subnet_coex_cluster_key_update(const uint8_t k_bridge[16], uint32_t new_cluster_epoch, uint32_t new_term) { (void)k_bridge; (void)new_cluster_epoch; (void)new_term; return MESHX_SUCCESS; }

#endif /* CONFIG_MESHX_ISLAND_SUBNET_COEXISTENCE */
