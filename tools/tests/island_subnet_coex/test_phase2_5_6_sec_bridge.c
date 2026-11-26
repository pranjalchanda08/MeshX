/**
 * @file test_phase2_5_6_sec_bridge.c
 * @brief TDD Test Suite for NVS, MitM Security, Discovery, Redundancy, Loop Prevention & Facade.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "island/meshx_subnet_nvs.h"
#include "island/meshx_subnet_security.h"
#include "island/meshx_subnet_discovery.h"
#include "island/meshx_subnet_bridge.h"
#include "island/meshx_island_subnet_coex.h"

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

    meshx_nvs_kbridge_blob_t kb;
    memset(&kb, 0, sizeof(kb));
    kb.cluster_epoch = 7;
    kb.active_term = 3;
    kb.valid = 1;
    memset(kb.k_bridge.bytes, 0x5A, sizeof(kb.k_bridge.bytes));
    assert(meshx_subnet_nvs_save_kbridge(&kb) == MESHX_SUCCESS);

    meshx_nvs_kbridge_blob_t kb_rd;
    memset(&kb_rd, 0, sizeof(kb_rd));
    assert(meshx_subnet_nvs_load_kbridge(&kb_rd) == MESHX_SUCCESS);
    assert(memcmp(&kb, &kb_rd, sizeof(kb)) == 0);

    assert(meshx_subnet_nvs_save_nonce(4096UL) == MESHX_SUCCESS);
    uint32_t ckpt = 0;
    assert(meshx_subnet_nvs_load_nonce(&ckpt) == MESHX_SUCCESS);
    assert(ckpt == 4096UL);

    meshx_nat_entry_t nat[2];
    memset(nat, 0, sizeof(nat));
    nat[0].global_subnet_src = 0x11112222UL;
    nat[0].orig_addr = 0x0501;
    nat[0].virtual_addr = 0x7800;
    nat[0].flags = MESHX_NAT_FLAG_VALID | MESHX_NAT_FLAG_PERSISTENT;
    assert(meshx_subnet_nvs_save_nat_table(nat, 1) == MESHX_SUCCESS);

    meshx_nat_entry_t nat_rd[MESHX_NAT_TIER2_CAP];
    memset(nat_rd, 0, sizeof(nat_rd));
    uint8_t nat_cnt = 0;
    assert(meshx_subnet_nvs_load_nat_table(nat_rd, &nat_cnt) == MESHX_SUCCESS);
    assert(nat_cnt == 1 && nat_rd[0].orig_addr == 0x0501);

    meshx_pair_record_t pr;
    memset(&pr, 0, sizeof(pr));
    pr.global_subnet_src = 0x1111;
    pr.global_subnet_dst = 0x2222;
    pr.local_elem_addr = 0x0002;
    pr.virtual_remote_addr = 0x4403;
    pr.remote_phys_addr = 0x0003;
    pr.func_id = 0x1001;
    pr.active = 1;
    assert(meshx_subnet_nvs_save_pair_records(&pr, 1) == MESHX_SUCCESS);
    meshx_pair_record_t pr_rd[MESHX_PAIR_RECORD_MAX];
    memset(pr_rd, 0, sizeof(pr_rd));
    uint8_t pr_cnt = 0;
    assert(meshx_subnet_nvs_load_pair_records(pr_rd, &pr_cnt) == MESHX_SUCCESS);
    assert(pr_cnt == 1 && pr_rd[0].func_id == 0x1001);
}

static void test_security_hmac_replay_devkey_rpl(void)
{
    meshx_subnet_key_t dev_key, k_bridge;
    memset(dev_key.bytes, 0x33, sizeof(dev_key.bytes));
    memset(k_bridge.bytes, 0x77, sizeof(k_bridge.bytes));

    /* DevKey AES-128-CCM seal + open (MESHX_UVP_OP_SUBNET_BRIDGE_INVITE) */
    meshx_uvp_bridge_invite_plain_t inv;
    memset(&inv, 0, sizeof(inv));
    inv.cluster_epoch = 4;
    inv.assigned_slot_n = 2;
    inv.boundary_id = 0xABCDEF01UL;
    uint8_t sealed[128], opened[128];
    uint16_t sealed_len = 0, opened_len = 0;
    meshx_sec_devkey_seal_req_t seal_req = {&dev_key, MESHX_UVP_OP_SUBNET_BRIDGE_INVITE, 0x0005, 42, (const uint8_t *)&inv, sizeof(inv)};
    meshx_sec_devkey_seal_rsp_t seal_rsp = {sealed, sizeof(sealed), &sealed_len};
    assert(meshx_subnet_sec_seal_devkey(&seal_req, &seal_rsp) == MESHX_SUCCESS);
    assert(meshx_subnet_sec_open_devkey(&dev_key, MESHX_UVP_OP_SUBNET_BRIDGE_INVITE,
                                        sealed, sealed_len,
                                        opened, sizeof(opened), &opened_len) == MESHX_SUCCESS);
    assert(opened_len == sizeof(inv) && memcmp(&inv, opened, sizeof(inv)) == 0);
    /* Tamper ciphertext/MIC -> fail */
    sealed[sealed_len - 1] ^= 0xFF;
    assert(meshx_subnet_sec_open_devkey(&dev_key, MESHX_UVP_OP_SUBNET_BRIDGE_INVITE,
                                        sealed, sealed_len,
                                        opened, sizeof(opened), &opened_len) != MESHX_SUCCESS);

    /* Sign & Verify + Boot +10000 margin + Checkpoints + Sliding Anti-Replay */
    s_ckpt_calls = 0;
    meshx_subnet_security_ctx_t tx_sec, rx_sec;
    assert(meshx_subnet_sec_init(&tx_sec, 2000UL, test_ckpt_cb, NULL) == MESHX_SUCCESS);
    assert(meshx_subnet_sec_init(&rx_sec, 0UL, NULL, NULL) == MESHX_SUCCESS);
    assert(tx_sec.tx_bridge_nonce == 12000UL); /* 2000 + 10000 margin */

    /* Without K_bridge -> fail */
    uint8_t frame[128];
    uint16_t flen = 0;
    uint8_t hb[8] = {1,2,3,4,5,6,7,8};
    meshx_sec_sign_req_t sreq = {MESHX_UVP_OP_SUBNET_HEARTBEAT, 0x0005, hb, 8};
    meshx_sec_sign_rsp_t srsp = {frame, sizeof(frame), &flen};
    assert(meshx_subnet_sec_sign_frame(&tx_sec, &sreq, &srsp) == MESHX_SUBNET_ERR_AUTH);

    assert(meshx_subnet_sec_set_k_bridge(&tx_sec, &k_bridge, 1, 2) == MESHX_SUCCESS);
    assert(meshx_subnet_sec_set_k_bridge(&rx_sec, &k_bridge, 1, 2) == MESHX_SUCCESS);

    assert(meshx_subnet_sec_sign_frame(&tx_sec, &sreq, &srsp) == MESHX_SUCCESS);
    assert(flen == (MESHX_CLUSTER_AUTH_OVERHEAD + 8U));

    meshx_sec_verify_rsp_t vrsp = {0};
    assert(meshx_subnet_sec_verify_frame(&rx_sec, frame, flen, &vrsp) == MESHX_SUCCESS);
    assert(vrsp.opcode == MESHX_UVP_OP_SUBNET_HEARTBEAT && vrsp.term == 2 && vrsp.sender == 0x0005 && vrsp.payload_len == 8);

    /* Replay exact same frame -> rejected */
    assert(meshx_subnet_sec_verify_frame(&rx_sec, frame, flen, &vrsp) == MESHX_SUBNET_ERR_REPLAY);

    /* Stale term rejected */
    tx_sec.active_term = 1;
    assert(meshx_subnet_sec_sign_frame(&tx_sec, &sreq, &srsp) == MESHX_SUCCESS);
    assert(meshx_subnet_sec_verify_frame(&rx_sec, frame, flen, &vrsp) == MESHX_SUBNET_ERR_AUTH);

    /* Restore term & verify checkpoint fires after 1024 increments */
    tx_sec.active_term = 2;
    meshx_sec_sign_req_t sreq4 = {MESHX_UVP_OP_SUBNET_HEARTBEAT, 0x0005, hb, 4};
    for (int i = 0; i < 1030; i++) {
        assert(meshx_subnet_sec_sign_frame(&tx_sec, &sreq4, &srsp) == MESHX_SUCCESS);
    }
    assert(s_ckpt_calls >= 1);

    /* Nonce wrap guard >= 0xFFFF0000 */
    tx_sec.tx_bridge_nonce = MESHX_BRIDGE_NONCE_WRAP_LIMIT;
    assert(meshx_subnet_sec_sign_frame(&tx_sec, &sreq4, &srsp) != MESHX_SUCCESS);

    /* Whitelist + RPL */
    assert(meshx_subnet_sec_whitelist_add(&rx_sec, 0x0009, 0x33330001UL) == MESHX_SUCCESS);
    bool wl_ok = false;
    assert(meshx_subnet_sec_whitelist_check(&rx_sec, 0x0009, 0x33330001UL, &wl_ok) == MESHX_SUCCESS && wl_ok == true);
    assert(meshx_subnet_sec_whitelist_check(&rx_sec, 0x0009, 0x99999999UL, &wl_ok) == MESHX_SUCCESS && wl_ok == false);

    bool rpl_ok = false;
    assert(meshx_subnet_sec_rpl_check_update(&rx_sec, 1, 0x0003, 1, 0, 10, &rpl_ok) == MESHX_SUCCESS && rpl_ok == true);
    assert(meshx_subnet_sec_rpl_check_update(&rx_sec, 1, 0x0003, 1, 0, 10, &rpl_ok) == MESHX_SUCCESS && rpl_ok == false);
    assert(meshx_subnet_sec_rpl_check_update(&rx_sec, 1, 0x0003, 2, 0, 1, &rpl_ok) == MESHX_SUCCESS && rpl_ok == true); /* new epoch allows seq=1 */

    assert(meshx_subnet_sec_canonical_rpl_check(&rx_sec, 0x33330001UL, 0x0003, 0, 55, &rpl_ok) == MESHX_SUCCESS && rpl_ok == true);
    assert(meshx_subnet_sec_canonical_rpl_check(&rx_sec, 0x33330001UL, 0x0003, 0, 55, &rpl_ok) == MESHX_SUCCESS && rpl_ok == false);

    assert(meshx_subnet_sec_clear_k_bridge(&tx_sec) == MESHX_SUCCESS);
    assert(!tx_sec.k_bridge_valid);
    assert(meshx_subnet_sec_deinit(&tx_sec) == MESHX_SUCCESS);
    assert(meshx_subnet_sec_deinit(&rx_sec) == MESHX_SUCCESS);
}

static void test_discovery_and_bridge_fsm(void)
{
    /* Discovery 30s 3-MIC detector + Witness eval + Pair commit */
    meshx_subnet_discovery_t disc;
    assert(meshx_subnet_disc_init(&disc) == MESHX_SUCCESS);
    bool mic_trig = false;
    assert(meshx_subnet_disc_record_mic_fail(&disc, 1000, &mic_trig) == MESHX_SUCCESS && mic_trig == false);
    assert(meshx_subnet_disc_record_mic_fail(&disc, 15000, &mic_trig) == MESHX_SUCCESS && mic_trig == false);
    assert(meshx_subnet_disc_record_mic_fail(&disc, 40000, &mic_trig) == MESHX_SUCCESS && mic_trig == false); /* 1000 expired (>30s) */
    assert(meshx_subnet_disc_record_mic_fail(&disc, 42000, &mic_trig) == MESHX_SUCCESS && mic_trig == true);  /* 15k, 40k, 42k within 30s! */

    bool cand_ok = false;
    assert(meshx_subnet_disc_eval_candidate(&disc, -80, true, 8192, &cand_ok) == MESHX_SUCCESS && cand_ok == true);
    assert(meshx_subnet_disc_eval_candidate(&disc, -85, true, 8192, &cand_ok) == MESHX_SUCCESS && cand_ok == false);
    assert(meshx_subnet_disc_eval_candidate(&disc, -70, false, 8192, &cand_ok) == MESHX_SUCCESS && cand_ok == false);
    assert(meshx_subnet_disc_eval_candidate(&disc, -70, true, 2048, &cand_ok) == MESHX_SUCCESS && cand_ok == false);

    meshx_nat_mgr_t nat;
    assert(meshx_nat_mgr_init(&nat, 0x0001) == MESHX_SUCCESS);
    uint8_t s0 = 0, s1 = 0;
    assert(meshx_nat_slot_assign(&nat, 0x1111, 0, true, &s0) == MESHX_SUCCESS);
    assert(meshx_nat_slot_assign(&nat, 0x2222, 1, false, &s1) == MESHX_SUCCESS);

    meshx_uvp_discover_rsp_t rsp = {
        .global_subnet_src = 0x2222, .global_subnet_dst = 0x1111,
        .requestor_addr = 0x0002, .trans_id = 9,
        .remote_phys_addr = 0x0500, .func_id = 0x1234, .role = 1, .elem_count = 1
    };
    uint16_t vnat = 0;
    /* Unsolicited response (no pending request) must not allocate Tier-2 NAT */
    assert(meshx_subnet_disc_handle_rsp(&disc, &nat, &rsp, 10, 5000, &vnat) == MESHX_NOT_FOUND);
    assert(meshx_subnet_disc_record_req(&disc, 0x1111, 0x0002, 9, 4500) == MESHX_SUCCESS);
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
    assert(meshx_nat_mgr_deinit(&nat) == MESHX_SUCCESS);
    assert(meshx_subnet_disc_deinit(&disc) == MESHX_SUCCESS);

    /* Redundancy Failover + Split-Brain + 23B Transit Loop Filter */
    meshx_subnet_bridge_mgr_t bm;
    meshx_router_queue_t q;
    assert(meshx_router_queue_init(&q) == MESHX_SUCCESS);
    assert(meshx_subnet_bridge_init(&bm, 0x0008, true, 50, 0) == MESHX_SUCCESS);

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
        .visited_subnets_mask = 0x0001U, .hop_limit = 3U,
        .orig_subnet_id = 0x1111U, .orig_phys_src = 0x0002U, .orig_seq_num = 0xCAFEU,
        .orig_seq_auth = 0x0001CAFEUL, .orig_iv_index = 1UL, .boundary_id = 0xB001U, .active_term = 5UL
    };
    bool transit_ok = false;
    assert(meshx_subnet_bridge_transit_check_and_mark(&bm, &th, 1, 1000, &transit_ok) == MESHX_SUCCESS && transit_ok == true);
    /* Immediate echo replay on another path within 5s -> suppressed */
    assert(meshx_subnet_bridge_transit_check_and_mark(&bm, &th, 2, 3000, &transit_ok) == MESHX_SUCCESS && transit_ok == false);
    /* Visited bit already set -> suppressed */
    assert(meshx_subnet_bridge_transit_check_and_mark(&bm, &th, 1, 7000, &transit_ok) == MESHX_SUCCESS && transit_ok == false);
    /* New tag after 5s passes */
    meshx_subnet_transit_hdr_t th2 = th;
    th2.visited_subnets_mask = 0;
    th2.hop_limit = 2;
    assert(meshx_subnet_bridge_transit_check_and_mark(&bm, &th2, 2, 7000, &transit_ok) == MESHX_SUCCESS && transit_ok == true);

    /* Facade & MXCP C API sanity check */
    assert(meshx_island_subnet_coex_init() == MESHX_SUCCESS);
    meshx_subnet_key_t kbr;
    memset(kbr.bytes, 0x99, sizeof(kbr.bytes));
    assert(meshx_subnet_coex_cluster_key_update(&kbr, 10, 4) == MESHX_SUCCESS);
    assert(meshx_subnet_coex_is_kr_quarantined() == true);
    meshx_subnet_key_t nk1, nk2;
    memset(nk1.bytes, 0x11, sizeof(nk1.bytes));
    memset(nk2.bytes, 0x22, sizeof(nk2.bytes));
    assert(meshx_subnet_coex_bridge_add(0, &nk1, 0x11110001UL, MESHX_BRIDGE_ROLE_ACTIVE) == MESHX_SUCCESS);
    assert(meshx_subnet_coex_bridge_add(1, &nk2, 0x22220002UL, MESHX_BRIDGE_ROLE_ACTIVE) == MESHX_SUCCESS);
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

static int     s_uvp_tx_count = 0;
static uint8_t s_last_uvp_opcode = 0;

static meshx_err_t test_uvp_tx(uint16_t net_idx, uint16_t dst_addr, uint8_t opcode,
                               const uint8_t *payload, uint16_t payload_len,
                               bool ack_req, void *user_ctx)
{
    (void)net_idx; (void)dst_addr; (void)payload; (void)payload_len;
    (void)ack_req; (void)user_ctx;
    s_uvp_tx_count++;
    s_last_uvp_opcode = opcode;
    return MESHX_SUCCESS;
}

/* TASK-04/05: boundary witness classification promotes BRIDGE_ROLE_CANDIDATE and
 * emits a 0xD3 candidate report only when RSSI/mains thresholds are met. */
static void test_candidate_witness_report(void)
{
    assert(meshx_island_subnet_coex_init() == MESHX_SUCCESS);
    meshx_subnet_coex_register_uvp_tx(test_uvp_tx, NULL);

    s_uvp_tx_count = 0;
    const uint8_t bcn[4] = {0x11, 0x22, 0x33, 0x44};
    meshx_subnet_coex_on_beacon_rx(0x02E5, bcn, sizeof(bcn), -90); /* below -82 dBm -> no report */
    assert(s_uvp_tx_count == 0);

    meshx_subnet_coex_on_beacon_rx(0x02E5, bcn, sizeof(bcn), -70); /* eligible -> MESHX_UVP_OP_SUBNET_CANDIDATE_RPT */
    assert(s_uvp_tx_count == 1);
    assert(s_last_uvp_opcode == MESHX_UVP_OP_SUBNET_CANDIDATE_RPT);

    /* Repeated beacons are debounced once candidate_active is latched */
    meshx_subnet_coex_on_beacon_rx(0x02E5, bcn, sizeof(bcn), -65);
    assert(s_uvp_tx_count == 1);

    meshx_subnet_info_t items[4];
    size_t cnt = 0;
    uint32_t ce = 0, tm = 0;
    assert(meshx_subnet_coex_bridge_list(items, 4, &cnt, &ce, &tm) == MESHX_SUCCESS);
    assert(cnt == 1);
    assert(items[0].role == (uint8_t)MESHX_BRIDGE_ROLE_CANDIDATE);

    meshx_island_subnet_coex_deinit();
    assert(g_host_live_bytes == 0);
}

static int      s_data_tx_calls = 0;
static uint16_t s_data_tx_net_idx = 0xFFFFU;
static uint32_t s_data_tx_iv = 0UL;
static meshx_err_t test_data_tx(const meshx_router_queue_entry_t *entry,
                                const meshx_ccm_nonce_t *nonce,
                                const meshx_subnet_key_t *tx_net_key,
                                void *user_ctx)
{
    (void)tx_net_key; (void)user_ctx;
    s_data_tx_calls++;
    s_data_tx_net_idx = entry->tx_net_idx;
    s_data_tx_iv = entry->orig_iv_index;
    assert(nonce->bytes[9] == (uint8_t)((entry->orig_iv_index >> 24) & 0xFFU));
    assert(nonce->bytes[12] == (uint8_t)(entry->orig_iv_index & 0xFFU));
    return MESHX_SUCCESS;
}

static void test_live_path_fixes(void)
{
    host_nvs_reset();
    s_uvp_tx_count = 0;
    s_data_tx_calls = 0;
    assert(meshx_island_subnet_coex_init() == MESHX_SUCCESS);
    meshx_subnet_coex_register_uvp_tx(test_uvp_tx, NULL);
    meshx_subnet_coex_register_data_tx(test_data_tx, NULL);

    /* C1: Cluster-only opcode rejected before K_bridge and when short */
    uint8_t short_hb[13] = {MESHX_UVP_OP_SUBNET_HEARTBEAT, 1, 2, 3};
    assert(meshx_subnet_coex_handle_uvp(MESHX_UVP_OP_SUBNET_HEARTBEAT, 0x0002, 0xFFFF, 0,
                                        short_hb, sizeof(short_hb)) == MESHX_SUBNET_ERR_AUTH);

    /* H2: 0xD4 DevKey invite -> NVS commit -> 0xDA ACK */
    meshx_subnet_key_t devk, kbr, nk0, nk1;
    memset(devk.bytes, 0x44, sizeof(devk.bytes));
    memset(kbr.bytes, 0x88, sizeof(kbr.bytes));
    memset(nk0.bytes, 0x11, sizeof(nk0.bytes));
    memset(nk1.bytes, 0x22, sizeof(nk1.bytes));
    assert(meshx_subnet_coex_set_dev_key(&devk) == MESHX_SUCCESS);
    assert(meshx_subnet_coex_bridge_add(0, &nk0, 0x11110001UL, MESHX_BRIDGE_ROLE_ACTIVE) == MESHX_SUCCESS);

    meshx_uvp_bridge_invite_plain_t inv;
    memset(&inv, 0, sizeof(inv));
    inv.cluster_bridge_key = kbr;
    inv.cluster_epoch = 2UL;
    inv.target_net_idx = 1U;
    inv.target_global_subnet_id = 0x22220002UL;
    inv.target_subnet_key = nk1;
    inv.assigned_role = (uint8_t)MESHX_BRIDGE_ROLE_ACTIVE;
    inv.boundary_id = 0x22220002UL;

    uint8_t sealed[128];
    uint16_t sealed_len = 0U;
    meshx_sec_devkey_seal_req_t sreq = {&devk, MESHX_UVP_OP_SUBNET_BRIDGE_INVITE, 0x0001, 100UL,
                                        (const uint8_t *)&inv, sizeof(inv)};
    meshx_sec_devkey_seal_rsp_t srsp = {sealed, sizeof(sealed), &sealed_len};
    assert(meshx_subnet_sec_seal_devkey(&sreq, &srsp) == MESHX_SUCCESS);
    uint8_t inv_wire[130];
    inv_wire[0] = MESHX_UVP_OP_SUBNET_BRIDGE_INVITE;
    memcpy(&inv_wire[1], sealed, sealed_len);
    assert(meshx_subnet_coex_handle_uvp(MESHX_UVP_OP_SUBNET_BRIDGE_INVITE, 0x0009, 0x0001, 0,
                                        inv_wire, (uint16_t)(sealed_len + 1U)) == MESHX_SUCCESS);
    assert(meshx_subnet_coex_is_kr_quarantined() == true);
    assert(s_last_uvp_opcode == MESHX_UVP_OP_SUBNET_INVITE_ACK);

    /* C1 after K_bridge: short frame rejected */
    assert(meshx_subnet_coex_handle_uvp(MESHX_UVP_OP_SUBNET_HEARTBEAT, 0x0002, 0xFFFF, 0,
                                        short_hb, sizeof(short_hb)) == MESHX_SUBNET_ERR_AUTH);

    /* C3/C4: Multi-boundary tick emits for all boundaries + bridge_list reports real fields */
    s_uvp_tx_count = 0;
    assert(meshx_subnet_coex_tick(5000UL) == MESHX_SUCCESS);
    assert(s_uvp_tx_count == 2); /* both 0x11110001 and 0x22220002 emitted */
    meshx_subnet_info_t items[4];
    size_t cnt = 0;
    uint32_t ce = 0, tm = 0;
    assert(meshx_subnet_coex_bridge_list(items, 4, &cnt, &ce, &tm) == MESHX_SUCCESS);
    assert(cnt == 2 && items[0].boundary_id == 0x11110001UL && items[1].boundary_id == 0x22220002UL);
    assert(meshx_subnet_coex_bridge_del(0, true) == MESHX_INVALID_ARG); /* primary NetKey guard */

    /* C2/H5: Stage 1 seg RX -> Stage 2 data TX preserves orig_iv_index and egress net_idx */
    meshx_bridge_rule_t rule = {.directions = 0x03, .net_idx_1 = 0, .net_idx_2 = 1, .addr_1 = 0xFFFF, .addr_2 = 0xFFFF};
    assert(meshx_subnet_coex_rule_set(&rule, true, 0x00000001UL) == MESHX_SUCCESS);
    uint8_t app_pdu[10] = {0xAA, 0xBB, 0xCC, 0xDD, 1, 2, 3, 4, 5, 6};
    meshx_trans_mic_t mic = {.bytes = {9,8,7,6}, .len = 4};
    meshx_router_seg_rx_t seg = {0, 0x0002, 0x4405, 1, 0x123, 0, 0, app_pdu, sizeof(app_pdu)};
    meshx_subnet_seg_meta_t seg_meta = {0x12345678UL, 0x000055UL, 4U, 0U, &mic, 5100UL};
    bool block_ack = false;
    assert(meshx_subnet_coex_on_seg_rx(&seg, &seg_meta, &block_ack) == MESHX_SUCCESS);
    assert(block_ack == true);
    assert(meshx_subnet_coex_tick(5200UL) == MESHX_SUCCESS);
    assert(s_data_tx_calls == 1 && s_data_tx_net_idx == 1 && s_data_tx_iv == 0x12345678UL);
    assert(meshx_subnet_coex_on_stage2_tx_complete(true, 5250UL) == MESHX_SUCCESS);

    /* H3/H7: commit_pair persists to NVS and survives reboot */
    meshx_pair_record_t pr = {
        .global_subnet_src = 0x22220002UL, .global_subnet_dst = 0x11110001UL,
        .local_elem_addr = 0x0002, .virtual_remote_addr = 0x7805,
        .remote_phys_addr = 0x0500, .func_id = 0x1001, .role_flags = 1, .active = 1
    };
    assert(meshx_subnet_coex_commit_pair(&pr) == MESHX_SUCCESS);

    /* H2: Revocation erases K_bridge in RAM and NVS */
    assert(meshx_subnet_coex_cluster_key_update_ex(NULL, 0, 0, true) == MESHX_SUCCESS);
    assert(meshx_subnet_coex_is_kr_quarantined() == false);

    meshx_island_subnet_coex_deinit();
    assert(meshx_island_subnet_coex_init() == MESHX_SUCCESS);
    assert(meshx_subnet_coex_is_kr_quarantined() == false);
    meshx_island_subnet_coex_deinit();
    assert(g_host_live_bytes == 0);
}

static meshx_err_t fail_ckpt_cb(uint32_t ckpt, void *ctx)
{
    (void)ckpt; (void)ctx;
    return MESHX_FAIL;
}

static void test_remaining_fixes_coverage(void)
{
    host_nvs_reset();
    s_uvp_tx_count = 0;
    s_data_tx_calls = 0;

    /* FIX-19 & FIX-20: 32-bit sliding window + ckpt_cb failure leaves last_saved_nonce_ckpt unchanged */
    meshx_subnet_security_ctx_t sec;
    assert(meshx_subnet_sec_init(&sec, 0UL, fail_ckpt_cb, NULL) == MESHX_SUCCESS);
    meshx_subnet_key_t kbr;
    memset(kbr.bytes, 0xAA, sizeof(kbr.bytes));
    assert(meshx_subnet_sec_set_k_bridge(&sec, &kbr, 1UL, 1UL) == MESHX_SUCCESS);

    uint8_t dummy[4] = {1, 2, 3, 4};
    uint8_t fbuf[64];
    uint16_t flen = 0U;
    meshx_sec_sign_req_t sq = {MESHX_UVP_OP_SUBNET_HEARTBEAT, 0x0002, dummy, sizeof(dummy)};
    meshx_sec_sign_rsp_t sr = {fbuf, sizeof(fbuf), &flen};
    sec.tx_bridge_nonce = MESHX_BRIDGE_NONCE_BOOT_MARGIN + MESHX_BRIDGE_NONCE_CHKPT_STEP - 1UL;
    uint32_t prev_ckpt = sec.last_saved_nonce_ckpt;
    assert(meshx_subnet_sec_sign_frame(&sec, &sq, &sr) == MESHX_FAIL);
    assert(sec.last_saved_nonce_ckpt == prev_ckpt);

    /* 32-bit sliding window in rpl_check_update: seq=40 then out-of-order seq=20 (diff=20 < 32) passes,
     * duplicate seq=20 fails, and seq=8 (diff=32) fails */
    bool rpl_ok = false;
    assert(meshx_subnet_sec_rpl_check_update(&sec, 0U, 0x0003, 1UL, 0UL, 40UL, &rpl_ok) == MESHX_SUCCESS && rpl_ok == true);
    assert(meshx_subnet_sec_rpl_check_update(&sec, 0U, 0x0003, 1UL, 0UL, 20UL, &rpl_ok) == MESHX_SUCCESS && rpl_ok == true);
    assert(meshx_subnet_sec_rpl_check_update(&sec, 0U, 0x0003, 1UL, 0UL, 20UL, &rpl_ok) == MESHX_SUCCESS && rpl_ok == false);
    assert(meshx_subnet_sec_rpl_check_update(&sec, 0U, 0x0003, 1UL, 0UL, 8UL, &rpl_ok) == MESHX_SUCCESS && rpl_ok == false);
    assert(meshx_subnet_sec_deinit(&sec) == MESHX_SUCCESS);

    /* FIX-15 & FIX-16: 4-tuple Tier-2 NAT + reverse_by_net + non-aliasing 1024-bit group mask */
    meshx_nat_mgr_t nat;
    assert(meshx_nat_mgr_init(&nat, 0x0001U) == MESHX_SUCCESS);
    uint16_t va1 = 0U, va2 = 0U;
    meshx_nat_tier2_flow_req_t fq1 = {NULL, 0x1111UL, 0x0500U, 0x2222UL, 0x0600U, 1U, MESHX_NAT_FLAG_PERSISTENT, 1U};
    meshx_nat_tier2_flow_req_t fq2 = {NULL, 0x1111UL, 0x0500U, 0x3333UL, 0x0700U, 2U, MESHX_NAT_FLAG_PERSISTENT, 1U};
    assert(meshx_nat_tier2_alloc_flow(&nat, &fq1, &va1) == MESHX_SUCCESS);
    assert(meshx_nat_tier2_alloc_flow(&nat, &fq2, &va2) == MESHX_SUCCESS);
    assert(va1 != va2);
    uint32_t sg = 0UL, dg = 0UL;
    uint16_t sa = 0U, da = 0U;
    assert(meshx_nat_tier2_reverse_by_net(&nat, 1U, va1, &sg, &sa, &dg, &da) == MESHX_SUCCESS);
    assert(sg == 0x1111UL && sa == 0x0500U && dg == 0x2222UL && da == 0x0600U);

    assert(meshx_nat_set_group_forward_mask(&nat, 0x0001U, true) == MESHX_SUCCESS);
    bool g_allowed = false;
    assert(meshx_nat_is_group_forward_allowed(&nat, 0xC001U, &g_allowed) == MESHX_SUCCESS && g_allowed == true);
    assert(meshx_nat_is_group_forward_allowed(&nat, 0xC021U, &g_allowed) == MESHX_SUCCESS && g_allowed == false); /* no 32-bit aliasing */
    assert(meshx_nat_mgr_deinit(&nat) == MESHX_SUCCESS);

    /* FIX-13: transit_check_and_mark_ex checks hop_limit <= 1, egress == orig, dual-bit, keyed echo */
    meshx_subnet_bridge_mgr_t bm;
    assert(meshx_subnet_bridge_init(&bm, 0x0001U, true, 50U, 0UL) == MESHX_SUCCESS);
    meshx_subnet_transit_hdr_t th = {
        .visited_subnets_mask = 0U, .hop_limit = 1U,
        .orig_subnet_id = 0x1111U, .orig_phys_src = 0x0002U, .orig_seq_num = 0x1234U,
        .orig_seq_auth = 0x1234UL, .orig_iv_index = 1UL, .boundary_id = 0xB001U, .active_term = 1UL
    };
    bool transit_ok = false;
    meshx_bridge_transit_ctx_t tctx1 = {1U, 2U, 0x3333UL, &kbr, 1000UL};
    meshx_bridge_transit_ctx_t tctx2 = {1U, 2U, 0x1111UL, &kbr, 1000UL};
    assert(meshx_subnet_bridge_transit_check_and_mark_ex(&bm, &th, &tctx1, &transit_ok) == MESHX_SUCCESS && transit_ok == false);
    th.hop_limit = 3U;
    assert(meshx_subnet_bridge_transit_check_and_mark_ex(&bm, &th, &tctx2, &transit_ok) == MESHX_SUCCESS && transit_ok == false);
    assert(meshx_subnet_bridge_transit_check_and_mark_ex(&bm, &th, &tctx1, &transit_ok) == MESHX_SUCCESS && transit_ok == true);
    assert((th.visited_subnets_mask & ((1U << 1U) | (1U << 2U))) == ((1U << 1U) | (1U << 2U)));

    /* FIX-03, FIX-09, FIX-10, FIX-21, FIX-23, FIX-28: edge frames, 0xD7->0xD9, 0xDC->0xDD->0xDE, demote */
    assert(meshx_island_subnet_coex_init() == MESHX_SUCCESS);
    meshx_subnet_coex_register_uvp_tx(test_uvp_tx, NULL);
    meshx_subnet_key_t nk0, nk1;
    memset(nk0.bytes, 0x11, sizeof(nk0.bytes));
    memset(nk1.bytes, 0x22, sizeof(nk1.bytes));
    assert(meshx_subnet_coex_bridge_add(0, &nk0, 0x11110001UL, MESHX_BRIDGE_ROLE_ACTIVE) == MESHX_SUCCESS);
    assert(meshx_subnet_coex_bridge_add(1, &nk1, 0x22220002UL, MESHX_BRIDGE_ROLE_ACTIVE) == MESHX_SUCCESS);
    assert(meshx_subnet_coex_cluster_key_update(&kbr, 1UL, 1UL) == MESHX_SUCCESS);

    /* Edge 0xDC DISCOVER_REQ relays to other subnet & records correlation */
    uint8_t dreq[1 + sizeof(meshx_uvp_discover_req_t)];
    meshx_uvp_discover_req_t req = {0x11110001UL, 0x22220002UL, 0x0002U, 77U, 0U, 0U};
    dreq[0] = MESHX_UVP_OP_SUBNET_DISCOVER_REQ;
    memcpy(&dreq[1], &req, sizeof(req));
    s_uvp_tx_count = 0;
    assert(meshx_subnet_coex_handle_uvp(MESHX_UVP_OP_SUBNET_DISCOVER_REQ, 0x0002U, 0x8000U, 0U,
                                        dreq, sizeof(dreq)) == MESHX_SUCCESS);
    assert(s_uvp_tx_count == 1 && s_last_uvp_opcode == MESHX_UVP_OP_SUBNET_DISCOVER_REQ);

    /* Edge 0xDE QUERY_SUBNET_DIRECTORY responds with synthesized directory */
    uint8_t qdir[1 + sizeof(meshx_uvp_dir_query_t)] = {MESHX_UVP_OP_QUERY_SUBNET_DIRECTORY};
    assert(meshx_subnet_coex_handle_uvp(MESHX_UVP_OP_QUERY_SUBNET_DIRECTORY, 0x0002U, 0x0001U, 0U,
                                        qdir, sizeof(qdir)) == MESHX_SUCCESS);
    assert(s_last_uvp_opcode == MESHX_UVP_OP_QUERY_SUBNET_DIRECTORY);

    /* Edge 0xD7 ROUTER_NACK triggers 0xD9 NACK_ACK */
    uint8_t nack_w[1 + sizeof(meshx_uvp_router_nack_t)];
    meshx_uvp_router_nack_t nk = {42UL, 10UL, 0x11110001UL, 0x22220002UL, 0x0002U, 0x0005U, MESHX_NACK_REASON_STAGE2_FAIL};
    nack_w[0] = MESHX_UVP_OP_SUBNET_ROUTER_NACK;
    memcpy(&nack_w[1], &nk, sizeof(nk));
    assert(meshx_subnet_coex_handle_uvp(MESHX_UVP_OP_SUBNET_ROUTER_NACK, 0x0003U, 0x0001U, 0U,
                                        nack_w, sizeof(nack_w)) == MESHX_SUCCESS);
    assert(s_last_uvp_opcode == MESHX_UVP_OP_SUBNET_NACK_ACK);

    /* Option B (FIX-06): 0xDF UVP Data Tunnel RX on bridge -> Stage 2 -> 0xDF egress TX */
    assert(meshx_subnet_coex_pal_bridge_set(true) == MESHX_SUCCESS);
    uint8_t tun_frame[1 + sizeof(meshx_uvp_data_tunnel_hdr_t) + 4U];
    meshx_uvp_data_tunnel_hdr_t thdr;
    memset(&thdr, 0, sizeof(thdr));
    thdr.ttl = 4U;
    thdr.flags = MESHX_ROUTER_FLAG_IS_UNICAST;
    thdr.orig_src_addr = 0x0002U;
    thdr.orig_dst_addr = 0x0005U;
    thdr.egress_net_idx = 1U;
    thdr.orig_seq_auth = 55UL;
    thdr.orig_iv_index = 1UL;
    thdr.trans_mic.len = 4U;
    thdr.pdu_len = 4U;
    tun_frame[0] = MESHX_UVP_OP_SUBNET_DATA_TUNNEL;
    memcpy(&tun_frame[1], &thdr, sizeof(thdr));
    tun_frame[1 + sizeof(thdr) + 0] = 0xAAU;
    tun_frame[1 + sizeof(thdr) + 1] = 0xBBU;
    tun_frame[1 + sizeof(thdr) + 2] = 0xCCU;
    tun_frame[1 + sizeof(thdr) + 3] = 0xDDU;
    s_uvp_tx_count = 0;
    assert(meshx_subnet_coex_handle_uvp(MESHX_UVP_OP_SUBNET_DATA_TUNNEL, 0x0002U, 0x0001U, 0U,
                                        tun_frame, sizeof(tun_frame)) == MESHX_SUCCESS);
    assert(meshx_subnet_coex_tick(1000UL) == MESHX_SUCCESS);
    assert(s_uvp_tx_count == 1 && s_last_uvp_opcode == MESHX_UVP_OP_SUBNET_DATA_TUNNEL);

    /* REQ-F08.3: KR burst opcode quarantined across bridge */
    uint8_t kr_pdu[4] = {MESHX_UVP_OP_KR_DISTRIBUTE, 1, 2, 3};
    meshx_router_stage1_pdu_t s1_kr;
    memset(&s1_kr, 0, sizeof(s1_kr));
    s1_kr.net_idx = 0U; s1_kr.egress_net_idx = 1U;
    s1_kr.src_addr = 0x0002U; s1_kr.dst_addr = 0x0005U;
    s1_kr.iv_index = 1UL; s1_kr.seq_auth = 99UL; s1_kr.ttl = 4U;
    s1_kr.pdu = kr_pdu; s1_kr.pdu_len = sizeof(kr_pdu);
    bool kr_ack = false;
    assert(meshx_subnet_coex_on_pdu_rx(&s1_kr, 1000UL, &kr_ack) == MESHX_SUBNET_ERR_AUTH);

    /* REQ-F07.2: Multihop transit whitelist rejects unlisted bridge */
    meshx_subnet_transit_hdr_t tr_hdr = {
        .visited_subnets_mask = 0U, .hop_limit = 3U,
        .orig_subnet_id = 0x0003U, .orig_phys_src = 0x0010U, .orig_seq_num = 100U,
        .orig_seq_auth = 100UL, .orig_iv_index = 1UL, .boundary_id = 0xB002U, .active_term = 1UL
    };
    meshx_router_stage1_pdu_t s1_tr;
    memset(&s1_tr, 0, sizeof(s1_tr));
    s1_tr.net_idx = 0U; s1_tr.egress_net_idx = 1U;
    s1_tr.src_addr = 0x0099U; s1_tr.dst_addr = 0x0005U;
    s1_tr.iv_index = 1UL; s1_tr.seq_auth = 100UL; s1_tr.ttl = 4U;
    s1_tr.is_multihop_transit = true;
    s1_tr.global_subnet_src = 0x33330003UL;
    s1_tr.global_subnet_dst = 0x22220002UL;
    s1_tr.pdu = (const uint8_t *)&tr_hdr; s1_tr.pdu_len = sizeof(tr_hdr);
    assert(meshx_subnet_coex_on_pdu_rx(&s1_tr, 1000UL, &kr_ack) == MESHX_SUBNET_ERR_AUTH);

    /* REQ-F07.1: OTA K_bridge rekey via DevKey-sealed 0xD4 */
    meshx_subnet_key_t devk, new_kbr;
    memset(devk.bytes, 0x5A, sizeof(devk.bytes));
    memset(new_kbr.bytes, 0xA5, sizeof(new_kbr.bytes));
    assert(meshx_subnet_coex_set_dev_key(&devk) == MESHX_SUCCESS);
    s_uvp_tx_count = 0;
    assert(meshx_subnet_coex_ota_rekey_peer(0U, 0x0003U, &devk, &new_kbr, 9UL) == MESHX_SUCCESS);
    assert(s_uvp_tx_count == 1 && s_last_uvp_opcode == MESHX_UVP_OP_SUBNET_BRIDGE_INVITE);

    /* REQ-F05.2 / REQ-NF02.3: Standby failover timing <= 12.5s */
    meshx_island_subnet_coex_deinit();
    assert(meshx_island_subnet_coex_init() == MESHX_SUCCESS);
    meshx_subnet_coex_register_uvp_tx(test_uvp_tx, NULL);
    assert(meshx_subnet_coex_bridge_add(0U, &nk0, 0x44440004UL, MESHX_BRIDGE_ROLE_STANDBY) == MESHX_SUCCESS);
    s_uvp_tx_count = 0;
    assert(meshx_subnet_coex_tick(8000UL) == MESHX_SUCCESS);
    assert(s_uvp_tx_count == 0);
    assert(meshx_subnet_coex_tick(12400UL) == MESHX_SUCCESS);
    assert(s_uvp_tx_count == 1 && s_last_uvp_opcode == MESHX_UVP_OP_SUBNET_TAKEOVER_CLAIM);

    /* REQ-F01.1: Intent-driven probe/bind commissioning driver */
    assert(meshx_subnet_coex_bridge_add(1U, &nk1, 0x22220002UL, MESHX_BRIDGE_ROLE_ACTIVE) == MESHX_SUCCESS);
    uint16_t comm_vaddr = 0U;
    s_uvp_tx_count = 0;
    assert(meshx_subnet_coex_commission_pair(0U, 0x0002U, 0x22220002UL, 0x0005U, 0x0101U, 0x01U, &comm_vaddr) == MESHX_SUCCESS);
    assert((comm_vaddr & 0x03FFU) == 0x0005U && comm_vaddr >= 0x4000U);
    assert(s_uvp_tx_count == 1 && s_last_uvp_opcode == MESHX_UVP_OP_SUBNET_DISCOVER_REQ);

    meshx_island_subnet_coex_deinit();
    assert(g_host_live_bytes == 0);
}

int main(void)
{
    test_nvs_persistence_under_1kb();
    test_security_hmac_replay_devkey_rpl();
    test_discovery_and_bridge_fsm();
    test_candidate_witness_report();
    test_live_path_fixes();
    test_remaining_fixes_coverage();
    printf("PASS: test_phase2_5_6_sec_bridge\n");
    return 0;
}
