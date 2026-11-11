/**
 * @file test_phase2_5_6_sec_bridge.c
 * @brief TDD Test Suite for NVS, MitM Security, Discovery, Redundancy, Loop Prevention & Facade.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "island_subnet_coex/meshx_subnet_nvs.h"
#include "island_subnet_coex/meshx_subnet_security.h"
#include "island_subnet_coex/meshx_subnet_discovery.h"
#include "island_subnet_coex/meshx_subnet_bridge.h"
#include "island_subnet_coex/meshx_island_subnet_coex.h"

extern size_t g_host_live_bytes;
extern void host_nvs_reset(void);

static uint32_t s_saved_ckpt = 0;
static int s_ckpt_calls = 0;
static meshx_err_t test_ckpt_cb(uint32_t ckpt, void *ctx)
{
    (void)ctx;
    s_saved_ckpt = ckpt;
    s_ckpt_calls++;
    return MESHX_SUCCESS;
}

static void test_nvs_persistence_under_1kb(void)
{
    host_nvs_reset();
    size_t total_blob = sizeof(meshx_nvs_kbridge_blob_t) +
                        sizeof(meshx_nvs_nonce_blob_t) +
                        sizeof(meshx_nvs_nat_blob_t) +
                        sizeof(meshx_nvs_pairs_blob_t);
    assert(total_blob < 1024U);

    meshx_nvs_kbridge_blob_t kb = {.cluster_epoch = 7, .active_term = 3, .valid = 1};
    memset(kb.k_bridge, 0x5A, 16);
    assert(meshx_subnet_nvs_save_kbridge(&kb) == MESHX_SUCCESS);

    meshx_nvs_kbridge_blob_t kb_rd = {0};
    assert(meshx_subnet_nvs_load_kbridge(&kb_rd) == MESHX_SUCCESS);
    assert(memcmp(&kb, &kb_rd, sizeof(kb)) == 0);

    assert(meshx_subnet_nvs_save_nonce(4096UL) == MESHX_SUCCESS);
    uint32_t ckpt = 0;
    assert(meshx_subnet_nvs_load_nonce(&ckpt) == MESHX_SUCCESS);
    assert(ckpt == 4096UL);

    meshx_nat_entry_t nat[2] = {0};
    nat[0].global_subnet_src = 0x11112222UL;
    nat[0].orig_addr = 0x0501;
    nat[0].virtual_addr = 0x7800;
    nat[0].flags = MESHX_NAT_FLAG_VALID | MESHX_NAT_FLAG_PERSISTENT;
    assert(meshx_subnet_nvs_save_nat_table(nat, 1) == MESHX_SUCCESS);

    meshx_nat_entry_t nat_rd[MESHX_NAT_TIER2_CAP] = {0};
    uint8_t nat_cnt = 0;
    assert(meshx_subnet_nvs_load_nat_table(nat_rd, &nat_cnt) == MESHX_SUCCESS);
    assert(nat_cnt == 1 && nat_rd[0].orig_addr == 0x0501);

    meshx_pair_record_t pr = {
        .global_subnet_src = 0x1111, .global_subnet_dst = 0x2222,
        .local_elem_addr = 0x0002, .virtual_remote_addr = 0x4403,
        .remote_phys_addr = 0x0003, .func_id = 0x1001, .active = 1
    };
    assert(meshx_subnet_nvs_save_pair_records(&pr, 1) == MESHX_SUCCESS);
    meshx_pair_record_t pr_rd[MESHX_PAIR_RECORD_MAX] = {0};
    uint8_t pr_cnt = 0;
    assert(meshx_subnet_nvs_load_pair_records(pr_rd, &pr_cnt) == MESHX_SUCCESS);
    assert(pr_cnt == 1 && pr_rd[0].func_id == 0x1001);
}

static void test_security_hmac_replay_devkey_rpl(void)
{
    uint8_t dev_key[16], k_bridge[16];
    memset(dev_key, 0x33, 16);
    memset(k_bridge, 0x77, 16);

    /* DevKey AES-128-CCM seal + open (0xD4) */
    meshx_uvp_bridge_invite_plain_t inv = {0};
    inv.cluster_epoch = 4;
    inv.assigned_slot_n = 2;
    inv.boundary_id = 0xABCDEF01UL;
    uint8_t sealed[128], opened[128];
    uint16_t sealed_len = 0, opened_len = 0;
    assert(meshx_subnet_sec_seal_devkey(dev_key, 0xD4, 0x0005, 42, (const uint8_t *)&inv, sizeof(inv), sealed, sizeof(sealed), &sealed_len) == MESHX_SUCCESS);
    assert(meshx_subnet_sec_open_devkey(dev_key, 0xD4, sealed, sealed_len, opened, sizeof(opened), &opened_len) == MESHX_SUCCESS);
    assert(opened_len == sizeof(inv) && memcmp(&inv, opened, sizeof(inv)) == 0);
    /* Tamper ciphertext/MIC -> fail */
    sealed[sealed_len - 1] ^= 0xFF;
    assert(meshx_subnet_sec_open_devkey(dev_key, 0xD4, sealed, sealed_len, opened, sizeof(opened), &opened_len) != MESHX_SUCCESS);

    /* Sign & Verify + Boot +10000 margin + Checkpoints + Sliding Anti-Replay */
    s_ckpt_calls = 0;
    meshx_subnet_security_ctx_t tx_sec, rx_sec;
    meshx_subnet_sec_init(&tx_sec, 2000UL, test_ckpt_cb, NULL);
    meshx_subnet_sec_init(&rx_sec, 0UL, NULL, NULL);
    assert(tx_sec.tx_bridge_nonce == 12000UL); /* 2000 + 10000 margin */

    /* Without K_bridge -> fail */
    uint8_t frame[128];
    uint16_t flen = 0;
    uint8_t hb[8] = {1,2,3,4,5,6,7,8};
    assert(meshx_subnet_sec_sign_frame(&tx_sec, 0xD1, 0x0005, hb, 8, frame, sizeof(frame), &flen) == MESHX_SUBNET_ERR_AUTH);

    assert(meshx_subnet_sec_set_k_bridge(&tx_sec, k_bridge, 1, 2) == MESHX_SUCCESS);
    assert(meshx_subnet_sec_set_k_bridge(&rx_sec, k_bridge, 1, 2) == MESHX_SUCCESS);

    assert(meshx_subnet_sec_sign_frame(&tx_sec, 0xD1, 0x0005, hb, 8, frame, sizeof(frame), &flen) == MESHX_SUCCESS);
    assert(flen == (MESHX_CLUSTER_AUTH_OVERHEAD + 8U));

    uint8_t op = 0;
    uint32_t term = 0;
    uint16_t sender = 0;
    const uint8_t *pl = NULL;
    uint16_t pl_len = 0;
    assert(meshx_subnet_sec_verify_frame(&rx_sec, frame, flen, &op, &term, &sender, &pl, &pl_len) == MESHX_SUCCESS);
    assert(op == 0xD1 && term == 2 && sender == 0x0005 && pl_len == 8);

    /* Replay exact same frame -> rejected */
    assert(meshx_subnet_sec_verify_frame(&rx_sec, frame, flen, &op, &term, &sender, &pl, &pl_len) == MESHX_SUBNET_ERR_REPLAY);

    /* Stale term rejected */
    tx_sec.active_term = 1;
    assert(meshx_subnet_sec_sign_frame(&tx_sec, 0xD1, 0x0005, hb, 8, frame, sizeof(frame), &flen) == MESHX_SUCCESS);
    assert(meshx_subnet_sec_verify_frame(&rx_sec, frame, flen, &op, &term, &sender, &pl, &pl_len) == MESHX_SUBNET_ERR_AUTH);

    /* Restore term & verify checkpoint fires after 1024 increments */
    tx_sec.active_term = 2;
    for (int i = 0; i < 1030; i++) {
        assert(meshx_subnet_sec_sign_frame(&tx_sec, 0xD1, 0x0005, hb, 4, frame, sizeof(frame), &flen) == MESHX_SUCCESS);
    }
    assert(s_ckpt_calls >= 1);

    /* Nonce wrap guard >= 0xFFFF0000 */
    tx_sec.tx_bridge_nonce = MESHX_BRIDGE_NONCE_WRAP_LIMIT;
    assert(meshx_subnet_sec_sign_frame(&tx_sec, 0xD1, 0x0005, hb, 4, frame, sizeof(frame), &flen) != MESHX_SUCCESS);

    /* Whitelist + RPL */
    assert(meshx_subnet_sec_whitelist_add(&rx_sec, 0x0009, 0x33330001UL) == MESHX_SUCCESS);
    assert(meshx_subnet_sec_whitelist_check(&rx_sec, 0x0009, 0x33330001UL) == true);
    assert(meshx_subnet_sec_whitelist_check(&rx_sec, 0x0009, 0x99999999UL) == false);

    assert(meshx_subnet_sec_rpl_check_update(&rx_sec, 1, 0x0003, 1, 0, 10) == true);
    assert(meshx_subnet_sec_rpl_check_update(&rx_sec, 1, 0x0003, 1, 0, 10) == false);
    assert(meshx_subnet_sec_rpl_check_update(&rx_sec, 1, 0x0003, 2, 0, 1) == true); /* new epoch allows seq=1 */

    assert(meshx_subnet_sec_canonical_rpl_check(&rx_sec, 0x33330001UL, 0x0003, 0, 55) == true);
    assert(meshx_subnet_sec_canonical_rpl_check(&rx_sec, 0x33330001UL, 0x0003, 0, 55) == false);

    meshx_subnet_sec_clear_k_bridge(&tx_sec);
    assert(!tx_sec.k_bridge_valid);
    meshx_subnet_sec_deinit(&tx_sec);
    meshx_subnet_sec_deinit(&rx_sec);
}

static void test_discovery_and_bridge_fsm(void)
{
    /* Discovery 30s 3-MIC detector + Witness eval + Pair commit */
    meshx_subnet_discovery_t disc;
    meshx_subnet_disc_init(&disc);
    assert(meshx_subnet_disc_record_mic_fail(&disc, 1000) == false);
    assert(meshx_subnet_disc_record_mic_fail(&disc, 15000) == false);
    assert(meshx_subnet_disc_record_mic_fail(&disc, 40000) == false); /* 1000 expired (>30s) */
    assert(meshx_subnet_disc_record_mic_fail(&disc, 42000) == true);  /* 15k, 40k, 42k within 30s! */

    assert(meshx_subnet_disc_eval_candidate(&disc, -80, true, 8192) == true);
    assert(meshx_subnet_disc_eval_candidate(&disc, -85, true, 8192) == false);
    assert(meshx_subnet_disc_eval_candidate(&disc, -70, false, 8192) == false);
    assert(meshx_subnet_disc_eval_candidate(&disc, -70, true, 2048) == false);

    meshx_nat_mgr_t nat;
    meshx_nat_mgr_init(&nat, 0x0001);
    uint8_t s0 = 0, s1 = 0;
    assert(meshx_nat_slot_assign(&nat, 0x1111, 0, true, &s0) == MESHX_SUCCESS);
    assert(meshx_nat_slot_assign(&nat, 0x2222, 1, false, &s1) == MESHX_SUCCESS);

    meshx_uvp_discover_rsp_t rsp = {
        .global_subnet_src = 0x2222, .global_subnet_dst = 0x1111,
        .requestor_addr = 0x0002, .trans_id = 9,
        .remote_phys_addr = 0x0500, .func_id = 0x1234, .role = 1, .elem_count = 1
    };
    uint16_t vnat = 0;
    assert(meshx_subnet_disc_handle_rsp(&disc, &nat, &rsp, 10, 5000, &vnat) == MESHX_SUCCESS);
    assert(vnat >= MESHX_NAT_TIER2_START && vnat <= MESHX_NAT_TIER2_END);

    meshx_pair_record_t rec = {
        .global_subnet_src = 0x2222, .global_subnet_dst = 0x1111,
        .local_elem_addr = 0x0002, .virtual_remote_addr = vnat,
        .remote_phys_addr = 0x0500, .func_id = 0x1234, .role_flags = 1, .active = 1
    };
    assert(meshx_subnet_disc_commit_pair(&disc, &nat, &rec) == MESHX_SUCCESS);
    assert(disc.pair_count == 1);
    assert(meshx_subnet_disc_remove_pair(&disc, 0x0002, vnat) == MESHX_SUCCESS);
    assert(disc.pair_count == 0);
    meshx_nat_mgr_deinit(&nat);
    meshx_subnet_disc_deinit(&disc);

    /* Redundancy Failover + Split-Brain + 23B Transit Loop Filter */
    meshx_subnet_bridge_mgr_t bm;
    meshx_router_queue_t q;
    meshx_router_queue_init(&q);
    meshx_subnet_bridge_init(&bm, 0x0008, true, 50, 0);

    assert(meshx_subnet_bridge_add_boundary(&bm, 0xB001, MESHX_BRIDGE_ROLE_STANDBY, 0x0005, 1, 1, 0) == MESHX_SUCCESS);
    uint32_t hb_id = 0, claim_id = 0;
    /* Within 6s grace -> no claim */
    assert(meshx_subnet_bridge_tick(&bm, 5000, 16384, &hb_id, &claim_id) == MESHX_SUCCESS);
    assert(claim_id == 0);
    /* After 12s watchdog + 1*400ms rank backoff -> claim triggers and promotes to ACTIVE */
    assert(meshx_subnet_bridge_tick(&bm, 12500, 16384, &hb_id, &claim_id) == MESHX_SUCCESS);
    assert(claim_id == 0xB001 && bm.boundaries[0].role == MESHX_BRIDGE_ROLE_ACTIVE);

    /* Push 1 flow on boundary 0xB001, then receive higher-term demote -> purges queue + standby */
    uint8_t dummy[8] = {1,2,3,4,5,6,7,8};
    meshx_router_enqueue_params_t ep = {
        .ingress_boundary_id = 0xB001, .egress_boundary_id = 0x9999,
        .seq_auth = 1, .now_ms = 12600, .canonical_src = 2, .canonical_dst = 3,
        .flags = MESHX_ROUTER_FLAG_IS_UNICAST, .pdu = dummy, .pdu_len = 8
    };
    bool nack = false;
    assert(meshx_router_queue_push(&q, &ep, &nack) == MESHX_SUCCESS);
    assert(meshx_subnet_bridge_on_demote(&bm, 0xB001, 5, 0x0004, 0xFFFFU, 12700, &q) == MESHX_SUCCESS);
    assert(bm.boundaries[0].role == MESHX_BRIDGE_ROLE_STANDBY);
    assert(q.count == 0);

    /* 23B Transit visited_subnets_mask + hop_limit + 5.0s echo filter */
    meshx_subnet_transit_hdr_t th = {
        .magic_ver = MESHX_TRANSIT_MAGIC_VER, .hop_limit = 3, .payload_len = 16,
        .visited_subnets_mask = 0x00000001UL, .global_subnet_src = 0x1111, .global_subnet_dst = 0x3333,
        .orig_src_addr = 0x0002, .orig_dst_addr = 0x0003, .tx_tag_crc16 = 0xCAFE
    };
    assert(meshx_subnet_bridge_transit_check_and_mark(&bm, &th, 1, 1000) == true);
    /* Immediate echo replay on another path within 5s -> suppressed */
    assert(meshx_subnet_bridge_transit_check_and_mark(&bm, &th, 2, 3000) == false);
    /* Visited bit already set -> suppressed */
    assert(meshx_subnet_bridge_transit_check_and_mark(&bm, &th, 1, 7000) == false);
    /* New tag after 5s passes */
    meshx_subnet_transit_hdr_t th2 = th;
    th2.visited_subnets_mask = 0;
    th2.hop_limit = 2;
    assert(meshx_subnet_bridge_transit_check_and_mark(&bm, &th2, 2, 7000) == true);

    /* Facade & MXCP C API sanity check */
    assert(meshx_island_subnet_coex_init() == MESHX_SUCCESS);
    uint8_t kbr[16] = {0x99};
    assert(meshx_subnet_coex_cluster_key_update(kbr, 10, 4) == MESHX_SUCCESS);
    assert(meshx_subnet_coex_is_kr_quarantined() == true);
    uint8_t nk1[16] = {0x11}, nk2[16] = {0x22};
    assert(meshx_subnet_coex_bridge_add(0, nk1, 0x11110001UL, MESHX_BRIDGE_ROLE_ACTIVE) == MESHX_SUCCESS);
    assert(meshx_subnet_coex_bridge_add(1, nk2, 0x22220002UL, MESHX_BRIDGE_ROLE_ACTIVE) == MESHX_SUCCESS);
    meshx_bridge_rule_t rule = {.directions = 0x03, .net_idx_1 = 0, .net_idx_2 = 1, .addr_1 = 0xFFFF, .addr_2 = 0xFFFF};
    assert(meshx_subnet_coex_rule_set(&rule, true, 0x00000020UL) == MESHX_SUCCESS);

    meshx_subnet_info_t items[4];
    size_t cnt = 0;
    uint32_t ce = 0, tm = 0;
    assert(meshx_subnet_coex_bridge_list(items, 4, &cnt, &ce, &tm) == MESHX_SUCCESS);
    assert(cnt == 2 && ce == 10 && tm == 4);
    assert(meshx_subnet_coex_bridge_del(1, true) == MESHX_SUCCESS);
    meshx_island_subnet_coex_deinit();
    assert(g_host_live_bytes == 0);
}

int main(void)
{
    test_nvs_persistence_under_1kb();
    test_security_hmac_replay_devkey_rpl();
    test_discovery_and_bridge_fsm();
    printf("PASS: test_phase2_5_6_sec_bridge\n");
    return 0;
}
