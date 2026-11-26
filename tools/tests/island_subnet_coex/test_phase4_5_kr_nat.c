/**
 * @file test_phase4_5_kr_nat.c
 * @brief TDD Test Suite for Phase 4 (TASK-09..10) & Phase 5a (TASK-11..12): KR Slots & NAT Manager.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "island/meshx_subnet_kr.h"
#include "island/meshx_nat_mgr.h"

extern size_t g_host_live_bytes;

static void test_kr_slots_and_phases(void)
{
    meshx_subnet_kr_mgr_t mgr;
    assert(meshx_subnet_kr_init(&mgr, true) == MESHX_SUCCESS);

    meshx_subnet_key_t k0_old, k1_old, k1_new;
    memset(k0_old.bytes, 0x10, sizeof(k0_old.bytes));
    memset(k1_old.bytes, 0x20, sizeof(k1_old.bytes));
    memset(k1_new.bytes, 0x22, sizeof(k1_new.bytes));

    assert(meshx_subnet_kr_slot_bind(&mgr, 0, 0xAAAA0001UL, &k0_old, 1, 0x100) == MESHX_SUCCESS);
    assert(meshx_subnet_kr_slot_bind(&mgr, 1, 0xBBBB0002UL, &k1_old, 1, 0x200) == MESHX_SUCCESS);

    meshx_subnet_crypto_slot_t *s1 = NULL;
    assert(meshx_subnet_kr_find_by_net_idx(&mgr, 1, &s1) == MESHX_SUCCESS && s1 != NULL);
    meshx_subnet_crypto_slot_t *s1_by_gid = NULL;
    assert(meshx_subnet_kr_find_by_global_id(&mgr, 0xBBBB0002UL, &s1_by_gid) == MESHX_SUCCESS && s1_by_gid == s1);

    /* Phase 0: 1 ingress key (old), egress = old */
    meshx_kr_active_keys_t in_keys;
    memset(&in_keys, 0, sizeof(in_keys));
    uint8_t tx_epoch = 99;
    assert(meshx_subnet_kr_get_ingress_keys(s1, &in_keys) == MESHX_SUCCESS);
    assert(in_keys.count == 1);
    const meshx_subnet_key_t *egress_k = NULL;
    assert(meshx_subnet_kr_select_egress_key(s1, &tx_epoch, &egress_k) == MESHX_SUCCESS);
    assert(egress_k != NULL && memcmp(egress_k->bytes, k1_old.bytes, 16) == 0);
    assert(tx_epoch == 0);

    /* Phase 1 (DIST): Dual-RX (old + new), egress latched on old (Seg 0 stability) */
    assert(meshx_subnet_kr_set_phase(&mgr, 1, MESHX_SUBNET_KR_PHASE_DIST, &k1_new, 2) == MESHX_SUCCESS);
    assert(meshx_subnet_kr_get_ingress_keys(s1, &in_keys) == MESHX_SUCCESS);
    assert(in_keys.count == 2);
    assert(meshx_subnet_kr_select_egress_key(s1, &tx_epoch, &egress_k) == MESHX_SUCCESS);
    assert(egress_k != NULL && memcmp(egress_k->bytes, k1_old.bytes, 16) == 0);
    assert(tx_epoch == 0);

    /* Phase 2 (SWITCH): Dual-RX (new + old), egress switches to new */
    assert(meshx_subnet_kr_set_phase(&mgr, 1, MESHX_SUBNET_KR_PHASE_SWITCH, &k1_new, 2) == MESHX_SUCCESS);
    assert(meshx_subnet_kr_get_ingress_keys(s1, &in_keys) == MESHX_SUCCESS);
    assert(in_keys.count == 2);
    assert(meshx_subnet_kr_select_egress_key(s1, &tx_epoch, &egress_k) == MESHX_SUCCESS);
    assert(egress_k != NULL && memcmp(egress_k->bytes, k1_new.bytes, 16) == 0);
    assert(tx_epoch == 1);

    /* Phase 3 (REVOKE): old wiped, 1 ingress key (new), slot 0 unaffected */
    assert(meshx_subnet_kr_set_phase(&mgr, 1, MESHX_SUBNET_KR_PHASE_REVOKE, NULL, 2) == MESHX_SUCCESS);
    assert(meshx_subnet_kr_get_ingress_keys(s1, &in_keys) == MESHX_SUCCESS);
    assert(in_keys.count == 1);
    assert(meshx_subnet_kr_select_egress_key(s1, &tx_epoch, &egress_k) == MESHX_SUCCESS);
    assert(egress_k != NULL && memcmp(egress_k->bytes, k1_new.bytes, 16) == 0);
    meshx_subnet_crypto_slot_t *s0_check = NULL;
    assert(meshx_subnet_kr_find_by_net_idx(&mgr, 0, &s0_check) == MESHX_SUCCESS && s0_check->kr_phase == MESHX_SUBNET_KR_PHASE_NORMAL);

    assert(meshx_subnet_kr_slot_unbind(&mgr, 0) == MESHX_SUCCESS);
    assert(meshx_subnet_kr_find_by_net_idx(&mgr, 0, &s0_check) == MESHX_NOT_FOUND && s0_check == NULL);
}

static void test_kr_quarantine_and_iv_guard(void)
{
    meshx_subnet_kr_mgr_t mgr;
    assert(meshx_subnet_kr_init(&mgr, true) == MESHX_SUCCESS);
    meshx_subnet_key_t key;
    memset(key.bytes, 1, sizeof(key.bytes));
    assert(meshx_subnet_kr_slot_bind(&mgr, 0, 0x1111UL, &key, 1, 0) == MESHX_SUCCESS);
    assert(meshx_subnet_kr_slot_bind(&mgr, 1, 0x2222UL, &key, 1, 0) == MESHX_SUCCESS);

    /* 0xC4, 0xC5, 0xC6 quarantined cross-subnet, allowed intra-subnet */
    bool quar = false;
    assert(meshx_subnet_kr_is_opcode_quarantined(&mgr, 0xC4, 0, 1, &quar) == MESHX_SUCCESS && quar == true);
    assert(meshx_subnet_kr_is_opcode_quarantined(&mgr, 0xC5, 0, 1, &quar) == MESHX_SUCCESS && quar == true);
    assert(meshx_subnet_kr_is_opcode_quarantined(&mgr, 0xC6, 0, 1, &quar) == MESHX_SUCCESS && quar == true);
    assert(meshx_subnet_kr_is_opcode_quarantined(&mgr, 0xC4, 0, 0, &quar) == MESHX_SUCCESS && quar == false);
    assert(meshx_subnet_kr_is_opcode_quarantined(&mgr, MESHX_UVP_OP_SUBNET_HEARTBEAT, 0, 1, &quar) == MESHX_SUCCESS && quar == false);

    /* Sequence counter triggers IV Update at >= 0x00F00000 */
    meshx_subnet_crypto_slot_t *s0 = NULL;
    assert(meshx_subnet_kr_find_by_net_idx(&mgr, 0, &s0) == MESHX_SUCCESS && s0 != NULL);
    s0->last_seq_num = 0x00EFFFFEUL;
    uint32_t seq = 0;
    bool iv_needed = false;
    assert(meshx_subnet_kr_alloc_seq(&mgr, 0, &seq, &iv_needed) == MESHX_SUCCESS);
    assert(seq == 0x00EFFFFFUL);
    assert(!iv_needed);
    assert(meshx_subnet_kr_alloc_seq(&mgr, 0, &seq, &iv_needed) == MESHX_SUCCESS);
    assert(seq == 0x00F00000UL);
    assert(iv_needed);
}

static void test_global_id_and_slots(void)
{
    meshx_subnet_key_t nk = { .bytes = {0xAA, 0xBB, 0xCC, 0xDD, 1, 2, 3, 4, 0, 0, 0, 0, 0, 0, 0, 0} };
    uint8_t salt = 99;
    uint32_t base_id = MESHX_GLOBAL_SUBNET_INVALID;
    assert(meshx_nat_derive_global_subnet_id(&nk, 0x02E5U, NULL, 0, &salt, &base_id) == MESHX_SUCCESS);
    assert(base_id != MESHX_GLOBAL_SUBNET_INVALID);
    assert(salt == 0);

    /* Collision triggers bounded remap loop with salt increment */
    uint32_t peers[1] = {base_id};
    uint32_t remapped_id = MESHX_GLOBAL_SUBNET_INVALID;
    assert(meshx_nat_derive_global_subnet_id(&nk, 0x02E5U, peers, 1, &salt, &remapped_id) == MESHX_SUCCESS);
    assert(remapped_id != MESHX_GLOBAL_SUBNET_INVALID);
    assert(remapped_id != base_id);
    assert(salt >= 1);

    meshx_nat_mgr_t mgr;
    assert(meshx_nat_mgr_init(&mgr, 0x0010U) == MESHX_SUCCESS);
    uint8_t s0 = 99, s1 = 99, s2 = 99;
    assert(meshx_nat_slot_assign(&mgr, base_id, 0, true, &s0) == MESHX_SUCCESS);
    assert(s0 == 0);
    assert(meshx_nat_slot_assign(&mgr, 0x22220001UL, 1, false, &s1) == MESHX_SUCCESS);
    assert(s1 == 1);
    /* Duplicate assign increments ref_count */
    assert(meshx_nat_slot_assign(&mgr, 0x22220001UL, 1, false, &s2) == MESHX_SUCCESS);
    assert(s2 == 1);

    uint32_t d1 = 0;
    assert(meshx_nat_recompute_slot_digest(&mgr, &d1) == MESHX_SUCCESS);
    assert(d1 != 0);

    assert(meshx_nat_slot_release(&mgr, 0x22220001UL) == MESHX_SUCCESS);
    uint8_t found_slot = 99;
    assert(meshx_nat_slot_get_by_global_id(&mgr, 0x22220001UL, &found_slot) == MESHX_SUCCESS && found_slot == 1); /* still ref_count=1 */
    assert(meshx_nat_slot_release(&mgr, 0x22220001UL) == MESHX_SUCCESS);
    assert(meshx_nat_slot_get_by_global_id(&mgr, 0x22220001UL, &found_slot) == MESHX_NOT_FOUND);

    assert(meshx_nat_mgr_deinit(&mgr) == MESHX_SUCCESS);
    assert(g_host_live_bytes == 0);
}

static void test_tier1_tier2_and_groups(void)
{
    meshx_nat_mgr_t mgr;
    assert(meshx_nat_mgr_init(&mgr, 0x0002U) == MESHX_SUCCESS);
    uint8_t slot0 = 0, slot2 = 0;
    assert(meshx_nat_slot_assign(&mgr, 0x10000001UL, 0, true, &slot0) == MESHX_SUCCESS);
    assert(meshx_nat_slot_assign(&mgr, 0x10000002UL, 2, false, &slot2) == MESHX_SUCCESS);
    /* Assign dummy slot 1 so next free slot becomes 2 if needed; verify slot2 directly */
    uint16_t v1 = 0;
    uint8_t rev_slot = 0;
    uint16_t rev_orig = 0;
    assert(meshx_nat_translate_tier1(2, 0x0003U, &v1) == MESHX_SUCCESS);
    assert(v1 == 0x4803U);
    assert(meshx_nat_reverse_tier1(v1, &rev_slot, &rev_orig) == MESHX_SUCCESS);
    assert(rev_slot == 2 && rev_orig == 0x0003U);
    assert(meshx_nat_translate_tier1(14, 0x0003U, &v1) == MESHX_INVALID_ARG);

    /* Tier-2 16-entry cache + LRU temp eviction + PERSISTENT full protection */
    meshx_subnet_uuid8_t uuid = { .bytes = {1,2,3,4,5,6,7,8} };
    uint16_t v2 = 0;
    for (uint16_t i = 0; i < MESHX_NAT_TIER2_CAP; i++) {
        uuid.bytes[7] = (uint8_t)i;
        uint8_t fl = (i == 0) ? MESHX_NAT_FLAG_TEMP_DISCOVERY : MESHX_NAT_FLAG_PERSISTENT;
        assert(meshx_nat_tier2_alloc(&mgr, &uuid, 0x10000002UL, (uint16_t)(0x0400U + i), fl, (uint16_t)(10 + i), &v2) == MESHX_SUCCESS);
    }
    /* 17th entry evicts temp entry 0 (min time 10) */
    uuid.bytes[7] = 0xAA;
    assert(meshx_nat_tier2_alloc(&mgr, &uuid, 0x10000002UL, 0x0555U, MESHX_NAT_FLAG_PERSISTENT, 50, &v2) == MESHX_SUCCESS);
    /* Now all 16 are PERSISTENT -> 18th fails with MESHX_SUBNET_ERR_NAT_FULL */
    uuid.bytes[7] = 0xBB;
    assert(meshx_nat_tier2_alloc(&mgr, &uuid, 0x10000002UL, 0x0556U, MESHX_NAT_FLAG_PERSISTENT, 60, &v2) == MESHX_SUBNET_ERR_NAT_FULL);

    /* 0xD5 sync delete frees 1 slot -> GC & subsequent alloc works */
    meshx_nat_entry_t del_e;
    memset(&del_e, 0, sizeof(del_e));
    del_e.global_subnet_src = 0x10000002UL;
    del_e.orig_addr = 0x0555U;
    assert(meshx_nat_tier2_apply_sync(&mgr, &del_e, 2) == MESHX_SUCCESS);
    uuid.bytes[7] = 0xCC;
    assert(meshx_nat_tier2_alloc(&mgr, &uuid, 0x10000002UL, 0x0600U, MESHX_NAT_FLAG_TEMP_DISCOVERY, 70, &v2) == MESHX_SUCCESS);
    uint8_t gc_evicted = 0;
    assert(meshx_nat_tier2_gc(&mgr, 80, &gc_evicted) == MESHX_SUCCESS && gc_evicted == 1); /* elapsed 80 - 70 = 10 > 5 min TTL */

    /* Group prefixing & selective mask */
    uint16_t pgrp = 0;
    assert(meshx_nat_prefix_group_addr(1, 0xC005U, &pgrp) == MESHX_SUCCESS);
    assert(pgrp == 0xC805U);
    assert(meshx_nat_unprefix_group_addr(pgrp, &rev_slot, &rev_orig) == MESHX_SUCCESS);
    assert(rev_slot == 1 && rev_orig == 0xC005U);
    bool g_allowed = true;
    assert(meshx_nat_is_group_forward_allowed(&mgr, 0xC005U, &g_allowed) == MESHX_SUCCESS && g_allowed == false);
    assert(meshx_nat_set_group_forward_mask(&mgr, 5, true) == MESHX_SUCCESS);
    assert(meshx_nat_is_group_forward_allowed(&mgr, 0xC005U, &g_allowed) == MESHX_SUCCESS && g_allowed == true);

    assert(meshx_nat_mgr_deinit(&mgr) == MESHX_SUCCESS);
    assert(g_host_live_bytes == 0);
}

int main(void)
{
    test_kr_slots_and_phases();
    test_kr_quarantine_and_iv_guard();
    test_global_id_and_slots();
    test_tier1_tier2_and_groups();
    printf("PASS: test_phase4_5_kr_nat\n");
    return 0;
}
