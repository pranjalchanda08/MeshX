/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_crypto_zephyr.c
 * @brief Zephyr crypto bindings for MeshX portable crypto abstraction.
 *
 * Implements HMAC-SHA256 and AES-128-CCM using TinyCrypt (available in Zephyr
 * modules/crypto/tinycrypt) without vendor bloat.
 *
 * @author Pranjal Chanda
 */

#include "interface/crypto/meshx_crypto.h"
#include "interface/logging/meshx_log.h"

#include <tinycrypt/hmac.h>
#include <tinycrypt/aes.h>
#include <tinycrypt/ccm_mode.h>
#include <tinycrypt/constants.h>
#include <string.h>
#include "meshx_uvp.h"

#define MODULE_ID_CRYPTO MODULE_ID_COMMON

/**
 * @brief Compute HMAC-SHA256 signature over input data.
 *
 * Uses TinyCrypt HMAC-SHA256. Output is the full 32-byte HMAC; callers may
 * truncate as needed (e.g. to 16 bytes for the 0xCE beacon).
 */
meshx_err_t meshx_crypto_hmac_sha256(const uint8_t *key, size_t key_len,
                                     const uint8_t *data, size_t data_len,
                                     uint8_t *out_mac)
{
    struct tc_hmac_state_struct hmac_state;
    int ret;

    if (!key || !data || !out_mac)
    {
        return MESHX_INVALID_ARG;
    }

    ret = tc_hmac_set_key(&hmac_state, key, (unsigned int)key_len);
    if (ret != TC_CRYPTO_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_CRYPTO, "HMAC-SHA256: tc_hmac_set_key failed (%d)", ret);
        return MESHX_FAIL;
    }

    ret = tc_hmac_init(&hmac_state);
    if (ret != TC_CRYPTO_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_CRYPTO, "HMAC-SHA256: tc_hmac_init failed (%d)", ret);
        return MESHX_FAIL;
    }

    ret = tc_hmac_update(&hmac_state, data, (unsigned int)data_len);
    if (ret != TC_CRYPTO_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_CRYPTO, "HMAC-SHA256: tc_hmac_update failed (%d)", ret);
        return MESHX_FAIL;
    }

    ret = tc_hmac_final(out_mac, TC_SHA256_DIGEST_SIZE, &hmac_state);
    if (ret != TC_CRYPTO_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_CRYPTO, "HMAC-SHA256: tc_hmac_final failed (%d)", ret);
        return MESHX_FAIL;
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Encrypt data using AES-128-CCM via TinyCrypt.
 *
 * TinyCrypt CCM writes ciphertext and tag contiguously (out buffer must be
 * plen + mlen bytes). We split them into the caller-provided ciphertext and
 * tag buffers.
 */
meshx_err_t meshx_crypto_aes_ccm_encrypt(const uint8_t *key,
                                         const uint8_t *nonce, size_t nonce_len,
                                         const uint8_t *plaintext, size_t len,
                                         uint8_t *ciphertext,
                                         uint8_t *tag, size_t tag_len)
{
    unsigned char sched[sizeof(unsigned int) * Nb * (Nr + 1)];
    struct tc_ccm_mode_struct ccm;
    TCAesKeySched_t aes_sched = (TCAesKeySched_t)sched;
    uint8_t combined[MESHX_UVP_MAX_PAYLOAD + MESHX_CCM_MIC_LEN];
    int ret;

    if (!key || !nonce || !plaintext || !ciphertext || !tag)
    {
        return MESHX_INVALID_ARG;
    }

    if (len + tag_len > sizeof(combined))
    {
        return MESHX_NO_MEM;
    }

    ret = tc_aes128_set_encrypt_key(aes_sched, key);
    if (ret != TC_CRYPTO_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_CRYPTO, "AES-CCM encrypt: tc_aes128_set_encrypt_key failed (%d)", ret);
        return MESHX_FAIL;
    }

    ret = tc_ccm_config(&ccm, aes_sched, (uint8_t *)nonce,
                        (unsigned int)nonce_len, (unsigned int)tag_len);
    if (ret != TC_CRYPTO_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_CRYPTO, "AES-CCM encrypt: tc_ccm_config failed (%d)", ret);
        return MESHX_FAIL;
    }

    ret = tc_ccm_generation_encryption(combined, (unsigned int)(len + tag_len),
                                       NULL, 0,                /* no AAD */
                                       plaintext, (unsigned int)len,
                                       &ccm);
    if (ret != TC_CRYPTO_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_CRYPTO, "AES-CCM encrypt: tc_ccm_generation_encryption failed (%d)", ret);
        return MESHX_FAIL;
    }

    memcpy(ciphertext, combined, len);
    memcpy(tag, combined + len, tag_len);
    return MESHX_SUCCESS;
}

/**
 * @brief Decrypt and verify data using AES-128-CCM via TinyCrypt.
 *
 * TinyCrypt CCM expects ciphertext+tag contiguous in the payload buffer. We
 * combine them into a temporary buffer before calling the verify function.
 * Returns MESHX_SUCCESS only if decryption succeeds and the authentication
 * tag verifies. On tag mismatch returns MESHX_FAIL.
 */
meshx_err_t meshx_crypto_aes_ccm_decrypt(const uint8_t *key,
                                         const uint8_t *nonce, size_t nonce_len,
                                         const uint8_t *ciphertext, size_t len,
                                         const uint8_t *tag, size_t tag_len,
                                         uint8_t *plaintext)
{
    unsigned char sched[sizeof(unsigned int) * Nb * (Nr + 1)];
    struct tc_ccm_mode_struct ccm;
    TCAesKeySched_t aes_sched = (TCAesKeySched_t)sched;
    uint8_t combined[MESHX_UVP_MAX_PAYLOAD + MESHX_CCM_MIC_LEN];
    int ret;

    if (!key || !nonce || !ciphertext || !tag || !plaintext)
    {
        return MESHX_INVALID_ARG;
    }

    if (len + tag_len > sizeof(combined))
    {
        return MESHX_NO_MEM;
    }

    ret = tc_aes128_set_decrypt_key(aes_sched, key);
    if (ret != TC_CRYPTO_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_CRYPTO, "AES-CCM decrypt: tc_aes128_set_decrypt_key failed (%d)", ret);
        return MESHX_FAIL;
    }

    ret = tc_ccm_config(&ccm, aes_sched, (uint8_t *)nonce,
                        (unsigned int)nonce_len, (unsigned int)tag_len);
    if (ret != TC_CRYPTO_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_CRYPTO, "AES-CCM decrypt: tc_ccm_config failed (%d)", ret);
        return MESHX_FAIL;
    }

    memcpy(combined, ciphertext, len);
    memcpy(combined + len, tag, tag_len);

    ret = tc_ccm_decryption_verification(plaintext, (unsigned int)len,
                                         NULL, 0,                /* no AAD */
                                         combined, (unsigned int)(len + tag_len),
                                         &ccm);
    if (ret != TC_CRYPTO_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_CRYPTO, "AES-CCM decrypt: tc_ccm_decryption_verification failed (%d)", ret);
        return MESHX_FAIL;
    }

    return MESHX_SUCCESS;
}

#if defined(CONFIG_TINYCRYPT_ECC_DH)
#include <tinycrypt/ecc_dh.h>
#endif

/**
 * @brief Generate a NIST P-256 ECC ephemeral keypair using TinyCrypt.
 *
 * Calls uECC_make_key() under the hood from TinyCrypt ecc_dh.
 *
 * @param[out] out_public_key  64-byte public key (X || Y raw coordinates).
 * @param[out] out_private_key 32-byte private key (raw scalar d).
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_crypto_p256_keypair_gen(meshx_p256_pubkey_t out_public_key,
                                         meshx_p256_privkey_t out_private_key)
{
#if defined(CONFIG_TINYCRYPT_ECC_DH)
    if (!out_public_key || !out_private_key)
    {
        MESHX_LOGE(MODULE_ID_CRYPTO, "P-256 gen: NULL parameter received");
        return MESHX_INVALID_ARG;
    }

    MESHX_LOGD(MODULE_ID_CRYPTO, "Generating NIST P-256 keypair via TinyCrypt");

    int ret = uECC_make_key(out_public_key, out_private_key, &curve_secp256r1);
    if (ret != TC_CRYPTO_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_CRYPTO, "P-256 gen: uECC_make_key failed (%d)", ret);
        return MESHX_FAIL;
    }

    MESHX_LOGD(MODULE_ID_CRYPTO, "NIST P-256 keypair generated successfully via TinyCrypt");
    return MESHX_SUCCESS;
#else
    MESHX_LOGE(MODULE_ID_CRYPTO, "P-256 gen: CONFIG_TINYCRYPT_ECC_DH not enabled");
    return MESHX_NOT_SUPPORTED;
#endif
}

/**
 * @brief Compute ECDH shared secret over NIST P-256 curve using TinyCrypt.
 *
 * Calls uECC_shared_secret() from TinyCrypt ecc_dh. The shared secret output
 * is the 32-byte X coordinate of the resulting point.
 *
 * @param[in]  peer_public_key  64-byte peer public key (X || Y).
 * @param[in]  local_private_key 32-byte local private key scalar.
 * @param[out] out_shared_secret Buffer of 32 bytes for shared secret (X-coordinate).
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_crypto_p256_ecdh_compute(const meshx_p256_pubkey_t peer_public_key,
                                          const meshx_p256_privkey_t local_private_key,
                                          meshx_p256_secret_t out_shared_secret)
{
#if defined(CONFIG_TINYCRYPT_ECC_DH)
    if (!peer_public_key || !local_private_key || !out_shared_secret)
    {
        MESHX_LOGE(MODULE_ID_CRYPTO, "P-256 ECDH: NULL parameter received");
        return MESHX_INVALID_ARG;
    }

    MESHX_LOGD(MODULE_ID_CRYPTO, "Computing NIST P-256 ECDH shared secret via TinyCrypt");

    int ret = uECC_shared_secret(peer_public_key, local_private_key, out_shared_secret, &curve_secp256r1);
    if (ret != TC_CRYPTO_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_CRYPTO, "P-256 ECDH: uECC_shared_secret failed (%d)", ret);
        return MESHX_FAIL;
    }

    MESHX_LOGD(MODULE_ID_CRYPTO, "NIST P-256 ECDH computed successfully via TinyCrypt");
    return MESHX_SUCCESS;
#else
    MESHX_LOGE(MODULE_ID_CRYPTO, "P-256 ECDH: CONFIG_TINYCRYPT_ECC_DH not enabled");
    return MESHX_NOT_SUPPORTED;
#endif
}
