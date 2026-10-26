/**
 * @file meshx_crypto.h
 * @brief Portable cryptographic abstraction for MeshX (HMAC-SHA256 & AES-128-CCM)
 *
 * Provides a clean interface for cryptographic operations across ESP-IDF and Zephyr
 * hardware accelerators or underlying mbedTLS libraries without vendor bloat.
 *
 * @copyright Copyright 2024 - 2026 MeshX
 */

#ifndef __MESHX_CRYPTO_H__
#define __MESHX_CRYPTO_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "interface/utils/meshx_fal_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MESHX_P256_COORD_LEN  (32U)
#define MESHX_P256_PUBKEY_LEN (64U)
#define MESHX_P256_PRIV_LEN   (32U)
#define MESHX_P256_SECRET_LEN (32U)

typedef uint8_t meshx_p256_pubkey_t[MESHX_P256_PUBKEY_LEN];
typedef uint8_t meshx_p256_privkey_t[MESHX_P256_PRIV_LEN];
typedef uint8_t meshx_p256_secret_t[MESHX_P256_SECRET_LEN];

/**
 * @brief Generate NIST P-256 Elliptic Curve keypair.
 *
 * @param[out] out_public_key  64-byte public key (X || Y raw coordinates).
 * @param[out] out_private_key 32-byte private key (raw scalar d).
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_crypto_p256_keypair_gen(meshx_p256_pubkey_t out_public_key,
                                         meshx_p256_privkey_t out_private_key);

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
                                          meshx_p256_secret_t out_shared_secret);

/**
 * @brief Compute HMAC-SHA256 signature over input data.
 *
 * @param[in]  key       Pointer to secret key buffer.
 * @param[in]  key_len   Length of secret key in bytes (typically 16 bytes for DevKey).
 * @param[in]  data      Pointer to input data buffer.
 * @param[in]  data_len  Length of input data in bytes.
 * @param[out] out_mac   Buffer of at least 32 bytes to store the generated SHA-256 HMAC.
 * @return MESHX_SUCCESS on success, or appropriate error code.
 */
meshx_err_t meshx_crypto_hmac_sha256(const uint8_t *key, size_t key_len,
                                    const uint8_t *data, size_t data_len,
                                    uint8_t *out_mac);

/**
 * @brief Encrypt data using AES-128-CCM (Counter with CBC-MAC).
 *
 * @param[in]  key        16-byte AES-128 key.
 * @param[in]  nonce      Pointer to nonce buffer.
 * @param[in]  nonce_len  Length of nonce in bytes (typically 12 or 13 bytes).
 * @param[in]  plaintext  Pointer to plaintext input data buffer.
 * @param[in]  len        Length of plaintext buffer in bytes.
 * @param[out] ciphertext Output buffer for ciphertext (must be at least len bytes).
 * @param[out] tag        Output buffer for authentication tag (typically 4 bytes for MIC).
 * @param[in]  tag_len    Length of authentication tag (e.g. 4 bytes).
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_crypto_aes_ccm_encrypt(const uint8_t *key,
                                        const uint8_t *nonce, size_t nonce_len,
                                        const uint8_t *plaintext, size_t len,
                                        uint8_t *ciphertext,
                                        uint8_t *tag, size_t tag_len);

/**
 * @brief Decrypt and verify data using AES-128-CCM.
 *
 * @param[in]  key        16-byte AES-128 key.
 * @param[in]  nonce      Pointer to nonce buffer.
 * @param[in]  nonce_len  Length of nonce in bytes.
 * @param[in]  ciphertext Pointer to ciphertext input buffer.
 * @param[in]  len        Length of ciphertext buffer in bytes.
 * @param[in]  tag        Authentication tag buffer.
 * @param[in]  tag_len    Length of authentication tag.
 * @param[out] plaintext  Output buffer for decrypted plaintext (must be at least len bytes).
 * @return MESHX_SUCCESS on successful decryption and MAC verification, or error code.
 */
meshx_err_t meshx_crypto_aes_ccm_decrypt(const uint8_t *key,
                                        const uint8_t *nonce, size_t nonce_len,
                                        const uint8_t *ciphertext, size_t len,
                                        const uint8_t *tag, size_t tag_len,
                                        uint8_t *plaintext);

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_CRYPTO_H__ */
