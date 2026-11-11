/**
 * @file host_platform_shim.c
 * @brief Host-native OS/Crypto/Allocation shim for Island Subnet Coexistence TDD tests.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include "meshx_err.h"

/* Allocation tracking for leak & zero-scrub verification */
size_t g_host_alloc_count = 0;
size_t g_host_free_count = 0;
size_t g_host_live_bytes = 0;
uint32_t g_host_now_ms = 1000;

typedef struct {
    size_t size;
    uint32_t magic;
} host_alloc_hdr_t;

meshx_err_t meshx_rtos_malloc(void **ptr, size_t size)
{
    if (!ptr || size == 0) return MESHX_INVALID_ARG;
    host_alloc_hdr_t *hdr = (host_alloc_hdr_t *)malloc(sizeof(host_alloc_hdr_t) + size);
    if (!hdr) return MESHX_NO_MEM;
    hdr->size = size;
    hdr->magic = 0x4D58414CUL;
    g_host_alloc_count++;
    g_host_live_bytes += size;
    *ptr = (void *)(hdr + 1);
    return MESHX_SUCCESS;
}

meshx_err_t meshx_rtos_calloc(void **ptr, size_t nmemb, size_t size)
{
    size_t total = nmemb * size;
    meshx_err_t err = meshx_rtos_malloc(ptr, total);
    if (err == MESHX_SUCCESS && *ptr) {
        memset(*ptr, 0, total);
    }
    return err;
}

meshx_err_t meshx_rtos_free(void **ptr)
{
    if (!ptr || !(*ptr)) return MESHX_SUCCESS;
    host_alloc_hdr_t *hdr = ((host_alloc_hdr_t *)(*ptr)) - 1;
    if (hdr->magic == 0x4D58414CUL) {
        g_host_free_count++;
        if (g_host_live_bytes >= hdr->size) {
            g_host_live_bytes -= hdr->size;
        }
        hdr->magic = 0;
    }
    free(hdr);
    *ptr = NULL;
    return MESHX_SUCCESS;
}

meshx_err_t meshx_rtos_get_sys_time(unsigned int *millis)
{
    if (!millis) return MESHX_INVALID_ARG;
    *millis = g_host_now_ms;
    return MESHX_SUCCESS;
}

size_t meshx_rtos_get_free_heap(void)
{
    return 65536U;
}

void meshx_log_write(int level, uint32_t module_id, const char *fmt, ...)
{
    (void)level; (void)module_id; (void)fmt;
}

/* Mock NVS backing store (in-memory KV map) */
#define MAX_KV 32
typedef struct {
    char key[32];
    uint8_t data[512];
    uint16_t len;
    bool used;
} host_kv_t;
static host_kv_t s_kv[MAX_KV];

void host_nvs_reset(void) { memset(s_kv, 0, sizeof(s_kv)); }

meshx_err_t meshx_nvs_get(char const *key, void *blob, uint16_t blob_size)
{
    if (!key || !blob) return MESHX_INVALID_ARG;
    for (int i = 0; i < MAX_KV; i++) {
        if (s_kv[i].used && strcmp(s_kv[i].key, key) == 0) {
            if (blob_size < s_kv[i].len) return MESHX_INVALID_ARG;
            memcpy(blob, s_kv[i].data, s_kv[i].len);
            return MESHX_SUCCESS;
        }
    }
    return MESHX_NOT_FOUND;
}

meshx_err_t meshx_nvs_set(char const *key, void const *blob, uint16_t blob_size, bool arm_timer)
{
    (void)arm_timer;
    if (!key || !blob || blob_size > sizeof(s_kv[0].data)) return MESHX_INVALID_ARG;
    for (int i = 0; i < MAX_KV; i++) {
        if (s_kv[i].used && strcmp(s_kv[i].key, key) == 0) {
            memcpy(s_kv[i].data, blob, blob_size);
            s_kv[i].len = blob_size;
            return MESHX_SUCCESS;
        }
    }
    for (int i = 0; i < MAX_KV; i++) {
        if (!s_kv[i].used) {
            s_kv[i].used = true;
            snprintf(s_kv[i].key, sizeof(s_kv[i].key), "%s", key);
            memcpy(s_kv[i].data, blob, blob_size);
            s_kv[i].len = blob_size;
            return MESHX_SUCCESS;
        }
    }
    return MESHX_NO_MEM;
}

meshx_err_t meshx_nvs_commit(void) { return MESHX_SUCCESS; }

/* FIPS 180-4 SHA-256 + RFC 2104 HMAC-SHA256 for genuine cryptographic host testing */
static uint32_t rotr32(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }

static void sha256_transform(uint32_t state[8], const uint8_t block[64])
{
    static const uint32_t k[64] = {
        0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
        0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
        0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
        0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
        0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
        0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
        0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
        0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U
    };
    uint32_t w[64], a, b, c, d, e, f, g, h;
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i*4] << 24) | ((uint32_t)block[i*4+1] << 16) |
               ((uint32_t)block[i*4+2] << 8) | (uint32_t)block[i*4+3];
    }
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = rotr32(w[i-15], 7) ^ rotr32(w[i-15], 18) ^ (w[i-15] >> 3);
        uint32_t s1 = rotr32(w[i-2], 17) ^ rotr32(w[i-2], 19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    a = state[0]; b = state[1]; c = state[2]; d = state[3];
    e = state[4]; f = state[5]; g = state[6]; h = state[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = h + S1 + ch + k[i] + w[i];
        uint32_t S0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + maj;
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

static void sha256_oneshot(const uint8_t *d1, size_t l1, const uint8_t *d2, size_t l2, uint8_t out[32])
{
    uint32_t st[8] = {
        0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,
        0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U
    };
    uint8_t blk[64];
    size_t idx = 0;
    size_t total = l1 + l2;
    for (size_t i = 0; i < l1; i++) {
        blk[idx++] = d1[i];
        if (idx == 64) { sha256_transform(st, blk); idx = 0; }
    }
    for (size_t i = 0; i < l2; i++) {
        blk[idx++] = d2[i];
        if (idx == 64) { sha256_transform(st, blk); idx = 0; }
    }
    blk[idx++] = 0x80;
    if (idx > 56) {
        while (idx < 64) blk[idx++] = 0;
        sha256_transform(st, blk);
        idx = 0;
    }
    while (idx < 56) blk[idx++] = 0;
    uint64_t bits = (uint64_t)total * 8ULL;
    for (int i = 0; i < 8; i++) blk[56 + i] = (uint8_t)(bits >> (56 - 8*i));
    sha256_transform(st, blk);
    for (int i = 0; i < 8; i++) {
        out[i*4]   = (uint8_t)(st[i] >> 24);
        out[i*4+1] = (uint8_t)(st[i] >> 16);
        out[i*4+2] = (uint8_t)(st[i] >> 8);
        out[i*4+3] = (uint8_t)(st[i]);
    }
}

meshx_err_t meshx_crypto_hmac_sha256(const uint8_t *key, size_t key_len,
                                     const uint8_t *data, size_t data_len,
                                     uint8_t *out_mac)
{
    if (!key || !data || !out_mac) return MESHX_INVALID_ARG;
    uint8_t k0[64];
    memset(k0, 0, sizeof(k0));
    if (key_len > 64) {
        sha256_oneshot(key, key_len, NULL, 0, k0);
    } else {
        memcpy(k0, key, key_len);
    }
    uint8_t ipad[64], opad[64], inner[32];
    for (int i = 0; i < 64; i++) {
        ipad[i] = k0[i] ^ 0x36U;
        opad[i] = k0[i] ^ 0x5CU;
    }
    sha256_oneshot(ipad, 64, data, data_len, inner);
    sha256_oneshot(opad, 64, inner, 32, out_mac);
    return MESHX_SUCCESS;
}

/* Host AES-CCM test shim: keyed stream XOR + HMAC-derived MIC tag (verifies key, nonce, AAD/len & tamper resistance) */
meshx_err_t meshx_crypto_aes_ccm_encrypt(const uint8_t *key,
                                         const uint8_t *nonce, size_t nonce_len,
                                         const uint8_t *plaintext, size_t len,
                                         uint8_t *ciphertext,
                                         uint8_t *tag, size_t tag_len)
{
    if (!key || !nonce || !plaintext || !ciphertext || !tag || tag_len > 16 || nonce_len > 16) {
        return MESHX_INVALID_ARG;
    }
    uint8_t stream_blk[32], ctr_in[20];
    memcpy(ctr_in, nonce, nonce_len);
    for (size_t i = 0; i < len; i++) {
        if ((i % 32) == 0) {
            ctr_in[nonce_len] = (uint8_t)(i >> 8);
            ctr_in[nonce_len + 1] = (uint8_t)(i & 0xFF);
            meshx_crypto_hmac_sha256(key, 16, ctr_in, nonce_len + 2, stream_blk);
        }
        ciphertext[i] = plaintext[i] ^ stream_blk[i % 32];
    }
    uint8_t mac_in[256];
    if (nonce_len + len > sizeof(mac_in)) return MESHX_INVALID_ARG;
    memcpy(mac_in, nonce, nonce_len);
    memcpy(mac_in + nonce_len, plaintext, len);
    uint8_t full_tag[32];
    meshx_crypto_hmac_sha256(key, 16, mac_in, nonce_len + len, full_tag);
    memcpy(tag, full_tag, tag_len);
    return MESHX_SUCCESS;
}

meshx_err_t meshx_crypto_aes_ccm_decrypt(const uint8_t *key,
                                         const uint8_t *nonce, size_t nonce_len,
                                         const uint8_t *ciphertext, size_t len,
                                         const uint8_t *tag, size_t tag_len,
                                         uint8_t *plaintext)
{
    if (!key || !nonce || !ciphertext || !tag || !plaintext || tag_len > 16 || nonce_len > 16) {
        return MESHX_INVALID_ARG;
    }
    uint8_t stream_blk[32], ctr_in[20];
    memcpy(ctr_in, nonce, nonce_len);
    for (size_t i = 0; i < len; i++) {
        if ((i % 32) == 0) {
            ctr_in[nonce_len] = (uint8_t)(i >> 8);
            ctr_in[nonce_len + 1] = (uint8_t)(i & 0xFF);
            meshx_crypto_hmac_sha256(key, 16, ctr_in, nonce_len + 2, stream_blk);
        }
        plaintext[i] = ciphertext[i] ^ stream_blk[i % 32];
    }
    uint8_t mac_in[256];
    if (nonce_len + len > sizeof(mac_in)) return MESHX_INVALID_ARG;
    memcpy(mac_in, nonce, nonce_len);
    memcpy(mac_in + nonce_len, plaintext, len);
    uint8_t full_tag[32];
    meshx_crypto_hmac_sha256(key, 16, mac_in, nonce_len + len, full_tag);
    uint8_t diff = 0;
    for (size_t i = 0; i < tag_len; i++) {
        diff |= (uint8_t)(full_tag[i] ^ tag[i]);
    }
    if (diff != 0) {
        memset(plaintext, 0, len);
        return MESHX_FAIL;
    }
    return MESHX_SUCCESS;
}

meshx_err_t meshx_crypto_random_fill(uint8_t *buf, size_t len)
{
    static uint32_t prng = 0x12345678UL;
    for (size_t i = 0; i < len; i++) {
        prng = prng * 1664525UL + 1013904223UL;
        buf[i] = (uint8_t)(prng >> 24);
    }
    return MESHX_SUCCESS;
}

