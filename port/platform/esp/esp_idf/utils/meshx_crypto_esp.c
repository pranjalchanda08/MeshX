/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_crypto_esp.c
 * @brief ESP-IDF mbedTLS bindings for MeshX portable crypto abstraction.
 *
 * Implements HMAC-SHA256 and AES-128-CCM using ESP-IDF mbedTLS
 * hardware accelerator bindings without vendor bloat.
 *
 * @author Pranjal Chanda
 */

#include "interface/crypto/meshx_crypto.h"
#include "interface/logging/meshx_log.h"

#include "mbedtls/md.h"
#include "mbedtls/aes.h"
#include "mbedtls/ccm.h"

#include "mbedtls/ecdh.h"
#include "mbedtls/ecp.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include <string.h>

#define MODULE_ID_CRYPTO MODULE_ID_COMMON

/**
 * @brief Compute HMAC-SHA256 signature over input data.
 *
 * Uses mbedTLS MD module with SHA-256 as the underlying hash.
 * Output is the full 32-byte HMAC; callers may truncate as needed.
 */
meshx_err_t meshx_crypto_hmac_sha256(const uint8_t *key, size_t key_len,
                                     const uint8_t *data, size_t data_len,
                                     uint8_t *out_mac)
{
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_alg = MBEDTLS_MD_SHA256;
    int ret;

    if (!key || !data || !out_mac)
    {
        return MESHX_INVALID_ARG;
    }

    mbedtls_md_init(&ctx);
    ret = mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_alg), 1); /* 1 = HMAC */
    if (ret != 0)
    {
        mbedtls_md_free(&ctx);
        MESHX_LOGE(MODULE_ID_CRYPTO, "HMAC-SHA256: mbedtls_md_setup failed (%d)", ret);
        return MESHX_FAIL;
    }

    ret = mbedtls_md_hmac_starts(&ctx, key, key_len);
    if (ret != 0)
    {
        mbedtls_md_free(&ctx);
        MESHX_LOGE(MODULE_ID_CRYPTO, "HMAC-SHA256: mbedtls_md_hmac_starts failed (%d)", ret);
        return MESHX_FAIL;
    }

    ret = mbedtls_md_hmac_update(&ctx, data, data_len);
    if (ret != 0)
    {
        mbedtls_md_free(&ctx);
        MESHX_LOGE(MODULE_ID_CRYPTO, "HMAC-SHA256: mbedtls_md_hmac_update failed (%d)", ret);
        return MESHX_FAIL;
    }

    ret = mbedtls_md_hmac_finish(&ctx, out_mac);
    mbedtls_md_free(&ctx);

    if (ret != 0)
    {
        MESHX_LOGE(MODULE_ID_CRYPTO, "HMAC-SHA256: mbedtls_md_hmac_finish failed (%d)", ret);
        return MESHX_FAIL;
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Encrypt data using AES-128-CCM.
 *
 * Uses mbedTLS CCM module. Output ciphertext is same length as plaintext,
 * and the authentication tag is written to the caller-provided tag buffer.
 */
meshx_err_t meshx_crypto_aes_ccm_encrypt(const uint8_t *key,
                                         const uint8_t *nonce, size_t nonce_len,
                                         const uint8_t *plaintext, size_t len,
                                         uint8_t *ciphertext,
                                         uint8_t *tag, size_t tag_len)
{
    mbedtls_ccm_context ctx;
    int ret;

    if (!key || !nonce || !plaintext || !ciphertext || !tag)
    {
        return MESHX_INVALID_ARG;
    }

    mbedtls_ccm_init(&ctx);
    ret = mbedtls_ccm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 128);
    if (ret != 0)
    {
        mbedtls_ccm_free(&ctx);
        MESHX_LOGE(MODULE_ID_CRYPTO, "AES-CCM encrypt: mbedtls_ccm_setkey failed (%d)", ret);
        return MESHX_FAIL;
    }

    ret = mbedtls_ccm_encrypt_and_tag(&ctx, len, nonce, nonce_len,
                                      NULL, 0,                   /* no additional data */
                                      plaintext, ciphertext,
                                      tag, tag_len);
    mbedtls_ccm_free(&ctx);

    if (ret != 0)
    {
        MESHX_LOGE(MODULE_ID_CRYPTO, "AES-CCM encrypt: mbedtls_ccm_encrypt_and_tag failed (%d)", ret);
        return MESHX_FAIL;
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Decrypt and verify data using AES-128-CCM.
 *
 * Returns MESHX_SUCCESS only if decryption succeeds and the authentication
 * tag verifies. On tag mismatch returns MESHX_FAIL.
 */
meshx_err_t meshx_crypto_aes_ccm_decrypt(const uint8_t *key,
                                         const uint8_t *nonce, size_t nonce_len,
                                         const uint8_t *ciphertext, size_t len,
                                         const uint8_t *tag, size_t tag_len,
                                         uint8_t *plaintext)
{
    mbedtls_ccm_context ctx;
    int ret;

    if (!key || !nonce || !ciphertext || !tag || !plaintext)
    {
        return MESHX_INVALID_ARG;
    }

    mbedtls_ccm_init(&ctx);
    ret = mbedtls_ccm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 128);
    if (ret != 0)
    {
        mbedtls_ccm_free(&ctx);
        MESHX_LOGE(MODULE_ID_CRYPTO, "AES-CCM decrypt: mbedtls_ccm_setkey failed (%d)", ret);
        return MESHX_FAIL;
    }

    ret = mbedtls_ccm_auth_decrypt(&ctx, len, nonce, nonce_len,
                                   NULL, 0,                   /* no additional data */
                                   ciphertext, plaintext,
                                   tag, tag_len);
    mbedtls_ccm_free(&ctx);

    if (ret != 0)
    {
        MESHX_LOGE(MODULE_ID_CRYPTO, "AES-CCM decrypt: mbedtls_ccm_auth_decrypt failed (%d)", ret);
        return MESHX_FAIL;
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Generate NIST P-256 Elliptic Curve keypair.
 *
 * @param[out] out_public_key  64-byte public key (X || Y raw coordinates).
 * @param[out] out_private_key 32-byte private key (raw scalar d).
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_crypto_p256_keypair_gen(meshx_p256_pubkey_t out_public_key,
                                         meshx_p256_privkey_t out_private_key)
{
    mbedtls_ecp_group grp;
    mbedtls_mpi d;
    mbedtls_ecp_point Q;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    int ret;

    if (!out_public_key || !out_private_key)
    {
        MESHX_LOGE(MODULE_ID_CRYPTO, "P-256 gen: NULL parameter received");
        return MESHX_INVALID_ARG;
    }

    MESHX_LOGD(MODULE_ID_CRYPTO, "Generating NIST P-256 ephemeral keypair");

    mbedtls_ecp_group_init(&grp);
    mbedtls_mpi_init(&d);
    mbedtls_ecp_point_init(&Q);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    const char *pers = "meshx_p256_gen";
    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                (const unsigned char *)pers, strlen(pers));
    if (ret != 0)
    {
        MESHX_LOGE(MODULE_ID_CRYPTO, "P-256: ctr_drbg_seed failed (%d)", ret);
        goto cleanup;
    }

    ret = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1);
    if (ret != 0)
    {
        MESHX_LOGE(MODULE_ID_CRYPTO, "P-256: ecp_group_load failed (%d)", ret);
        goto cleanup;
    }

    ret = mbedtls_ecp_gen_keypair(&grp, &d, &Q, mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0)
    {
        MESHX_LOGE(MODULE_ID_CRYPTO, "P-256: ecp_gen_keypair failed (%d)", ret);
        goto cleanup;
    }

    /* Serialize public key in uncompressed format (0x04 || X || Y = 65 bytes) */
    uint8_t uncompressed[1 + MESHX_P256_PUBKEY_LEN];
    size_t olen = 0;
    ret = mbedtls_ecp_point_write_binary(&grp, &Q, MBEDTLS_ECP_PF_UNCOMPRESSED,
                                         &olen, uncompressed, sizeof(uncompressed));
    if (ret == 0 && olen == sizeof(uncompressed))
    {
        memcpy(out_public_key, uncompressed + 1, MESHX_P256_PUBKEY_LEN);
    }
    else
    {
        MESHX_LOGE(MODULE_ID_CRYPTO, "P-256: point_write_binary failed (%d)", ret);
        goto cleanup;
    }

    ret = mbedtls_mpi_write_binary(&d, out_private_key, MESHX_P256_PRIV_LEN);
    if (ret != 0)
    {
        MESHX_LOGE(MODULE_ID_CRYPTO, "P-256: mpi_write_binary d failed (%d)", ret);
        goto cleanup;
    }

    MESHX_LOGD(MODULE_ID_CRYPTO, "NIST P-256 keypair generated successfully");

cleanup:
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    mbedtls_ecp_point_free(&Q);
    mbedtls_mpi_free(&d);
    mbedtls_ecp_group_free(&grp);

    return (ret == 0) ? MESHX_SUCCESS : MESHX_FAIL;
}

/**
 * @brief Compute ECDH shared secret over NIST P-256 curve.
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
    mbedtls_ecp_group grp;
    mbedtls_mpi d;
    mbedtls_ecp_point Qp;
    mbedtls_ecp_point P;
    int ret;

    if (!peer_public_key || !local_private_key || !out_shared_secret)
    {
        MESHX_LOGE(MODULE_ID_CRYPTO, "P-256 ECDH: NULL parameter received");
        return MESHX_INVALID_ARG;
    }

    MESHX_LOGD(MODULE_ID_CRYPTO, "Computing NIST P-256 ECDH shared secret");

    mbedtls_ecp_group_init(&grp);
    mbedtls_mpi_init(&d);
    mbedtls_ecp_point_init(&Qp);
    mbedtls_ecp_point_init(&P);

    ret = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1);
    if (ret != 0)
    {
        MESHX_LOGE(MODULE_ID_CRYPTO, "P-256 ECDH: ecp_group_load failed (%d)", ret);
        goto cleanup;
    }

    /* Wrap raw 64-byte public key into uncompressed format (0x04 || X || Y) */
    uint8_t uncompressed[1 + MESHX_P256_PUBKEY_LEN];
    uncompressed[0] = 0x04;
    memcpy(uncompressed + 1, peer_public_key, MESHX_P256_PUBKEY_LEN);

    ret = mbedtls_ecp_point_read_binary(&grp, &Qp, uncompressed, sizeof(uncompressed));
    if (ret != 0)
    {
        MESHX_LOGE(MODULE_ID_CRYPTO, "P-256 ECDH: point_read_binary failed (%d)", ret);
        goto cleanup;
    }

    ret = mbedtls_mpi_read_binary(&d, local_private_key, MESHX_P256_PRIV_LEN);
    if (ret != 0)
    {
        MESHX_LOGE(MODULE_ID_CRYPTO, "P-256 ECDH: mpi_read_binary d failed (%d)", ret);
        goto cleanup;
    }

    /* Compute shared point P = d * Qp */
    ret = mbedtls_ecp_mul(&grp, &P, &d, &Qp, NULL, NULL);
    if (ret != 0)
    {
        MESHX_LOGE(MODULE_ID_CRYPTO, "P-256 ECDH: ecp_mul failed (%d)", ret);
        goto cleanup;
    }

    /* Shared secret in BLE Mesh is the X-coordinate of P */
    size_t olen = 0;
    ret = mbedtls_ecp_point_write_binary(&grp, &P, MBEDTLS_ECP_PF_UNCOMPRESSED,
                                         &olen, uncompressed, sizeof(uncompressed));
    if (ret == 0 && olen == sizeof(uncompressed))
    {
        memcpy(out_shared_secret, uncompressed + 1, MESHX_P256_COORD_LEN);
        MESHX_LOGD(MODULE_ID_CRYPTO, "P-256 ECDH: shared secret computed successfully");
    }
    else
    {
        MESHX_LOGE(MODULE_ID_CRYPTO, "P-256 ECDH: point_write_binary for secret failed (%d)", ret);
    }

cleanup:
    mbedtls_ecp_point_free(&P);
    mbedtls_ecp_point_free(&Qp);
    mbedtls_mpi_free(&d);
    mbedtls_ecp_group_free(&grp);

    return (ret == 0) ? MESHX_SUCCESS : MESHX_FAIL;
}
