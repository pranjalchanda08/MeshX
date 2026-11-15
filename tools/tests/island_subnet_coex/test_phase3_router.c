/**
 * @file test_phase3_router.c
 * @brief TDD Test Suite for Phase 3 (TASK-06..08): Store-and-Forward Queue, Reassembly, Canonical Nonce, Stage 2 NACK.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "island/meshx_router_core.h"

extern size_t g_host_alloc_count;
extern size_t g_host_free_count;
extern size_t g_host_live_bytes;

typedef struct {
    int tx_calls;
    int nack_calls;
    uint16_t last_tx_net_idx;
    uint16_t last_nack_dst;
    uint8_t last_nonce[13];
    uint8_t last_nack_buf[64];
    uint8_t last_nack_len;
    bool next_tx_fail;
} test_router_spy_t;

static meshx_err_t spy_tx(const meshx_router_queue_entry_t *entry,
                          const meshx_ccm_nonce_t *nonce,
                          const meshx_subnet_key_t *tx_net_key,
                          void *user_ctx)
{
    (void)tx_net_key;
    test_router_spy_t *spy = (test_router_spy_t *)user_ctx;
    spy->tx_calls++;
    spy->last_tx_net_idx = entry->tx_net_idx;
    if (nonce)
    {
        memcpy(spy->last_nonce, nonce->bytes, sizeof(spy->last_nonce));
    }
    return spy->next_tx_fail ? MESHX_FAIL : MESHX_SUCCESS;
}

static meshx_err_t spy_nack(uint16_t net_idx, uint16_t dst_addr,
                            const uint8_t *nack_buf, uint8_t nack_len, void *user_ctx)
{
    (void)net_idx;
    test_router_spy_t *spy = (test_router_spy_t *)user_ctx;
    spy->nack_calls++;
    spy->last_nack_dst = dst_addr;
    if (nack_len <= sizeof(spy->last_nack_buf)) {
        memcpy(spy->last_nack_buf, nack_buf, nack_len);
        spy->last_nack_len = nack_len;
    }
    return MESHX_SUCCESS;
}

static void test_queue_limits_and_backpressure(void)
{
    meshx_router_queue_t q;
    meshx_router_queue_init(&q);

    uint8_t pdu[120];
    memset(pdu, 0xA5, sizeof(pdu));
    meshx_router_enqueue_params_t p = {
        .ingress_boundary_id = 0x11111111UL,
        .egress_boundary_id  = 0x22222222UL,
        .seq_auth            = 0x000010UL,
        .now_ms              = 1000,
        .canonical_src       = 0x0003,
        .canonical_dst       = 0x0005,
        .orig_src_addr       = 0x0003,
        .orig_dst_addr       = 0x4405,
        .ingress_src_addr    = 0x0003,
        .tx_net_idx          = 1,
        .epoch_id            = 1,
        .ttl                 = 5,
        .trans_mic           = { .bytes = {1,2,3,4,5,6,7,8}, .len = 4 },
        .tx_epoch            = 0,
        .flags               = MESHX_ROUTER_FLAG_IS_UNICAST,
        .pdu                 = pdu,
        .pdu_len             = 109 /* > 108B */
    };

    bool should_nack = false;
    assert(meshx_router_queue_push(&q, &p, &should_nack) == MESHX_SUBNET_ERR_MTU);

    /* Multihop limit: > 85B must fail */
    p.flags |= MESHX_ROUTER_FLAG_MULTIHOP;
    p.pdu_len = 86;
    assert(meshx_router_queue_push(&q, &p, &should_nack) == MESHX_SUBNET_ERR_MTU);

    /* Enqueue 4 valid flows (max capacity = 4) */
    p.flags = MESHX_ROUTER_FLAG_IS_UNICAST;
    p.pdu_len = 32;
    size_t alloc_before = g_host_alloc_count;
    for (int i = 0; i < 4; i++) {
        p.seq_auth = (uint32_t)(100 + i);
        should_nack = false;
        assert(meshx_router_queue_push(&q, &p, &should_nack) == MESHX_SUCCESS);
        assert(!should_nack);
    }
    assert(q.count == 4);
    assert((g_host_alloc_count - alloc_before) == 4);

    /* 5th push triggers pre-ACK Drop-Newest using ZERO additional heap allocation */
    size_t alloc_at_full = g_host_alloc_count;
    p.seq_auth = 999;
    meshx_err_t full_err = meshx_router_queue_push(&q, &p, &should_nack);
    assert(full_err == MESHX_NO_MEM);
    assert(should_nack);
    assert(g_host_alloc_count == alloc_at_full);
    assert(q.dropped_newest_cnt == 1);

    /* Build emergency NACK into 64B static buffer (zero heap) */
    const uint8_t *nack_ptr = NULL;
    uint8_t nack_len = 0;
    assert(meshx_router_queue_build_emergency_nack(&q, 77, &p, MESHX_NACK_REASON_QUEUE_FULL, &nack_ptr, &nack_len) == MESHX_SUCCESS);
    assert(nack_ptr == q.emergency_nack_buf);
    assert(nack_len == sizeof(meshx_uvp_router_nack_t));
    assert(g_host_alloc_count == alloc_at_full);

    /* Purge by boundary ID clears all 4 entries and frees heap cleanly */
    uint8_t purged = meshx_router_queue_purge_boundary(&q, 0x22222222UL);
    assert(purged == 4);
    assert(q.count == 0);
    assert(g_host_live_bytes == 0);
    meshx_router_queue_deinit(&q);
}

static void test_canonical_nonce_and_reasm_epoch(void)
{
    meshx_ccm_nonce_t n1, n2;
    memset(&n1, 0, sizeof(n1));
    memset(&n2, 0, sizeof(n2));
    /* Must preserve canonical src/dst (e.g., before NAT rewriting) for both Unicast and Group */
    assert(meshx_router_build_canonical_nonce(0x0002, 0xC005, 0x00000001UL, 0x000042UL, 1, true, &n1) == MESHX_SUCCESS);
    assert(meshx_router_build_canonical_nonce(0x0002, 0xC005, 0x00000001UL, 0x000042UL, 1, true, &n2) == MESHX_SUCCESS);
    assert(memcmp(n1.bytes, n2.bytes, sizeof(n1.bytes)) == 0);
    assert(n1.bytes[0] == 0x01); /* App Nonce type = 0x01 */
    assert(n1.bytes[1] == 0x80); /* SZMIC = 1 bit 7 */

    /* Group / Unicast DST bytes big-endian at [7..8] */
    assert(n1.bytes[5] == 0x00 && n1.bytes[6] == 0x02);
    assert(n1.bytes[7] == 0xC0 && n1.bytes[8] == 0x05);

    /* Single-Epoch reassembly rejects mismatched epoch on same SAR flow */
    meshx_router_ctx_t ctx;
    test_router_spy_t spy = {0};
    meshx_router_core_init(&ctx, spy_tx, spy_nack, &spy);

    uint8_t s0[12] = {1,2,3,4,5,6,7,8,9,10,11,12};
    uint8_t s1[12] = {13,14,15,16,17,18,19,20,21,22,23,24};
    bool done = false;
    assert(meshx_router_reasm_segment(&ctx, 0, 0x0002, 0x4403, 1, 0x100, 0, 1, s0, 12, &done) == MESHX_SUCCESS);
    assert(!done);

    /* Epoch change mid-SAR must reset cleanly rather than splicing mixed-epoch segments */
    assert(meshx_router_reasm_segment(&ctx, 0, 0x0002, 0x4403, 2, 0x100, 1, 1, s1, 12, &done) == MESHX_INVALID_STATE);
    assert(!done);

    /* Fresh epoch 2 segment 0 + segment 1 completes properly */
    assert(meshx_router_reasm_segment(&ctx, 0, 0x0002, 0x4403, 2, 0x100, 0, 1, s0, 12, &done) == MESHX_SUCCESS);
    assert(!done);
    assert(meshx_router_reasm_segment(&ctx, 0, 0x0002, 0x4403, 2, 0x100, 1, 1, s1, 12, &done) == MESHX_SUCCESS);
    assert(done);
    assert(ctx.reasm.len == 24);
    meshx_router_core_deinit(&ctx);
}

static void test_stage2_retries_and_reliable_nack(void)
{
    meshx_router_ctx_t ctx;
    test_router_spy_t spy = {0};
    meshx_router_core_init(&ctx, spy_tx, spy_nack, &spy);

    uint8_t payload[16] = {0x11, 0x22, 0x33};
    meshx_router_enqueue_params_t p = {
        .ingress_boundary_id = 0x1111,
        .egress_boundary_id  = 0x2222,
        .seq_auth            = 0x555,
        .now_ms              = 1000,
        .canonical_src       = 0x0002,
        .canonical_dst       = 0x0003,
        .orig_src_addr       = 0x0002,
        .orig_dst_addr       = 0x4403,
        .ingress_src_addr    = 0x0002,
        .tx_net_idx          = 1,
        .epoch_id            = 1,
        .ttl                 = 4,
        .trans_mic           = { .bytes = {0xAA, 0xBB, 0xCC, 0xDD}, .len = 4 },
        .tx_epoch            = 0,
        .flags               = MESHX_ROUTER_FLAG_IS_UNICAST,
        .pdu                 = payload,
        .pdu_len             = sizeof(payload)
    };
    bool should_nack = false;
    assert(meshx_router_queue_push(&ctx.queue, &p, &should_nack) == MESHX_SUCCESS);

    /* Pump initial TX + fail 3 attempts -> triggers reliable NACK back to ingress_src_addr */
    assert(meshx_router_stage2_pump(&ctx, 1000) == MESHX_SUCCESS);
    assert(spy.tx_calls == 1);
    assert(meshx_router_stage2_on_tx_complete(&ctx, false, 1100) == MESHX_SUCCESS); /* retry 1 */
    assert(meshx_router_stage2_on_tx_complete(&ctx, false, 1200) == MESHX_SUCCESS); /* retry 2 */
    assert(meshx_router_stage2_on_tx_complete(&ctx, false, 1300) == MESHX_SUCCESS); /* retry 3 exhausted -> NACK! */
    assert(ctx.queue.count == 0);
    assert(spy.nack_calls == 1);
    assert(spy.last_nack_dst == 0x0002);
    assert(ctx.pending_nack.active);

    /* Pump after 3000ms retransmits reliable NACK */
    assert(meshx_router_stage2_pump(&ctx, 4400) == MESHX_SUCCESS);
    assert(spy.nack_calls == 2);

    /* 0xD9 NACK-ACK cancels further NACK retransmissions */
    uint32_t nack_id = ctx.pending_nack.nack_id;
    assert(meshx_router_on_nack_ack(&ctx, nack_id) == MESHX_SUCCESS);
    assert(!ctx.pending_nack.active);

    meshx_router_core_deinit(&ctx);
    assert(g_host_live_bytes == 0);
}

int main(void)
{
    test_queue_limits_and_backpressure();
    test_canonical_nonce_and_reasm_epoch();
    test_stage2_retries_and_reliable_nack();
    printf("PASS: test_phase3_router\n");
    return 0;
}
