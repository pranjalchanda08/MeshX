# Consolidated Audit & Remediation Report: Island Subnet Coexistence (Round 7)

**Audit Target:** `.specs\island-subnet-coexistence`
**Parent Reviews:** Remediates all remaining gaps and open ends from [REVIEW_ROUND5_FINDINGS.md](REVIEW_ROUND5_FINDINGS.md) and Round 6 audits.
**Overall Verdict: PASS (100% READY FOR IMPLEMENTATION)**

---

## 1. Resolution of Identified Open Ends

| Item | Problem Description | Resolution & Document Updates | Status |
|---|---|---|---|
| **NAT Sizing Conflicts** | 16 entries $\times$ 24 B (384 B) vs 26 B (416 B). | **Resolved in `trd/page2:67`, `trd/page9:49`, and `requirements.md:20`**: `meshx_nat_entry_t` contains 26 bytes packed (with `egress_net_idx` included). $16 \times 26\text{ B} = 416\text{ B}$. Reconciled identically across all documents. | **PASS** |
| **`0xD4_ACK` & Opcode Gaps** | `0xD4_ACK` lacked dedicated numeric wire opcode; `BRIDGE_DEMOTE`, `DISCOVER_REQ/RSP`, and `QUERY_SUBNET_DIRECTORY` had no formal numeric allocation. | **Resolved in `trd/page8:2.0` (Unified Opcode Table), `requirements.md:93-94`, and `tasks.md:69-71`**: Assigned explicit opcodes: `0xDA` (`MESHX_UVP_OP_BRIDGE_INVITE_ACK`), `0xDB` (`MESHX_UVP_OP_BRIDGE_DEMOTE`), `0xDC` (`MESHX_UVP_OP_DISCOVER_REQ`), `0xDD` (`MESHX_UVP_OP_DISCOVER_RSP`), and `0xDE` (`MESHX_UVP_OP_QUERY_SUBNET_DIRECTORY`). | **PASS** |
| **`meshx_pair_record_t` Undefined** | Persistent pair record referenced in `page2` and `page4` lacked formal C struct definition. | **Resolved in `trd/page5:3`**: Added packed definition of `meshx_pair_record_t` (canonical addresses, global subnet IDs, virtual proxy, AppKey index, function ID, and link epoch) detailing destination TransMIC recovery and standby bridge takeover reconciliation. | **PASS** |
| **`visited_subnets_mask` Bit Indexing** | Local slot indices ($K \in [0..13]$) differ across bridges; bitmask checking could cause false drops. | **Resolved in `trd/page7:4.2` and `trd/page10:55-66`**: Standardized multi-hop mask update rules where bridges record both ingress and egress slot visits (`(1 << K_in) \| (1 << K_out)`), backed by the 32-entry Echo Filter as the authoritative loop-drop backstop. | **PASS** |
| **Transit-Frame `active_term` Auth** | Forwarded transit frames carried `active_term` without explicit verification semantics. | **Resolved in `trd/page7:56-61`**: Transit frames are authenticated under transit subnet network encryption (`NetKey_Transit`) or $K_{\text{bridge}}$ HMAC. Term comparison is strictly scoped to `boundary_id == local_boundary_id`; orthogonal upstream boundaries bypass term drops. | **PASS** |
| **`0xFC00..0xFEFF` Labeling Mismatch** | `requirements.md` and `design.md` labeled `0xFC00..0xFFFF` as fixed BLE Mesh addresses. | **Resolved in `requirements.md:57` and `design.md:71`**: Aligned with Bluetooth Mesh v1.1 §3.4.2.4: `0xFF00..0xFFFF` are fixed group addresses; `0xFC00..0xFEFF` is unallocated dynamically assignable group space. Group formula derives up to `0xFBFF`; overflow uses dynamic 1:1 translation. | **PASS** |
| **Transit Header Size Discrepancy** | `requirements.md:107` stated 21 bytes, but struct fields sum to 23 bytes packed. | **Resolved in `requirements.md:107`**: Updated text from 21-byte to 23-byte packed transit header, exactly matching `sizeof(meshx_subnet_transit_hdr_t)` in `trd/page7:88-99`. | **PASS** |
| **Static RAM Budget Claim** | Inconsistency between static BSS RAM and peak RAM claims. | **Resolved in `requirements.md:138`, `design.md:85`, and `trd/page9:57-61`**: Formally unified across all documents: Persistent static BSS RAM is $2,875\text{ B}$ ($\le 3.0\text{ KB}$), dynamic queue peak is $592\text{ B}$, yielding worst-case peak RAM of **$3,467\text{ B}$** ($\le 3.5\text{ KB}$). | **PASS** |
| **Genesis Remap Seed & Collision Fallback** | `page2:21-25` seed buffer was undeclared and fallback offset modulo 128 was not collision-proof. | **Resolved in `trd/page2:15-30`**: Declared 20-byte seed `[anchor_uuid (16 B) \| net_key_epoch_0 (4 B)]`. Fallback rule computes `crc16_ccitt(anchor_uuid, 16, 0xA5A5)` in range `0x0002..0xFFFE`, eliminating modulo-128 collisions. | **PASS** |
| **B1 TransMIC Nonce for Segmented PDUs** | Clarified whether Lower Transport segmentation affects Upper Transport TransMIC nonce. | **Resolved in `trd/page4:80-101`**: Clarified that under BLE Mesh v1.1 §3.8.4, TransMIC is computed **once per Upper Transport Access PDU** using the 24-bit `SeqAuth` (sequence number of first segment) and invariant `(CANONICAL_SRC, CANONICAL_DST)`. Lower Transport reassembly at the bridge and destination strips segment headers before TransMIC authentication, ensuring segmented Store-and-Forward traffic verifies with 100% cryptographic fidelity. | **PASS** |

---

## 2. Final Readiness Summary

All 10 open ends, data structure discrepancies, and wire opcode assignments have been resolved, verified, and synchronized across:
- `requirements.md`
- `design.md`
- `tasks.md`
- `trd/page1_overview_topologies.md` through `trd/page11_slot_assignment.md`

**Status:** Ready to proceed with implementation tasks.

