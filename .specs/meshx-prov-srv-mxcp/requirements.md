# Requirements Specification: UVP-Driven Provisioning & Security Management with MXCP Capability

**Feature:** UVP-Driven Remote Provisioning, 5-Minute Key Rotation, Provisioner Redundancy, Fast Wavefront Infill, and Supervised Node Replacement via MXCP  
**Status:** Approved Architectural Pivot (Zero-Bloat UVP Architecture)  
**Target Subsystems:** `meshx_prov_srv`, `meshx_uvp_dispatcher`, `meshx_mxcp`, `meshx_api`, `meshx_nvs`  
**Related Design Document:** [design.md](./design.md)  
**Implementation Tasks:** [tasks.md](./tasks.md)  
**Technical Reference Documents (Modular TRD Suite):** 👉 **[TRD Documentation Index](./trd/README.md)**  
- [Page 1: Architecture & Zero-Bloat Philosophy](./trd/page1_overview.md)
- [Page 2: Protocol Definitions & Packed Structs](./trd/page2_protocol_definitions.md)
- [Page 3: UVP Remote Provisioning (RPR) Tunneling](./trd/page3_remote_provisioning.md)
- [Page 4: 5-Minute Zero-ACK Key Rotation](./trd/page4_key_rotation.md)
- [Page 5: Cascading Redundancy & Redundancy Claim](./trd/page5_redundancy_cascading.md)
- [Page 6: Long-Term Eviction & DevKey Self-Healing](./trd/page6_eviction_healing.md)
- [Page 7: Failure Modes, Resilience & Memory Analysis](./trd/page7_resilience_memory.md)
- [Page 8: Wavefront Provisioning Engine (Configurable K) & JIT Dynamic Allocation](./trd/page8_fast_bulk_provisioning.md)
- [Page 9: Node Replacement & Unicast Lease Reallocation](./trd/page9_node_replacement.md)

---

## 1. Executive Summary & Architectural Paradigm

Previous designs considered enabling heavy native BLE Mesh SIG stacks (`CONFIG_BLE_MESH_PROVISIONER`, `CONFIG_BLE_MESH_RPR_SRV`, `CONFIG_BLE_MESH_RPR_CLI`, `CONFIG_BLE_MESH_CONFIG_CLIENT`) which would consume `>140 KB` of flash and `>6 KB` of static RAM.

This specification adopts a **Zero-Bloat UVP Architecture** documented across the [TRD Suite](./trd/README.md):
1. **Virgin Node Onboarding (Native Prov Server Only)**: The local node utilizes the lightweight, mandatory `CONFIG_BLE_MESH_PROV=y` (Provisioning Server) solely for initial network joining (receiving its primary NetKey and Unicast Address). *(See [TRD Page 1](./trd/page1_overview.md))*
2. **Remote Provisioning (RPR) via UVP Tunneling**: Out-of-range unprovisioned device beacons are captured by standard mesh nodes via BLE advertisement scanning and tunneled across the mesh network as UVP Vendor Messages (`OP_UVP_RPR_*`). **Native RPR Client/Server stacks remain completely disabled (`=n`)**. *(See [TRD Page 3](./trd/page3_remote_provisioning.md))*
3. **5-Minute Key Rotation via UVP Command Protocol**: Key Refresh (Phases 1, 2, and 3) is orchestrated using authenticated, encrypted UVP vendor frames with zero-ACK 3-burst push and 4-minute dual-key overlap. Receiving nodes update keys locally via node-level key management APIs. **Native Config Client / Provisioner stacks remain completely disabled (`=n`)**. *(See [TRD Page 4](./trd/page4_key_rotation.md))*
4. **Provisioner Redundancy (Active/Standby Pair & N+1 Cascading)**: Real-time state synchronization (address map deltas, DevKey vault mirroring, 5s heartbeats `0xC7`) over UVP vendor opcodes backed by a Unified 15-Bit Address Space (`0x0002..0x7FFF`) and State Freshness Vector arbitration. *(See [TRD Page 5](./trd/page5_redundancy_cascading.md) and [TRD Page 7](./trd/page7_resilience_memory.md))*
5. **Wavefront Bulk Provisioning Engine (Configurable $K$)**: Autonomous distributed provisioning with 100% local proxy crypto, JIT contiguous element block leasing (`0xC8`/`0xCC`), and Paged Sparse 2-Bit Bitmap memory management. *(See [TRD Page 8](./trd/page8_fast_bulk_provisioning.md))*
6. **Supervised Node Replacement Engine**: Seamless field swapping with in-place address reuse for identical composition, composition schema diff for mismatched models, trailing surplus quarantine for contraction, and JIT relocation for expansion. *(See [TRD Page 9](./trd/page9_node_replacement.md))*

---

## 2. Functional Requirements (REQ-F)

### REQ-F01: Separation of Concerns
- **Local Prov Server**: Native PB-ADV/PB-GATT provisioning server is maintained only for first-time onboarding.
- **Advanced Management (Remote Prov, Key Rotation, Redundancy, Fast Bulk, Replacement)**: Executed entirely over the **MeshX Unified Vendor Protocol (UVP)** and exposed to external Host controllers via **MXCP**.
- No native SIG Client models (`CONFIG_BLE_MESH_CONFIG_CLIENT=n`) or native Provisioner stacks (`CONFIG_BLE_MESH_PROVISIONER=n`) shall be introduced.
- *Reference Specification:* [TRD Page 1: Architecture & Philosophy](./trd/page1_overview.md).

### REQ-F02: Local Provisioning MXCP Command Set (Host → Engine)
- **`MXCP_CMD_PROV_ENABLE`**: Enable unprovisioned device beaconing on PB-ADV/PB-GATT.
- **`MXCP_CMD_PROV_DISABLE`**: Disable device provisioning beacons.
- **`MXCP_CMD_PROV_SET_DEVICE_NAME`**: Set or update unprovisioned device advertising friendly name.
- **`MXCP_CMD_PROV_SET_STATIC_OOB`**: Configure static OOB authentication key.
- **`MXCP_CMD_PROV_INPUT_AUTH_RSP`**: Submit host input response for input OOB authentication.
- **`MXCP_CMD_PROV_GET_STATE`**: Query current provisioning state, unicast address, and NetKey index.
- *Reference Specification:* [TRD Page 2: Protocol & Wire Definitions](./trd/page2_protocol_definitions.md).

### REQ-F03: Local Provisioning MXCP Event Set (Engine → Host)
- **`MXCP_EVT_PROV_STATE_IND`**: Status indication (Unprovisioned, Beaconing, Provisioning, Provisioned).
- **`MXCP_EVT_PROV_LINK_OPEN`**: Emitted when PB-ADV/PB-GATT link opens.
- **`MXCP_EVT_PROV_LINK_CLOSE`**: Emitted when link closes.
- **`MXCP_EVT_PROV_AUTH_REQ`**: Emitted when OOB display or user input is needed.
- **`MXCP_EVT_PROV_COMP`**: Emitted when provisioning completes.
- **`MXCP_EVT_PROV_FAILED`**: Emitted on failure or timeout.
- **`MXCP_EVT_NODE_RESET_IND`**: Emitted when node is reset.
- *Reference Specification:* [TRD Page 2: Protocol & Wire Definitions](./trd/page2_protocol_definitions.md).

### REQ-F04: UVP-Driven Remote Provisioning (RPR)
- Any operational mesh node with BLE scan enabled shall capture raw unprovisioned beacons (`0x2B` Mesh Beacon with unprov flag).
- The node encapsulates the beacon (UUID, OOB info, RSSI) and transmits a UVP message (`OP_UVP_RPR_BEACON_REPORT`) to the Master/Host.
- The Master transmits provisioning PDUs encapsulated in `OP_UVP_RPR_TUNNEL_DATA`.
- The intermediate node injects the raw PDU into the air via PB-ADV to the target unprovisioned node.
- **Zero dependency on SIG RPR models (`CONFIG_BLE_MESH_RPR_*=n`)**.
- *Reference Specification:* [TRD Page 3: Remote Provisioning & Wavefront Tunneling](./trd/page3_remote_provisioning.md).

### REQ-F05: UVP-Driven 5-Minute Key Rotation (P1 Priority)
- The Master (or Host via MXCP) triggers Key Rotation every 5 minutes (`300 seconds` $\pm 15$s jitter).
- **Zero-ACK 3-Burst Push**: Master broadcasts Phase 1 (`0xC4`), Phase 2 (`0xC5`), and Phase 3 (`0xC6`) without requiring individual node ACKs ($O(1)$ constant RF traffic).
- **4-Minute Dual-Key Overlap**: Nodes install the new key in the secondary key slot and accept both old and new keys for a 4-minute dual-RX window.
- **RFC 1982 Modular Epoch Rollover**: Nodes enforce signed distance modular arithmetic to handle 32-bit integer overflow safely.
- **Pre-Broadcast Atomic NVS Commit**: Master commits `kr_epoch = N+1` to NVS *before* transmitting Phase 1 to prevent epoch rollback across reboots.
- **Company Secret Virtual Addressing**: Target address resolved from `prod_profile.yml` (`mgmt_virtual_addr`) with defense-in-depth security (NetKey/AppKey AES-CCM MIC and eFuse flash encryption).
- *Reference Specification:* [TRD Page 4: 5-Minute Zero-ACK Key Rotation](./trd/page4_key_rotation.md).

### REQ-F06: Unified Address Space & Real-Time Provisioner Redundancy
- **Unified 15-Bit Address Space**: Eliminates legacy hard-partitioned pools (`Pool A` vs `Pool B`); all provisioner roles operate over a single continuous address space (`0x0002..0x7FFF`).
- **Real-Time State Replication Engine**: Master mirrors address state to Standby using 4 UVP message subtypes (`0xC7`):
  - `Subtype 0x01`: 5s Periodic Heartbeat (16B) with `sync_epoch`, `kr_epoch`, and address high-water mark.
  - `Subtype 0x02`: Incremental Address Delta Sync (8B) pushed immediately when addresses are allocated.
  - `Subtype 0x03`: DevKey Vault Replication (36B) mirroring target UUID and DevKey.
  - `Subtype 0x04`: Bulk Catch-Up Chunk Transfer (67B) for resynchronizing rebooted standby nodes.
- **15-Second Watchdog Failover**: Standby promotes to Master if 3 consecutive heartbeats (15s) are missed.
- *Reference Specification:* [TRD Page 5: Cascading Redundancy & Redundancy Claim](./trd/page5_redundancy_cascading.md) and [TRD Page 7](./trd/page7_resilience_memory.md).

### REQ-F07: Wavefront Provisioning Engine (Configurable $K$) with JIT Dynamic Leasing
- When multiple unprovisioned devices are detected in a cluster, the Master Provisioner SHALL support **Autonomous Wavefront Bulk Provisioning**:
  - **100% Local Proxy Crypto**: Intermediate proxies perform P-256 ECDH (~35 ms) locally, eliminating round-trip public key tunneling over the mesh.
  - **JIT Contiguous Element Block Allocation**: Proxies lease variable-sized address blocks ($N_{\text{elem}} \in [1..16]$) via `0xC8 INFILL_LEASE_REQ` and receive `0xCC INFILL_LEASE_RSP` in the background during public key exchange.
  - **Configurable $K$-Factor**:
    - `k_factor = 0` ($K = \infty$, Default): Natural Spatial Clustering. Proxies onboard local devices until a 15-second quiet window ($T_{\text{quiet}}$) triggers retirement.
    - `k_factor > 0` ($K = 2, 4$): Bounded Geometric Wavefront. Proxies retire after onboarding $K$ children, fanning out multi-radio concurrency.
  - **Dual-Phase Infill Sentry Mode**: Retired proxies switch to a micro-duty background scan (2% duty cycle) to onboard late-arriving interior stragglers.
  - **Paged Sparse 2-Bit Bitmap**: Master manages address state using dynamic 32-byte chunks (< 1 KB total RAM for 1,000 nodes).
- *Reference Specification:* [TRD Page 8: Wavefront Provisioning Engine](./trd/page8_fast_bulk_provisioning.md).

### REQ-F08: N+1 Dynamic Provisioner Cascading & State Freshness Vector Arbitration
- When an active Master fails, the Standby Node promotes to Active Master and broadcasts `UVP_OP_REDUNDANCY_CLAIM` (`0xCD`) inviting idle backup-capable nodes (`ROLE_CANDIDATE`) to claim `ROLE_STANDBY`.
- **State Freshness Vector Hierarchy**: Split-brain or multi-master collisions are arbitrated lexicographically:
  $$\vec{V}_{\text{fresh}} = \langle \mathbf{kr\_epoch},\; \mathbf{sync\_epoch},\; \mathbf{next\_free\_addr},\; -\text{Addr},\; -\text{UUID} \rangle$$
- **Stale Master Trap Elimination**: Rebooted nodes with stale epochs demote to `ROLE_CANDIDATE` unconditionally, preserving newer provisioning data on active nodes.
- **Additive Bitwise Union (`OR` Merge)**: Losing nodes flush address deltas via `Subtype 0x02` to achieve 0% data loss.
- **Dual Standby Deterministic Tie-Breaker**: Node with $\min(\text{Addr}, \text{UUID})$ retains `ROLE_STANDBY`; higher node demotes to `ROLE_CANDIDATE`.
- *Reference Specification:* [TRD Page 5: Cascading Redundancy](./trd/page5_redundancy_cascading.md) and [TRD Page 7](./trd/page7_resilience_memory.md).

### REQ-F09: Long-Term Inactivity Eviction & Autonomous DevKey Re-Healing
- **Eviction Horizon**: Nodes offline for $> 10\text{ minutes}$ ($\Delta\text{Epoch} \ge 2$) are marked `DECOMMISSIONED / STALE` by the provisioner.
- **Autonomous DevKey Self-Healing (`0xCE`/`0xCF`)**:
  - Evicted nodes detect cryptographic isolation and emit an authenticated HMAC-SHA256 re-healing beacon (`0xCE`) signed with permanent `DevKey` over `[UUID || Old_Addr || Nonce]`.
  - Master verifies HMAC against its NVS DevKey Vault and returns `0xCF REHEAL_ADMIT_RSP` encrypted with target's `DevKey` containing the active `NetKey` and current `Epoch`.
  - Node is autonomously re-admitted without button presses or factory wipes.
- *Reference Specification:* [TRD Page 6: Long-Term Eviction & DevKey Self-Healing](./trd/page6_eviction_healing.md).

### REQ-F10: Supervised Node Replacement & Unicast Reallocation
- The Provisioner SHALL provide a deterministic **Node Replacement Engine** exposed via MXCP (`MXCP_CMD_NODE_REPLACE_START` = `0x36`, `MXCP_EVT_NODE_REPLACE_STATUS` = `0xB6`):
  - **Modality A (Identical Composition $N_{\text{new}} = N_{\text{old}}$, Schema Match)**: Inherits original base unicast address in-place ($\text{Addr}_{\text{new}} = \text{Addr}_{\text{old}}$), preserving all existing group subscriptions, scene memberships, and publication bindings without network reconfiguration.
  - **Modality B (Identical Elements $N_1 = N_2$, Differing Models)**: Re-uses base address in-place; Master reads Composition Data Page 0, reconciles shared models, binds AppKeys to new models, and emits `MXCP_EVT_NODE_REPLACE_SCHEMA_DIFF` (`0xB7`) for host configuration.
  - **Modality C1 (Contraction $N_{\text{new}} < N_{\text{old}}$)**: Provisions new device into $[\text{Addr}_{\text{base}} .. \text{Addr}_{\text{base}} + N_{\text{new}} - 1]$; surplus trailing addresses transition to `00b (UNALLOCATED)` and are placed in the **24-Hour RPL Quarantine Table** to prevent premature recycling while neighbor caches settle.
  - **Modality C2 (Expansion $N_{\text{new}} > N_{\text{old}}$)**: Checks adjacent address contiguity; if collision is detected, retires old block to quarantine, JIT leases a fresh contiguous block from the unified pool, and migrates group subscriptions.
  - **Replay Protection (RPL) Fast-Forward**: Programs initial sequence number to $\text{SeqNum}_{\text{init}} = \text{SeqNum}_{\text{last}} + 1000$ to eliminate silent neighbor packet drops.
  - **DevKey Vault Swap & Anti-Zombie Blacklist**: Overwrites DevKey in NVS Vault; registers old hardware UUID in blacklist to permanently reject rogue `0xCE` re-healing attempts.
- *Reference Specification:* [TRD Page 9: Node Replacement & Unicast Lease Reallocation](./trd/page9_node_replacement.md).

---

## 3. Non-Functional Requirements (REQ-NF)

### REQ-NF01: Flash & RAM Footprint Budget (< 15 KB Flash, < 1.0 KB RAM)
- Total new code footprint for the complete UVP suite SHALL NOT exceed **15 KB** of flash (measured: +11 KB on ESP32-C3).
- Static and dynamic RAM impact for 1,000 provisioned nodes SHALL NOT exceed **1.0 KB** (measured: 832 Bytes max).
- Application flash headroom SHALL remain **$\ge 40\%$ free** (budget: 1900 KB partition, 1042 KB total binary, **45.1% free headroom**).
- *Reference Specification:* [TRD Page 7: Resilience & Memory Analysis](./trd/page7_resilience_memory.md).

### REQ-NF02: Baseline Native Stack Preservation
- Retain the baseline lightweight node configuration (`CONFIG_BLE_MESH_NODE=y`, `CONFIG_BLE_MESH_PROV=y`) unaltered to preserve standard SIG smartphone app onboarding compatibility over PB-ADV/PB-GATT.
- No heavy SIG models (`CONFIG_BLE_MESH_PROVISIONER`, `CONFIG_BLE_MESH_CONFIG_CLIENT`, `CONFIG_BLE_MESH_RPR_*`) shall be enabled; all advanced operations are executed over the UVP vendor layer.

### REQ-NF03: Deterministic Timeout & Link Teardown
- RPR tunnel sessions and OOB authentication links shall timeout deterministically after 30 seconds of radio inactivity.
- In-flight dynamic leases (`0xC8`) shall rollback atomically from `01b (ON_LEASE)` to `00b (UNALLOCATED)` after 30 seconds if uncommitted.
- Supervised node replacement windows (`0x36`) shall disarm and rollback after 60 seconds if the replacement device is not detected.

### REQ-NF04: Flash Wear Endurance & OS-Agnostic SAL Debouncing
- In high-speed bulk provisioning (e.g. 100 nodes in 20s), the Storage Abstraction Layer (SAL) SHALL buffer Paged Sparse Bitmap updates in RAM and flush to physical flash with a **1000 ms quiet debounce timer** or when dirty node count $\ge 10$.
- Achieves a **$7.5\times$ to $10\times$ reduction in SPI flash wear**, preventing sector wearout and radio interrupt blocking on Zephyr OS and FreeRTOS.
- *Reference Specification:* [TRD Page 7](./trd/page7_resilience_memory.md#24-flash-endurance-wearout-mitigation-os-agnostic-nvs-debounce-layer-vector-8) and [TRD Page 8](./trd/page8_fast_bulk_provisioning.md#84-flash-wear-mitigation--os-portability-esp-idf-vs-zephyr).

---

## 4. Future Requirements Roadmap (Gateway & Connectivity Evolution)

### REQ-F-ROADMAP-01: Binary MXCP Encapsulation over MQTT
- The Gateway capability SHALL support transporting MXCP frames encapsulated directly as binary payloads over MQTT topics rather than heavy JSON text representations.
- **Topic Hierarchy**:
  - `meshx/{mesh_id}/downlink`: Host $\rightarrow$ Gateway $\rightarrow$ Mesh nodes (`MXCP_CMD_*`).
  - `meshx/{mesh_id}/uplink`: Mesh nodes $\rightarrow$ Gateway $\rightarrow$ Host / Cloud (`MXCP_EVT_*`).
- **Memory & Bandwidth Savings**:
  - Eliminates JSON string parsing, deserialization buffers, and formatting overhead, saving ~15 KB heap and reducing Wi-Fi payload sizes to bare wire bytes (`[SOF][LEN][TYPE][PAYLOAD][CHK][EOF]`).

### REQ-F-ROADMAP-02: TCP / Stream-based OTA Client
- The Gateway node SHALL provide background TCP/HTTPS OTA fetch capability targeting the dual `ota_0`/`ota_1` (`1900 KB`) slots.
- Download pipelines SHALL stream image blocks directly into flash sectors with minimal internal RAM buffering (e.g. 4 KB chunking).

### REQ-F-ROADMAP-03: lwIP mDNS Discovery & Zero-Config LAN Provisioning
- The network stack SHALL integrate `CONFIG_LWIP_MDNS_ENABLED=y` on the lwIP layer.
- **Service Advertising**:
  - Broadcasts service `_meshx-gateway._tcp.local` on port 1883 (MQTT) or 8080 (Web Console / REST) allowing auto-discovery by mobile apps, local dashboards, and LAN controllers without hardcoded IP configuration.
- **Footprint Constraint**:
  - mDNS daemon memory overhead shall remain constrained below 4 KB DRAM.

