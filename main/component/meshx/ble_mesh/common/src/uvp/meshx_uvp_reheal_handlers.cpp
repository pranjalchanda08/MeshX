/**
 * @file meshx_uvp_reheal_handlers.cpp
 * @brief UVP Self-Healing and Courier Forwarding Opcode Handlers (0xCE..0xCF).
 *
 * Implements 15-second blindness detection, periodic raw BLE 0xCE beaconing,
 * zero-trust relay courier forwarding with rate limiting, and 0xCF re-admission.
 *
 * @author Pranjal Chanda
 * @date 2024-2026
 */

#include "meshx_uvp_registry.hpp"
#include <meshx_composition.hpp>
#include <meshx_c_header.h>

/* =========================================================================
 * Re-Healing State Variables & Definitions
 * ========================================================================= */

/**
 * @brief Maximum entries in the relay-side re-healing courier rate-limit table.
 */
#define REHEAL_COURIER_MAX_ENTRIES 8

/**
 * @brief State machine states for evicted node re-healing workflow.
 */
typedef enum {
    REHEAL_STATE_PROVISIONED  = 0, /**< Node is provisioned and operational */
    REHEAL_STATE_ISOLATED     = 1, /**< Blindness detected, awaiting healing beacon */
    REHEAL_STATE_HEALING      = 2, /**< Beaconing, waiting for 0xCF admit response */
} reheal_state_t;

/**
 * @brief Entry in the relay-side courier rate-limiting table.
 */
typedef struct {
    meshx_uuid_t  uuid;            /**< Target node device UUID */
    uint32_t      last_forward_ms; /**< Timestamp in milliseconds of last forwarded 0xCE beacon */
    bool          active;          /**< True if entry is actively occupied */
} reheal_courier_entry_t;

static reheal_state_t         s_reheal_state        = REHEAL_STATE_PROVISIONED;
static meshx_os_timer_t      *s_reheal_blind_timer  = NULL;
static meshx_os_timer_t      *s_reheal_beacon_timer = NULL;
static meshx_uuid_t           s_reheal_uuid         = {0};
static uint16_t               s_reheal_prev_addr    = 0x0000;
static reheal_courier_entry_t s_courier_table[REHEAL_COURIER_MAX_ENTRIES];

/* --- Forward declarations --- */
static void uvp_reheal_blind_timer_cb(const meshx_os_timer_t *p_timer);
static void uvp_reheal_beacon_timer_cb(const meshx_os_timer_t *p_timer);

/* =========================================================================
 * Re-Healing Helpers & Timers
 * ========================================================================= */

/**
 * @brief Helper to get the current system time in milliseconds.
 *
 * @return Current system time in milliseconds.
 */
static uint32_t reheal_now_ms(void)
{
    unsigned int ms = 0;
    meshx_rtos_get_sys_time(&ms);
    return (uint32_t)ms;
}

/**
 * @brief Check and update the relay-side courier rate limiter for re-healing beacons.
 *
 * Rate limits forwarded 0xCE beacons to at most 1 forward per unique node UUID
 * per 30 seconds (MESHX_REHEAL_COURIER_RATE_LIMIT_MS).
 *
 * @param[in] uuid Pointer to 16-byte node UUID.
 * @return true if beacon should be dropped due to rate limiting, false if forward is allowed.
 */
static bool reheal_courier_rate_limited(const uint8_t *uuid)
{
    if (!uuid)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "UVP Re-Heal: Null UUID passed to courier rate limiter");
        return true;
    }

    uint32_t now = reheal_now_ms();
    for (int i = 0; i < REHEAL_COURIER_MAX_ENTRIES; i++)
    {
        if (!s_courier_table[i].active)
            continue;
        if (memcmp(s_courier_table[i].uuid, uuid, MESHX_KEY_LEN) == 0)
        {
            if (now - s_courier_table[i].last_forward_ms < MESHX_REHEAL_COURIER_RATE_LIMIT_MS)
            {
                MESHX_LOGD(MODULE_ID_COMMON, "UVP Re-Heal: Courier rate limit hit for UUID %02X%02X... (%lu ms elapsed)",
                           uuid[0], uuid[1], (unsigned long)(now - s_courier_table[i].last_forward_ms));
                return true;
            }
            s_courier_table[i].last_forward_ms = now;
            MESHX_LOGD(MODULE_ID_COMMON, "UVP Re-Heal: Courier entry updated for UUID %02X%02X...", uuid[0], uuid[1]);
            return false;
        }
    }
    /* Not found: add new entry (overwrite oldest if table full) */
    static int s_courier_idx = 0;
    s_courier_idx = (s_courier_idx + 1) % REHEAL_COURIER_MAX_ENTRIES;
    memcpy(s_courier_table[s_courier_idx].uuid, uuid, MESHX_KEY_LEN);
    s_courier_table[s_courier_idx].active = true;
    s_courier_table[s_courier_idx].last_forward_ms = now;
    MESHX_LOGD(MODULE_ID_COMMON, "UVP Re-Heal: New courier entry added at index %d for UUID %02X%02X...",
               s_courier_idx, uuid[0], uuid[1]);
    return false;
}

/**
 * @brief Timer callback triggered upon 15-second blindness detection timeout.
 *
 * Enters the ISOLATED state and arms the periodic 0xCE re-healing beacon timer.
 *
 * @param[in] p_timer Pointer to the OS timer structure.
 */
static void uvp_reheal_blind_timer_cb(const meshx_os_timer_t *p_timer)
{
    MESHX_UNUSED(p_timer);
    if (s_reheal_state != REHEAL_STATE_PROVISIONED)
        return;
    MESHX_LOGW(MODULE_ID_COMMON, "UVP Re-Heal: Blindness detection expired (15s). Entering ISOLATED_HEALING.");
    s_reheal_state = REHEAL_STATE_ISOLATED;

    /* Arm periodic beacon timer */
    if (!s_reheal_beacon_timer)
    {
        meshx_os_timer_create("reheal_beacon", MESHX_REHEAL_BLINDNESS_TIMEOUT_MS, true,
                              uvp_reheal_beacon_timer_cb, &s_reheal_beacon_timer);
    }
    if (s_reheal_beacon_timer)
        meshx_os_timer_start(s_reheal_beacon_timer);
}

/**
 * @brief Periodic timer callback transmitting 0xCE Re-Healing Beacons over BLE adv.
 *
 * Transmits authenticated HMAC-SHA256 beacons while in ISOLATED or HEALING state.
 *
 * @param[in] p_timer Pointer to the OS timer structure.
 */
static void uvp_reheal_beacon_timer_cb(const meshx_os_timer_t *p_timer)
{
    MESHX_UNUSED(p_timer);
    MESHX_LOGD(MODULE_ID_COMMON, "UVP Re-Heal: Beacon timer fired, state=%d", s_reheal_state);
    if (s_reheal_state != REHEAL_STATE_ISOLATED && s_reheal_state != REHEAL_STATE_HEALING)
    {
        MESHX_LOGD(MODULE_ID_COMMON, "UVP Re-Heal: Not in isolated/healing state (%d), skipping beacon", s_reheal_state);
        return;
    }

    /* Build Manufacturer Data: [CID(2B LE) | msg_type(1B) | UUID(16B) | prev_addr(2B) | nonce(4B) | hmac(16B)] = 41 bytes */
    uint8_t dev_key[MESHX_KEY_LEN];
    uint8_t hmac_full[MESHX_HMAC_SHA256_FULL_LEN];
    uint8_t mfg_data[2 + 1 + sizeof(meshx_uvp_reheal_req_t)];
    uint8_t signed_data[MESHX_KEY_LEN + sizeof(uint16_t) + sizeof(uint32_t)];

    memset(mfg_data, 0, sizeof(mfg_data));
    uint16_t cid = MESHX_COMPANY_ID_UVP;
    mfg_data[0] = (uint8_t)(cid & 0xFF);
    mfg_data[1] = (uint8_t)((cid >> 8) & 0xFF);
    mfg_data[2] = MESHX_UVP_OP_REHEAL_BEACON_REQ;

    meshx_uvp_reheal_req_t *req = (meshx_uvp_reheal_req_t *)&mfg_data[3];
    memcpy(req->target_uuid, s_reheal_uuid, sizeof(req->target_uuid));
    req->previous_addr = s_reheal_prev_addr;
    req->timestamp_nonce = reheal_now_ms();

    /* Compute HMAC-SHA256 over [UUID || Addr || Nonce] using local DevKey */
    memcpy(signed_data, s_reheal_uuid, sizeof(s_reheal_uuid));
    memcpy(&signed_data[MESHX_KEY_LEN], &req->previous_addr, sizeof(req->previous_addr));
    memcpy(&signed_data[MESHX_KEY_LEN + sizeof(uint16_t)], &req->timestamp_nonce, sizeof(req->timestamp_nonce));

    if (meshx_nvs_get_device_key(&s_reheal_uuid, dev_key) != MESHX_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "UVP Re-Heal: No DevKey in NVS for local UUID, cannot beacon");
        return;
    }

    if (meshx_crypto_hmac_sha256(dev_key, MESHX_KEY_LEN, signed_data, sizeof(signed_data), hmac_full) != MESHX_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "UVP Re-Heal: HMAC-SHA256 failed for beacon");
        return;
    }
    memcpy(req->hmac_signature, hmac_full, MESHX_UVP_HMAC_LEN); /* truncate to 16 bytes */

    /* Transmit via platform raw BLE advertisement */
    meshx_err_t err = meshx_plat_ble_adv_mfg_data(mfg_data, sizeof(mfg_data));
    if (err != MESHX_SUCCESS)
    {
        MESHX_LOGW(MODULE_ID_COMMON, "UVP Re-Heal: 0xCE beacon adv failed (err=%d)", err);
    }
    else
    {
        MESHX_LOGI(MODULE_ID_COMMON, "UVP Re-Heal: 0xCE beacon transmitted (nonce=%lu)",
                   (unsigned long)req->timestamp_nonce);
    }
}

/* =========================================================================
 * Re-Healing Handler Implementations
 * ========================================================================= */

/**
 * @brief Handle 0xCE Re-Healing Beacon Request (Master side).
 *
 * Received by the Master via encrypted courier forward from an intermediate relay node.
 * Decrypts the outer payload using active network key K_new, verifies DevKey authentication
 * via HMAC-SHA256 over [UUID || prev_addr || nonce], checks nonce freshness, and transmits
 * an encrypted 0xCF Re-Healing Admit Response to the evicted node.
 *
 * @param[in] p_meta      Pointer to UVP message metadata header.
 * @param[in] p_ctx       Pointer to UVP routing context.
 * @param[in] payload     Pointer to received AES-128-CCM encrypted beacon payload.
 * @param[in] payload_len Length of payload in bytes.
 * @return MESHX_SUCCESS on successful processing, MESHX_INVALID_ARG on bad format,
 *         or MESHX_FAIL on decryption/HMAC failure.
 */
static meshx_err_t uvp_handle_reheal_beacon_req(const control_task_uvp_meta_t *p_meta,
                                                const meshx_uvp_ctx_t *p_ctx,
                                                const uint8_t *payload,
                                                uint16_t payload_len)
{
    MESHX_UNUSED(p_meta);
    if (!p_ctx || !payload || payload_len < 1 + sizeof(meshx_uvp_reheal_req_t) + MESHX_CCM_MIC_LEN)
        return MESHX_INVALID_ARG;

    /* Only the Master processes 0xCE. Relay nodes handle raw BLE adv reception in the
     * platform layer and forward via meshx_comp_send_uvp directly. */
    if (meshx_get_node_addr() != MESHX_UVP_MASTER_ADDR)
    {
        MESHX_LOGD(MODULE_ID_COMMON, "UVP Re-Heal: 0xCE received but local node is not Master (0x%04X), ignoring",
                   meshx_get_node_addr());
        return MESHX_SUCCESS;
    }

    const uint8_t *ciphertext = payload;
    uint16_t       cipher_len = (uint16_t)(payload_len - MESHX_CCM_MIC_LEN);   /* strip trailing MIC */
    const uint8_t *mic        = &payload[cipher_len];

    /* Decrypt with active K_new */
    uint8_t cur_key[MESHX_KEY_LEN];
    if (meshx_plat_net_key_get(meshx_get_net_key_id(), cur_key) != MESHX_SUCCESS)
    {
        MESHX_LOGW(MODULE_ID_COMMON, "UVP Re-Heal: No active NetKey, cannot decrypt 0xCE");
        return MESHX_FAIL;
    }

    /* AES-CCM nonce: [src_addr(sizeof(uint16_t)) | zeros] */
    uint8_t nonce[MESHX_CCM_NONCE_LEN] = {0};
    memcpy(nonce, &p_ctx->src_addr, sizeof(uint16_t));

    uint8_t plain[1 + sizeof(meshx_uvp_reheal_req_t)];
    if (meshx_crypto_aes_ccm_decrypt(cur_key, nonce, MESHX_CCM_NONCE_LEN,
                                     ciphertext, cipher_len, mic, MESHX_CCM_MIC_LEN,
                                     plain) != MESHX_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "UVP Re-Heal: 0xCE AES-CCM decrypt failed");
        return MESHX_FAIL;
    }

    if (plain[0] != MESHX_UVP_OP_REHEAL_BEACON_REQ)
        return MESHX_INVALID_ARG;

    const meshx_uvp_reheal_req_t *req = (const meshx_uvp_reheal_req_t *)&plain[1];

    /* Look up UUID in NVS DevKey registry */
    uint8_t dev_key[MESHX_KEY_LEN];
    if (meshx_nvs_get_device_key(&req->target_uuid, dev_key) != MESHX_SUCCESS)
    {
        MESHX_LOGW(MODULE_ID_COMMON, "UVP Re-Heal: UUID %02X%02X... not in NVS DevKey registry, dropping 0xCE",
                   req->target_uuid[0], req->target_uuid[1]);
        return MESHX_SUCCESS;
    }

    /* Verify HMAC-SHA256 signature over [UUID || Addr || Nonce] */
    uint8_t hmac_full[MESHX_HMAC_SHA256_FULL_LEN];
    uint8_t signed_data[MESHX_KEY_LEN + sizeof(uint16_t) + sizeof(uint32_t)];
    memcpy(signed_data, req->target_uuid, sizeof(req->target_uuid));
    memcpy(&signed_data[MESHX_KEY_LEN], &req->previous_addr, sizeof(req->previous_addr));
    memcpy(&signed_data[MESHX_KEY_LEN + sizeof(uint16_t)], &req->timestamp_nonce, sizeof(req->timestamp_nonce));

    if (meshx_crypto_hmac_sha256(dev_key, MESHX_KEY_LEN, signed_data, sizeof(signed_data), hmac_full) != MESHX_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "UVP Re-Heal: HMAC-SHA256 computation failed");
        return MESHX_FAIL;
    }

    if (memcmp(hmac_full, req->hmac_signature, MESHX_UVP_HMAC_LEN) != 0)
    {
        MESHX_LOGW(MODULE_ID_COMMON, "UVP Re-Heal: HMAC signature mismatch for UUID %02X%02X..., dropping",
                   req->target_uuid[0], req->target_uuid[1]);
        return MESHX_SUCCESS;
    }

    if (req->timestamp_nonce == 0)
    {
        MESHX_LOGW(MODULE_ID_COMMON, "UVP Re-Heal: Replay/nonce=0 detected, dropping 0xCE");
        return MESHX_SUCCESS;
    }

    MESHX_LOGI(MODULE_ID_COMMON, "UVP Re-Heal: 0xCE verified for UUID %02X%02X... (addr=0x%04X, nonce=%lu)",
               req->target_uuid[0], req->target_uuid[1], req->previous_addr,
               (unsigned long)req->timestamp_nonce);

    /* Generate 0xCF Re-Healing Admit Response (encrypted with DevKey) */
    uint8_t cf_plain[1 + sizeof(meshx_uvp_reheal_rsp_plain_t)];
    cf_plain[0] = MESHX_UVP_OP_REHEAL_ADMIT_RSP;
    meshx_uvp_reheal_rsp_plain_t *rsp = (meshx_uvp_reheal_rsp_plain_t *)&cf_plain[1];

    uint8_t active_netkey[MESHX_KEY_LEN];
    if (meshx_plat_net_key_get(meshx_get_net_key_id(), active_netkey) == MESHX_SUCCESS)
        memcpy(rsp->active_netkey, active_netkey, MESHX_KEY_LEN);
    rsp->active_epoch = meshx_nvs_get_kr_epoch();
    rsp->assigned_addr = req->previous_addr;

    uint8_t cf_nonce[MESHX_CCM_NONCE_LEN] = {0};
    memcpy(cf_nonce, &req->previous_addr, sizeof(uint16_t));

    uint8_t cf_cipher[1 + sizeof(meshx_uvp_reheal_rsp_plain_t)];
    uint8_t cf_tag[MESHX_CCM_MIC_LEN];
    if (meshx_crypto_aes_ccm_encrypt(dev_key, cf_nonce, MESHX_CCM_NONCE_LEN,
                                     cf_plain, sizeof(cf_plain),
                                     cf_cipher, cf_tag, MESHX_CCM_MIC_LEN) != MESHX_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "UVP Re-Heal: 0xCF AES-CCM encrypt failed");
        return MESHX_FAIL;
    }

    uint8_t cf_enc_buf[sizeof(cf_cipher) + sizeof(cf_tag)];
    memcpy(cf_enc_buf, cf_cipher, sizeof(cf_cipher));
    memcpy(&cf_enc_buf[sizeof(cf_cipher)], cf_tag, sizeof(cf_tag));

    meshx_err_t err = meshx_comp_send_uvp(0,
                                          req->previous_addr,
                                          MESHX_FUNC_ID_SYS_PAIRING,
                                          cf_enc_buf,
                                          (uint16_t)sizeof(cf_enc_buf),
                                          false);
    if (err == MESHX_SUCCESS)
    {
        MESHX_LOGI(MODULE_ID_COMMON, "UVP Re-Heal: 0xCF admit response sent to 0x%04X (epoch=%lu)",
                   req->previous_addr, (unsigned long)rsp->active_epoch);
    }
    return err;
}

/**
 * @brief Handle 0xCF Re-Healing Admit Response (Evicted Node side).
 *
 * Received by the isolated/evicted node from Master. Decrypts the response using local
 * DevKey from NVS, installs the active NetKey, sets the new key refresh epoch, restores
 * operational state, and dispatches an MXCP_EVT_UVP_REHEAL_ADMITTED event to Host MCU.
 *
 * @param[in] p_meta      Pointer to UVP message metadata header.
 * @param[in] p_ctx       Pointer to UVP routing context.
 * @param[in] payload     Pointer to received AES-128-CCM encrypted response payload.
 * @param[in] payload_len Length of payload in bytes.
 * @return MESHX_SUCCESS on success, MESHX_INVALID_ARG on bad payload, or error code.
 */
static meshx_err_t uvp_handle_reheal_admit_rsp(const control_task_uvp_meta_t *p_meta,
                                               const meshx_uvp_ctx_t *p_ctx,
                                               const uint8_t *payload,
                                               uint16_t payload_len)
{
    MESHX_UNUSED(p_meta);
    if (!p_ctx || !payload || payload_len < 1 + sizeof(uint32_t) + MESHX_KEY_LEN + MESHX_CCM_MIC_LEN)
        return MESHX_INVALID_ARG;

    const uint8_t *ciphertext = payload;
    uint16_t       cipher_len = (uint16_t)(payload_len - MESHX_CCM_MIC_LEN);
    const uint8_t *mic        = &payload[cipher_len];

    uint8_t dev_key[MESHX_KEY_LEN];
    if (meshx_nvs_get_device_key(&s_reheal_uuid, dev_key) != MESHX_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "UVP Re-Heal: No DevKey in NVS, cannot decrypt 0xCF");
        return MESHX_FAIL;
    }

    uint8_t nonce[MESHX_CCM_NONCE_LEN] = {0};
    memcpy(nonce, &s_reheal_prev_addr, sizeof(uint16_t));

    uint8_t plain[1 + sizeof(meshx_uvp_reheal_rsp_plain_t)];
    if (meshx_crypto_aes_ccm_decrypt(dev_key, nonce, MESHX_CCM_NONCE_LEN,
                                     ciphertext, cipher_len, mic, MESHX_CCM_MIC_LEN,
                                     plain) != MESHX_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "UVP Re-Heal: 0xCF AES-CCM decrypt failed");
        return MESHX_FAIL;
    }

    if (plain[0] != MESHX_UVP_OP_REHEAL_ADMIT_RSP)
        return MESHX_INVALID_ARG;

    const meshx_uvp_reheal_rsp_plain_t *rsp = (const meshx_uvp_reheal_rsp_plain_t *)&plain[1];

    meshx_plat_net_key_add(rsp->active_netkey, meshx_get_net_key_id());
    meshx_nvs_set_kr_epoch(rsp->active_epoch);

    s_reheal_state = REHEAL_STATE_PROVISIONED;

    mxcp_evt_uvp_reheal_admitted_t evt;
    memset(&evt, 0, sizeof(evt));
    memcpy(evt.node_uuid, s_reheal_uuid, sizeof(s_reheal_uuid));
    evt.restored_addr = rsp->assigned_addr;
    evt.new_epoch = rsp->active_epoch;
    mxcp_send_event(MXCP_EVT_UVP_REHEAL_ADMITTED, (const uint8_t *)&evt, sizeof(evt));

    MESHX_LOGI(MODULE_ID_COMMON, "UVP Re-Heal: Node re-admitted, epoch=%lu, addr=0x%04X",
               (unsigned long)rsp->active_epoch, rsp->assigned_addr);
    return MESHX_SUCCESS;
}

meshx_err_t meshx_reheal_start_eviction_detection(dev_struct_t *pdev)
{
    if (!pdev)
        return MESHX_INVALID_ARG;

    memcpy(s_reheal_uuid, pdev->uuid, sizeof(s_reheal_uuid));
    s_reheal_prev_addr = pdev->meshx_store.node_addr;
    s_reheal_state = REHEAL_STATE_PROVISIONED;

    if (!s_reheal_blind_timer)
    {
        meshx_os_timer_create("reheal_blind", MESHX_REHEAL_BLINDNESS_TIMEOUT_MS, false,
                              uvp_reheal_blind_timer_cb, &s_reheal_blind_timer);
    }
    if (s_reheal_blind_timer)
    {
        meshx_os_timer_start(s_reheal_blind_timer);
        MESHX_LOGI(MODULE_ID_COMMON, "UVP Re-Heal: 15s blindness detection timer armed");
    }
    return MESHX_SUCCESS;
}

meshx_err_t meshx_reheal_courier_forward_to_master(const uint8_t *beacon_payload, uint16_t beacon_payload_len,
                                                   uint16_t relay_addr)
{
    if (!beacon_payload || beacon_payload_len < sizeof(meshx_uvp_reheal_req_t))
        return MESHX_INVALID_ARG;

    const meshx_uvp_reheal_req_t *req = (const meshx_uvp_reheal_req_t *)beacon_payload;
    uint8_t cur_key[MESHX_KEY_LEN];
    uint8_t tag[MESHX_CCM_MIC_LEN];
    uint8_t fwd_buf[1 + sizeof(meshx_uvp_reheal_req_t)];
    uint8_t cipher[1 + sizeof(meshx_uvp_reheal_req_t)];
    uint8_t enc_buf[sizeof(cipher) + sizeof(tag)];

    /* Rate limiter - max 1 forward per unique UUID per 30s */
    if (reheal_courier_rate_limited(req->target_uuid))
    {
        MESHX_LOGD(MODULE_ID_COMMON, "UVP Re-Heal: Rate limited, dropping 0xCE for UUID %02X%02X...",
                   req->target_uuid[0], req->target_uuid[1]);
        return MESHX_SUCCESS;
    }

    if (meshx_plat_net_key_get(meshx_get_net_key_id(), cur_key) != MESHX_SUCCESS)
    {
        MESHX_LOGW(MODULE_ID_COMMON, "UVP Re-Heal: No active NetKey, cannot forward 0xCE");
        return MESHX_FAIL;
    }

    fwd_buf[0] = MESHX_UVP_OP_REHEAL_BEACON_REQ;
    memcpy(&fwd_buf[1], req, sizeof(meshx_uvp_reheal_req_t));

    uint8_t nonce[MESHX_CCM_NONCE_LEN] = {0};
    memcpy(nonce, &relay_addr, sizeof(uint16_t));
    memcpy(&nonce[sizeof(uint16_t)], &req->timestamp_nonce, sizeof(uint32_t));

    if (meshx_crypto_aes_ccm_encrypt(cur_key, nonce, MESHX_CCM_NONCE_LEN, fwd_buf, sizeof(fwd_buf),
                                     cipher, tag, MESHX_CCM_MIC_LEN) != MESHX_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "UVP Re-Heal: AES-CCM encrypt failed for 0xCE forward");
        return MESHX_FAIL;
    }

    memcpy(enc_buf, cipher, sizeof(cipher));
    memcpy(&enc_buf[sizeof(cipher)], tag, sizeof(tag));

    meshx_err_t err = meshx_comp_send_uvp(0,
                                          MESHX_UVP_MASTER_ADDR,
                                          MESHX_FUNC_ID_SYS_PAIRING,
                                          enc_buf,
                                          (uint16_t)sizeof(enc_buf),
                                          false);
    if (err == MESHX_SUCCESS)
    {
        MESHX_LOGI(MODULE_ID_COMMON, "UVP Re-Heal: 0xCE forwarded to Master 0x%04X (courier)",
                   MESHX_UVP_MASTER_ADDR);
    }
    return err;
}

/* =========================================================================
 * Registration Entry Point
 * ========================================================================= */

/**
 * @brief Registers all UVP Re-Heal handlers.
 * @return MESHX_SUCCESS on successful registration.
 */
meshx_err_t meshx_uvp_register_reheal_handlers(void)
{
    meshx_err_t err;

    err = meshx_uvp_register_handler(MESHX_FUNC_ID_SYS_PAIRING, MESHX_UVP_OP_REHEAL_BEACON_REQ, uvp_handle_reheal_beacon_req);
    if (err != MESHX_SUCCESS) return err;

    err = meshx_uvp_register_handler(MESHX_FUNC_ID_SYS_PAIRING, MESHX_UVP_OP_REHEAL_ADMIT_RSP, uvp_handle_reheal_admit_rsp);
    return err;
}
