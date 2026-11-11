# Requirements: Island Subnet Coexistence, Bridging, Multi-Island & Key Rotation

**Feature:** Island Subnet Coexistence, Non-Destructive Bridging, Redundancy, Multi-Island, KR Orchestration, MitM Defense, 2-Stage Store-and-Forward Routing & Subnet Prefix NAT (Case 4)  
**Issue Link:** https://github.com/pranjalchanda08/MeshX/issues/162  
**Subsystems:** `meshx_zgb`, `meshx_platform`, `meshx_uvp_dispatcher`, `meshx_mxcp`, `meshx_crypto`, `meshx_nvs`, `meshx_virtual_topo`, `meshx_pair_orchestrator`  
**Status:** In Progress / Remediated (Round 2)  
**Parent Specs:** [.specs/zero-gateway-bootstrapping/](..//zero-gateway-bootstrapping/feature.md), [.specs/autonomous-element-pairing/](../autonomous-element-pairing/requirements.md)  
**Review Cross-Ref:** Remediates all findings in [REVIEW_ROUND2_FINDINGS.md](REVIEW_ROUND2_FINDINGS.md)  

---

## 1. Executive Summary

In a Zero-Gateway Bootstrapping (ZGB) environment, two or more provisioned mesh islands can bootstrap out of mutual RF proximity. Each island independently establishes its own Anchor at unicast `0x0001`, its own primary `NetKey`, and populates unicast addresses within the range `0x0001..0x7FFF`. When these islands later move into RF proximity or expand until their RF coverage overlaps, Bluetooth Mesh Network Layer decryption fails across islands (causing continuous MIC verification failures) because UVP and standard mesh network frames are encrypted using the respective island's NetKey.

Standard re-provisioning to merge islands into a single NetKey is destructive: it wipes all element models, node subscriptions, publication addresses, scenes, groups, and device keys.

This specification governs the architecture and implementation of **non-destructive island subnet coexistence**:
1. **Target Discovery & Initial Address Learning**: How nodes learn about target addresses across islands: (a) Intent-driven mobile/gateway commissioning, (b) 2-Stage Subnet Virtual Group Broadcast Discovery (`0x8000`), or (c) Router Capability Directory announcements.
2. **Subnet Prefix Virtual NAT (Non-Overlapping Allocation)**: Strict non-colliding partitioning of the 15-bit unicast address space (`0x0001..0x7FFF`) into Local Device Space (`0x0001..0x3FFF`, 16,383 addresses), Tier-1 Static Subnet Proxy Slots (`0x4000..0x77FF`, 14 slots $\times$ 1,024 elements), and Tier-2 Dynamic Flow Aliases (`0x7800..0x7FFF`, 2,048 aliases with bounded 16-entry active RAM footprint).
3. **Autonomous Group Address Derivation & Cross-Subnet Translation**: Full interoperability with the Anchor Group Formula (`0xC000 + (Anchor & 0x03FF)`), where group addresses derived locally in Island A (`0xC000..0xC3FF`) are translated and scoped across subnets via explicit Router Group Translation Mappings (`0xC000_A` $\leftrightarrow$ `0xC400_B`) bounded by `band(n) = 0xC000 + ((n + 1) << 10)`.
4. **2-Stage Store-and-Forward Router Architecture**: Hop-by-hop transport flow control where the bridge acts as a true router: receiving segments on Subnet A, acknowledging Lower Transport ACKs locally, queueing Upper Transport PDUs (with TransMIC preserved end-to-end), and independently dispatching across Subnet B with fresh SAR segmentation and reliable `0xD7` NACK failure reporting.
5. **Authenticated Fencing & Active-Standby Redundancy ($N+1$)**: Eliminating single points of failure via coordinated Active/Standby bridge pairs with 4.0s heartbeats (`0xD1`), 12.0s failover watchdogs (`0xD2`), authenticated fencing epoch barriers (`active_term`), deterministic demotion tie-breaking, and 16-bit Subnet Path Vector loop prevention.
6. **Decoupled Key Rotation & Phase Isolation**: Efficient multi-island KR tracking using O(1) crypto-slot indexing, single-epoch reassembly context quarantine, burst isolation, and non-blocking phase transitions.
7. **Comprehensive Man-in-the-Middle (MitM) Defense**: Replay-protected HMAC-SHA256 control frame authentication using $K_{\text{bridge}}$ (with monotonic nonce/timestamp), DevKey-encrypted commissioning (`0xD4`), NVS erase on revocation, and multihop transit whitelist validation.
8. **Strict Resource & Latency Alignment**: Total static RAM strictly bounded to **$\le 3.5\text{ KB}$**, NVS flash storage to **$< 1.0\text{ KB}$**, and failover takeover timing to **$12.0\text{s} \dots 12.5\text{s}$ ($\le 15.5\text{s}$)**.

---

## 2. Functional Requirements (REQ-F)

### REQ-F01: Cross-Island Target Address Discovery
- **REQ-F01.1: Discovery Modality 1 — Intent-Driven Commissioning (User/App Initiated)**:
  - When an operator or smartphone connects to the mesh via GATT proxy or MXCP serial:
    - The commissioner queries both Subnet A and Subnet B directories.
    - The commissioner computes the virtual proxy address:
      $$\text{virtual\_addr} = 0\text{x4000} \mid (\text{slot\_B} \ll 10) \mid (\text{lamp\_addr} \ \& \ 0\text{x03FF})$$
    - The commissioner instructs Switch `0x0002` to probe and bind `virtual_addr`.
- **REQ-F01.2: Discovery Modality 2 — Autonomous Virtual Group Discovery Broadcast (`0x8000`)**:
  - When Node `0x0002` enters autonomous pairing mode (physical button hold):
    - It emits `MESHX_UVP_OP_DISCOVER_REQ` to `0x8000` (Management Virtual Address) with a 16-bit Transaction ID (`trans_id`).
    - The **Bridge Router** receives the `0x8000` broadcast on Subnet A and forwards it into Subnet B over `NetKey_B`.
    - Node `0x0005` on Subnet B responds with `MESHX_UVP_OP_DISCOVER_RSP`.
    - The **Bridge Router** intercepts the response, isolates concurrent pairing sessions by keying discovery aliases using `(global_subnet_src, requestor_addr, trans_id, remote_phys_addr)`, rewrites the source address to the assigned virtual proxy address, and transmits it into Subnet A.
    - Switch `0x0002` receives the response: source header contains the virtual proxy address without collision.
- **REQ-F01.3: Discovery Modality 3 — Bridge Router Directory Synthesis**:
  - The Bridge Router periodically synthesizes a list of active consumers on neighboring subnets (`meshx_subnet_directory_t`), queryable via `MESHX_UVP_OP_QUERY_SUBNET_DIRECTORY`.

### REQ-F02: Autonomous Group Address Derivation & Subnet Translation
- **REQ-F02.1: Subnet-Scoped Group Domain Isolation (Default Rule)**:
  - Group addresses (`0xC000..0xFEFF`) derived via the Anchor Formula ($0\text{xC000} \mid (\text{Anchor\_Unicast} \ \& \ 0\text{x03FF})$) SHALL remain isolated to their respective local NetKey broadcast domain by default.
  - The router SHALL NOT forward group multicast frames across subnets unless an explicit Group Translation Rule is active.
- **REQ-F02.2: Subnet Group Offset Prefixing & Band Allocation**:
  - The 14-band group formula is strictly bounded to the multicast window `0xC000..0xFBFF`:
    $$\mathbf{\text{band}(n) = 0\text{xC000} + ((n + 1) \ll 10)} \quad \text{for } n \in [0..13]$$
  - Slots $n \in [0..13]$ map to non-overlapping 1,024-address group blocks (`0xC400..0xFBFF`).
  - Overflow requests beyond the 14 static slots fallback to explicit dynamic group translation mappings, preventing overflow into unallocated (`0xFC00..0xFEFF`) and fixed BLE Mesh addresses (`0xFF00..0xFFFF`).
- **REQ-F02.3: Dynamic Group Translation Execution**:
  - When a switch on Island A is paired to lamps on both Island A and Island B:
    - Router registers Group Translation: $\{ \text{NetKey\_A}, 0\text{xC001} \} \longleftrightarrow \{ \text{NetKey\_B}, 0\text{xC401} \}$.
    - Local lamps on Island A execute `0xC001`; remote lamps on Island B execute `0xC401`.

### REQ-F03: Subnet Prefix Virtual NAT (Non-Overlapping Allocation)
- **REQ-F03.1: Address Space Segmentation (15-Bit Unicast `0x0001..0x7FFF`)**:
  - **Local Physical Node Space (`0x0001..0x3FFF`, 16,383 addresses)**: Assigned to physical hardware elements provisioned directly within that island.
  - **Tier-1 Static Virtual Subnet Slots (`0x4000..0x77FF`, 14 slots $\times$ 1,024 elements)**: Assigned to foreign islands with $\le 1,024$ physical elements.
  - **Tier-2 Dynamic Flow Aliases (`0x7800..0x7FFF`, 2,048 addresses)**: Reserved exclusively for overflow, large physical element addresses ($> 1,024$), and transit proxy sessions.
  - **Strict Reservation Invariant**: Tier 1 and Tier 2 ranges are completely disjoint; Tier 2 addresses never intersect Tier 1 slot ranges.
- **REQ-F03.2: Bidirectional Flow NAT Keying**:
  - Dynamic NAT entries SHALL be keyed by full bidirectional session tuple:
    $$\mathbf{\text{Forward Key:} \ (global\_subnet\_src, phys\_src\_addr, global\_subnet\_dst, phys\_dst\_addr)}$$
    $$\mathbf{\text{Reverse Key:} \ (egress\_net\_idx, virtual\_proxy\_addr)}$$
  - Guarantees deterministic many-to-one reverse translation without reply blackholing.

### REQ-F04: 2-Stage Store-and-Forward Router Architecture & Reliability
- **REQ-F04.1: Stage 1 Ingress Termination & Hop Flow Control**:
  - Ingress segmented packet arrives on Subnet A $\rightarrow$ router reassembles segments locally into a reassembly buffer.
  - Reassembly context SHALL be tagged with the active `epoch_id` used for ingress decryption. Any subsequent segment decrypted under a mismatched key epoch SHALL be rejected, preventing mixed-key reassembly corruption.
  - Upon full reassembly, router emits Lower Transport Block ACK (`Segment ACK`) directly to the Subnet A sender as **hop flow control**.
- **REQ-F04.2: Upper Transport TransMIC Preservation**:
  - The router operates at Network and Lower Transport layers. The Upper Transport PDU TransMIC (calculated with `AppKey_Shared`) SHALL be preserved byte-for-byte end-to-end.
- **REQ-F04.3: Stage 2 Egress Dispatch & Reliable NACK Feedback**:
  - Router allocates a fresh `SeqZero` from destination subnet pool, segments Upper PDU, and transmits over `NetKey_B`.
  - Router monitors destination Transport Block ACK with up to 3 retries.
  - If Stage 2 fails: Router emits `MESHX_UVP_OP_ROUTER_NACK` (`0xD7`) authenticated with $K_{\text{bridge}}$ back to the originating sender via multi-hop reverse path routing. The transaction requires a `MESHX_UVP_OP_ROUTER_NACK_ACK` (`0xD9`) handshake. The MeshX transport shim intercepts `0xD7`, correlates with flow metadata (`orig_flow_id`), and triggers retransmission or surfaces failure to the application.
- **REQ-F04.4: Forwarding Queue Overflow & Backpressure**:
  - Dynamically-allocated FIFO flow queue bounded to 4 concurrent entries (`meshx_router_queue_t`).
  - When queue capacity is reached or allocation fails, router enforces **Drop-Newest pre-ACK**: rejects newly arriving packet before Stage 1 ACK emission and returns immediate `0xD7` (`ERR_QUEUE_FULL`) backpressure, rate-limited per sender to prevent NACK storms.

### REQ-F05: Authenticated Fencing & Active-Standby Redundancy ($N+1$)
- **REQ-F05.1: Periodic Heartbeat & Term Fencing**:
  - Active Bridge SHALL broadcast `MESHX_UVP_OP_BRIDGE_HEARTBEAT` (`0xD1`) every **$4.0\text{ seconds}$** carrying boundary identifier `boundary_id` and monotonic fencing token `active_term`.
  - All inter-bridge control frames (`0xD1`, `0xD2`, `0xD3`, `0xD5`, `0xD6`, `0xD7`, `0xD8`, `0xD9`, `0xDA`, `0xDB`) SHALL append a 16-byte HMAC signature calculated over `[Opcode || Term || Sender_Addr || Nonce || Payload]` using $K_{\text{bridge}}$. Replay protection is enforced via per-sender monotonic sliding window caches without requiring wall-clock RTC synchronization.
  - Ordinary edge nodes lacking $K_{\text{bridge}}$ receive and transmit edge control frames (`0xD7`, `0xD8`, `0xD9`, `0xDC`, `0xDD`, `0xDE`) authenticated under the local subnet's `NetKey` / `AppKey_Shared`.
- **REQ-F05.2: Watchdog Failover & Takeover Claim**:
  - Standby Bridges SHALL arm a **$12.0\text{ second}$ watchdog timer** (reset on every valid heartbeat matching the local `boundary_id`).
  - Upon expiration, Standby increments $\text{new\_term} = \text{last\_observed\_term} + 1$, promotes to Active, and broadcasts authenticated `MESHX_UVP_OP_BRIDGE_TAKEOVER_CLAIM` (`0xD2`) embedding `boundary_id`, `active_term`, `priority_rank`, and `bridge_unicast_addr`.
  - Total failover takeover completes in **$12.0\text{s} \dots 12.5\text{s}$ ($\le 15.5\text{s}$)**.
- **REQ-F05.3: Deterministic Boundary-Scoped Split-Brain Fencing**:
  - Fencing comparisons and takeover claims SHALL be evaluated strictly within the matching `boundary_id`. Frames from orthogonal boundaries SHALL NOT trigger term drops or demotions.
  - Any Active Bridge observing an authenticated claim with matching `boundary_id` and higher `active_term` SHALL demote to `BRIDGE_ROLE_STANDBY` immediately.
  - If two bridges claim identical `active_term` within the same `boundary_id`, each evaluates:
    $$\mathbf{\text{Score} = (\text{Priority\_Rank} \ll 16) \mid (\sim\text{Unicast\_Address} \ \& \ 0\text{xFFFF})}$$
  - The bridge with the lower score MUST emit `MESHX_UVP_OP_BRIDGE_DEMOTE` (`0xDB`) and demote to `BRIDGE_ROLE_STANDBY` within $100\text{ ms}$.

### REQ-F06: Multi-Island Graph Transit & Loop Prevention
- **REQ-F06.1: Subnet Transit Header (`meshx_subnet_transit_hdr_t`)**:
  - Cross-subnet frames SHALL encapsulate a 23-byte packed transit header carrying 16-bit `visited_subnets_mask`, 8-bit `hop_limit`, 16-bit `orig_subnet_id`, 16-bit `orig_phys_src`, 16-bit `orig_seq_num`, 32-bit `orig_seq_auth`, 32-bit `orig_iv_index`, 16-bit `boundary_id`, and 32-bit `active_term`.
  - Initial `hop_limit` SHALL be initialized to 15 by the originating bridge.
- **REQ-F06.2: Loop Prevention Execution**:
  - When a Bridge receives a transit frame on ingress subnet $S_{\text{in}}$ and attempts to forward into egress subnet $S_{\text{out}}$:
    - If `egress_global_subnet == hdr.orig_subnet_id` OR `hop_limit <= 1` OR Keyed Echo Filter hits within $5.0\text{s}$: **DROP**.
    - Otherwise: updates global hash telemetry `visited_subnets_mask |= (1U << (global_subnet_in & 0x0F)) | (1U << (global_subnet_out & 0x0F))`, decrements `hop_limit -= 1`.
- **REQ-F06.3: Keyed Circular Echo Filter**:
  - 32-entry circular FIFO storing `(Keyed_Digest, Timestamp_Sec)`. Entries older than $5.0\text{s}$ expire automatically, preventing false-positive echo drops on legitimate retransmissions.

### REQ-F07: Man-in-the-Middle (MitM) & Adversary Defense
- **REQ-F07.1: Bridge Cluster Key ($K_{\text{bridge}}$) Lifecycle**:
  - 128-bit key generated by Anchor TRNG, delivered via DevKey-encrypted `0xD4`, committed to `NVS_KEY_BRIDGE_CLUSTER` before control plane activation.
  - Revocation via `MXCP_CMD_BRIDGE_CLUSTER_KEY_UPDATE` (`0x76`) forces NVS erasure on quarantined nodes and OTA cluster re-keying of surviving bridges.
- **REQ-F07.2: Multihop Ingress Whitelist**:
  - Ingress frames on Subnet B are accepted if and only if source address is:
    1. Local physical device (`0x0001..0x3FFF`), OR
    2. Authorized virtual proxy in active dynamic NAT table (`0x7800..0x7FFF`), OR
    3. Valid transit frame authenticated under $K_{\text{bridge}}$ HMAC (`hop_limit >= 1`, `orig_subnet_id != egress_subnet_id`) matching a valid Tier-1 or Tier-2 transit proxy mapping for `orig_subnet_id`.
- **REQ-F07.3: Replay Protection Keying**:
  - Replay Protection Lists (`RPL`) are strictly keyed by **`(net_idx, src_addr, key_epoch)`** with a 32-bit sliding window, reinforced by a canonical `(CANONICAL_SRC, global_subnet_src)` sequence floor at the destination shim. Egress sequence numbers are allocated monotonically from persistent counters with IV Index Update triggered before 24-bit saturation.

### REQ-F08: Multi-Subnet Key Rotation (KR) Efficiency & Phase Management
- **REQ-F08.1: O(1) Asynchronous KR Tracking**: Dedicated crypto slot per subnet (`meshx_subnet_crypto_slot_t`) tracking Phase 0..3 with zero cross-subnet blocking.
- **REQ-F08.2: Phase & Epoch Inter-Bridge Sync**: Active Bridge includes active `kr_phase` and `epoch` for all subnets inside authenticated `0xD1` heartbeats, preventing Standby phase desynchronization.
- **REQ-F08.3: Burst Quarantine**: UVP Key Refresh bursts (`0xC4`, `0xC5`, `0xC6`) SHALL NEVER be relayed across subnet boundaries.

---

## 3. Non-Functional & Performance Requirements (REQ-NF)

### REQ-NF01: Memory Budget & Footprint Constraints
- **REQ-NF01.1: Static RAM Allocation**: The persistent static BSS RAM SHALL NOT exceed **$3.0\text{ KB}$** ($2,570\text{ B}$ allocated), and worst-case peak RAM under full 4-flow routing load and max 14-node dynamic slot linked list (including heap headers) SHALL NOT exceed **$3.5\text{ KB}$** ($3,536\text{ B}$ total).
- **REQ-NF01.2: NVS Flash Budget**: Subnet credentials, bridge keys, and bridging rules persisted in NVS SHALL NOT exceed **$1\text{ KB}$ of flash storage** ($796\text{ B}$ allocated).

### REQ-NF02: Latency & Transit Timing Bounds
- **REQ-NF02.1: Single-Hop Bridge Latency**: Best-case $\le 2.0\text{ ms}$, Worst-case $\le 12.0\text{ ms}$.
- **REQ-NF02.2: Cross-Subnet End-to-End Latency**: Best-case $\le 30\text{ ms}$, Worst-case operational $\le 200\text{ ms}$.
- **REQ-NF02.3: Redundant Failover Recovery Time**: Guaranteed **$\le 15.5\text{ seconds}$** ($12.0\text{s} \le T_{\text{failover}} \le 12.5\text{s}$).
