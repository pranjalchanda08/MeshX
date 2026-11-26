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
#include "interface/rtos/meshx_rtos_utils.h"

#if defined(CONFIG_MESHX_ZGB) && CONFIG_MESHX_ZGB
#include "zgb/meshx_zgb.h"
#endif

#if defined(CONFIG_MESHX_ISLAND_SUBNET_COEXISTENCE) && CONFIG_MESHX_ISLAND_SUBNET_COEXISTENCE

#define MESHX_PAL_MAX_BRIDGE_RULES 8U

/**
 * @brief Registered transport and data callbacks for coexistence.
 */
typedef struct {
    meshx_subnet_uvp_tx_fn_t  uvp_tx_cb;
    void                     *uvp_tx_ctx;
    meshx_router_send_fn_t    data_tx_cb;
    void                     *data_tx_ctx;
    meshx_subnet_data_rx_fn_t data_rx_cb;
    void                     *data_rx_ctx;
} meshx_coex_callbacks_t;

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
    meshx_coex_callbacks_t      cbs;
    meshx_subnet_key_t          local_dev_key;
    bool                        has_dev_key;
    bool                        invite_acked;
    uint32_t                    now_ms;
    uint32_t                    last_gc_min;
    uint32_t                    last_route_adv_ms;
} meshx_coex_state_t;

static meshx_coex_state_t s_coex;

static meshx_boundary_redundancy_t *find_coex_boundary(uint32_t boundary_id);

/**
 * @brief Securely zero sensitive key material in a way the compiler cannot optimize away.
 *
 * @param[out] buf Pointer to buffer to clear.
 * @param[in]  len Number of bytes to clear.
 */
static void coex_secure_zero(void *buf, size_t len)
{
    volatile uint8_t *p = (volatile uint8_t *)buf;
    while (len > 0U)
    {
        *p++ = 0U;
        len--;
    }
}

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
    if (!s_coex.cbs.uvp_tx_cb)
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
        return s_coex.cbs.uvp_tx_cb(net_idx, dst_addr, opcode,
                                    plain, (uint16_t)(payload_len + 1U), ack_req, s_coex.cbs.uvp_tx_ctx);
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
    return s_coex.cbs.uvp_tx_cb(net_idx, dst_addr, opcode, frame, frame_len, ack_req, s_coex.cbs.uvp_tx_ctx);
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
static meshx_err_t coex_router_tx_cb(const meshx_router_queue_entry_t *entry,
                                     const meshx_ccm_nonce_t *nonce,
                                     const meshx_subnet_key_t *tx_net_key,
                                     void *user_ctx)
{
    MESHX_UNUSED(tx_net_key);
    MESHX_UNUSED(user_ctx);
    if (!entry || !s_coex.initialized)
    {
        return MESHX_INVALID_ARG;
    }
    if (!s_coex.bridge_enabled)
    {
        return MESHX_INVALID_STATE;
    }

    const meshx_subnet_key_t *egress_key = NULL;
    uint8_t tx_epoch = 0U;
    meshx_subnet_crypto_slot_t *slot = NULL;
    if (meshx_subnet_kr_find_by_net_idx(&s_coex.kr, entry->tx_net_idx, &slot) == MESHX_SUCCESS && slot)
    {
        meshx_err_t kerr = meshx_subnet_kr_select_egress_key(slot, &tx_epoch, &egress_key);
        if (kerr != MESHX_SUCCESS)
        {
            return kerr;
        }
        uint32_t seq = 0UL;
        bool ivu = false;
        meshx_err_t seq_err = meshx_subnet_kr_alloc_seq(&s_coex.kr, entry->tx_net_idx, &seq, &ivu);
        if (seq_err != MESHX_SUCCESS)
        {
            return seq_err;
        }
    }

    if (s_coex.cbs.data_tx_cb)
    {
        return s_coex.cbs.data_tx_cb(entry, nonce, egress_key, s_coex.cbs.data_tx_ctx);
    }
    if (s_coex.cbs.uvp_tx_cb && entry->pdu_len >= 1U && entry->pdu_len <= MESHX_MAX_UPPER_PDU_LEN)
    {
        uint8_t tunnel_buf[1U + sizeof(meshx_uvp_data_tunnel_hdr_t) + MESHX_MAX_UPPER_PDU_LEN];
        meshx_uvp_data_tunnel_hdr_t hdr;
        memset(&hdr, 0, sizeof(hdr));
        hdr.ttl = entry->ttl;
        hdr.flags = entry->flags;
        hdr.key_epoch = tx_epoch;
        hdr.orig_src_addr = entry->canonical_src;
        hdr.orig_dst_addr = entry->canonical_dst;
        hdr.egress_net_idx = entry->tx_net_idx;
        hdr.orig_seq_auth = entry->seq_auth;
        hdr.orig_iv_index = entry->orig_iv_index;
        hdr.trans_mic = entry->trans_mic;
        hdr.pdu_len = entry->pdu_len;

        tunnel_buf[0] = MESHX_UVP_OP_SUBNET_DATA_TUNNEL;
        memcpy(&tunnel_buf[1], &hdr, sizeof(hdr));
        memcpy(&tunnel_buf[1U + sizeof(hdr)], entry->pdu, entry->pdu_len);

        uint16_t total_len = (uint16_t)(1U + sizeof(hdr) + entry->pdu_len);
        meshx_err_t tx_err = s_coex.cbs.uvp_tx_cb(entry->tx_net_idx,
                                                  entry->orig_dst_addr,
                                                  MESHX_UVP_OP_SUBNET_DATA_TUNNEL,
                                                  tunnel_buf,
                                                  total_len,
                                                  (entry->flags & MESHX_ROUTER_FLAG_IS_UNICAST) != 0U,
                                                  s_coex.cbs.uvp_tx_ctx);
        if (tx_err == MESHX_SUCCESS)
        {
            meshx_err_t done_err = meshx_router_stage2_on_tx_complete(&s_coex.router, true, s_coex.now_ms);
            if (done_err != MESHX_SUCCESS)
            {
                return done_err;
            }
        }
        return tx_err;
    }
    return MESHX_NOT_SUPPORTED;
}

/**
 * @brief Router emergency NACK transmission callback (0xD7).
 *
 * @param[in] net_idx  Target NetKey index.
 * @param[in] dst_addr Destination address.
 * @param[in] nack_buf Pointer to serialized 0xD7 NACK payload.
 * @param[in] nack_len Length of NACK payload in bytes.
 * @param[in] user_ctx Unused user context pointer.
 *
 * @return MESHX_SUCCESS on success, or UVP TX error code.
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
                                  MESHX_UVP_OP_SUBNET_NAT_SYNC, &sync, sizeof(sync), true);
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
    meshx_coex_callbacks_t saved_cbs = s_coex.cbs;

    memset(&s_coex, 0, sizeof(s_coex));

    s_coex.cbs = saved_cbs;

    uint32_t nonce_ckpt = 0UL;
    meshx_err_t err = meshx_subnet_nvs_load_nonce(&nonce_ckpt);
    if (err != MESHX_SUCCESS && err != MESHX_NOT_FOUND)
    {
        return err;
    }

    err = meshx_router_core_init(&s_coex.router, coex_router_tx_cb, coex_router_nack_cb, &s_coex);
    if (err != MESHX_SUCCESS)
    {
        return err;
    }
    err = meshx_subnet_kr_init(&s_coex.kr, false);
    if (err != MESHX_SUCCESS)
    {
        return err;
    }
    err = meshx_nat_mgr_init(&s_coex.nat, MESHX_COEX_DEFAULT_LOCAL_ADDR);
    if (err != MESHX_SUCCESS)
    {
        return err;
    }
    err = meshx_subnet_sec_init(&s_coex.sec, nonce_ckpt, save_nonce_ckpt_cb, &s_coex);
    if (err != MESHX_SUCCESS)
    {
        return err;
    }
    err = meshx_subnet_disc_init(&s_coex.disc);
    if (err != MESHX_SUCCESS)
    {
        return err;
    }
    err = meshx_subnet_bridge_init(&s_coex.bridge, MESHX_COEX_DEFAULT_LOCAL_ADDR, true, 50U, 0UL);
    if (err != MESHX_SUCCESS)
    {
        return err;
    }

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
    if (meshx_router_core_deinit(&s_coex.router) != MESHX_SUCCESS)
    {
        memset(&s_coex.router, 0, sizeof(s_coex.router));
    }
    if (meshx_nat_mgr_deinit(&s_coex.nat) != MESHX_SUCCESS)
    {
        s_coex.nat.slots.active_count = 0U;
    }
    if (meshx_subnet_sec_deinit(&s_coex.sec) != MESHX_SUCCESS)
    {
        memset(&s_coex.sec, 0, sizeof(s_coex.sec));
    }
    if (meshx_subnet_disc_deinit(&s_coex.disc) != MESHX_SUCCESS)
    {
        memset(&s_coex.disc, 0, sizeof(s_coex.disc));
    }

    /* Keep the UVP TX binding across teardown/re-init cycles. */
    meshx_subnet_uvp_tx_fn_t saved_tx_cb = s_coex.cbs.uvp_tx_cb;
    void *saved_tx_ctx = s_coex.cbs.uvp_tx_ctx;
    memset(&s_coex, 0, sizeof(s_coex));
    s_coex.cbs.uvp_tx_cb = saved_tx_cb;
    s_coex.cbs.uvp_tx_ctx = saved_tx_ctx;
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
    s_coex.cbs.uvp_tx_cb = tx_cb;
    s_coex.cbs.uvp_tx_ctx = user_ctx;
}

/**
 * @brief Register Stage 2 data-plane TX callback.
 *
 * @param[in] tx_cb    Stage 2 transmit callback.
 * @param[in] user_ctx User context pointer.
 */
void meshx_subnet_coex_register_data_tx(meshx_router_send_fn_t tx_cb, void *user_ctx)
{
    s_coex.cbs.data_tx_cb = tx_cb;
    s_coex.cbs.data_tx_ctx = user_ctx;
}

/**
 * @brief Register Option B local delivery callback for tunneled data PDUs.
 *
 * @param[in] rx_cb    Local data delivery callback.
 * @param[in] user_ctx User context pointer.
 */
void meshx_subnet_coex_register_data_rx(meshx_subnet_data_rx_fn_t rx_cb, void *user_ctx)
{
    s_coex.cbs.data_rx_cb = rx_cb;
    s_coex.cbs.data_rx_ctx = user_ctx;
}

/**
 * @brief Send an Upper PDU across subnets via the Option B UVP data tunnel (0xDF).
 *
 * @param[in] req Pointer to data-tunnel transmit request descriptor.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_coex_send_data(const meshx_subnet_data_tx_req_t *req)
{
    if (!s_coex.initialized || !req || !req->pdu || req->pdu_len == 0U || req->pdu_len > MESHX_MAX_UPPER_PDU_LEN)
    {
        return MESHX_INVALID_ARG;
    }
    if (!s_coex.cbs.uvp_tx_cb)
    {
        return MESHX_NOT_SUPPORTED;
    }
    uint8_t tunnel_buf[1U + sizeof(meshx_uvp_data_tunnel_hdr_t) + MESHX_MAX_UPPER_PDU_LEN];
    meshx_uvp_data_tunnel_hdr_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.ttl = req->ttl;
    hdr.flags = (req->canonical_dst < 0x8000U) ? MESHX_ROUTER_FLAG_IS_UNICAST : 0U;
    hdr.orig_src_addr = s_coex.nat.local_node_addr;
    hdr.orig_dst_addr = req->canonical_dst;
    hdr.egress_net_idx = req->net_idx;
    hdr.orig_seq_auth = req->seq_auth;
    hdr.orig_iv_index = req->iv_index;
    hdr.pdu_len = req->pdu_len;

    tunnel_buf[0] = MESHX_UVP_OP_SUBNET_DATA_TUNNEL;
    memcpy(&tunnel_buf[1], &hdr, sizeof(hdr));
    memcpy(&tunnel_buf[1U + sizeof(hdr)], req->pdu, req->pdu_len);

    return s_coex.cbs.uvp_tx_cb(req->net_idx, req->bridge_or_dst_addr, MESHX_UVP_OP_SUBNET_DATA_TUNNEL,
                                tunnel_buf, (uint16_t)(1U + sizeof(hdr) + req->pdu_len),
                                (req->canonical_dst < 0x8000U), s_coex.cbs.uvp_tx_ctx);
}

/**
 * @brief Resolve destination NAT/group routing for an inbound Stage 1 PDU.
 *
 * @param[in]     pdu      Inbound Stage 1 PDU.
 * @param[in,out] resolved Resolved Stage 1 PDU to populate.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
static meshx_err_t coex_resolve_pdu_route(const meshx_router_stage1_pdu_t *pdu,
                                          meshx_router_stage1_pdu_t *resolved)
{
    if (pdu->dst_addr >= 0xC000U && pdu->dst_addr <= 0xFBFFU)
    {
        bool allowed = false;
        meshx_err_t gerr = meshx_nat_is_group_forward_allowed(&s_coex.nat, pdu->dst_addr, &allowed);
        if (gerr != MESHX_SUCCESS)
        {
            return gerr;
        }
        if (!allowed)
        {
            return MESHX_NOT_SUPPORTED;
        }
        uint8_t in_slot_n = 0U;
        if (meshx_nat_slot_get_by_net_idx(&s_coex.nat, pdu->net_idx, &in_slot_n) == MESHX_SUCCESS &&
            resolved->orig_dst_addr == 0U)
        {
            uint16_t prefixed = 0U;
            if (meshx_nat_prefix_group_addr(in_slot_n, pdu->dst_addr, &prefixed) == MESHX_SUCCESS)
            {
                resolved->orig_dst_addr = prefixed;
            }
        }
        return MESHX_SUCCESS;
    }

    if (pdu->dst_addr >= MESHX_NAT_TIER1_BASE && pdu->dst_addr <= MESHX_NAT_TIER2_END)
    {
        uint32_t dst_gid = MESHX_GLOBAL_SUBNET_INVALID;
        uint16_t phys_dst = 0U;
        meshx_err_t nerr = meshx_nat_translate_outbound(&s_coex.nat, pdu->dst_addr, &dst_gid, &phys_dst);
        if (nerr != MESHX_SUCCESS)
        {
            return nerr;
        }
        resolved->global_subnet_dst = dst_gid;
        resolved->orig_dst_addr = phys_dst;
        resolved->canonical_dst = phys_dst;
        meshx_subnet_crypto_slot_t *out_slot = NULL;
        if (meshx_subnet_kr_find_by_global_id(&s_coex.kr, dst_gid, &out_slot) == MESHX_SUCCESS && out_slot)
        {
            resolved->egress_net_idx = out_slot->net_idx;
        }
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Enforce KR quarantine, multihop transit whitelist/loop check, and canonical RPL.
 *
 * @param[in] pdu      Original Stage 1 PDU.
 * @param[in] resolved Resolved Stage 1 PDU.
 * @param[in] now_ms   Current monotonic timestamp in milliseconds.
 *
 * @return MESHX_SUCCESS if authorized, or MESHX_SUBNET_ERR_AUTH / MESHX_SUBNET_ERR_REPLAY.
 */
/**
 * @brief Authorize multihop transit header against whitelist, loop mask, and echo filter.
 *
 * @param[in] pdu      Inbound Stage 1 PDU.
 * @param[in] resolved Resolved Stage 1 PDU.
 * @param[in] now_ms   Current monotonic timestamp in milliseconds.
 *
 * @return MESHX_SUCCESS if authorized, or MESHX_SUBNET_ERR_AUTH / error code.
 */
static meshx_err_t coex_check_multihop_transit(const meshx_router_stage1_pdu_t *pdu,
                                               const meshx_router_stage1_pdu_t *resolved,
                                               uint32_t now_ms)
{
    meshx_subnet_transit_hdr_t th;
    memcpy(&th, resolved->pdu, sizeof(th));
    uint32_t claimed_gid = (resolved->global_subnet_src != MESHX_GLOBAL_SUBNET_INVALID)
                               ? resolved->global_subnet_src
                               : (uint32_t)th.orig_subnet_id;
    bool wl_allowed = false;
    meshx_err_t werr = meshx_subnet_sec_whitelist_check(&s_coex.sec, pdu->src_addr, claimed_gid, &wl_allowed);
    if (werr != MESHX_SUCCESS)
    {
        return werr;
    }
    if (!wl_allowed)
    {
        return MESHX_SUBNET_ERR_AUTH;
    }

    uint8_t in_nat_slot = 0U;
    uint8_t out_nat_slot = 0U;
    uint8_t in_bit = (meshx_nat_slot_get_by_net_idx(&s_coex.nat, pdu->net_idx, &in_nat_slot) == MESHX_SUCCESS)
                         ? in_nat_slot
                         : 0U;
    uint8_t out_bit = (meshx_nat_slot_get_by_net_idx(&s_coex.nat, resolved->egress_net_idx, &out_nat_slot) == MESHX_SUCCESS)
                          ? out_nat_slot
                          : 0U;
    const meshx_subnet_key_t *kb = s_coex.sec.k_bridge_valid ? &s_coex.sec.k_bridge : NULL;
    meshx_bridge_transit_ctx_t tctx = {
        .in_slot_bit      = in_bit,
        .out_slot_bit     = out_bit,
        .egress_subnet_id = resolved->global_subnet_dst,
        .k_bridge         = kb,
        .now_ms           = now_ms,
    };
    bool transit_ok = false;
    meshx_err_t terr = meshx_subnet_bridge_transit_check_and_mark_ex(&s_coex.bridge, &th, &tctx, &transit_ok);
    if (terr != MESHX_SUCCESS)
    {
        return terr;
    }
    return transit_ok ? MESHX_SUCCESS : MESHX_SUBNET_ERR_AUTH;
}

static meshx_err_t coex_authorize_pdu_transit(const meshx_router_stage1_pdu_t *pdu,
                                              const meshx_router_stage1_pdu_t *resolved,
                                              uint32_t now_ms)
{
    bool quarantined = false;
    meshx_err_t qerr = meshx_subnet_kr_is_opcode_quarantined(&s_coex.kr, pdu->pdu ? pdu->pdu[0] : 0U,
                                                             pdu->net_idx, resolved->egress_net_idx,
                                                             &quarantined);
    if (qerr != MESHX_SUCCESS)
    {
        return qerr;
    }
    if (pdu->pdu && pdu->pdu_len >= 1U && quarantined)
    {
        return MESHX_SUBNET_ERR_AUTH;
    }

    if (resolved->is_multihop_transit && resolved->pdu &&
        resolved->pdu_len >= sizeof(meshx_subnet_transit_hdr_t))
    {
        meshx_err_t merr = coex_check_multihop_transit(pdu, resolved, now_ms);
        if (merr != MESHX_SUCCESS)
        {
            return merr;
        }
    }

    if (resolved->canonical_src != 0U)
    {
        bool rpl_ok = false;
        meshx_err_t rerr = meshx_subnet_sec_canonical_rpl_check(&s_coex.sec, resolved->global_subnet_src,
                                                                resolved->canonical_src, resolved->iv_index,
                                                                resolved->seq_auth, &rpl_ok);
        if (rerr != MESHX_SUCCESS)
        {
            return rerr;
        }
        if (!rpl_ok)
        {
            return MESHX_SUBNET_ERR_REPLAY;
        }
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Ingest a reassembled Stage 1 Upper Transport PDU for cross-subnet bridging.
 *
 * @param[in]  pdu                Reassembled Stage 1 PDU descriptor.
 * @param[in]  now_ms             Current monotonic timestamp in milliseconds.
 * @param[out] out_send_block_ack Set true if a Lower Transport Block ACK should be sent.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_coex_on_pdu_rx(const meshx_router_stage1_pdu_t *pdu,
                                        uint32_t now_ms,
                                        bool *out_send_block_ack)
{
    if (!s_coex.initialized || !pdu || !out_send_block_ack)
    {
        return MESHX_INVALID_ARG;
    }
    *out_send_block_ack = false;
    if (!s_coex.bridge_enabled)
    {
        return MESHX_INVALID_STATE;
    }

    uint32_t src_gid = MESHX_GLOBAL_SUBNET_INVALID;
    if (meshx_nat_get_global_id_by_net_idx(&s_coex.nat, pdu->net_idx, &src_gid) != MESHX_SUCCESS)
    {
        src_gid = MESHX_GLOBAL_SUBNET_INVALID;
    }
    meshx_subnet_crypto_slot_t *in_slot = NULL;
    uint32_t epoch = (meshx_subnet_kr_find_by_net_idx(&s_coex.kr, pdu->net_idx, &in_slot) == MESHX_SUCCESS && in_slot)
                         ? in_slot->key_epoch
                         : 0UL;

    bool rpl_ok = false;
    meshx_err_t rerr = meshx_subnet_sec_rpl_check_update(&s_coex.sec, pdu->net_idx, pdu->src_addr,
                                                         epoch, pdu->iv_index, pdu->seq_auth, &rpl_ok);
    if (rerr != MESHX_SUCCESS)
    {
        return rerr;
    }
    if (!rpl_ok)
    {
        return MESHX_SUBNET_ERR_REPLAY;
    }

    meshx_router_stage1_pdu_t resolved = *pdu;
    resolved.global_subnet_src = (pdu->global_subnet_src != MESHX_GLOBAL_SUBNET_INVALID)
                                     ? pdu->global_subnet_src
                                     : src_gid;

    meshx_err_t err = coex_resolve_pdu_route(pdu, &resolved);
    if (err != MESHX_SUCCESS)
    {
        return err;
    }

    err = coex_authorize_pdu_transit(pdu, &resolved, now_ms);
    if (err != MESHX_SUCCESS)
    {
        return err;
    }

    return meshx_router_stage1_ingress(&s_coex.router, &resolved, now_ms, out_send_block_ack);
}

/**
 * @brief Ingest a Stage 1 SAR segment into the coexistence router.
 *
 * @param[in]  seg                Incoming segment descriptor.
 * @param[in]  meta               Segment metadata (IV index, SeqAuth, TTL, MIC, timestamp).
 * @param[out] out_send_block_ack Set true if a Lower Transport Block ACK should be sent.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_coex_on_seg_rx(const meshx_router_seg_rx_t *seg,
                                        const meshx_subnet_seg_meta_t *meta,
                                        bool *out_send_block_ack)
{
    if (!s_coex.initialized || !seg || !meta || !out_send_block_ack)
    {
        return MESHX_INVALID_ARG;
    }
    *out_send_block_ack = false;
    bool complete = false;
    meshx_err_t err = meshx_router_reasm_segment(&s_coex.router, seg, &complete);
    if (err != MESHX_SUCCESS || !complete)
    {
        return err;
    }
    meshx_router_stage1_pdu_t pdu;
    memset(&pdu, 0, sizeof(pdu));
    pdu.net_idx = seg->net_idx;
    pdu.src_addr = seg->src_addr;
    pdu.dst_addr = seg->dst_addr;
    pdu.iv_index = meta->iv_index;
    pdu.seq_auth = meta->seq_auth;
    pdu.ttl = meta->ttl;
    pdu.szmic = meta->szmic;
    if (meta->trans_mic)
    {
        pdu.trans_mic = *meta->trans_mic;
    }
    pdu.pdu = s_coex.router.reasm.buf;
    pdu.pdu_len = s_coex.router.reasm.len;
    return meshx_subnet_coex_on_pdu_rx(&pdu, meta->now_ms, out_send_block_ack);
}

/**
 * @brief Notify coexistence router of Stage 2 Lower SAR completion.
 *
 * @param[in] ack_ok true if egress transmission/ACK succeeded, false otherwise.
 * @param[in] now_ms Current monotonic timestamp in milliseconds.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_coex_on_stage2_tx_complete(bool ack_ok, uint32_t now_ms)
{
    if (!s_coex.initialized)
    {
        return MESHX_INVALID_STATE;
    }
    return meshx_router_stage2_on_tx_complete(&s_coex.router, ack_ok, now_ms);
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
 * @brief Emit scheduled heartbeats and takeover claims for all active boundaries.
 *
 * @param[in] hb_mask    Bitmask of boundaries due for heartbeat TX.
 * @param[in] claim_mask Bitmask of boundaries due for takeover claim TX.
 *
 * @return MESHX_SUCCESS on success, or UVP TX error code.
 */
static meshx_err_t coex_tick_emit_boundaries(uint32_t hb_mask, uint32_t claim_mask)
{
    for (size_t i = 0; i < MESHX_MAX_BRIDGING_SLOTS; i++)
    {
        const meshx_boundary_redundancy_t *b = &s_coex.bridge.boundaries[i];
        if (!b->valid)
        {
            continue;
        }
        meshx_subnet_crypto_slot_t *ks = NULL;
        uint16_t tx_net = (meshx_subnet_kr_find_by_global_id(&s_coex.kr, b->boundary_id, &ks) == MESHX_SUCCESS && ks)
                              ? ks->net_idx
                              : MESHX_SUBNET_PRIMARY_NET_IDX;
        uint32_t saved_term = s_coex.sec.active_term;
        s_coex.sec.active_term = b->active_term;

        meshx_err_t err = MESHX_SUCCESS;
        if ((hb_mask & (1UL << i)) != 0UL)
        {
            meshx_uvp_heartbeat_payload_t hb;
            memset(&hb, 0, sizeof(hb));
            hb.boundary_id = b->boundary_id;
            hb.slot_digest_crc32 = s_coex.nat.slots.table_digest_crc32;
            hb.key_epoch = ks ? ks->key_epoch : 0UL;
            hb.kr_phase = ks ? ks->kr_phase : 0U;
            hb.role = (uint8_t)b->role;
            hb.score = b->local_score;
            err = send_authenticated_uvp(tx_net, MESHX_SUBNET_ADDR_ALL_NODES,
                                         MESHX_UVP_OP_SUBNET_HEARTBEAT, &hb, sizeof(hb), false);
        }
        if (err == MESHX_SUCCESS && (claim_mask & (1UL << i)) != 0UL)
        {
            meshx_uvp_takeover_payload_t tk;
            memset(&tk, 0, sizeof(tk));
            tk.boundary_id = b->boundary_id;
            tk.score = b->local_score;
            tk.candidate_addr = s_coex.nat.local_node_addr;
            tk.prev_active_addr = b->active_bridge_addr;
            tk.rank = b->standby_rank;
            err = send_authenticated_uvp(tx_net, MESHX_SUBNET_ADDR_ALL_NODES,
                                         MESHX_UVP_OP_SUBNET_TAKEOVER_CLAIM, &tk, sizeof(tk), false);
        }

        s_coex.sec.active_term = saved_term;
        if (err != MESHX_SUCCESS)
        {
            return err;
        }
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Periodically broadcast 0xD6 split-horizon route advertisements across bound subnets.
 *
 * @param[in] now_ms Current monotonic timestamp in milliseconds.
 *
 * @return MESHX_SUCCESS on success, or UVP TX error code.
 */
static meshx_err_t coex_tick_route_adv(uint32_t now_ms)
{
    if (!s_coex.bridge_enabled || !s_coex.sec.k_bridge_valid ||
        (now_ms - s_coex.last_route_adv_ms) < MESHX_ROUTE_ADV_INTERVAL_MS)
    {
        return MESHX_SUCCESS;
    }

    s_coex.last_route_adv_ms = now_ms;
    for (size_t i = 0; i < MESHX_MAX_BRIDGING_SLOTS; i++)
    {
        if (!s_coex.kr.slot_valid[i])
        {
            continue;
        }
        meshx_uvp_route_adv_t adv;
        if (meshx_subnet_bridge_route_export(&s_coex.bridge,
                                             s_coex.kr.slots[i].global_subnet_id,
                                             s_coex.nat.local_node_addr,
                                             &adv) == MESHX_SUCCESS &&
            adv.entry_count > 0U)
        {
            meshx_err_t err = send_authenticated_uvp(s_coex.kr.slots[i].net_idx,
                                                     MESHX_SUBNET_ADDR_ALL_NODES,
                                                     MESHX_UVP_OP_SUBNET_ROUTE_ADV,
                                                     &adv, sizeof(adv), false);
            if (err != MESHX_SUCCESS)
            {
                return err;
            }
        }
    }
    return MESHX_SUCCESS;
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
        if (s_coex.cbs.data_tx_cb || s_coex.router.queue.active_sar_flow_present == 0U)
        {
            return err;
        }
        meshx_err_t fail_err = meshx_router_stage2_on_tx_complete(&s_coex.router, false, now_ms);
        if (fail_err != MESHX_SUCCESS && fail_err != MESHX_NOT_FOUND)
        {
            return fail_err;
        }
    }

    uint32_t hb_mask = 0UL;
    uint32_t claim_mask = 0UL;
    err = meshx_subnet_bridge_tick_all(&s_coex.bridge,
                                       now_ms,
                                       MESHX_WITNESS_DEFAULT_HEAP_BYTES,
                                       &hb_mask,
                                       &claim_mask);
    if (err == MESHX_SUCCESS)
    {
        err = coex_tick_emit_boundaries(hb_mask, claim_mask);
    }
    if (err == MESHX_SUCCESS)
    {
        err = coex_tick_route_adv(now_ms);
    }
    if (err != MESHX_SUCCESS)
    {
        return err;
    }

    uint32_t now_min = now_ms / MESHX_MS_PER_MINUTE;
    if (now_min != s_coex.last_gc_min)
    {
        s_coex.last_gc_min = now_min;
        uint8_t evicted = 0U;
        meshx_err_t gc_err = meshx_nat_tier2_gc(&s_coex.nat, (uint16_t)now_min, &evicted);
        if (gc_err != MESHX_SUCCESS)
        {
            return gc_err;
        }
        if (evicted > 0U)
        {
            return meshx_subnet_nvs_save_nat_table(s_coex.nat.tier2, MESHX_NAT_TIER2_CAP);
        }
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Handle an incoming UVP Heartbeat frame (0xD1) and emit 0xDB Demote if local wins.
 *
 * @param[in] net_idx  Ingress NetKey index.
 * @param[in] rx_term  Authenticated cluster term.
 * @param[in] sender   Originator unicast address.
 * @param[in] body     Heartbeat payload bytes.
 * @param[in] body_len Payload length in bytes.
 *
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
    meshx_bridge_peer_event_t evt = {
        .boundary_id    = hb.boundary_id,
        .rx_term        = rx_term,
        .peer_addr      = sender,
        .peer_score     = hb.score,
        .now_ms         = s_coex.now_ms,
        .queue_to_purge = &s_coex.router.queue,
    };
    meshx_err_t err = meshx_subnet_bridge_on_heartbeat(&s_coex.bridge, &evt, &send_demote, &demote_dst);
    if (err != MESHX_SUCCESS)
    {
        return err;
    }
    if (send_demote)
    {
        const meshx_boundary_redundancy_t *b = find_coex_boundary(hb.boundary_id);
        meshx_uvp_demote_payload_t dm = {
            .boundary_id = hb.boundary_id,
            .winner_addr = s_coex.nat.local_node_addr,
            .winner_score = b ? b->local_score : 0U,
            .winning_term = b ? b->active_term : rx_term,
        };
        uint32_t saved_term = s_coex.sec.active_term;
        s_coex.sec.active_term = dm.winning_term;
        meshx_err_t derr = send_authenticated_uvp(net_idx, demote_dst, MESHX_UVP_OP_SUBNET_DEMOTE, &dm, sizeof(dm), false);
        s_coex.sec.active_term = saved_term;
        return derr;
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
static bool is_cluster_only_opcode(uint8_t opcode)
{
    return (opcode == MESHX_UVP_OP_SUBNET_HEARTBEAT ||
            opcode == MESHX_UVP_OP_SUBNET_TAKEOVER_CLAIM ||
            opcode == MESHX_UVP_OP_SUBNET_CANDIDATE_RPT ||
            opcode == MESHX_UVP_OP_SUBNET_NAT_SYNC ||
            opcode == MESHX_UVP_OP_SUBNET_ROUTE_ADV ||
            opcode == MESHX_UVP_OP_SUBNET_DEMOTE);
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
static meshx_boundary_redundancy_t *find_coex_boundary(uint32_t boundary_id)
{
    for (size_t i = 0; i < MESHX_MAX_BRIDGING_SLOTS; i++)
    {
        if (s_coex.bridge.boundaries[i].valid &&
            s_coex.bridge.boundaries[i].boundary_id == boundary_id)
        {
            return &s_coex.bridge.boundaries[i];
        }
    }
    return NULL;
}

/**
 * @brief Verified inbound UVP envelope fields returned by coex_verify_inbound_uvp().
 */
typedef struct {
    uint8_t        opcode;   /**< Resolved inner UVP opcode */
    uint16_t       sender;   /**< Resolved sender unicast address */
    uint32_t       term;     /**< Verified cluster term (0 for non-cluster frames) */
    const uint8_t *body;     /**< Pointer to inner payload body */
    uint16_t       body_len; /**< Length of inner payload body in bytes */
} coex_uvp_verify_out_t;

/**
 * @brief Verify inbound UVP frame authentication and unwrap payload body/metadata.
 *
 * @param[in]     net_idx     Ingress NetKey index.
 * @param[in]     payload     Raw incoming UVP frame buffer.
 * @param[in]     payload_len Length of incoming frame in bytes.
 * @param[in,out] out         Outer opcode/sender on entry; verified fields on return.
 *
 * @return MESHX_SUCCESS on success, or MESHX_SUBNET_ERR_AUTH / MESHX_SUBNET_ERR_REPLAY.
 */
static meshx_err_t coex_verify_inbound_uvp(uint16_t net_idx,
                                           const uint8_t *payload,
                                           uint16_t payload_len,
                                           coex_uvp_verify_out_t *out)
{
    uint8_t opcode = out->opcode;
    uint16_t src_addr = out->sender;
    out->term = 0UL;
    out->body = payload + 1U;
    out->body_len = (uint16_t)(payload_len - 1U);

    if (opcode == MESHX_UVP_OP_SUBNET_BRIDGE_INVITE ||
        opcode == MESHX_UVP_OP_SUBNET_INVITE_ACK ||
        opcode == MESHX_UVP_OP_SUBNET_DATA_TUNNEL)
    {
        if (opcode == MESHX_UVP_OP_SUBNET_DATA_TUNNEL &&
            (src_addr == 0x0000U || src_addr >= 0x8000U || net_idx == MESHX_NET_IDX_INVALID))
        {
            return MESHX_SUBNET_ERR_AUTH;
        }
        return MESHX_SUCCESS;
    }

    if (is_cluster_only_opcode(opcode) ||
        (s_coex.sec.k_bridge_valid && payload_len >= MESHX_CLUSTER_AUTH_OVERHEAD))
    {
        if (!s_coex.sec.k_bridge_valid || payload_len < MESHX_CLUSTER_AUTH_OVERHEAD)
        {
            return MESHX_SUBNET_ERR_AUTH;
        }
        uint32_t scope_bid = 0UL;
        uint32_t scratch_term = 0UL;
        uint32_t *term_ptr = NULL;
        if ((opcode == MESHX_UVP_OP_SUBNET_HEARTBEAT ||
             opcode == MESHX_UVP_OP_SUBNET_TAKEOVER_CLAIM ||
             opcode == MESHX_UVP_OP_SUBNET_DEMOTE) &&
            payload_len >= (MESHX_CLUSTER_AUTH_OVERHEAD + sizeof(uint32_t)))
        {
            memcpy(&scope_bid, payload + MESHX_CLUSTER_AUTH_HDR_LEN, sizeof(scope_bid));
            meshx_boundary_redundancy_t *b = find_coex_boundary(scope_bid);
            term_ptr = b ? &b->active_term : &scratch_term;
        }
        meshx_sec_verify_rsp_t vrsp;
        memset(&vrsp, 0, sizeof(vrsp));
        meshx_err_t verr = meshx_subnet_sec_verify_frame_scoped(&s_coex.sec, scope_bid, term_ptr,
                                                                payload, payload_len, &vrsp);
        if (verr != MESHX_SUCCESS)
        {
            return verr;
        }
        out->opcode = vrsp.opcode;
        out->sender = vrsp.sender;
        out->term = vrsp.term;
        out->body = vrsp.payload;
        out->body_len = vrsp.payload_len;
        return MESHX_SUCCESS;
    }

    if (src_addr == 0x0000U || src_addr >= 0x8000U || net_idx == MESHX_NET_IDX_INVALID)
    {
        return MESHX_SUBNET_ERR_AUTH;
    }
    uint8_t dummy_slot = 0U;
    if (s_coex.nat.slots.active_count > 0U &&
        meshx_nat_slot_get_by_net_idx(&s_coex.nat, net_idx, &dummy_slot) != MESHX_SUCCESS)
    {
        return MESHX_SUBNET_ERR_AUTH;
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Handle an incoming UVP Takeover Claim frame (0xD2) and emit 0xDB Demote if contested.
 *
 * @param[in] net_idx  Ingress NetKey index.
 * @param[in] rx_term  Authenticated cluster term.
 * @param[in] body     Claim payload bytes.
 * @param[in] body_len Payload length in bytes.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
static meshx_err_t handle_uvp_takeover_claim(uint16_t net_idx, uint32_t rx_term,
                                             const uint8_t *body, uint16_t body_len)
{
    if (body_len < sizeof(meshx_uvp_takeover_payload_t))
    {
        return MESHX_SUCCESS;
    }
    meshx_uvp_takeover_payload_t tk;
    memcpy(&tk, body, sizeof(tk));
    bool send_demote = false;
    uint16_t demote_dst = 0U;
    meshx_bridge_peer_event_t evt = {
        .boundary_id    = tk.boundary_id,
        .rx_term        = rx_term,
        .peer_addr      = tk.candidate_addr,
        .peer_score     = tk.score,
        .now_ms         = s_coex.now_ms,
        .queue_to_purge = &s_coex.router.queue,
    };
    meshx_err_t terr = meshx_subnet_bridge_on_takeover_claim(&s_coex.bridge, &evt, &send_demote, &demote_dst);
    if (terr == MESHX_SUCCESS && send_demote)
    {
        const meshx_boundary_redundancy_t *b = find_coex_boundary(tk.boundary_id);
        meshx_uvp_demote_payload_t dm = {
            .boundary_id = tk.boundary_id,
            .winner_addr = s_coex.nat.local_node_addr,
            .winner_score = b ? b->local_score : 0U,
            .winning_term = b ? b->active_term : rx_term,
        };
        uint32_t saved_term = s_coex.sec.active_term;
        s_coex.sec.active_term = dm.winning_term;
        terr = send_authenticated_uvp(net_idx, demote_dst, MESHX_UVP_OP_SUBNET_DEMOTE, &dm, sizeof(dm), false);
        s_coex.sec.active_term = saved_term;
    }
    return terr;
}

/**
 * @brief Handle a DevKey-sealed UVP Bridge Invite / OTA Rekey frame (0xD4) and emit 0xDA ACK.
 *
 * @param[in] net_idx  Ingress NetKey index.
 * @param[in] sender   Originator unicast address.
 * @param[in] body     Sealed invite payload bytes.
 * @param[in] body_len Payload length in bytes.
 *
 * @return MESHX_SUCCESS on success, or MESHX_SUBNET_ERR_AUTH / error code.
 */
static meshx_err_t handle_uvp_bridge_invite(uint16_t net_idx, uint16_t sender,
                                            const uint8_t *body, uint16_t body_len)
{
    if (!s_coex.has_dev_key || body_len < MESHX_DEVKEY_SEAL_OVERHEAD)
    {
        return MESHX_SUBNET_ERR_AUTH;
    }
    uint8_t plain[64];
    uint16_t plain_len = 0U;
    meshx_err_t oerr = meshx_subnet_sec_open_devkey(&s_coex.local_dev_key, MESHX_UVP_OP_SUBNET_BRIDGE_INVITE,
                                                    body, body_len, plain, sizeof(plain), &plain_len);
    if (oerr != MESHX_SUCCESS || plain_len < sizeof(meshx_uvp_bridge_invite_plain_t))
    {
        coex_secure_zero(plain, sizeof(plain));
        return MESHX_SUBNET_ERR_AUTH;
    }
    meshx_uvp_bridge_invite_plain_t inv;
    memcpy(&inv, plain, sizeof(inv));
    coex_secure_zero(plain, sizeof(plain));

    meshx_err_t kerr = meshx_subnet_coex_cluster_key_update_ex(&inv.cluster_bridge_key, inv.cluster_epoch, 1UL, false);
    if (kerr != MESHX_SUCCESS)
    {
        coex_secure_zero(&inv, sizeof(inv));
        return kerr;
    }
    if (inv.target_net_idx != MESHX_NET_IDX_INVALID)
    {
        meshx_err_t berr = meshx_subnet_coex_bridge_add(inv.target_net_idx, &inv.target_subnet_key,
                                                      inv.target_global_subnet_id, inv.assigned_role);
        if (berr != MESHX_SUCCESS)
        {
            coex_secure_zero(&inv, sizeof(inv));
            return berr;
        }
    }
    meshx_uvp_bridge_invite_ack_plain_t ack = {
        .boundary_id = inv.boundary_id,
        .bridge_addr = s_coex.nat.local_node_addr,
        .status = 0U,
        .committed_epoch = inv.cluster_epoch,
    };
    coex_secure_zero(&inv, sizeof(inv));

    uint8_t ack_sealed[64];
    uint16_t ack_len = 0U;
    meshx_sec_devkey_seal_req_t sreq = {
        .dev_key = &s_coex.local_dev_key,
        .opcode = MESHX_UVP_OP_SUBNET_INVITE_ACK,
        .dst_addr = sender,
        .nonce32 = s_coex.sec.tx_bridge_nonce + 1UL,
        .plain = (const uint8_t *)&ack,
        .plain_len = (uint16_t)sizeof(ack),
    };
    meshx_sec_devkey_seal_rsp_t srsp = {ack_sealed, sizeof(ack_sealed), &ack_len};
    if (meshx_subnet_sec_seal_devkey(&sreq, &srsp) == MESHX_SUCCESS)
    {
        return send_authenticated_uvp(net_idx, sender, MESHX_UVP_OP_SUBNET_INVITE_ACK, ack_sealed, ack_len, true);
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Handle an incoming UVP Route Advertisement frame (0xD6) and update transit whitelist.
 *
 * @param[in] net_idx  Ingress NetKey index.
 * @param[in] sender   Advertising neighbor bridge unicast address.
 * @param[in] body     Route advertisement payload bytes.
 * @param[in] body_len Payload length in bytes.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
static meshx_err_t handle_uvp_route_adv(uint16_t net_idx, uint16_t sender,
                                        const uint8_t *body, uint16_t body_len)
{
    if (body_len < sizeof(meshx_uvp_route_adv_t))
    {
        return MESHX_INVALID_ARG;
    }
    meshx_uvp_route_adv_t adv;
    memcpy(&adv, body, sizeof(adv));
    uint32_t src_gid = MESHX_GLOBAL_SUBNET_INVALID;
    meshx_err_t rerr = meshx_nat_get_global_id_by_net_idx(&s_coex.nat, net_idx, &src_gid);
    if (rerr != MESHX_SUCCESS)
    {
        return rerr;
    }
    rerr = meshx_subnet_bridge_route_update(&s_coex.bridge, &adv, src_gid, s_coex.now_ms);
    if (rerr != MESHX_SUCCESS)
    {
        return rerr;
    }
    for (uint8_t i = 0U; i < adv.entry_count && i < MESHX_ROUTE_ADV_MAX_ENTRIES_PER_MSG; i++)
    {
        if (adv.entries[i].hop_count != 0xFFU)
        {
            rerr = meshx_subnet_sec_whitelist_add(&s_coex.sec, sender, adv.entries[i].dest_subnet_id);
            if(rerr != MESHX_SUCCESS)
            {
                return rerr;
            }
        }
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Handle an incoming UVP Discovery Response (0xDD), rewrite NAT address, and relay.
 *
 * @param[in] net_idx  Ingress NetKey index.
 * @param[in] body     Discovery response payload bytes.
 * @param[in] body_len Payload length in bytes.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
static meshx_err_t handle_uvp_discover_rsp(uint16_t net_idx, const uint8_t *body, uint16_t body_len)
{
    if (body_len < sizeof(meshx_uvp_discover_rsp_t))
    {
        return MESHX_INVALID_ARG;
    }
    meshx_uvp_discover_rsp_t rsp;
    memcpy(&rsp, body, sizeof(rsp));
    uint16_t vnat = 0U;
    meshx_err_t derr = meshx_subnet_disc_handle_rsp(&s_coex.disc, &s_coex.nat, &rsp,
                                                    (uint16_t)(s_coex.now_ms / MESHX_MS_PER_MINUTE),
                                                    s_coex.now_ms, &vnat);
    if (derr != MESHX_SUCCESS)
    {
        return derr;
    }
    rsp.virtual_nat_addr = vnat;
    derr = meshx_subnet_nvs_save_nat_table(s_coex.nat.tier2, MESHX_NAT_TIER2_CAP);
    if (derr == MESHX_SUCCESS)
    {
        derr = emit_nat_sync_if_tier2(vnat);
    }
    if (derr == MESHX_SUCCESS && rsp.global_subnet_dst != MESHX_GLOBAL_SUBNET_INVALID)
    {
        meshx_subnet_crypto_slot_t *dst_slot = NULL;
        if (meshx_subnet_kr_find_by_global_id(&s_coex.kr, rsp.global_subnet_dst, &dst_slot) == MESHX_SUCCESS &&
            dst_slot && dst_slot->net_idx != net_idx)
        {
            derr = send_authenticated_uvp(dst_slot->net_idx, rsp.requestor_addr,
                                          MESHX_UVP_OP_SUBNET_DISCOVER_RSP, &rsp, sizeof(rsp), false);
        }
    }
    return derr;
}

/**
 * @brief Handle an incoming UVP Discovery Request (0xDC), record correlation, and relay.
 *
 * @param[in] net_idx  Ingress NetKey index.
 * @param[in] sender   Requesting node unicast address.
 * @param[in] body     Discovery request payload bytes.
 * @param[in] body_len Payload length in bytes.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
static meshx_err_t handle_uvp_discover_req(uint16_t net_idx, uint16_t sender,
                                           const uint8_t *body, uint16_t body_len)
{
    if (body_len < sizeof(meshx_uvp_discover_req_t))
    {
        return MESHX_INVALID_ARG;
    }
    meshx_uvp_discover_req_t req;
    memcpy(&req, body, sizeof(req));
    uint32_t src_gid = req.global_subnet_src;
    if (src_gid == MESHX_GLOBAL_SUBNET_INVALID &&
        meshx_nat_get_global_id_by_net_idx(&s_coex.nat, net_idx, &src_gid) != MESHX_SUCCESS)
    {
        src_gid = MESHX_GLOBAL_SUBNET_INVALID;
    }
    uint16_t req_addr = (req.requestor_addr != 0U) ? req.requestor_addr : sender;
    meshx_err_t rerr = meshx_subnet_disc_record_req(&s_coex.disc, src_gid, req_addr, req.trans_id, s_coex.now_ms);
    if (rerr != MESHX_SUCCESS)
    {
        return rerr;
    }
    meshx_err_t relay_err = MESHX_SUCCESS;
    for (size_t i = 0; i < MESHX_MAX_BRIDGING_SLOTS; i++)
    {
        if (!s_coex.kr.slot_valid[i] || s_coex.kr.slots[i].net_idx == net_idx)
        {
            continue;
        }
        meshx_err_t terr = send_authenticated_uvp(s_coex.kr.slots[i].net_idx, MESHX_DISCOVERY_PROXY_ADDR,
                                                  MESHX_UVP_OP_SUBNET_DISCOVER_REQ, &req, sizeof(req), false);
        if (terr != MESHX_SUCCESS && relay_err == MESHX_SUCCESS)
        {
            relay_err = terr;
        }
    }
    return relay_err;
}

/**
 * @brief Handle an incoming UVP Subnet Directory Query (0xDE) with optional pagination.
 *
 * @param[in] net_idx  Ingress NetKey index.
 * @param[in] sender   Requesting node unicast address.
 * @param[in] body     Query payload bytes.
 * @param[in] body_len Payload length in bytes.
 *
 * @return MESHX_SUCCESS on success, or UVP TX error code.
 */
static meshx_err_t handle_uvp_dir_query(uint16_t net_idx, uint16_t sender,
                                        const uint8_t *body, uint16_t body_len)
{
    uint8_t start_idx = 0U;
    uint16_t reply_addr = sender;
    if (body_len >= sizeof(meshx_uvp_dir_query_t))
    {
        meshx_uvp_dir_query_t q;
        memcpy(&q, body, sizeof(q));
        start_idx = q.start_idx;
        if (q.requestor_addr != 0U)
        {
            reply_addr = q.requestor_addr;
        }
    }
    meshx_subnet_directory_t dir;
    memset(&dir, 0, sizeof(dir));
    uint8_t active_seen = 0U;
    for (size_t i = 0; i < MESHX_PAIR_RECORD_MAX; i++)
    {
        const meshx_pair_record_t *p = &s_coex.disc.pairs[i];
        if (p->active == 0U)
        {
            continue;
        }
        if (active_seen >= start_idx && dir.entry_count < MESHX_SUBNET_DIRECTORY_MAX_ENTRIES)
        {
            meshx_subnet_directory_entry_t *de = &dir.entries[dir.entry_count++];
            de->global_subnet_id = p->global_subnet_dst;
            de->phys_addr = p->remote_phys_addr;
            de->virtual_addr = p->virtual_remote_addr;
            de->func_id = p->func_id;
            de->role_flags = p->role_flags;
        }
        active_seen++;
    }
    return send_authenticated_uvp(net_idx, reply_addr, MESHX_UVP_OP_QUERY_SUBNET_DIRECTORY, &dir, sizeof(dir), false);
}

/**
 * @brief Handle an incoming Option B UVP Data Tunnel frame (0xDF) for bridge or local delivery.
 *
 * @param[in] net_idx  Ingress NetKey index.
 * @param[in] sender   Immediate transmitter unicast address.
 * @param[in] body     Tunnel header followed by Upper PDU bytes.
 * @param[in] body_len Payload length in bytes.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
static meshx_err_t handle_uvp_data_tunnel(uint16_t net_idx, uint16_t sender,
                                          const uint8_t *body, uint16_t body_len)
{
    if (body_len < sizeof(meshx_uvp_data_tunnel_hdr_t))
    {
        return MESHX_INVALID_ARG;
    }
    meshx_uvp_data_tunnel_hdr_t hdr;
    memcpy(&hdr, body, sizeof(hdr));
    if (hdr.pdu_len == 0U || hdr.pdu_len > MESHX_MAX_UPPER_PDU_LEN ||
        body_len < (uint16_t)(sizeof(hdr) + hdr.pdu_len))
    {
        return MESHX_INVALID_ARG;
    }
    const uint8_t *tun_pdu = body + sizeof(hdr);
    if (s_coex.bridge_enabled && hdr.orig_dst_addr != s_coex.nat.local_node_addr && hdr.ttl > 1U)
    {
        meshx_router_stage1_pdu_t s1;
        memset(&s1, 0, sizeof(s1));
        s1.net_idx = net_idx;
        s1.egress_net_idx = hdr.egress_net_idx;
        s1.src_addr = (hdr.orig_src_addr != 0U) ? hdr.orig_src_addr : sender;
        s1.dst_addr = hdr.orig_dst_addr;
        s1.canonical_src = (hdr.orig_src_addr != 0U) ? hdr.orig_src_addr : sender;
        s1.canonical_dst = hdr.orig_dst_addr;
        s1.iv_index = hdr.orig_iv_index;
        s1.seq_auth = hdr.orig_seq_auth;
        s1.ttl = hdr.ttl;
        s1.szmic = (hdr.trans_mic.len >= MESHX_TRANS_MIC_MAX_LEN) ? 1U : 0U;
        s1.trans_mic = hdr.trans_mic;
        s1.pdu = tun_pdu;
        s1.pdu_len = hdr.pdu_len;
        bool send_ack = false;
        return meshx_subnet_coex_on_pdu_rx(&s1, s_coex.now_ms, &send_ack);
    }
    bool quarantined = false;
    meshx_err_t qerr = meshx_subnet_kr_is_opcode_quarantined(&s_coex.kr, tun_pdu[0], net_idx, hdr.egress_net_idx, &quarantined);
    if (qerr != MESHX_SUCCESS)
    {
        return qerr;
    }
    if (quarantined)
    {
        return MESHX_SUBNET_ERR_AUTH;
    }
    uint32_t src_gid = MESHX_GLOBAL_SUBNET_INVALID;
    meshx_err_t gerr = meshx_nat_get_global_id_by_net_idx(&s_coex.nat, net_idx, &src_gid);
    if (gerr != MESHX_SUCCESS)
    {
        return gerr;
    }
    bool rpl_ok = false;
    meshx_err_t rerr = meshx_subnet_sec_canonical_rpl_check(&s_coex.sec, src_gid, hdr.orig_src_addr,
                                                            hdr.orig_iv_index, hdr.orig_seq_auth, &rpl_ok);
    if (rerr != MESHX_SUCCESS)
    {
        return rerr;
    }
    if (!rpl_ok)
    {
        return MESHX_SUBNET_ERR_REPLAY;
    }
    if (s_coex.cbs.data_rx_cb)
    {
        s_coex.cbs.data_rx_cb(net_idx, hdr.orig_src_addr, hdr.orig_dst_addr,
                              tun_pdu, hdr.pdu_len, s_coex.cbs.data_rx_ctx);
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Verify and dispatch an inbound Unified Vendor Protocol (UVP) subnet frame.
 *
 * Authenticates the frame via `coex_verify_inbound_uvp()` (handling authenticated
 * UVP envelopes, DevKey-sealed invites, and unauthenticated discovery messages)
 * and routes the verified payload to the corresponding subnet coexistence handler
 * (heartbeat, takeover claim, NAT sync, bridge invite/ACK, route advertisement,
 * NACK/NACK-ACK, demote, discovery request/response, epoch sync, directory query,
 * or data tunnel).
 *
 * @param[in] opcode      UVP opcode byte (or `MESHX_UVP_AUTH_WRAPPER_OP` when wrapped).
 * @param[in] src_addr    Source unicast address of the sender.
 * @param[in] dst_addr    Destination address (unused; kept for transport callback symmetry).
 * @param[in] net_idx     NetKey index on which the frame was received.
 * @param[in] payload     Pointer to the received UVP payload bytes.
 * @param[in] payload_len Length of @p payload in bytes (must be >= 1).
 *
 * @return MESHX_SUCCESS on success (or unrecognized opcode), MESHX_INVALID_ARG if
 *         uninitialized or malformed, or an error code from verification or the
 *         dispatched handler.
 */
meshx_err_t meshx_subnet_coex_handle_uvp(uint8_t opcode,
                                         uint16_t src_addr,
                                         uint16_t dst_addr,
                                         uint16_t net_idx,
                                         const uint8_t *payload,
                                         uint16_t payload_len)
{
    MESHX_UNUSED(dst_addr);
    if (!s_coex.initialized || !payload || payload_len < 1U)
    {
        return MESHX_INVALID_ARG;
    }

    coex_uvp_verify_out_t vout = {
        .opcode   = opcode,
        .sender   = src_addr,
        .term     = 0UL,
        .body     = NULL,
        .body_len = 0U,
    };
    meshx_err_t verr = coex_verify_inbound_uvp(net_idx, payload, payload_len, &vout);
    if (verr != MESHX_SUCCESS)
    {
        return verr;
    }
    opcode = vout.opcode;
    uint16_t sender = vout.sender;
    uint32_t rx_term = vout.term;
    const uint8_t *body = vout.body;
    uint16_t body_len = vout.body_len;

    switch (opcode)
    {
        case MESHX_UVP_OP_SUBNET_HEARTBEAT:
            return handle_uvp_heartbeat(net_idx, rx_term, sender, body, body_len);
        case MESHX_UVP_OP_SUBNET_TAKEOVER_CLAIM:
            return handle_uvp_takeover_claim(net_idx, rx_term, body, body_len);
        case MESHX_UVP_OP_SUBNET_NAT_SYNC:
            if (body_len < sizeof(meshx_uvp_nat_sync_payload_t))
            {
                return MESHX_INVALID_ARG;
            }
            {
                meshx_uvp_nat_sync_payload_t sync;
                memcpy(&sync, body, sizeof(sync));
                meshx_err_t nerr = meshx_nat_tier2_apply_sync(&s_coex.nat, &sync.entry, sync.op);
                return (nerr == MESHX_SUCCESS)
                           ? meshx_subnet_nvs_save_nat_table(s_coex.nat.tier2, MESHX_NAT_TIER2_CAP)
                           : nerr;
            }
        case MESHX_UVP_OP_SUBNET_BRIDGE_INVITE:
            return handle_uvp_bridge_invite(net_idx, sender, body, body_len);
        case MESHX_UVP_OP_SUBNET_INVITE_ACK:
            s_coex.invite_acked = true;
            return MESHX_SUCCESS;
        case MESHX_UVP_OP_SUBNET_ROUTE_ADV:
            return handle_uvp_route_adv(net_idx, sender, body, body_len);
        case MESHX_UVP_OP_SUBNET_ROUTER_NACK:
            if (body_len < sizeof(meshx_uvp_router_nack_t))
            {
                return MESHX_INVALID_ARG;
            }
            {
                meshx_uvp_router_nack_t nack;
                memcpy(&nack, body, sizeof(nack));
                meshx_uvp_router_nack_ack_t nack_ack = {nack.nack_id, s_coex.nat.local_node_addr, 0U};
                return send_authenticated_uvp(net_idx, sender, MESHX_UVP_OP_SUBNET_NACK_ACK,
                                              &nack_ack, sizeof(nack_ack), false);
            }
        case MESHX_UVP_OP_SUBNET_NACK_ACK:
            if (body_len < sizeof(meshx_uvp_router_nack_ack_t))
            {
                return MESHX_INVALID_ARG;
            }
            {
                meshx_uvp_router_nack_ack_t ack;
                memcpy(&ack, body, sizeof(ack));
                return meshx_router_on_nack_ack(&s_coex.router, ack.nack_id);
            }
        case MESHX_UVP_OP_SUBNET_DEMOTE:
            if (body_len < sizeof(meshx_uvp_demote_payload_t))
            {
                return MESHX_INVALID_ARG;
            }
            {
                meshx_uvp_demote_payload_t dm;
                memcpy(&dm, body, sizeof(dm));
                return meshx_subnet_bridge_on_demote(&s_coex.bridge, dm.boundary_id, dm.winning_term,
                                                     dm.winner_addr, dm.winner_score, s_coex.now_ms,
                                                     &s_coex.router.queue);
            }
        case MESHX_UVP_OP_SUBNET_DISCOVER_RSP:
            return handle_uvp_discover_rsp(net_idx, body, body_len);
        case MESHX_UVP_OP_SUBNET_EPOCH_SYNC:
            if (body_len < sizeof(meshx_uvp_epoch_sync_t))
            {
                return MESHX_INVALID_ARG;
            }
            {
                meshx_uvp_epoch_sync_t es;
                memcpy(&es, body, sizeof(es));
                if (es.kr_epoch > s_coex.sec.cluster_epoch)
                {
                    s_coex.sec.cluster_epoch = es.kr_epoch;
                }
                return MESHX_SUCCESS;
            }
        case MESHX_UVP_OP_SUBNET_DISCOVER_REQ:
            return handle_uvp_discover_req(net_idx, sender, body, body_len);
        case MESHX_UVP_OP_QUERY_SUBNET_DIRECTORY:
            return handle_uvp_dir_query(net_idx, sender, body, body_len);
        case MESHX_UVP_OP_SUBNET_DATA_TUNNEL:
            return handle_uvp_data_tunnel(net_idx, sender, body, body_len);
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
    bool triggered = false;
    if (meshx_subnet_disc_record_mic_fail(&s_coex.disc, now_ms, &triggered) != MESHX_SUCCESS)
    {
        return false;
    }
    return triggered;
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
    if (meshx_subnet_disc_reset_mic_fails(&s_coex.disc) != MESHX_SUCCESS)
    {
        s_coex.disc.witness_eligible = false;
    }
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
    if (!s_coex.initialized || s_coex.bridge.candidate_active)
    {
        return;
    }
    if (company_id != MESHX_VND_MODEL_CID_MESHX || !payload || payload_len < 4U)
    {
        return;
    }

    /* TASK-04/05: Unrecognized network-ID beacon above the witness RSSI threshold
     * promotes this node to BRIDGE_ROLE_CANDIDATE and reports to the Master. */
    bool eligible = false;
    if (meshx_subnet_disc_eval_candidate(&s_coex.disc, rssi, true, MESHX_WITNESS_DEFAULT_HEAP_BYTES, &eligible) != MESHX_SUCCESS ||
        !eligible)
    {
        return;
    }

    s_coex.disc.witness_eligible = true;
    s_coex.bridge.candidate_active = true;

    uint32_t observed_gid = 0UL;
    memcpy(&observed_gid, payload, sizeof(observed_gid));
    uint32_t observer_gid = 0UL;
    if (meshx_nat_get_global_id_by_net_idx(&s_coex.nat, 0U, &observer_gid) != MESHX_SUCCESS)
    {
        observer_gid = 0UL;
    }

    meshx_uvp_candidate_report_t rpt;
    memset(&rpt, 0, sizeof(rpt));
    rpt.observed_subnet_id = observed_gid;
    rpt.observer_subnet_id = (observer_gid == MESHX_GLOBAL_SUBNET_INVALID) ? 0UL : observer_gid;
    rpt.rssi_dbm = rssi;
    rpt.mains_powered = 1U;
    rpt.free_heap_kb = MESHX_WITNESS_FREE_HEAP_KB;
    uint16_t raw_score = 0U;
    if (meshx_subnet_bridge_compute_score(true, s_coex.bridge.rssi_score,
                                          MESHX_WITNESS_DEFAULT_HEAP_BYTES,
                                          s_coex.nat.local_node_addr, &raw_score) != MESHX_SUCCESS)
    {
        return;
    }
    rpt.candidate_score = (uint8_t)(raw_score >> 8);

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
        meshx_subnet_crypto_slot_t *ks = NULL;
        if (meshx_subnet_kr_find_by_net_idx(&s_coex.kr, c->local_net_idx, &ks) == MESHX_SUCCESS && ks)
        {
            info->kr_phase = ks->kr_phase;
            info->key_epoch = ks->key_epoch;
        }
        const meshx_boundary_redundancy_t *b = find_coex_boundary(c->global_subnet_id);
        info->role = b ? (uint8_t)b->role : (uint8_t)MESHX_BRIDGE_ROLE_IDLE;
        info->boundary_id = b ? b->boundary_id : 0UL;
        info->active_bridge_addr = b ? b->active_bridge_addr : 0U;
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
    uint32_t gid = global_subnet_id;
    if (gid == MESHX_GLOBAL_SUBNET_INVALID)
    {
        meshx_err_t derr = meshx_nat_derive_global_subnet_id(net_key, MESHX_VND_MODEL_CID_MESHX, NULL, 0U, NULL, &gid);
        if (derr != MESHX_SUCCESS)
        {
            return derr;
        }
    }

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
    uint8_t rank = ((meshx_bridge_role_t)role == MESHX_BRIDGE_ROLE_STANDBY) ? 1U : 0U;
    err = meshx_subnet_bridge_add_boundary(&s_coex.bridge,
                                           gid,
                                           (meshx_bridge_role_t)role,
                                           s_coex.nat.local_node_addr,
                                           rank,
                                           s_coex.sec.active_term,
                                           s_coex.now_ms);
    if (err != MESHX_SUCCESS)
    {
        err = meshx_subnet_kr_slot_unbind(&s_coex.kr, net_idx);
        if(err != MESHX_SUCCESS)
        {
            return err;
        }
        err = meshx_nat_slot_release(&s_coex.nat, gid);
        if(err != MESHX_SUCCESS)
        {
            return err;
        }
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Delete Subnet from Local NetWork
 *
 * @param[in] net_idx
 * @param[in] force_purge
 *
 * @return meshx_err_t
 */
meshx_err_t meshx_subnet_coex_bridge_del(uint16_t net_idx, bool force_purge)
{
    if (!s_coex.initialized)
    {
        return MESHX_INVALID_STATE;
    }
    if (net_idx == MESHX_SUBNET_PRIMARY_NET_IDX)
    {
        return MESHX_INVALID_ARG;
    }
    uint32_t gid = MESHX_GLOBAL_SUBNET_INVALID;
    meshx_err_t gerr = meshx_nat_get_global_id_by_net_idx(&s_coex.nat, net_idx, &gid);
    if (gerr != MESHX_SUCCESS || gid == MESHX_GLOBAL_SUBNET_INVALID)
    {
        return (gerr == MESHX_INVALID_ARG) ? gerr : MESHX_NOT_FOUND;
    }
    if (force_purge)
    {
        meshx_err_t perr = meshx_router_queue_purge_boundary(&s_coex.router.queue, gid, NULL);
        if (perr != MESHX_SUCCESS && perr != MESHX_NOT_FOUND)
        {
            return perr;
        }
    }
    meshx_err_t kr_err = meshx_subnet_kr_slot_unbind(&s_coex.kr, net_idx);
    if (kr_err != MESHX_SUCCESS && kr_err != MESHX_NOT_FOUND)
    {
        return kr_err;
    }
    meshx_err_t berr = meshx_subnet_bridge_remove_boundary(&s_coex.bridge, gid);
    if (berr != MESHX_SUCCESS && berr != MESHX_NOT_FOUND)
    {
        return berr;
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
    s_coex.nat.group_forward_bits[0] = group_forward_bitmap;
    return MESHX_SUCCESS;
}

/**
 * @brief Install, update, or revoke the shared Cluster Bridge Key (K_bridge).
 *
 * @param[in] k_bridge          Pointer to 16-byte K_bridge key (unused when @p revoke is true).
 * @param[in] new_cluster_epoch Updated cluster epoch counter.
 * @param[in] new_term          Updated cluster term counter.
 * @param[in] revoke            true to wipe K_bridge from RAM and NVS, false to install/update.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_coex_cluster_key_update_ex(const meshx_subnet_key_t *k_bridge,
                                                    uint32_t new_cluster_epoch,
                                                    uint32_t new_term,
                                                    bool revoke)
{
    if (!s_coex.initialized || (!k_bridge && !revoke))
    {
        return MESHX_INVALID_ARG;
    }

    if (revoke)
    {
        meshx_err_t cerr = meshx_subnet_sec_clear_k_bridge(&s_coex.sec);
        if (cerr != MESHX_SUCCESS)
        {
            return cerr;
        }
        s_coex.kr.has_cluster_bridge_key = false;
        return meshx_subnet_nvs_erase_kbridge();
    }

    /* Persist-before-activate rule (TRD Page 8) */
    meshx_nvs_kbridge_blob_t blob;
    memset(&blob, 0, sizeof(blob));
    blob.k_bridge = *k_bridge;
    blob.cluster_epoch = new_cluster_epoch;
    blob.active_term = new_term;
    blob.valid = 1U;
    meshx_err_t err = meshx_subnet_nvs_save_kbridge(&blob);
    if (err != MESHX_SUCCESS)
    {
        return err;
    }

    coex_secure_zero(&blob, sizeof(blob));
    err = meshx_subnet_sec_set_k_bridge(&s_coex.sec, k_bridge, new_cluster_epoch, new_term);
    if (err == MESHX_SUCCESS)
    {
        s_coex.kr.has_cluster_bridge_key = true;
    }
    return err;
}

/**
 * @brief Install or update the shared Cluster Bridge Key (K_bridge).
 *
 * @param[in] k_bridge          Pointer to 16-byte K_bridge key.
 * @param[in] new_cluster_epoch Updated cluster epoch counter.
 * @param[in] new_term          Updated cluster term counter.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_coex_cluster_key_update(const meshx_subnet_key_t *k_bridge,
                                                 uint32_t new_cluster_epoch,
                                                 uint32_t new_term)
{
    return meshx_subnet_coex_cluster_key_update_ex(k_bridge, new_cluster_epoch, new_term, false);
}

/**
 * @brief Set local immutable Device Key used to decrypt 0xD4 Bridge Invite envelopes.
 *
 * @param[in] dev_key Pointer to 16-byte local Device Key.
 *
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG.
 */
meshx_err_t meshx_subnet_coex_set_dev_key(const meshx_subnet_key_t *dev_key)
{
    if (!s_coex.initialized || !dev_key)
    {
        return MESHX_INVALID_ARG;
    }
    memcpy(s_coex.local_dev_key.bytes, dev_key->bytes, MESHX_SUBNET_KEY_LEN);
    s_coex.has_dev_key = true;
    return MESHX_SUCCESS;
}

/**
 * @brief Commit a cross-subnet pairing record and persist NAT/pair state to NVS.
 *
 * @param[in] rec Pointer to pairing record to commit.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_coex_commit_pair(const meshx_pair_record_t *rec)
{
    if (!s_coex.initialized || !rec)
    {
        return MESHX_INVALID_ARG;
    }
    meshx_err_t err = meshx_subnet_disc_commit_pair(&s_coex.disc, &s_coex.nat, rec);
    if (err != MESHX_SUCCESS)
    {
        return err;
    }
    err = meshx_subnet_nvs_save_pair_records(s_coex.disc.pairs, s_coex.disc.pair_count);
    if (err != MESHX_SUCCESS)
    {
        return err;
    }
    return meshx_subnet_nvs_save_nat_table(s_coex.nat.tier2, MESHX_NAT_TIER2_CAP);
}

/**
 * @brief Send a DevKey-sealed OTA K_bridge rekey (0xD4) to a commissioned peer bridge.
 *
 * @param[in] net_idx           Target NetKey index.
 * @param[in] peer_addr         Unicast address of peer bridge.
 * @param[in] peer_dev_key      Peer bridge 16-byte Device Key.
 * @param[in] new_k_bridge      New 16-byte Cluster Bridge Key.
 * @param[in] new_cluster_epoch Updated cluster epoch counter.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_coex_ota_rekey_peer(uint16_t net_idx,
                                             uint16_t peer_addr,
                                             const meshx_subnet_key_t *peer_dev_key,
                                             const meshx_subnet_key_t *new_k_bridge,
                                             uint32_t new_cluster_epoch)
{
    if (!s_coex.initialized || !peer_dev_key || !new_k_bridge || peer_addr == 0U)
    {
        return MESHX_INVALID_ARG;
    }
    meshx_uvp_bridge_invite_plain_t inv;
    memset(&inv, 0, sizeof(inv));
    inv.cluster_bridge_key = *new_k_bridge;
    inv.cluster_epoch = new_cluster_epoch;
    inv.target_net_idx = MESHX_NET_IDX_INVALID;

    uint16_t sealed_cap = (uint16_t)(sizeof(inv) + MESHX_DEVKEY_SEAL_OVERHEAD);
    uint8_t *sealed = NULL;
    if (MESHX_MALLOC(&sealed, sealed_cap) != MESHX_SUCCESS || !sealed)
    {
        coex_secure_zero(&inv, sizeof(inv));
        return MESHX_NO_MEM;
    }
    uint16_t sealed_len = 0U;
    meshx_sec_devkey_seal_req_t sreq = {
        .dev_key    = peer_dev_key,
        .opcode     = MESHX_UVP_OP_SUBNET_BRIDGE_INVITE,
        .dst_addr   = peer_addr,
        .nonce32    = s_coex.sec.tx_bridge_nonce + 1UL,
        .plain      = (const uint8_t *)&inv,
        .plain_len  = (uint16_t)sizeof(inv),
    };
    meshx_sec_devkey_seal_rsp_t srsp = {sealed, sealed_cap, &sealed_len};
    meshx_err_t err = meshx_subnet_sec_seal_devkey(&sreq, &srsp);
    coex_secure_zero(&inv, sizeof(inv));
    if (err == MESHX_SUCCESS)
    {
        err = send_authenticated_uvp(net_idx, peer_addr, MESHX_UVP_OP_SUBNET_BRIDGE_INVITE,
                                     sealed, sealed_len, true);
    }
    meshx_err_t ferr = MESHX_FREE(&sealed);
    if (err == MESHX_SUCCESS && ferr != MESHX_SUCCESS)
    {
        err = ferr;
    }
    return err;
}

/**
 * @brief Intent-driven REQ-F01.1 probe and bind commissioning for a cross-subnet pair.
 *
 * @param[in]  src_net_idx             Source subnet NetKey index.
 * @param[in]  local_elem_addr         Local element unicast address.
 * @param[in]  remote_global_subnet_id Remote Global Subnet ID.
 * @param[in]  remote_phys_addr        Remote physical element unicast address.
 * @param[in]  func_id                 Bound functional ID.
 * @param[in]  role_flags              Role bitmask (producer/consumer).
 * @param[out] out_virtual_addr        Optional pointer to receive assigned virtual NAT address.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_coex_commission_pair(uint16_t src_net_idx,
                                              uint16_t local_elem_addr,
                                              uint32_t remote_global_subnet_id,
                                              uint16_t remote_phys_addr,
                                              uint16_t func_id,
                                              uint8_t role_flags,
                                              uint16_t *out_virtual_addr)
{
    if (!s_coex.initialized || local_elem_addr == 0U || remote_phys_addr == 0U ||
        remote_global_subnet_id == MESHX_GLOBAL_SUBNET_INVALID)
    {
        return MESHX_INVALID_ARG;
    }

    uint32_t src_gid = MESHX_GLOBAL_SUBNET_INVALID;
    meshx_err_t gerr = meshx_nat_get_global_id_by_net_idx(&s_coex.nat, src_net_idx, &src_gid);
    if (gerr != MESHX_SUCCESS && gerr != MESHX_NOT_FOUND)
    {
        return gerr;
    }

    uint16_t virtual_addr = 0U;
    uint8_t slot_b = 0U;
    if (meshx_nat_slot_get_by_global_id(&s_coex.nat, remote_global_subnet_id, &slot_b) == MESHX_SUCCESS)
    {
        meshx_err_t err = meshx_nat_translate_tier1(slot_b, remote_phys_addr, &virtual_addr);
        if (err != MESHX_SUCCESS)
        {
            return err;
        }
    }
    else
    {
        meshx_nat_tier2_flow_req_t freq = {
            .uuid8              = NULL,
            .flags              = MESHX_NAT_FLAG_VALID,
            .dst_addr           = local_elem_addr,
            .orig_addr          = remote_phys_addr,
            .egress_net_idx     = src_net_idx,
            .global_subnet_src  = remote_global_subnet_id,
            .now_min            = (uint16_t)(s_coex.now_ms / MESHX_MS_PER_MINUTE),
            .global_subnet_dst  = src_gid,
        };
        meshx_err_t err = meshx_nat_tier2_alloc_flow(&s_coex.nat, &freq, &virtual_addr);
        if (err != MESHX_SUCCESS)
        {
            return err;
        }
    }

    meshx_pair_record_t rec;
    memset(&rec, 0, sizeof(rec));
    rec.global_subnet_dst   = remote_global_subnet_id;
    rec.local_elem_addr     = local_elem_addr;
    rec.virtual_remote_addr = virtual_addr;
    rec.remote_phys_addr    = remote_phys_addr;
    rec.func_id             = func_id;
    rec.role_flags          = role_flags;
    rec.active              = 1U;

    meshx_err_t cerr = meshx_subnet_coex_commit_pair(&rec);
    if (cerr != MESHX_SUCCESS)
    {
        return cerr;
    }

    if (out_virtual_addr)
    {
        *out_virtual_addr = virtual_addr;
    }

    if (s_coex.cbs.uvp_tx_cb)
    {
        static uint16_t s_commission_trans_id = 0U;
        s_commission_trans_id++;
        if (s_commission_trans_id == 0U)
        {
            s_commission_trans_id = 1U;
        }
        meshx_uvp_discover_req_t probe;
        memset(&probe, 0, sizeof(probe));
        probe.global_subnet_src     = src_gid;
        probe.global_subnet_dst     = remote_global_subnet_id;
        probe.requestor_addr        = local_elem_addr;
        probe.trans_id              = s_commission_trans_id;
        probe.func_id_filter        = func_id;
        probe.role_filter           = role_flags;
        meshx_err_t rerr = meshx_subnet_disc_record_req(&s_coex.disc,
                                                        probe.global_subnet_src,
                                                        probe.requestor_addr,
                                                        probe.trans_id,
                                                        s_coex.now_ms);
        if (rerr != MESHX_SUCCESS)
        {
            return rerr;
        }
        return send_authenticated_uvp(src_net_idx, MESHX_DISCOVERY_PROXY_ADDR,
                                      MESHX_UVP_OP_SUBNET_DISCOVER_REQ,
                                      &probe, sizeof(probe), false);
    }
    return MESHX_SUCCESS;
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
void meshx_subnet_coex_register_data_tx(meshx_router_send_fn_t tx_cb, void *user_ctx)
{
    MESHX_UNUSED(tx_cb);
    MESHX_UNUSED(user_ctx);
}
/**
 * @brief Ingest a Stage 1 SAR segment into the coexistence router.
 *
 * @param[in]  seg                Incoming segment descriptor.
 * @param[in]  meta               Segment metadata (IV index, SeqAuth, TTL, MIC, timestamp).
 * @param[out] out_send_block_ack Set true if a Lower Transport Block ACK should be sent.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_coex_on_seg_rx(const meshx_router_seg_rx_t *seg, const meshx_subnet_seg_meta_t *meta, bool *out_send_block_ack)
{
    MESHX_UNUSED(seg);
    MESHX_UNUSED(meta);
    if (out_send_block_ack) *out_send_block_ack = false;
    return MESHX_SUCCESS;
}
/**
 * @brief Ingest a reassembled Stage 1 Upper Transport PDU for cross-subnet bridging.
 *
 * @param[in]  pdu                Reassembled Stage 1 PDU descriptor.
 * @param[in]  now_ms             Current monotonic timestamp in milliseconds.
 * @param[out] out_send_block_ack Set true if a Lower Transport Block ACK should be sent.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_coex_on_pdu_rx(const meshx_router_stage1_pdu_t *pdu, uint32_t now_ms, bool *out_send_block_ack)
{
    MESHX_UNUSED(pdu); MESHX_UNUSED(now_ms);
    if (out_send_block_ack) *out_send_block_ack = false;
    return MESHX_SUCCESS;
}
/**
 * @brief Notify coexistence router of Stage 2 Lower SAR completion.
 *
 * @param[in] ack_ok true if egress transmission/ACK succeeded, false otherwise.
 * @param[in] now_ms Current monotonic timestamp in milliseconds.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_coex_on_stage2_tx_complete(bool ack_ok, uint32_t now_ms)
{
    MESHX_UNUSED(ack_ok); MESHX_UNUSED(now_ms);
    return MESHX_SUCCESS;
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
/**
 * @brief Install, update, or revoke the shared Cluster Bridge Key (K_bridge).
 *
 * @param[in] k_bridge          Pointer to 16-byte K_bridge key (unused when @p revoke is true).
 * @param[in] new_cluster_epoch Updated cluster epoch counter.
 * @param[in] new_term          Updated cluster term counter.
 * @param[in] revoke            true to wipe K_bridge from RAM and NVS, false to install/update.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_coex_cluster_key_update_ex(const meshx_subnet_key_t *k_bridge, uint32_t new_cluster_epoch, uint32_t new_term, bool revoke)
{
    MESHX_UNUSED(k_bridge); MESHX_UNUSED(new_cluster_epoch); MESHX_UNUSED(new_term); MESHX_UNUSED(revoke);
    return MESHX_SUCCESS;
}
meshx_err_t meshx_subnet_coex_set_dev_key(const meshx_subnet_key_t *dev_key)
{
    MESHX_UNUSED(dev_key);
    return MESHX_SUCCESS;
}
/**
 * @brief Commit a cross-subnet pairing record and persist NAT/pair state to NVS.
 *
 * @param[in] rec Pointer to pairing record to commit.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_coex_commit_pair(const meshx_pair_record_t *rec)
{
    MESHX_UNUSED(rec);
    return MESHX_SUCCESS;
}
void meshx_subnet_coex_register_data_rx(meshx_subnet_data_rx_fn_t rx_cb, void *user_ctx)
{
    MESHX_UNUSED(rx_cb);
    MESHX_UNUSED(user_ctx);
}
meshx_err_t meshx_subnet_coex_send_data(const meshx_subnet_data_tx_req_t *req)
{
    MESHX_UNUSED(req);
    return MESHX_SUCCESS;
}
/**
 * @brief Send a DevKey-sealed OTA K_bridge rekey (0xD4) to a commissioned peer bridge.
 *
 * @param[in] net_idx           Target NetKey index.
 * @param[in] peer_addr         Unicast address of peer bridge.
 * @param[in] peer_dev_key      Peer bridge 16-byte Device Key.
 * @param[in] new_k_bridge      New 16-byte Cluster Bridge Key.
 * @param[in] new_cluster_epoch Updated cluster epoch counter.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_coex_ota_rekey_peer(uint16_t net_idx, uint16_t peer_addr, const meshx_subnet_key_t *peer_dev_key, const meshx_subnet_key_t *new_k_bridge, uint32_t new_cluster_epoch)
{
    MESHX_UNUSED(net_idx); MESHX_UNUSED(peer_addr);
    MESHX_UNUSED(peer_dev_key); MESHX_UNUSED(new_k_bridge); MESHX_UNUSED(new_cluster_epoch);
    return MESHX_SUCCESS;
}
/**
 * @brief Intent-driven REQ-F01.1 probe and bind commissioning for a cross-subnet pair.
 *
 * @param[in]  src_net_idx             Source subnet NetKey index.
 * @param[in]  local_elem_addr         Local element unicast address.
 * @param[in]  remote_global_subnet_id Remote Global Subnet ID.
 * @param[in]  remote_phys_addr        Remote physical element unicast address.
 * @param[in]  func_id                 Bound functional ID.
 * @param[in]  role_flags              Role bitmask (producer/consumer).
 * @param[out] out_virtual_addr        Optional pointer to receive assigned virtual NAT address.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_coex_commission_pair(uint16_t src_net_idx, uint16_t local_elem_addr, uint32_t remote_global_subnet_id, uint16_t remote_phys_addr, uint16_t func_id, uint8_t role_flags, uint16_t *out_virtual_addr)
{
    MESHX_UNUSED(src_net_idx); MESHX_UNUSED(local_elem_addr);
    MESHX_UNUSED(remote_global_subnet_id); MESHX_UNUSED(remote_phys_addr);
    MESHX_UNUSED(func_id); MESHX_UNUSED(role_flags);
    if (out_virtual_addr)
        *out_virtual_addr = 0U;
    return MESHX_SUCCESS;
}

#endif /* CONFIG_MESHX_ISLAND_SUBNET_COEXISTENCE */
