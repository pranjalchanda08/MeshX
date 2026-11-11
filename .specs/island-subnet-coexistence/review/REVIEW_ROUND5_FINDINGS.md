# Consolidated Report: .specs\island-subnet-coexistence

**Seven free‑model reviews (Rounds 2‑4): all FAIL. No consensus PASS.**

## 3 Structural Blockers (Round 4, cannot be fixed by minor edits)

| Block | File:Line | Why it blocks implementation |
|---|---|---|
| **B1** | `page4:81‑94` | E2E TransMIC nonce unconstructible at destination: bridge rewrites SRC/DST but cannot re‑sign → TransMIC fails on every cross‑subnet PDU. |
| **B2** | `page4:32`, `page8:60‑66` | `K_bridge`‑authenticated frames (`0xD7`, `0xD8`) terminate at ordinary mesh nodes (switch 0x0002, lamps) that never receive the bridge cluster key → those nodes cannot verify HMAC or emit a valid `0xD7_ACK`. Control‑plane liveness impossible at the edge. |
| **B3** | `page7:50,89‑102` | `active_term` / `visited_subnets_mask` comparisons are globally scoped, but each boundary maintains independent counters → downstream bridge permanently drops all frames from an upstream boundary with a lower term. Failover/fencing cannot work across >1 bridge boundary. |

## Pervasive Arithmetic / Scope / Opcode Inconsistencies

| Category | File:Lines | Issue |
|---|---|---|
| **RAM budget** | `page9:44‑49` | Queue entry ≈140 B (not 128 B), RPL entry ≥12 B (not 10 B). Computed total ≈3,720 B > 3,584 B ceiling → REQ‑NF01.1 breached. Three‑way total disagreement across design.md / page9 / requirements.md. |
| **NAT reverse key** | `page2:73‑81` | REQ‑F03.2 defines reverse key as `(egress_net_idx, virtual_proxy_addr)`, but `meshx_nat_entry_t` omits `egress_net_idx` → reverse translation unkeyable as specified. |
| **Standby alias recovery** | `page2:67` | Standby cannot reconstruct Tier‑2 alias on first return packet: hash input includes `phys_dst_addr` and `global_subnet_dst`, which a return packet does not reveal. |
| **Missing opcodes** | `page4:32`, `requirements.md:93`, `tasks.md:67` | `0xD7_ACK` opcode never assigned (opcodes only 0xD1‑0xD8). `0xD3` and `0xD7_ACK` omitted from authenticated‑frame list. `0xD8` EPOCH_SYNC unauthenticated → spoofed `0xD8` forces `LINK_STATE_SUSPENDED` DoS. |
| **`active_term` boot rule** | `page7:46‑48` | No boot rule for `active_term`; rebooted Active re‑claiming a stale/equal term relies on unspecified tie‑break. |
| **Group‑band labeling** | `page3:42` | `0xFC00‑0xFEFF` mislabeled as reserved; only `0xFF00‑0xFFFF` is reserved in BLE Mesh. |
| **16‑slot remnant** | `requirements.md:57` | Stray “Slots 14 and 15 fallback” — only 14 slots (0..13) exist per design. |
| **Remap loop seed** | `page2:20‑21` | `crc16_ccitt(seed, sizeof(seed), nonce++)` — seed unchanged across iterations; if `nonce` is the init value the loop recomputes an identical ID 9 times before falling back. |
| **Crypto table KR metadata** | `page9:74` | “4 subnets × 16 B = 64 B” cannot persist mid‑rotation state: Phase 1/2 requires both `current_key` and `new_key` (32 B/subnet = 128 B). |

## Cross‑Round Themes (All 7 Reviewers Identified)

- **C3 split‑brain fencing**: `active_term` carried in forwarding frames but not HMAC’d; no quorum; demotion is soft “MUST demote within 100 ms” with no enforcement.
- **C4 K_bridge**: lacks replay protection, commit‑before‑activate, revocation‑erase, per‑bridge isolation.
- **C1 transit whitelist**: unsatisfiable for forward multihop hop — no NAT binding for transit source proxy, no defined transit‑header creation.
- **R1 two‑tier NAT**: Tier‑2 window `0x7E00‑0x7FFF` collides with Tier‑1 slot 15 `0x7C00‑0x7FFF`; blows 3.5 KB RAM budget (528 entries ≫ cap).
- **C2 premature Stage 1 ACK**: silently loses unacknowledged‑model traffic; `0xD7` NACK is fire‑and‑forget, unauthenticated, no defined multihop return path.
- **0xD7 fire‑and‑forget**: no ACK/retry, not in `K_bridge`‑authenticated set, lost 0xD7 = silent loss even for acknowledged models.
- **Hop_limit initialization**: undefined; `trd/page7:83` says “max 4” but initial value unspecified. Chain of 4 bridge hops exhausts a 4‑init; “max 4” yields only 3 usable hops.
- **Memory‑budget contradictions**: 16 / 32 / 512 NAT table sizes; queue‑entry size disputes; NVS budget omits KR metadata, sequence counters, framing overhead.

## Key Missing / Opcodes

- `0xD7_ACK` opcode value never assigned.
- `0xD3` (BRIDGE_CANDIDATE_REPORT) and `0xD7_ACK` omitted from authenticated‑frame list.
- `0xD8` EPOCH_SYNC unauthenticated → spoofed `0xD8` forces `LINK_STATE_SUSPENDED` DoS.

## Final Verdict

**NOT READY** — The specification has three structural blockers (E2E TransMIC construction, `K_bridge` scope, per‑boundary `active_term` comparison) plus pervasive arithmetic/scope/opcode inconsistencies that make the budget, data structures, and wire formats incoherent across the spec set. Round 3 fixes introduced new conflicts when viewed across the full set. Implementation should be postponed until the structural blockers are resolved — this is not a round of incremental edits.

*Excluding `IMPLEMENTATION_READINESS_REPORT.md` as requested. All findings from `design.md`, `requirements.md`, `tasks.md`, all `trd/*.md` pages, and Review Rounds 2‑4.*