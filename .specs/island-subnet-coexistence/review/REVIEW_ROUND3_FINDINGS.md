# Independent Review Report: Island Subnet Coexistence (Round 3)

**Spec under review:** `D:\MeshX\.specs\island-subnet-coexistence\`
**Review team:** 2 separate free models (OpenCode provider, $0 cost), randomized delegation
**Overall verdict: FAIL** (GLM = CONDITIONAL; DeepSeek = FAIL)

---

## Reviewer Roster

| Reviewer | Model | Charter | Verdict |
|---|---|---|---|
| E | opencode/glm-5.3-flash-free | Data-plane & control-plane security / NAT / MIC / RPL | CONDITIONAL |
| F | opencode/deepseek-v4.1-flash-free | Reliability / failover / queue / discovery / memory / timing | FAIL |

---

## Round 3 Consensus — Remaining Open Defects

### CRITICAL (unresolved)

| # | File:line | Problem | Reviewer |
|---|---|---|---|
| C3a | `trd/page4_store_forward_router.md:74-76` | TransMIC nonce binds src/dst/seq/iv; router rewrites egress SRC/DST but cannot re-sign (no AppKey). Destination recomputes nonce with egress addresses → TransMIC mismatch on every rewritten frame. "Canonical physical addresses agreed during pairing" escape hatch fails because sender cannot know remote island's locally-scoped slot prefix (slot_id is LOCAL per page11). E2E auth is fiction unless nonce inputs are exactly documented. | E |
| C3b | `trd/page8_mitm_security.md:45-46` | `Monotonic_Nonce` is never persisted to NVS (page9 NVS table has no nonce counter). Bridge reboot resets nonce to 0; receivers holding higher seen-nonces reject every valid `0xD1` heartbeat → watchdog fires → failover storm (page7:64-66). | E |
| C3c | `trd/page8_mitm_security.md:46` | Stale-timestamp rejection (>2.0s delta) requires synchronized clocks, but no time source, domain, or clock-sync protocol is defined. Independently-bootstrapped islands have divergent clocks; honest frames die on arrival. | E |
| C3d | `trd/page4_store_forward_router.md:33`, `:89` | `0xD7` NACK is fire-and-forget — no ACK/retry; Stage 1 ACK already released the originator, so lost `0xD7` is indistinguishable from success. Unacknowledged models have no transport timer to consume 0xD7. | F |
| C3e | `trd/page10_multihop_transit.md:52` | Multi-hop `0xD7` return path undefined: Bridge B-C sends NACK to transit proxy `0x4002`, but no rule tells Bridge A-B to relay it to originator `0x0002` on Subnet A. Transit header lacks return-address / reverse-flow fields. | F |
| C3f | `trd/page4_store_forward_router.md:96-109` | `meshx_router_queue_entry_t` stores no `ingress_seq_zero` / `egress_seq_zero` / flow id, so a NACK cannot be mapped back to a flow even single-hop. | F |
| C3g | `trd/page2_addressing_nat.md:80` | "Deterministic fallback alias on first return packet" cannot work: Standby has no `(virtual_proxy_addr)` key match for the old alias in `0x7800..0x7FFF`, no derivation seed; any new alias differs from the originator's persisted `pub_addr`. Return packet dropped as unknown destination. | F |
| C3h | `trd/page9_latency_memory.md:44-49` | Budget breach: queue entry is ~140 B (natural alignment) not 128 B; RPL keyed `(net_idx,src_addr,key_epoch)` needs ≥10 B/entry not 8 B; NAT 32 entries not 16. Computed total ≈ 3,720 B > 3,584 B ceiling → REQ-NF01.1 breached by spec's own structs. | F |

### HIGH (unresolved)

| # | File:line | Problem | Reviewer |
|---|---|---|---|
| H3a | `trd/page8_mitm_security.md:49-51` | `0xD4_ACK` has no retry path; Master-side state on lost ACK unspecified; candidate committed-but-never-activated vs split view. | E |
| H3b | `trd/page8_mitm_security.md:60-64` vs `requirements.md:120-123` | TRD whitelist omits Condition 2 ("Authorized virtual proxy in active NAT table"); Tier-2 dynamic-alias sources (`0x7800..0x7FFF`) fail Condition 3 slot-range check and are dropped as spoofed. | E |
| H3c | `trd/page4_store_forward_router.md:104` vs `page6:61-66` | `key_epoch` in queue entry is defined as ingress epoch, but egress RPL is keyed `(net_idx,src_addr,key_epoch)`; no spec for which epoch is stamped at egress when ingress X is Phase 2 and egress Y is Phase 0. `meshx_subnet_crypto_slot_t` never linked to queue entry. | E |
| H3d | `trd/page6_key_rotation.md:63` | Table couples INGRESS acceptance to EGRESS subnet KR phase: if Y is Phase 0 while X rotates to Phase 2, row 1 rejects all forwarded traffic during X's rotation — violates page6:19 cascade isolation. | E |
| H3e | `trd/page10_multihop_transit.md:76-84` | `meshx_uvp_route_adv_t` carries no `active_term`/`Term`, yet HMAC input requires `Term` (page8:45, requirements.md:93). Stale demoted bridge's route can outlive fence within 2.0s timestamp window. | E |
| H3f | `trd/page10_multihop_transit.md:81` | `nonce` "monotonic counter" has no NVS persistence (same gap as C3b); nonce-cache scope for 0xD6 unspecified. Route-adv replay from rebooted bridge passes if old nonce aged out. | E |
| H3g | `trd/page4_store_forward_router.md:33`, `:35` | Unacknowledged models (Generic OnOff Set) have no Model timer; standard BT Mesh transport frees PDU after TX; `0xD7` finds nothing to retransmit. "MeshX transport shim consumes 0xD7" (requirements.md:85) but no element/model defined to receive a UVP opcode. | F |
| H3h | `trd/page4_store_forward_router.md:88` | NACK storm: queue-full rejects emit one `0xD7` per packet, no rate limit, no coalescing per `(src, err_reason)`. | F |
| H3i | `trd/page4_store_forward_router.md:88` | Retransmission after `0xD7` re-enters Stage 1 as new flow; no dedup key (`orig_subnet_id||orig_seq_num` both new). Non-idempotent models double-apply. | F |
| H3j | `trd/page7_redundancy_failover.md:54-58` | Demotion is self-enforced state change with no external enforcement; faulty loser holding `K_bridge` keeps forwarding with valid HMAC, producing duplicate delivery with no cross-bridge dedup. | F |
| H3j | `trd/page7_redundancy_failover.md:86` | `uint16_t active_term` truncates the 32-bit `active_term` counter (page7:46); at 65,536 takeovers comparison inverts and stale terms accepted. | F |
| H3k | `trd/page7_redundancy_failover.md:95` | Loop bit index `K` undefined: `1 << K` on a 16-bit mask with 16-bit `global_subnet_id` is UB for K ≥ 16; distinct islands can map to same bit → false loop drops. `K` must be local `slot_id` from registry with cross-bridge slot disagreement documented. | F |
| H3l | `trd/page7_redundancy_failover.md:65` | Watchdog resets on "any valid, authenticated heartbeat" — but page8:46 accepts frames within 2.0s timestamp delta, so chained replays push detection past 15.5s. | F |
| H3m | `trd/page9_latency_memory.md:27` | "3 missed 4s heartbeats + takeover" = 16.0s detection, arithmetically inconsistent with 12.0s watchdog (2 missed beats). Label wrong. | F |
| H3m | `trd/page2_addressing_nat.md:80` | NAT audit + route re-adv on takeover not budgeted inside ~200s takeover (page7:66); serial multi-hop failover can exceed 15.5s. | F |
| H3m | `trd/page7_redundancy_failover.md:26` | "Passive / Radio Mute" conflicts with Standby needing RX for `0xD1`/`0xD5`; mute scope undefined. | F |
| H3n | `trd/page5_discovery_pairing.md:77` | Bilateral suspension deadlock: both sides suspend → no traffic to trigger resume; no arbitration/resume probe. | F |
| H3o | `trd/page9_latency_memory.md:67` | NVS budget omits per-subnet KR metadata (`kr_phase`, `current_epoch`, `iv_index`, `last_seq_num`, ≥13 B × 3), cross-island pairing bindings (~16 B each), sequence counters (4 subnets × 4 B), and NVS framing overhead (~32 B × entries). Real total likely exceeds 1,024 B. | F |
| H3p | `tasks.md:66` | Authenticates only `0xD1/0xD2/0xD5`; omits `0xD6`/`0xD7`, contradicting page8:41 / requirements.md:93. `0xD8` EPOCH_SYNC unauthenticated → spoofed `0xD8` forces `LINK_STATE_SUSPENDED` DoS. | F |

### MEDIUM / LOW (unresolved)

| # | File:line | Problem | Reviewer |
|---|---|---|---|
| M3a | `trd/page8_mitm_security.md:45-46` | Nonce cache scope undefined (per-Sender_Addr vs per-bridge vs global); independent bridges have independent counters → global cache false rejects. | E |
| M3b | `trd/page10_multihop_transit.md:83` | `hmac_signature[16]` truncated HMAC-SHA256; truncation rationale unstated. | E |
| M3c | `requirements.md:54` | Stale text "16-slot group band formula" — actual is 14 bands (`n ∈ [0..13]`). | E |
| M3d | `trd/page9_latency_memory.md:47,69` vs `page11:33` | RAM/NVS size slot registry at 16 while type is `slots[14]`, `next_free_slot` bounded 0..13. Untracked 2 slots. | E |
| M3e | `trd/page9_latency_memory.md:46` vs `page2:61` vs `tasks.md:62` | Three irreconcilable NAT table sizes: 16 entries / 384 B (page9) vs 32 entries / 512 B (page2) vs "16 prefix slots + 512 dynamic aliases" (tasks). | E |
| M3f | `trd/page2_addressing_nat.md:79` | "carrying the full 16-bit session key" — K_bridge is 128-bit; no 16-bit session key exists. Wording error. | E |
| M3g | `trd/page2_addressing_nat.md:56,59` vs `page8:63-64` | Source-truncation residual: `(phys_addr & 0x03FF)` has no guard; Tier-2 reroute for `phys_addr >= 1024` is narrative, not enforced invariant. Node 0x0405 collides with 0x0005's proxy in same slot; transit whitelist can't distinguish. | E |
| M3g | `trd/page10_multihop_transit.md:89-93` | HMAC verification-before-cache only implied by enumeration order; no normative "MUST verify signature AND nonce-freshness before transit-table mutation". | E |

---

## Verified-Clean (Round 1 & Round 2 fixes hold)

| Area | Status |
|---|---|
| K_bridge HMAC covers `0xD1/0xD2/0xD5/0xD6/0xD7` with nonce+timestamp (page8:41-46) | Fixed |
| `active_term` fencing in authenticated transit header; immediate demote on `term > current_term` (page7:45-58) | Fixed |
| Transit whitelist Condition B with `popcount(visited_subnets_mask) >= 1` + slot-range cross-check (page8:58-66) | Fixed |
| Two-tier NAT: Tier-1 `0x4000..0x77FF` (14 slots) disjoint from Tier-2 `0x7800..0x7FFF` (page2:43-49) | Fixed |
| Bounded slot registry 0..13 with Tier-2 overflow (page11:25,33,39) | Fixed |
| Bounded CRC16 remap loop (page2:15-24) | Fixed |
| 14-band group formula bounded below `0xFC00` (page3:28,41-43) | Fixed |
| Single-epoch reassembly context; key_epoch widened to `uint32_t` (page4:78-81,104) | Fixed |
| RPL keyed `(net_idx, src_addr, key_epoch)`; seq counter persisted with +512 reboot margin (page8:72-75) | Fixed |
| EPOCH_SYNC: 3.0s timeout + 3 retries; offline → `LINK_STATE_SUSPENDED` (page5:77) | Fixed |
| Drop-newest pre-ACK queue overflow with `0xD7` backpressure (page4:84-89) | Fixed |
| Failover 12.0s watchdog / ~200ms takeover / 12.0–12.5s total ≤ 15.5s (page7:62-69) | Fixed |
| Keyed echo filter with `K_bridge[0..3]` digest + 5s TTL (page7:98-102) | Fixed |
| 0xD6 authenticated with nonce/timestamp/HMAC (page10:73-84) | Fixed |

---

## Scorecard

| Reviewer | Model | Verdict |
|---|---|---|
| E | opencode/glm-5.3-flash-free | CONDITIONAL |
| F | opencode/deepseek-v4.1-flash-free | FAIL |

**Round 3 verdict: FAIL** — 8 CRITICAL defects remain open (TransMIC nonce contradiction, nonce/timestamp persistence and clock domain, 0xD7 fire-and-forget with no flow correlation, multi-hop NACK return path, queue entry missing seq/flow fields, NAT standby alias recovery, memory-budget breach by spec's own structs). The spec tree is not ready for implementation.

---

## Recommended Remediation Order

1. **Fix CRITICAL C3a/C3b/C3c** (TransMIC nonce boundary; nonce persistence; clock domain) — breaks E2E auth and control-plane liveness.
2. **Fix C3d/C3e/C3f/C3g** (0xD7 reliability, NACK correlation, multi-hop return, NAT alias recovery) — breaks failover/reliability.
3. **Fix C3h** (RAM budget arithmetic breach) — invalidates REQ-NF01.1.
4. Then address HIGH items H3a–H3p.
