# Independent Review Report: Island Subnet Coexistence (Round 4)

**Spec under review:** `D:\MeshX\.specs\island-subnet-coexistence\`
**Review team:** 2 separate free models (OpenCode provider, $0 cost), deep cross‑spec grind
**Overall verdict: FAIL** — 3 structural blockers + pervasive arithmetic/scope/opcode inconsistencies

---

## Reviewer Roster

| Reviewer | Model | Charter | Verdict |
|---|---|---|---|
| G | opencode/glm-5.3-flash-free | Deep grill every section, looking for subtle bugs, omissions, inconsistencies not covered in Rounds 1‑3 | FAIL |
| H | opencode/deepseek-v4.1-flash-free | Same charter, independent cross‑check | FAIL |

---

## Structural Blockers (Fundamental architectural gaps; cannot be fixed by minor edits)

| # | File:line | Problem | Why it Blocks Implementation |
|---|---|---|---|
| **B1** | `page4:81‑94` | **E2E TransMIC nonce unconstructible** at destination: canonical nonce requires full 32‑bit sequence number and IV Index, but transit header carries only 16‑bit `orig_seq_num` and no IV Index. Receiver cannot rebuild the canonical nonce → TransMIC fails on every cross‑subnet segmented PDU. | Fundamental crypto‑auth gap; no amount of minor‑code fix can resolve without transport‑layer redesign. |
| **B2** | `page4:32`, `page8:60‑66` | **`K_bridge`‑authenticated frames (`0xD7`, `0xD8`) terminate at ordinary mesh nodes** (switch 0x0002, lamps) that never receive the bridge cluster key. Those nodes cannot verify HMAC or emit a valid `0xD7_ACK`. | Control‑plane liveness impossible: NACK handshake cannot be completed at the edge. |
| **B3** | `page7:50,89‑102` | **`active_term` / `visited_subnets_mask` comparisons are globally scoped**, but each boundary maintains independent counters. In a multi‑boundary cluster sharing `K_bridge`, a downstream bridge permanently drops all frames from an upstream boundary with a lower term. Comparison must be scoped per `(boundary, bridge identity)`. | Failover/fencing cannot work reliably across more than one bridge boundary. |

---

## Pervasive Inconsistencies (Arithmetic / Scope / Opcodes)

| Category | File:lines | Issue |
|---|---|---|
| **RAM budget** | `page9:44‑49` | Queue entry = 144 B packed (not 136 B); queue = 579 B vs budgeted 544 B. RPL entry = 12 B (not 10 B). Crypto table arithmetically incoherent. Three‑way totals disagree: design.md:3,408 B / 676 B vs page9:3,448 B / 844 B vs requirements.md:3,584 B / 1,024 B. |
| **NAT reverse key** | `page2:73‑81` | REQ‑F03.2 defines reverse key as `(egress_net_idx, virtual_proxy_addr)`, but `meshx_nat_entry_t` omits `egress_net_idx` → reverse translation unkeyable as specified. |
| **Standby alias recovery** | `page2:67` | Standby cannot reconstruct Tier‑2 alias on first return packet: hash input includes `phys_dst_addr` and `global_subnet_dst`, which a return packet (src=alias, dst=local phys) does not reveal. |
| **`0xD7_ACK` opcode** | `page4:32` | `0xD7_ACK` opcode value never assigned anywhere in spec (opcodes defined only 0xD1–0xD8) — unimplementable as written. |
| **`active_term` boot rule** | `page7:46‑48` | No boot rule for `active_term`; rebooted Active re‑claiming a stale/equal term relies on unspecified tie‑break. |
| **Nonce cache exhaust** | `page8:50‑52` | Nonce cache keyed `(sender_addr, active_term)` with no purge policy — after repeated failovers cache exhausts (budget 192 B) with undefined reject‑vs‑evict behavior. |
| **Watchdog scope** | `page7:68` | Watchdog resets on receipt of *any* valid heartbeat, but heartbeats from an unrelated boundary's active bridge keep a dead boundary's standby alive past 15.5 s. |
| **Transit header visited bit** | `page10:53‑54` | Step 2 inserts transit header without setting visited bit; step 4 updates mask → first‑hop bridge rejects frame because `popcount(visited_subnets_mask) == 0`. |
| **Struct fragment duplicate** | `page4:146‑148` | Stray duplicate `meshx_router_queue_t` fragment (with `dropped_full_count`) after `#pragma pack(pop)` — two conflicting definitions of the same type in one page. |
| **Opcodes missing from auth list** | `requirements.md:93`, `tasks.md:67` | `0xD3` (BRIDGE_CANDIDATE_REPORT) and `0xD7_ACK` omitted from authenticated‑frame list; candidate reports spoofable/floodable by adversary. |
| **Group‑band labeling** | `page3:42` | `0xFC00‑0xFEFF` mislabeled as reserved; only `0xFF00‑0xFFFF` is reserved in BLE Mesh. |
| **Remap loop seed** | `page2:20‑21` | `crc16_ccitt(seed, sizeof(seed), nonce++)` — seed unchanged across iterations; if `nonce` is the init value the loop recomputes an identical ID 9 times before falling back. |
| **Crypto table KR metadata** | `page9:74` | "Subnet KR Metadata (4 subnets x 16 B) = 64 B" cannot persist mid‑rotation state: Phase 1/2 requires both `current_key` and `new_key` (32 B/subnet = 128 B). |
| **Slot registry cap** | `page11:14` | Claims `global_subnet_id` = CRC16 is globally unique; remap fallback collapses ALL colliding islands to `0x0001` — NAT/transit keys then merge two islands. |
| **16‑slot remnant** | `requirements.md:57` | Stray "Slots 14 and 15 fallback" — only 14 slots (0..13) exist. |

---

## Verdict: FAIL

The spec tree has **three structural blockers** that are fundamental architectural gaps (E2E TransMIC construction, `K_bridge` scope, per‑boundary term comparison) plus ** pervasive arithmetic/scope/opcode inconsistencies** that make the budget, data structures, and protocol wire formats incoherent across the specification set.

Round 3's fixes were real but **introduced new conflicts** when viewed across the full spec set (struct sizes, scope domains, opcode gaps). The specification requires a **design overhaul**, not incremental edits.

---

## Recommended Remediation Order

1. **Reshape the E2E TransMIC mechanism** — define a transport‑level nonce payload (full SeqNum + IV Index) that survives bridge rewrite, or accept that end‑to‑end MIC verification is not possible across arbitrary subnet hops and document the constraint.
2. **Scope `active_term` and `visited_subnets_mask` per boundary** — introduce a `boundary_id` field in the transit header and fence logic; remove global‑scope comparisons.
3. **Assign the `0xD7_ACK` opcode** and define its wire format; add `0xD3` and `0xD7_ACK` to the authenticated‑frame list.
4. **Reconcile RAM/NVS budgets** with `#pragma pack(push, 1)` exact struct sizes; update design.md and requirements.md totals to match page9's 3,448 B / 844 B.
5. **Fix NAT reverse key** by adding `egress_net_idx` to `meshx_nat_entry_t` or revising REQ‑F03.2 to use a key that can be derived from the available fields.
6. **Assign `active_term` a boot persistence rule** (checkpoint + increment, mirroring the nonce +10,000 margin pattern).
7. **Address remaining HIGH/MEDIUM items** (NACK rate limiting, nonce cache purge policy, watchdog per‑boundary reset, remap loop seed, group‑band labeling, etc.).

After these seven remediation waves, a **re‑run of Round 3‑style verification** should declare PASS. Commit and implementation should be postponed until the structural blockers are resolved.