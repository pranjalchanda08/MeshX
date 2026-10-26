# TRD Page 5: Cascading Redundancy & Redundancy Claim

**Module:** `TRD-PROV-05`
**Parent Document:** [TRD Index](./README.md)
**Related Requirements:** [requirements.md](../requirements.md) (REQ-F06, REQ-F08)

---

## Table of Contents
- [1. Dynamic Cascading Redundancy ($N+1$)](#1-dynamic-cascading-redundancy-n1)
- [2. State Machine Roles & Unified Address Space](#2-state-machine-roles--unified-address-space)
- [3. Master $\leftrightarrow$ Standby State Synchronization Engine](#3-master-leftrightarrow-standby-state-synchronization-engine)
  - [3.1 Three Synchronization Domains](#31-three-synchronization-domains)
  - [3.2 Wire Protocol Layouts (MESHX_UVP_OP_REDUNDANCY_SYNC = 0xC7)](#32-wire-protocol-layouts-meshx_uvp_op_redundancy_sync--0xc7)
  - [3.3 End-to-End Synchronization Sequence](#33-end-to-end-synchronization-sequence)
- [4. Failure Scenarios & Mitigation Sequences](#4-failure-scenarios--mitigation-sequences)
  - [4.1 Scenario A: Standby Claim Broadcast (0xCD) Lost or No Candidate Responds](#41-scenario-a-standby-claim-broadcast-0xcd-lost-or-no-candidate-responds)
  - [4.2 Scenario B: Transient RF Blackout / Intermittent Heartbeat (0xC7) Loss](#42-scenario-b-transient-rf-blackout--intermittent-heartbeat-0xc7-loss)
  - [4.3 Scenario C: Dual Master Split-Brain Recovery & Bitmap Reconciliation](#43-scenario-c-dual-master-split-brain-recovery--bitmap-reconciliation)
  - [4.4 Scenario D: Dual Standby Collision & Deterministic Demotion to Candidate](#44-scenario-d-dual-standby-collision--deterministic-demotion-to-candidate)
  - [4.5 Scenario E: 3-Way Cascading Collision (Dual Master + Existing Standby) & Total Rank Convergence](#45-scenario-e-3-way-cascading-collision-dual-master--existing-standby--total-rank-convergence)

---

## 1. Dynamic Cascading Redundancy ($N+1$)

To prevent single-point-of-failure vulnerabilities when the primary Standby is promoted to Master, the system implements dynamic multi-tiered provisioner cascading:

```mermaid
flowchart TD
    classDef master fill:#2ecc71,stroke:#27ae60,stroke-width:2px,color:#fff;
    classDef standby fill:#3498db,stroke:#2980b9,stroke-width:2px,color:#fff;
    classDef candidate fill:#f39c12,stroke:#d68910,stroke-width:2px,color:#fff;
    classDef stale fill:#e74c3c,stroke:#c0392b,stroke-width:2px,color:#fff;

    N0_Active["Node 0: Master (Epoch N)<br/>[Cryptographic Trust Authority]"]:::master
    N2_Standby["Node 2: Standby<br/>[Syncs Epoch & Active NetKey]"]:::standby
    N4_Candidate["Node 4: Candidate<br/>[Idle Provisioner Backup]"]:::candidate
    N0_Reboot["Node 0: Stale Master<br/>[Rebooted / Yields Authority]"]:::stale

    N0_Active -- "Power Off / Hardware Outage" --> N0_Outage["Node 0 Down"]
    N2_Standby -- "15s Watchdog Timeout (3 Missed Beats)" --> N2_Promote["Node 2 Promotes to MASTER<br/>(Maintains Unicast 0x0002, Advances Epoch)"]:::master
    N2_Promote -- "Broadcasts MESHX_UVP_OP_REDUNDANCY_CLAIM (0xCD)" --> N4_Candidate
    N4_Candidate -- "Takes Over Shadow Duties" --> N4_Promote["Node 4 Promotes to STANDBY<br/>(Tracks Node 2)"]:::standby

    N0_Outage -- "Powers Back ON" --> N0_Reboot
    N0_Reboot -- "Sniffs 0xC7 from Node 2 (Yields Authority)" --> N0_Demote["Node 0 Steps Down to CANDIDATE"]:::candidate
```

---

## 2. State Machine Roles & Unified Address Space

Under the Master's Unified Address Space managed via the Paged Sparse 2-Bit Bitmap and JIT Dynamic Element Leasing (`0xC8`/`0xCC`), address collision between concurrent proxies is mathematically eliminated ($0.00\%$). Therefore, **artificial dual pools (`Pool A` and `Pool B`) are completely obsolete**. All nodes exist within the **unified continuous 15-bit address space (`0x0001..0x7FFF`)**.

To ensure seamless failover without state loss, the Provisioner state replicated between Master and Standby comprises:
$$\text{Replicated Provisioner State} = \{\text{Cryptographic KR State}, \ \text{Unified Address Allocation Bitmap}, \ \text{Master DevKey Vault}\}$$

| Role Identifier | Value | Primary Behavior |
|---|---|---|
| `MESHX_PROV_ROLE_NONE` | `0` | Standard non-provisioner node. |
| `MESHX_PROV_ROLE_MASTER` | `1` | Authoritative cryptographic root, emits 5s heartbeats (`0xC7`), orchestrates 5-minute key rotation, leases addresses via JIT dynamic element engine. |
| `MESHX_PROV_ROLE_STANDBY` | `2` | Active shadow backup: maintains mirrored Paged Sparse Bitmap and DevKey Vault via `0xC7` delta sync; promotes on 15s watchdog timeout (3 missed beats). **Dual-Standby Tie-Breaker:** Lower address/UUID retains Standby; higher address immediately demotes to `CANDIDATE`. |
| `MESHX_PROV_ROLE_CANDIDATE` | `3` | Standby-eligible backup node. Absorbs demoted redundant Standbys. Listens for `MESHX_UVP_OP_REDUNDANCY_CLAIM` (`0xCD`) to promote to Standby ($N+1$). |
| `MESHX_PROV_ROLE_STALE` | `4` | Recovered previous Master. Yields authority upon receiving heartbeat from active Master and demotes to Candidate. |

---

## 3. Master $\leftrightarrow$ Standby State Synchronization Engine

The Master and Standby provisioners maintain continuous real-time synchronization across three critical architectural domains using the `MESHX_UVP_OP_REDUNDANCY_SYNC` (`0xC7`) vendor protocol family:

### 3.1 Three Synchronization Domains

1. **Domain 1: Cryptographic Key Rotation State**
   - **Active Key & Epoch:** Current Key Rotation Epoch $N$, Active Primary NetKey ID, and Secondary Overlap NetKey ID.
   - **Phase Tracking:** Key rotation stage (`PHASE_IDLE`, `PHASE_DISTRIBUTE`, `PHASE_SWITCHOVER`, `PHASE_REVOKE`).
   - **Zero-Lag Advantage:** Because the Standby is an active mesh node subscribed to `MESHX_MGMT_VIRTUAL_ADDR` (`0x8F42`), it receives Key Rotation Phase 1 (`0xC4`), Phase 2 (`0xC5`), and Phase 3 (`0xC6`) bursts over the air concurrently with the rest of the mesh. The 5-second `0xC7` heartbeat serves as a sanity check and monotonic sequence barrier.

2. **Domain 2: Unified Address Allocation Bitmap (Paged Sparse 2-Bit Bitmap)**
   - **High-Water Mark Horizon:** Master advertises `next_free_addr` in every 5-second heartbeat, giving the Standby an immediate monotonic allocation baseline.
   - **Incremental Word Delta Sync (6 Bytes):** Whenever Master commits a dynamic element lease (from an incoming `0xC9 FAST_PROV_REPORT`), it immediately transmits an incremental delta packet carrying `[word_index (2B) | word_value (4B)]`. Standby writes the 32-bit word directly to its local RAM bitmap in $O(1)$ constant time.
   - **Zero-Collision Guarantee:** Because Standby mirrors the Master's exact bitmap, if Master experiences sudden power failure, the promoted Standby continues allocating strictly from the true next available slots ($Addr_{\text{new}} \ge \text{Next-Free-Addr}$), eliminating address overlap ($0.00\%$ collision rate).

3. **Domain 3: Master DevKey Vault Replication**
   - **Permanent Identity Records:** To support **Autonomous Re-Healing (`0xCE`/`0xCF`)**, the Master maintains persistent `[UUID -> DevKey]` records in NVS.
   - **Event-Driven Vault Mirroring:** When the Master receives `0xC9 FAST_PROV_REPORT` containing a newly provisioned child's `DevKey`, it replicates the 34-byte vault entry `[UUID (16B) | Addr (2B) | DevKey (16B)]` to the Standby over UVP. Standby commits it to local NVS (`MESHX_NVS_KEY_DEVKEY_PREFIX`). If Master dies, the Standby can re-admit and re-heal any node in the network without lost credentials.

---

### 3.2 Wire Protocol Layouts (`MESHX_UVP_OP_REDUNDANCY_SYNC = 0xC7`)

The `0xC7` opcode carries a 1-byte `subtype` header to multiplex routine heartbeats, event-driven bitmap deltas, and vault replication:

#### Subtype 0x01: Periodic Heartbeat & Liveness Token (Every 5 Seconds)
```
[ Opcode 0xC7 (1B) | Subtype 0x01 (1B) | Sync_Epoch (4B) | Master_Addr (2B) | KR_Epoch (4B) | Next_Free_Addr (2B) | Active_NetKey_ID (2B) ]
Total: 16 Bytes
```
- Transmitted every 5.0 seconds from Master to Standby unicast address (or `MESHX_MGMT_VIRTUAL_ADDR`).
- Resets the Standby's 15-second failover watchdog timer (`s_redundancy_missed_beats = 0`).

#### Subtype 0x02: Incremental Address Delta Sync (Event-Driven upon Node Enrollment)
```
[ Opcode 0xC7 (1B) | Subtype 0x02 (1B) | Word_Index (2B) | Word_Value (4B) ]
Total: 8 Bytes (6 Bytes payload)
```
- Transmitted by Master immediately after committing an address block in the sparse bitmap.
- `word_index`: Sparse bitmap word index ($0..2047$).
- `word_value`: Updated 32-bit 2-bit state representation of the 16 corresponding unicast addresses.

#### Subtype 0x03: DevKey Vault Replication Record (Event-Driven upon 0xC9 Post-Closure)
```
[ Opcode 0xC7 (1B) | Subtype 0x03 (1B) | Node_UUID (16B) | Allocated_Addr (2B) | DevKey (16B) ]
Total: 36 Bytes (34 Bytes payload)
```
- Replicates new device credentials into Standby NVS storage for future re-healing authentication.

#### Subtype 0x04: Bulk Catch-Up State Transfer (On Candidate Promotion / Post-Reboot)
```
[ Opcode 0xC7 (1B) | Subtype 0x04 (1B) | Chunk_Index (1B) | Active_Chunks_Mask (32B) | Chunk_Data (32B) ]
Total: 67 Bytes
```
- Used only during initial pairing of a new Standby candidate to bulk-transfer active bitmap pages without transmitting empty address space.

#### Standby Claim Wire Protocol Layout (`MESHX_UVP_OP_REDUNDANCY_CLAIM = 0xCD`)
```
[ Opcode 0xCD (1B) | Claim_Epoch (4B) | Master_Addr (2B) | Next_Free_Addr (2B) | Reserved (2B) ]
Total: 11 Bytes (10 Bytes payload)
```
- `claim_epoch`: Master claim epoch counter.
- `master_addr`: Promoted Master primary unicast address.
- `next_free_addr`: Monotonic allocation high-water mark across unified address space (`0x0002..0x7FFF`).
- `reserved`: 2-byte alignment padding.

#### Standby Claim Acknowledgment Layout (`MESHX_UVP_OP_REDUNDANCY_CLAIM_ACK = 0xD0`)
```
[ Opcode 0xD0 (1B) | Candidate_UUID (16B) | Candidate_Addr (2B) ]
Total: 19 Bytes (18 Bytes payload)
```
- Transmitted unicast to Master by accepting candidate node to confirm promotion to Standby role.

---

### 3.3 End-to-End Synchronization Sequence

```mermaid
sequenceDiagram
    autonumber
    participant Master as Master Provisioner (0x0001)
    participant Standby as Standby Provisioner (0x0002)
    participant Candidate as Candidate Node (0x0004)
    participant Proxy as Wavefront Proxy Node

    rect rgb(240, 248, 255)
        Note over Master,Standby: Phase 1: Routine Liveness Heartbeat (Every 5s)
        Master->>Standby: UVP 0xC7 (Subtype 0x01): Heartbeat [Sync_Epoch=42, KR_Epoch=10, Next_Addr=0x004A]
        Standby->>Standby: Reset Watchdog Timer (15s deadline refreshed, missed_beats=0)
    end

    rect rgb(255, 250, 240)
        Note over Proxy,Standby: Phase 2: Event-Driven State Replication (Node Provisioned)
        Proxy->>Master: UVP 0xC9: FAST_PROV_REPORT [UUID_X, Addr=0x004A, DevKey_X]
        Master->>Master: Update Paged Sparse Bitmap (Mark 0x004A = ALLOCATED 10b)<br/>Commit DevKey_X to Master NVS Vault
        Master->>Standby: UVP 0xC7 (Subtype 0x02): Address Delta [Word=4, Value=0x5555AA00]
        Standby->>Standby: Apply 32-bit word directly to local RAM bitmap in O(1)
        Master->>Standby: UVP 0xC7 (Subtype 0x03): DevKey Record [UUID_X, Addr=0x004A, DevKey_X]
        Standby->>Standby: Commit DevKey_X to Standby local NVS Vault
        Note over Master,Standby: Standby is now a 100% BIT-PERFECT mirror of Master state
    end

    rect rgb(255, 245, 245)
        Note over Master,Candidate: Phase 3: Sudden Master Outage & Failover (15s Watchdog)
        Master-xStandby: Power Outage / Hardware Failure (Heartbeats stop)
        Standby->>Standby: 5s Timer -> missed_beats = 1
        Standby->>Standby: 10s Timer -> missed_beats = 2
        Standby->>Standby: 15s Timer -> missed_beats = 3 (WATCHDOG EXPIRED!)
        Standby->>Standby: Promote local role: ROLE_MASTER<br/>Next provision starts seamlessly at 0x004B (Zero Collisions!)
        Standby->>Candidate: Broadcast: MESHX_UVP_OP_REDUNDANCY_CLAIM (0xCD)
        Candidate->>Standby: Unicast: MESHX_UVP_OP_REDUNDANCY_CLAIM_ACK (0xD0)
        Standby->>Candidate: UVP 0xC7 (Subtype 0x04): Bulk Catch-Up State Transfer
        Candidate->>Candidate: Promotes local role: ROLE_STANDBY
        Note over Standby,Candidate: Dynamic Cascading Redundancy (N+1) Fully Restored!
    end
```

---

## 4. Failure Scenarios & Mitigation Sequences

### 4.1 Scenario A: Standby Claim Broadcast (`0xCD`) Lost or No Candidate Responds

```mermaid
sequenceDiagram
    autonumber
    participant Node2 as Node 2 (Promoted Master)
    participant RF as BLE Mesh Medium (0x8F42)
    participant Node4 as Node 4 (Candidate Node)

    Note over Node2: Step 1: Promotion & Initial Claim Broadcast
    Node2->>Node2: 15s Heartbeat Watchdog fires -> Promotes to MASTER
    Node2->>RF: Broadcast: MESHX_UVP_OP_REDUNDANCY_CLAIM (0xCD) [Attempt 1]
    Note over RF: Packet Dropped / RF Collision or Node 4 Offline
    Node2->>Node2: Claim Response Timer (2s) expires -> No Candidate Reply

    Note over Node2: Step 2: Exponential Backoff Retries
    Node2->>RF: Broadcast: MESHX_UVP_OP_REDUNDANCY_CLAIM (0xCD) [Attempt 2: 4s interval]
    Note over RF: Still no candidate reply
    Node2->>Node2: Claim Response Timer (4s) expires
    Node2->>RF: Broadcast: MESHX_UVP_OP_REDUNDANCY_CLAIM (0xCD) [Attempt 3: 8s interval]

    Note over Node2,Node4: Step 3: Candidate Wakeup / Acknowledgment
    Node4->>Node4: Node 4 finishes boot / rejoins mesh as ROLE_CANDIDATE
    Node4->>RF: Hears Attempt 3 of MESHX_UVP_OP_REDUNDANCY_CLAIM (0xCD)
    Node4->>Node4: Transitions state = ROLE_STANDBY
    Node4->>Node2: Unicast UVP: MESHX_UVP_OP_REDUNDANCY_CLAIM_ACK (0xD0) [Node 4 UUID]
    Node2->>Node2: Disarms Claim Retry Timer
    Node2->>Node4: Standard 5s Heartbeats (0xC7) Established
    Note over Node2,Node4: Cascading Redundancy (N+1) Successfully Restored
```

### 4.2 Scenario B: Transient RF Blackout / Intermittent Heartbeat (`0xC7`) Loss

```mermaid
sequenceDiagram
    autonumber
    participant Master
    participant Medium
    participant Standby

    note over Master,Standby: Normal Heartbeat Operation
    Master->>Medium: 5.0s Heartbeat (0xC7) [Epoch N, Addr 0x0001]
    Medium->>Standby: Packet Delivered
    Standby->>Standby: Reset missed_beats = 0 (Refresh 15s Watchdog)

    note over Master,Standby: Transient Interference Event (e.g. Wi-Fi Burst)
    Master->>Medium: 10.0s Heartbeat (0xC7)
    note over Medium: Packet Lost in Air
    Standby->>Standby: 5s Timer fires -> missed_beats = 1 (Do not promote)

    note over Master,Standby: Channel Clears Before 15s Timeout
    Master->>Medium: 15.0s Heartbeat (0xC7) [Epoch N+1]
    Medium->>Standby: Packet Delivered
    Standby->>Standby: Heartbeat received -> Reset missed_beats = 0
    note over Standby: False-alarm failover prevented

    note over Master,Standby: Sustained Crash (3 Consecutive Misses)
    Master->>Medium: 20.0s Heartbeat (Dropped)
    Standby->>Standby: missed_beats = 1
    Master->>Medium: 25.0s Heartbeat (Dropped)
    Standby->>Standby: missed_beats = 2
    Master->>Medium: 30.0s Heartbeat (Dropped)
    Standby->>Standby: missed_beats = 3 (15s expired)
    Standby->>Standby: Confirm Master Outage -> Promote to Master
```

---

### 4.3 Scenario C: Dual Master Split-Brain Recovery & State Reconciliation

If a prolonged, localized RF partition isolates the Standby from Master heartbeats, the Standby promotion timer will fire, creating a temporary dual-Master state:

#### Multi-Tier Master Authority Hierarchy (State Freshness First)
When the RF partition heals and both provisioners receive inbound `0xC7` heartbeats from each other, authority is NOT decided by static address alone. To prevent a stale rebooted node from overwriting newer provisioning data, provisioners evaluate the **State Freshness Vector**:

$$\mathbf{Authority\ Vector} = \Big(\mathbf{kr\text{-}epoch}, \ \mathbf{sync\text{-}epoch}, \ \mathbf{next\text{-}free\text{-}addr}, \ -\text{Addr}, \ -\text{UUID}\Big)$$

1. **Tier 1: Key Rotation Epoch (`kr_epoch` - Highest Wins):**
   - The provisioner with the strictly higher Key Rotation Epoch has advanced the cryptographic network state further ($N_{active} > N_{stale}$).
   - If Node 3 has `kr_epoch = 15` and rebooted Node 1 has `kr_epoch = 10`, **Node 3 wins unconditionally**. Stale Node 1 instantly steps down to `ROLE_STALE` / `ROLE_CANDIDATE`.
2. **Tier 2: Sync Epoch & Allocation Horizon (`sync_epoch` / `next_free_addr` - Freshest Wins):**
   - If both nodes share the identical Key Rotation Epoch (`kr_epoch_A == kr_epoch_B`), the node with the higher `sync_epoch` or higher `next_free_addr` has processed more enrollments and holds the fresher database. That node retains `ROLE_MASTER`.
3. **Tier 3: Deterministic Static Tie-Breaker (`min(Addr, UUID)`):**
   - **Only if all epochs, sync counters, and allocation horizons are completely identical** (symmetric split-brain where neither advanced ahead) does the lower Unicast Address / Device UUID break the tie.
4. **Tier 4: Additive Bitwise Reconciliation (Zero Data Loss Guarantee):**
   - Regardless of who demotes, the losing provisioner **never deletes or drops its provisioning records**.
   - The demoting node transmits an immediate burst of `Subtype 0x02` address delta packets and `Subtype 0x03` DevKey vault records for any nodes it provisioned during the split.
   - The authoritative Master executes an **additive bitwise union (`OR`)** into its Paged Sparse Bitmap and commits new DevKeys to NVS. No newly provisioned device is ever lost, orphaned, or leaked.

---

### 4.4 Scenario D: Dual Standby Collision & Deterministic Demotion to Candidate

To maintain the architectural invariant of **strictly 1 Master and 1 Standby**, any condition where multiple provisioner-capable nodes believe they are simultaneously in `ROLE_STANDBY` is deterministically resolved using the exact same address/UUID tie-breaker:

```mermaid
sequenceDiagram
    autonumber
    participant Master as Master Provisioner (0x0001)
    participant StandbyA as Standby Node A (0x0002, UUID_A)
    participant StandbyB as Standby Node B (0x0004, UUID_B)

    Note over Master,StandbyB: Trigger: Concurrent Claim ACKs (0xD0) or Healed RF Partition
    Note over StandbyA,StandbyB: Both nodes are temporarily in ROLE_STANDBY

    Master->>StandbyA: UVP 0xC7 (Subtype 0x01): Heartbeat (DST: 0x8F42)
    StandbyA->>StandbyB: Over-the-air Standby activity / ACK observed

    Note over StandbyA,StandbyB: Deterministic Address & UUID Tie-Breaker
    StandbyA->>StandbyA: Compare: Own Addr (0x0002) < Peer Addr (0x0004) -> WINS!
    StandbyA->>StandbyA: RETAIN active ROLE_STANDBY

    StandbyB->>StandbyB: Compare: Own Addr (0x0004) > Peer Addr (0x0002) -> LOSES!
    StandbyB->>StandbyB: Immediate Demotion: ROLE_STANDBY -> ROLE_CANDIDATE
    StandbyB-->>Master: (Optionally notifies Master via MXCP / UVP status)

    Note over StandbyA,StandbyB: System Invariant Restored: 1 Master (0x0001), 1 Standby (0x0002), N Candidates
```

#### Tie-Breaker Rules:
1. **Unicast Address Comparison:**
   $$\text{Winner} = \min(\text{Standby}_A.\text{Addr}, \ \text{Standby}_B.\text{Addr})$$
   The node with the numerically lower unicast address retains `ROLE_STANDBY`.
2. **Device UUID Fallback:**
   If addresses are unassigned or identical (e.g., during virgin onboarding arbitration), the lexicographically lower 128-bit Device UUID breaks the tie.
3. **Graceful Demotion to Candidate:**
   The losing Standby does not disconnect from the mesh or wipe its keys; it transitions smoothly to `ROLE_CANDIDATE`. It stops responding to Standby duties, disarms its heartbeat failover watchdog, and waits in the candidate pool for future `0xCD REDUNDANCY_CLAIM` broadcasts if the active Standby promotes or drops offline.

---

### 4.5 Scenario E: 3-Way Cascading Collision (Dual Master + Existing Standby) & State-Freshness Convergence

#### The Multi-Provisioner Conflict Scenario
During an extended network partition or concurrent healing event, a complex 3-way multi-provisioner state may emerge:
- **Partition A:** Primary Master (`0x0001`) and its active Standby (`0x0002`).
- **Partition B:** Isolated node promoted to Master (`0x0003`) after 3 missed beats and operated the network.

When the RF partition heals, three nodes simultaneously claim elevated provisioner roles:
1. `Node 1` (`0x0001`): `ROLE_MASTER`
2. `Node 2` (`0x0002`): `ROLE_STANDBY`
3. `Node 3` (`0x0003`): `ROLE_MASTER`

> [!IMPORTANT]
> **The Stale Master Trap:** If authority were decided purely by static address (`min(Addr)`), a rebooted Node 1 (`0x0001`) with **stale offline data** would force an active Node 3 (`0x0003`) to demote. This would overwrite or drop recent Key Rotations, DevKeys, and node enrollments committed by Node 3!
>
> Therefore, MeshX enforces **State Freshness First**: the node with the most advanced cryptographic and provisioning generation ALWAYS takes precedence over static unicast address!

---

#### The State-Freshness Provisioner Authority Hierarchy
When any provisioner observes a role conflict, authority is evaluated using the lexicographical **State Freshness Vector**:
$$\mathbf{Authority\ Vector} = \Big(\mathbf{kr\text{-}epoch}, \ \mathbf{sync\text{-}epoch}, \ \mathbf{next\text{-}free\text{-}addr}, \ -\text{Addr}, \ -\text{UUID}\Big)$$

$$\begin{aligned}
\text{Rank \#1 (Freshest State)} &\implies \mathbf{ROLE\text{-}MASTER} \\
\text{Rank \#2 (Second Freshest / Lowest Peer)} &\implies \mathbf{ROLE\text{-}STANDBY} \\
\text{Rank \#3..N (Stale / Remaining Peers)} &\implies \mathbf{ROLE\text{-}CANDIDATE}
\end{aligned}$$

---

#### Case 1: Node 3 is the Active Master with Fresher State (Node 1 is Stale / Rebooted)
- **Condition:** Node 1 rebooted from a crash (`kr_epoch = 10, sync_epoch = 5`). Node 3 actively provisioned new devices during the outage (`kr_epoch = 15, sync_epoch = 42`).
- **Arbitration:** Node 3 has higher `kr_epoch` ($15 > 10$). **Node 3 WINS and RETAINS `ROLE_MASTER`!**
- **Action for Node 1:** Node 1 detects `kr_epoch_remote > kr_epoch_local`, marks itself **`ROLE_STALE`**, yields authority immediately, and **demotes directly to `ROLE_CANDIDATE`** (bypassing Standby). Node 1 requests a full catch-up sync (`Subtype 0x04`) from Node 3.
- **Action for Node 2:** Node 2 recognizes Node 3 as authoritative and continues as `ROLE_STANDBY` tracking Node 3.
- **Data Preservation:** **100% of Node 3's new nodes, DevKeys, and Key Rotation epochs remain active and intact.**

```mermaid
sequenceDiagram
    autonumber
    participant M3 as Node 3 (Active Master: Epoch 15, Sync 42)
    participant S2 as Node 2 (Standby: Epoch 15, Sync 42)
    participant M1 as Node 1 (Rebooted Stale Master: Epoch 10, Sync 5)

    Note over M3,M1: RF Restored / Node 1 Boots: Stale Node 1 Discovers Active Mesh
    M3->>M1: UVP 0xC7: Heartbeat [KR_Epoch=15, Sync=42, Addr=0x0003]
    M1->>M1: Inspect Inbound Epoch: Remote 15 > Local 10 (Local is STALE!)

    Note over M1: State-Freshness Evaluation: Node 3 WINS AUTHORITY!
    M1->>M1: Transitions state: ROLE_MASTER -> ROLE_STALE -> ROLE_CANDIDATE (Direct Demotion!)
    M1-->>M3: UVP 0xC7 (Subtype 0x04): Request Bulk Catch-Up State Transfer

    M3->>M1: UVP 0xC7 (Subtype 0x04): Paged Sparse Bitmap Chunks + DevKey Vault
    M1->>M1: Updates local NVS with Epoch 15 + latest address bitmap

    Note over M3,S2: Node 2 continues as active Standby tracking Node 3
    Note over M3,M1: Invariant Preserved: 1 Master (Node 3), 1 Standby (Node 2), 1 Candidate (Node 1). Zero Data Loss!
```

---

#### Case 2: Symmetrical Split-Brain (Both Nodes Have Equal Freshness)
- **Condition:** Both partitions experienced identical Key Rotation epochs (`kr_epoch_1 == kr_epoch_3`).
- **Arbitration:** Tie-breaker falls back to static address: $0x0001 < 0x0003$. Node 1 retains `ROLE_MASTER`; Node 3 cascades to `ROLE_CANDIDATE`.
- **Additive Bitwise Union (`OR` Merge):**
  1. Node 3 flushes its provisional lease log to Node 1 via `Subtype 0x02` (Address Deltas) and `Subtype 0x03` (DevKeys).
  2. Node 1 performs a **bitwise `OR` union** into its Paged Sparse Bitmap, incorporating all nodes provisioned by Node 3.
  3. Node 1 replicates the merged bitmap deltas to Node 2 (`ROLE_STANDBY`).
  4. **Result:** No addresses are orphaned or leaked; all newly enrolled devices remain fully authenticated and operational.

```mermaid
sequenceDiagram
    autonumber
    participant M1 as Node 1 (0x0001, Master: Epoch 12, Sync 20)
    participant S2 as Node 2 (0x0002, Standby: Epoch 12, Sync 20)
    participant M3 as Node 3 (0x0003, Split Master: Epoch 12, Sync 20)

    Note over M1,M3: RF Partition Heals: Symmetrical Split-Brain (Identical Epochs)
    M1->>M3: UVP 0xC7: Heartbeat [Epoch 12, Sync 20, Addr=0x0001]
    M3->>M1: Inbound Heartbeat [Epoch 12, Sync 20, Addr=0x0003]

    Note over M1,M3: Identical Epochs: Static Tie-Breaker Applied (0x0001 < 0x0003)
    Note over M1: Node 1: Rank #1 (0x0001) -> RETAIN MASTER
    Note over S2: Node 2: Rank #2 (0x0002) -> RETAIN STANDBY
    Note over M3: Node 3: Rank #3 (0x0003) -> CASCADES TO CANDIDATE!

    Note over M3: Additive State Reconciliation (Zero Data Loss)
    M3->>M3: Transitions: ROLE_MASTER -> ROLE_CANDIDATE
    M3->>M1: Burst: UVP 0xC7 (Subtype 0x02) Address Deltas + (Subtype 0x03) DevKeys
    M1->>M1: Bitwise OR Merge into Paged Sparse Bitmap and Commit DevKeys to NVS Vault
    M1->>S2: UVP 0xC7 (Subtype 0x02): Replicate merged bitmap words to Standby Node 2
    S2->>S2: Updates local mirror bitmap in RAM

    Note over M1,M3: Invariant Preserved: 1 Master (Node 1), 1 Standby (Node 2), N Candidates. 100% Data Preserved!
```
