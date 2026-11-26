/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_subnet_security.c
 * @brief MitM Authentication, HMAC-SHA256 Envelope, Anti-Replay, Whitelist & Cross-Subnet RPL.
 */

#include <stdint.h>
#include <string.h>
#include "island/meshx_subnet_security.h"
#include "interface/crypto/meshx_crypto.h"

/**
 * @brief Securely scrub sensitive key material from RAM.
 *
 * @param[out] buf Pointer to buffer to zero.
 * @param[in]  len Number of bytes to clear.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG if @p buf is NULL.
 */
static meshx_err_t wipe_sec_bytes(uint8_t *buf, size_t len)
{
    if (!buf)
    {
        return MESHX_INVALID_ARG;
    }
    volatile uint8_t *vp = (volatile uint8_t *)buf;
    for (size_t i = 0; i < len; i++)
    {
        vp[i] = 0U;
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Constant-time 16-byte memory comparison to mitigate side-channel timing attacks.
 *
 * @param[in] a First 16-byte buffer.
 * @param[in] b Second 16-byte buffer.
 * @return MESHX_SUCCESS if buffers are identical, MESHX_SUBNET_ERR_AUTH otherwise.
 */
static meshx_err_t ct_memcmp16(const uint8_t *a, const uint8_t *b)
{
    if (!a || !b)
    {
        return MESHX_INVALID_ARG;
    }
    uint8_t diff = 0U;
    for (size_t i = 0; i < MESHX_HMAC_TAG_LEN; i++)
    {
        diff |= (uint8_t)(a[i] ^ b[i]);
    }
    return (diff == 0U) ? MESHX_SUCCESS : MESHX_SUBNET_ERR_AUTH;
}

/**
 * @brief Format 13-byte Device Key AES-CCM nonce for commissioning envelopes.
 *
 * @param[in]  opcode    UVP message opcode.
 * @param[in]  dst_addr  Destination unicast address.
 * @param[in]  nonce32   32-bit transaction nonce.
 * @param[out] out_nonce Destination 13-byte nonce structure.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG if out_nonce is NULL.
 */
static meshx_err_t build_devkey_ccm_nonce(uint8_t opcode, uint16_t dst_addr, uint32_t nonce32, meshx_ccm_nonce_t *out_nonce)
{
    if (!out_nonce)
    {
        return MESHX_INVALID_ARG;
    }
    memset(out_nonce, 0, sizeof(*out_nonce));
    out_nonce->bytes[0] = MESHX_NONCE_TYPE_DEVICE;
    out_nonce->bytes[1] = opcode;
    out_nonce->bytes[2] = MESHX_BE_BYTE(nonce32, MESHX_SHIFT_MSB32);
    out_nonce->bytes[3] = MESHX_BE_BYTE(nonce32, MESHX_SHIFT_BYTE2);
    out_nonce->bytes[4] = MESHX_BE_BYTE(nonce32, MESHX_SHIFT_BYTE1);
    out_nonce->bytes[5] = MESHX_BE_BYTE(nonce32, MESHX_SHIFT_LSB);
    out_nonce->bytes[6] = MESHX_BE_BYTE(dst_addr, MESHX_SHIFT_BYTE1);
    out_nonce->bytes[7] = MESHX_BE_BYTE(dst_addr, MESHX_SHIFT_LSB);
    return MESHX_SUCCESS;
}

/**
 * @brief Initialize security subsystem with persisted nonce checkpoint.
 *
 * @param[out] ctx                  Pointer to security context.
 * @param[in]  persisted_nonce_ckpt Nonce checkpoint loaded from NVS.
 * @param[in]  ckpt_cb              Checkpoint commit callback.
 * @param[in]  ckpt_ctx             Callback context pointer.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG if ctx is NULL.
 */
meshx_err_t meshx_subnet_sec_init(meshx_subnet_security_ctx_t *ctx,
                                  uint32_t persisted_nonce_ckpt,
                                  meshx_sec_ckpt_save_fn_t ckpt_cb,
                                  void *ckpt_ctx)
{
    if (!ctx)
    {
        return MESHX_INVALID_ARG;
    }
    memset(ctx, 0, sizeof(*ctx));
    ctx->ckpt_cb = ckpt_cb;
    ctx->ckpt_ctx = ckpt_ctx;
    ctx->last_saved_nonce_ckpt = persisted_nonce_ckpt;
    if (persisted_nonce_ckpt <= (UINT32_MAX - MESHX_BRIDGE_NONCE_BOOT_MARGIN))
    {
        ctx->tx_bridge_nonce = persisted_nonce_ckpt + MESHX_BRIDGE_NONCE_BOOT_MARGIN;
    }
    else
    {
        ctx->tx_bridge_nonce = MESHX_BRIDGE_NONCE_WRAP_LIMIT;
    }
    return MESHX_SUCCESS;
}

/**
 * @brief De-initialize security context and scrub key material.
 *
 * @param[in,out] ctx Pointer to security context.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG if @p ctx is NULL.
 */
meshx_err_t meshx_subnet_sec_deinit(meshx_subnet_security_ctx_t *ctx)
{
    if (!ctx)
    {
        return MESHX_INVALID_ARG;
    }
    meshx_err_t err = wipe_sec_bytes(ctx->k_bridge.bytes, sizeof(ctx->k_bridge.bytes));
    memset(ctx, 0, sizeof(*ctx));
    return err;
}

/**
 * @brief Provision K_bridge atomically (callers must commit NVS before activating).
 *
 * @param[in,out] ctx           Pointer to security context.
 * @param[in]     k_bridge      Pointer to 16-byte Cluster Bridge Key.
 * @param[in]     cluster_epoch Cluster epoch counter.
 * @param[in]     term          Failover term.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG.
 */
meshx_err_t meshx_subnet_sec_set_k_bridge(meshx_subnet_security_ctx_t *ctx,
                                          const meshx_subnet_key_t *k_bridge,
                                          uint32_t cluster_epoch,
                                          uint32_t term)
{
    if (!ctx || !k_bridge)
    {
        return MESHX_INVALID_ARG;
    }
    memcpy(ctx->k_bridge.bytes, k_bridge->bytes, MESHX_SUBNET_KEY_LEN);
    ctx->k_bridge_valid = true;
    ctx->cluster_epoch = cluster_epoch;
    if (term > ctx->active_term)
    {
        ctx->active_term = term;
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Wipe Cluster Bridge Key from memory.
 *
 * @param[in,out] ctx Pointer to security context.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG if @p ctx is NULL.
 */
meshx_err_t meshx_subnet_sec_clear_k_bridge(meshx_subnet_security_ctx_t *ctx)
{
    if (!ctx)
    {
        return MESHX_INVALID_ARG;
    }
    meshx_err_t err = wipe_sec_bytes(ctx->k_bridge.bytes, sizeof(ctx->k_bridge.bytes));
    ctx->k_bridge_valid = false;
    return err;
}

/**
 * @brief Construct & authenticate outbound UVP frame with 11B header + 16B HMAC tag.
 *
 * @param[in,out] ctx         Pointer to security context.
 * @param[in]     opcode      UVP message opcode (0xD1..0xDE).
 * @param[in]     sender_addr Sender physical address.
 * @param[in]     payload     Payload to sign.
 * @param[in]     payload_len Length of payload in bytes.
 * @param[out]    out_frame   Destination buffer for signed frame.
 * @param[in]     out_cap     Capacity of out_frame buffer.
 * @param[out]    out_len     Total length of signed frame.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_sec_sign_frame(meshx_subnet_security_ctx_t *ctx,
                                        const meshx_sec_sign_req_t *req,
                                        meshx_sec_sign_rsp_t *rsp)
{
    if (!ctx || !req || !rsp || !rsp->out_frame || !rsp->out_len || (req->payload_len > 0U && !req->payload))
    {
        return MESHX_INVALID_ARG;
    }
    uint8_t opcode = req->opcode;
    uint16_t sender_addr = req->sender_addr;
    const uint8_t *payload = req->payload;
    uint16_t payload_len = req->payload_len;
    uint8_t *out_frame = rsp->out_frame;
    uint16_t out_cap = rsp->out_cap;
    uint16_t *out_len = rsp->out_len;
    if (!ctx->k_bridge_valid)
    {
        return MESHX_SUBNET_ERR_AUTH;
    }
    if (ctx->tx_bridge_nonce >= MESHX_BRIDGE_NONCE_WRAP_LIMIT)
    {
        return MESHX_SUBNET_ERR_REPLAY;
    }
    uint16_t needed = (uint16_t)(MESHX_CLUSTER_AUTH_OVERHEAD + payload_len);
    if (out_cap < needed)
    {
        return MESHX_NO_MEM;
    }

    ctx->tx_bridge_nonce++;
    meshx_uvp_auth_hdr_t hdr;
    hdr.opcode = opcode;
    hdr.term = ctx->active_term;
    hdr.sender_addr = sender_addr;
    hdr.bridge_nonce = ctx->tx_bridge_nonce;

    memcpy(out_frame, &hdr, sizeof(hdr));
    if (payload_len > 0U)
    {
        memcpy(out_frame + sizeof(hdr), payload, payload_len);
    }

    uint16_t mac_in_len = (uint16_t)(sizeof(hdr) + payload_len);
    uint8_t hmac_full[MESHX_SEC_ECDH_SECRET_LEN];
    meshx_err_t err = meshx_crypto_hmac_sha256(ctx->k_bridge.bytes,
                                               sizeof(ctx->k_bridge.bytes),
                                               out_frame,
                                               mac_in_len,
                                               hmac_full);
    if (err != MESHX_SUCCESS)
    {
        return err;
    }
    memcpy(out_frame + mac_in_len, hmac_full, MESHX_HMAC_TAG_LEN);
    wipe_sec_bytes(hmac_full, sizeof(hmac_full));

    if ((ctx->tx_bridge_nonce - ctx->last_saved_nonce_ckpt) >= MESHX_BRIDGE_NONCE_CHKPT_STEP)
    {
        if (ctx->ckpt_cb)
        {
            err = ctx->ckpt_cb(ctx->tx_bridge_nonce, ctx->ckpt_ctx);
            if (err != MESHX_SUCCESS)
            {
                return err;
            }
        }
        ctx->last_saved_nonce_ckpt = ctx->tx_bridge_nonce;
    }

    *out_len = needed;
    return MESHX_SUCCESS;
}

/**
 * @brief Sliding anti-replay verification helper.
 *
 * @param[in,out] ctx         Pointer to security context.
 * @param[in]     sender_addr Sender physical address.
 * @param[in]     nonce       Inbound bridge nonce.
 * @return MESHX_SUCCESS if nonce is accepted and recorded, MESHX_SUBNET_ERR_REPLAY if rejected as replay.
 */
static meshx_err_t replay_check_and_accept(meshx_subnet_security_ctx_t *ctx,
                                           uint32_t boundary_id,
                                           uint16_t sender_addr,
                                           uint32_t nonce)
{
    if (nonce == 0UL || nonce <= ctx->eviction_floor_nonce)
    {
        return MESHX_SUBNET_ERR_REPLAY;
    }

    int match_idx = -1;
    int free_idx = -1;
    int oldest_idx = 0;

    for (int i = 0; i < (int)MESHX_ANTI_REPLAY_CACHE_SIZE; i++)
    {
        const meshx_replay_sender_t *s = &ctx->replay_cache[i];
        if (s->valid)
        {
            if (s->sender_addr == sender_addr && s->boundary_id == boundary_id)
            {
                match_idx = i;
                break;
            }
            if (s->highest_nonce < ctx->replay_cache[oldest_idx].highest_nonce)
            {
                oldest_idx = i;
            }
        }
        else if (free_idx < 0)
        {
            free_idx = i;
        }
    }

    if (match_idx >= 0)
    {
        meshx_replay_sender_t *slot = &ctx->replay_cache[match_idx];
        if (nonce > slot->highest_nonce)
        {
            uint32_t delta = nonce - slot->highest_nonce;
            slot->seen_bitmap = (delta >= 32UL) ? 1UL : ((slot->seen_bitmap << delta) | 1UL);
            slot->highest_nonce = nonce;
            return MESHX_SUCCESS;
        }
        uint32_t diff = slot->highest_nonce - nonce;
        if (diff >= 32UL || (slot->seen_bitmap & (1UL << diff)) != 0UL)
        {
            return MESHX_SUBNET_ERR_REPLAY;
        }
        slot->seen_bitmap |= (1UL << diff);
        return MESHX_SUCCESS;
    }

    int dst_idx = free_idx;
    if (dst_idx < 0)
    {
        dst_idx = oldest_idx;
        if (ctx->replay_cache[dst_idx].highest_nonce > ctx->eviction_floor_nonce)
        {
            ctx->eviction_floor_nonce = ctx->replay_cache[dst_idx].highest_nonce;
        }
    }

    ctx->replay_cache[dst_idx].seen_bitmap   = 1UL;
    ctx->replay_cache[dst_idx].valid         = true;
    ctx->replay_cache[dst_idx].boundary_id   = boundary_id;
    ctx->replay_cache[dst_idx].highest_nonce = nonce;
    ctx->replay_cache[dst_idx].sender_addr   = sender_addr;
    return MESHX_SUCCESS;
}

/**
 * @brief Verify an authenticated UVP frame with boundary-scoped term tracking.
 *
 * @param[in,out] ctx           Pointer to security context.
 * @param[in]     boundary_id   Boundary Global Subnet ID (0 for global scope).
 * @param[in,out] boundary_term Optional pointer to per-boundary active term.
 * @param[in]     frame         Raw incoming UVP frame buffer.
 * @param[in]     frame_len     Length of incoming frame in bytes.
 * @param[out]    rsp           Parsed verification output descriptor.
 *
 * @return MESHX_SUCCESS on success, or MESHX_SUBNET_ERR_AUTH / MESHX_SUBNET_ERR_REPLAY.
 */
meshx_err_t meshx_subnet_sec_verify_frame_scoped(meshx_subnet_security_ctx_t *ctx,
                                                 uint32_t boundary_id,
                                                 uint32_t *boundary_term,
                                                 const uint8_t *frame,
                                                 uint16_t frame_len,
                                                 meshx_sec_verify_rsp_t *rsp)
{
    if (!ctx || !frame || !rsp)
    {
        return MESHX_INVALID_ARG;
    }
    uint8_t *out_opcode = &rsp->opcode;
    uint32_t *out_term = &rsp->term;
    uint16_t *out_sender = &rsp->sender;
    const uint8_t **out_payload = &rsp->payload;
    uint16_t *out_payload_len = &rsp->payload_len;
    if (!ctx->k_bridge_valid || frame_len < MESHX_CLUSTER_AUTH_OVERHEAD)
    {
        return MESHX_SUBNET_ERR_AUTH;
    }

    uint32_t active_term = boundary_term ? *boundary_term : ctx->active_term;
    meshx_uvp_auth_hdr_t hdr;
    memcpy(&hdr, frame, sizeof(hdr));
    if (hdr.term < active_term)
    {
        return MESHX_SUBNET_ERR_AUTH;
    }

    uint16_t mac_in_len = (uint16_t)(frame_len - MESHX_HMAC_TAG_LEN);
    uint8_t hmac_full[MESHX_SEC_ECDH_SECRET_LEN];
    meshx_err_t err = meshx_crypto_hmac_sha256(ctx->k_bridge.bytes,
                                               sizeof(ctx->k_bridge.bytes),
                                               frame,
                                               mac_in_len,
                                               hmac_full);
    if (err != MESHX_SUCCESS)
    {
        return err;
    }
    bool tag_ok = (ct_memcmp16(hmac_full, frame + mac_in_len) == MESHX_SUCCESS);
    wipe_sec_bytes(hmac_full, sizeof(hmac_full));
    if (!tag_ok)
    {
        return MESHX_SUBNET_ERR_AUTH;
    }

    if (replay_check_and_accept(ctx, boundary_id, hdr.sender_addr, hdr.bridge_nonce) != MESHX_SUCCESS)
    {
        return MESHX_SUBNET_ERR_REPLAY;
    }

    if (hdr.term > active_term)
    {
        if (boundary_term)
        {
            *boundary_term = hdr.term;
        }
        else
        {
            ctx->active_term = hdr.term;
        }
    }

    *out_term        = hdr.term;
    *out_opcode      = hdr.opcode;
    *out_sender      = hdr.sender_addr;
    *out_payload     = frame + sizeof(hdr);
    *out_payload_len = (uint16_t)(frame_len - MESHX_CLUSTER_AUTH_OVERHEAD);
    return MESHX_SUCCESS;
}

/**
 * @brief Verify an authenticated UVP frame using the global cluster term.
 *
 * @param[in,out] ctx       Pointer to security context.
 * @param[in]     frame     Raw incoming UVP frame buffer.
 * @param[in]     frame_len Length of incoming frame in bytes.
 * @param[out]    rsp       Parsed verification output descriptor.
 *
 * @return MESHX_SUCCESS on success, or MESHX_SUBNET_ERR_AUTH / MESHX_SUBNET_ERR_REPLAY.
 */
meshx_err_t meshx_subnet_sec_verify_frame(meshx_subnet_security_ctx_t *ctx,
                                          const uint8_t *frame,
                                          uint16_t frame_len,
                                          meshx_sec_verify_rsp_t *rsp)
{
    return meshx_subnet_sec_verify_frame_scoped(ctx, 0UL, NULL, frame, frame_len, rsp);
}

/**
 * @brief Encrypt DevKey AES-128-CCM commissioning envelopes (0xD4 Bridge Invite).
 *
 * @param[in]  dev_key    Pointer to 16-byte Device Key.
 * @param[in]  opcode     UVP opcode being sealed.
 * @param[in]  dst_addr   Target destination address.
 * @param[in]  nonce32    32-bit transaction nonce.
 * @param[in]  plain      Plaintext payload.
 * @param[in]  plain_len  Plaintext payload length.
 * @param[out] out_buf    Destination buffer for sealed envelope.
 * @param[in]  out_cap    Capacity of destination buffer.
 * @param[out] out_len    Length of generated sealed envelope.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_sec_seal_devkey(const meshx_sec_devkey_seal_req_t *req,
                                         meshx_sec_devkey_seal_rsp_t *rsp)
{
    if (!req || !rsp || !req->dev_key || !rsp->out_buf || !rsp->out_len ||
        (req->plain_len > 0U && !req->plain))
    {
        return MESHX_INVALID_ARG;
    }
    const meshx_subnet_key_t *dev_key = req->dev_key;
    uint8_t opcode = req->opcode;
    uint16_t dst_addr = req->dst_addr;
    uint32_t nonce32 = req->nonce32;
    const uint8_t *plain = req->plain;
    uint16_t plain_len = req->plain_len;
    uint8_t *out_buf = rsp->out_buf;
    uint16_t out_cap = rsp->out_cap;
    uint16_t *out_len = rsp->out_len;
    uint16_t needed = (uint16_t)(MESHX_CCM_NONCE_LEN + plain_len + MESHX_DEVKEY_CCM_MIC_LEN);
    if (out_cap < needed)
    {
        return MESHX_NO_MEM;
    }

    meshx_ccm_nonce_t nonce13;
    meshx_err_t err = build_devkey_ccm_nonce(opcode, dst_addr, nonce32, &nonce13);
    if (err != MESHX_SUCCESS)
    {
        return err;
    }
    memcpy(out_buf, nonce13.bytes, MESHX_CCM_NONCE_LEN);

    err = meshx_crypto_aes_ccm_encrypt(dev_key->bytes,
                                                   nonce13.bytes,
                                                   MESHX_CCM_NONCE_LEN,
                                                   plain,
                                                   plain_len,
                                                   out_buf + MESHX_CCM_NONCE_LEN,
                                                   out_buf + MESHX_CCM_NONCE_LEN + plain_len,
                                                   MESHX_DEVKEY_CCM_MIC_LEN);
    if (err != MESHX_SUCCESS)
    {
        return err;
    }
    *out_len = needed;
    return MESHX_SUCCESS;
}

/**
 * @brief Decrypt DevKey AES-128-CCM commissioning envelopes (0xD4 Bridge Invite / 0xDA Invite ACK).
 *
 * @param[in]  dev_key       Pointer to 16-byte Device Key.
 * @param[in]  opcode        Expected UVP opcode.
 * @param[in]  sealed        Sealed input buffer.
 * @param[in]  sealed_len    Length of sealed input buffer.
 * @param[out] out_plain     Destination buffer for decrypted plaintext.
 * @param[in]  out_cap       Capacity of out_plain buffer.
 * @param[out] out_plain_len Destination for length of decrypted plaintext.
 * @return MESHX_SUCCESS on success, MESHX_SUBNET_ERR_AUTH on authentication failure.
 */
meshx_err_t meshx_subnet_sec_open_devkey(const meshx_subnet_key_t *dev_key,
                                         uint8_t opcode,
                                         const uint8_t *sealed,
                                         uint16_t sealed_len,
                                         uint8_t *out_plain,
                                         uint16_t out_cap,
                                         uint16_t *out_plain_len)
{
    if (!dev_key || !sealed || !out_plain || !out_plain_len || sealed_len < MESHX_DEVKEY_SEAL_OVERHEAD)
    {
        return MESHX_INVALID_ARG;
    }
    uint16_t cipher_len = (uint16_t)(sealed_len - MESHX_DEVKEY_SEAL_OVERHEAD);
    if (out_cap < cipher_len || sealed[0] != MESHX_NONCE_TYPE_DEVICE || sealed[1] != opcode)
    {
        return MESHX_SUBNET_ERR_AUTH;
    }

    meshx_err_t err = meshx_crypto_aes_ccm_decrypt(dev_key->bytes,
                                                   sealed,
                                                   MESHX_CCM_NONCE_LEN,
                                                   sealed + MESHX_CCM_NONCE_LEN,
                                                   cipher_len,
                                                   sealed + MESHX_CCM_NONCE_LEN + cipher_len,
                                                   MESHX_DEVKEY_CCM_MIC_LEN,
                                                   out_plain);
    if (err != MESHX_SUCCESS)
    {
        return MESHX_SUBNET_ERR_AUTH;
    }
    *out_plain_len = cipher_len;
    return MESHX_SUCCESS;
}

/**
 * @brief Add rule to Multihop Transit Ingress Whitelist.
 *
 * @param[in,out] ctx                  Pointer to security context.
 * @param[in]     neighbor_bridge_addr Physical address of authorized neighbor bridge.
 * @param[in]     allowed_subnet_src   Authorized origin Global Subnet ID.
 * @return MESHX_SUCCESS on success, or MESHX_NO_MEM.
 */
meshx_err_t meshx_subnet_sec_whitelist_add(meshx_subnet_security_ctx_t *ctx,
                                           uint16_t neighbor_bridge_addr,
                                           uint32_t allowed_subnet_src)
{
    if (!ctx || neighbor_bridge_addr == 0U || allowed_subnet_src == MESHX_GLOBAL_SUBNET_INVALID)
    {
        return MESHX_INVALID_ARG;
    }
    int free_idx = -1;
    for (int i = 0; i < (int)MESHX_TRANSIT_WHITELIST_MAX; i++)
    {
        const meshx_transit_whitelist_entry_t *w = &ctx->whitelist[i];
        if (w->valid)
        {
            if (w->neighbor_bridge_addr == neighbor_bridge_addr &&
                w->allowed_global_subnet_src == allowed_subnet_src)
            {
                return MESHX_SUCCESS;
            }
        }
        else if (free_idx < 0)
        {
            free_idx = i;
        }
    }
    if (free_idx < 0)
    {
        return MESHX_NO_MEM;
    }
    ctx->whitelist[free_idx].neighbor_bridge_addr = neighbor_bridge_addr;
    ctx->whitelist[free_idx].allowed_global_subnet_src = allowed_subnet_src;
    ctx->whitelist[free_idx].valid = true;
    return MESHX_SUCCESS;
}

/**
 * @brief Check whether transit ingress packet from neighbor bridge is authorized for claimed subnet.
 *
 * @param[in]  ctx                  Pointer to security context.
 * @param[in]  neighbor_bridge_addr Immediate transmitter address.
 * @param[in]  claimed_subnet_src   Origin Global Subnet ID claimed in transit header.
 * @param[out] out_allowed          Set to true if authorized, false otherwise.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG if @p ctx or @p out_allowed is NULL.
 */
meshx_err_t meshx_subnet_sec_whitelist_check(const meshx_subnet_security_ctx_t *ctx,
                                             uint16_t neighbor_bridge_addr,
                                             uint32_t claimed_subnet_src,
                                             bool *out_allowed)
{
    if (!ctx || !out_allowed)
    {
        return MESHX_INVALID_ARG;
    }
    *out_allowed = false;
    for (size_t i = 0; i < MESHX_TRANSIT_WHITELIST_MAX; i++)
    {
        const meshx_transit_whitelist_entry_t *w = &ctx->whitelist[i];
        if (w->valid &&
            w->neighbor_bridge_addr == neighbor_bridge_addr &&
            w->allowed_global_subnet_src == claimed_subnet_src)
        {
            *out_allowed = true;
            return MESHX_SUCCESS;
        }
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Evaluate and update a matched Replay Protection List entry's 32-bit sliding window.
 *
 * @param[in,out] e         Matched RPL entry.
 * @param[in]     iv_index  Current IV Index.
 * @param[in]     seq_num   Received packet sequence number.
 * @param[out]    out_valid Set to true if fresh and recorded, false if replayed.
 * @return MESHX_SUCCESS on success.
 */
static meshx_err_t rpl_update_matched_entry(meshx_subnet_rpl_entry_t *e,
                                            uint32_t iv_index,
                                            uint32_t seq_num,
                                            bool *out_valid)
{
    if (iv_index < e->iv_index)
    {
        return MESHX_SUCCESS;
    }
    if (iv_index > e->iv_index)
    {
        e->iv_index = iv_index;
        e->last_seq_num = seq_num;
        e->seq_window = 1UL;
        *out_valid = true;
        return MESHX_SUCCESS;
    }
    if (seq_num > e->last_seq_num)
    {
        uint32_t delta = seq_num - e->last_seq_num;
        e->seq_window = (delta >= 32UL) ? 1UL : ((e->seq_window << delta) | 1UL);
        e->last_seq_num = seq_num;
        *out_valid = true;
        return MESHX_SUCCESS;
    }
    uint32_t diff = e->last_seq_num - seq_num;
    if (diff >= 32UL || (e->seq_window & (1UL << diff)) != 0UL)
    {
        return MESHX_SUCCESS;
    }
    e->seq_window |= (1UL << diff);
    *out_valid = true;
    return MESHX_SUCCESS;
}

/**
 * @brief Check and update subnet-partitioned Replay Protection List.
 *
 * @param[in,out] ctx       Pointer to security context.
 * @param[in]     net_idx   BLE Mesh NetKey Index.
 * @param[in]     src_addr  Source unicast address.
 * @param[in]     key_epoch Subnet key rotation epoch.
 * @param[in]     iv_index  Current IV Index.
 * @param[in]     seq_num   Received packet sequence number.
 * @param[out]    out_valid Set to true if packet is fresh and recorded, false if replayed.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG if @p ctx or @p out_valid is NULL or @p src_addr is 0.
 */
meshx_err_t meshx_subnet_sec_rpl_check_update(meshx_subnet_security_ctx_t *ctx,
                                              uint16_t net_idx,
                                              uint16_t src_addr,
                                              uint32_t key_epoch,
                                              uint32_t iv_index,
                                              uint32_t seq_num,
                                              bool *out_valid)
{
    if (!ctx || !out_valid || src_addr == 0U)
    {
        return MESHX_INVALID_ARG;
    }
    *out_valid = false;
    int free_idx = -1;
    int lru_idx = 0;

    for (int i = 0; i < (int)MESHX_RPL_CACHE_SIZE; i++)
    {
        meshx_subnet_rpl_entry_t *e = &ctx->rpl[i];
        if (e->src_addr == 0U)
        {
            if (free_idx < 0)
            {
                free_idx = i;
            }
            continue;
        }
        if (e->net_idx == net_idx && e->src_addr == src_addr && e->key_epoch == key_epoch)
        {
            return rpl_update_matched_entry(e, iv_index, seq_num, out_valid);
        }
        if (e->iv_index < ctx->rpl[lru_idx].iv_index ||
            (e->iv_index == ctx->rpl[lru_idx].iv_index &&
             e->last_seq_num < ctx->rpl[lru_idx].last_seq_num))
        {
            lru_idx = i;
        }
    }

    int idx = (free_idx >= 0) ? free_idx : lru_idx;
    ctx->rpl[idx].net_idx       = net_idx;
    ctx->rpl[idx].last_seq_num  = seq_num;
    ctx->rpl[idx].seq_window    = 1UL;
    ctx->rpl[idx].src_addr      = src_addr;
    ctx->rpl[idx].iv_index      = iv_index;
    ctx->rpl[idx].key_epoch     = key_epoch;
    *out_valid = true;
    return MESHX_SUCCESS;
}

/**
 * @brief Check and update Stage 2 Canonical Source RPL (canonical_src, global_subnet_src).
 *
 * @param[in,out] ctx               Pointer to security context.
 * @param[in]     global_subnet_src Origin island Global Subnet ID.
 * @param[in]     canonical_src     Canonical source unicast address.
 * @param[in]     iv_index          Current IV Index.
 * @param[in]     seq_num           Sequence number.
 * @param[out]    out_valid         Set to true if fresh and recorded, false if replayed.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG if @p ctx or @p out_valid is NULL or @p canonical_src is 0.
 */
meshx_err_t meshx_subnet_sec_canonical_rpl_check(meshx_subnet_security_ctx_t *ctx,
                                                 uint32_t global_subnet_src,
                                                 uint16_t canonical_src,
                                                 uint32_t iv_index,
                                                 uint32_t seq_num,
                                                 bool *out_valid)
{
    if (!ctx || !out_valid || canonical_src == 0U)
    {
        return MESHX_INVALID_ARG;
    }
    *out_valid = false;
    int free_idx = -1;
    int lru_idx = 0;

    for (int i = 0; i < (int)MESHX_NAT_TIER2_CAP; i++)
    {
        meshx_canonical_rpl_entry_t *e = &ctx->canonical_rpl[i];
        if (!e->valid)
        {
            if (free_idx < 0)
            {
                free_idx = i;
            }
            continue;
        }
        if (e->global_subnet_src == global_subnet_src && e->canonical_src == canonical_src)
        {
            if (iv_index < e->iv_index ||
                (iv_index == e->iv_index && seq_num <= e->last_seq))
            {
                return MESHX_SUCCESS;
            }
            e->iv_index = iv_index;
            e->last_seq = seq_num;
            *out_valid = true;
            return MESHX_SUCCESS;
        }
        if (e->last_seq < ctx->canonical_rpl[lru_idx].last_seq)
        {
            lru_idx = i;
        }
    }

    int idx = (free_idx >= 0) ? free_idx : lru_idx;
    ctx->canonical_rpl[idx].valid               = 1U;
    ctx->canonical_rpl[idx].last_seq            = seq_num;
    ctx->canonical_rpl[idx].iv_index            = iv_index;
    ctx->canonical_rpl[idx].canonical_src       = canonical_src;
    ctx->canonical_rpl[idx].global_subnet_src   = global_subnet_src;
    *out_valid = true;
    return MESHX_SUCCESS;
}
