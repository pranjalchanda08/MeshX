# Design Specification: UVP-Driven Provisioning & Security Management Architecture

**Subsystems:** `meshx_prov_srv`, `meshx_uvp_dispatcher`, `meshx_mxcp`, `meshx_api`, `meshx_nvs`
**Status:** Approved Architecture (Zero-Bloat UVP Architecture)
**Parent Requirements:** [requirements.md](./requirements.md) (REQ-F01 to REQ-F10, REQ-NF01 to REQ-NF04)
**Implementation Tasks:** [tasks.md](./tasks.md)
**Master TRD Index:** 👉 **[Technical Reference Manual (TRD Index)](./trd/README.md)**

---

## 1. System Overview & Architectural Paradigm

MeshX adopts the **Zero-Bloat UVP Architecture**, eliminating heavy native BLE Mesh SIG models (`CONFIG_BLE_MESH_PROVISIONER`, `CONFIG_BLE_MESH_CONFIG_CLIENT`, `CONFIG_BLE_MESH_RPR_SRV`, `CONFIG_BLE_MESH_RPR_CLI`) from the firmware. All remote onboarding, cryptographic key updates, cascading redundancy, and node replacements are handled over the **MeshX Unified Vendor Protocol (UVP)** and managed via the **MeshX Control Protocol (MXCP)**.

Detailed engineering specifications, protocol wire formats, algorithms, and sequence diagrams are modularized across the TRD suite below.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           Host MCU / Web Console                            │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │ MXCP Frames (UART / Serial)
┌──────────────────────────────────────▼──────────────────────────────────────┐
│                   MXCP Host Interface Dispatch Engine                       │
│                             (meshx_mxcp.c)                                  │
└──────────────────────┬───────────────────────────────┬──────────────────────┘
                       │ Control Path [0x0B..0x0F]     │ Mgmt & Bulk [0x30..0x36]
┌──────────────────────▼──────┐     ┌──────────────────▼──────────────────────┐
│  Native Prov Server (Local) │     │       MeshX UVP Dispatch Engine         │
│  CONFIG_BLE_MESH_PROV=y     │     │     (meshx_uvp_dispatcher.cpp)          │
│  - First-Time Onboarding    │     │  - RPR Tunneling & Wavefront(0xC0..0xC3)│
│  - PB-ADV / PB-GATT Only    │     │  - 5-Min Key Rotation (0xC4..0xC6)      │
└─────────────────────────────┘     │  - Provisioner Redundancy (0xC7, 0xCD)  │
                                    │  - Infill Dynamic Leasing (0xC8, 0xCC)  │
                                    │  - Post-Closure Report & Vault (0xC9)   │
                                    │  - Autonomous DevKey Re-Healing (0xCE)  │
                                    └──────────────────┬──────────────────────┘
                                                       │
┌──────────────────────────────────────────────────────▼──────────────────────┐
│                    Baseline Platform BLE Mesh Subsystem                     │
│               CONFIG_BLE_MESH_NODE=y (NO SIG PROVISIONER / NO RPR)          │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Technical Reference Manual (TRD) Navigation & Module Index

All normative technical designs, wire structures, state machines, and sequence diagrams are formally maintained in the following dedicated TRD modules:

| TRD Module | Document Link | Core Topics & System Architecture Covered |
|---|---|---|
| **TRD-PROV-01** | **[Page 1: Architecture & Zero-Bloat Philosophy](./trd/page1_overview.md)** | • Zero-Bloat Architectural Philosophy & Footprint Comparison<br/>• Unified 15-Bit Address Space (`0x0002..0x7FFF`) vs. Deprecated Pools<br/>• Separation of Concerns (Control Plane vs. Data Plane) |
| **TRD-PROV-02** | **[Page 2: Protocol Definitions & Wire Framing](./trd/page2_protocol_definitions.md)** | • MXCP Frame Structure (`SOF 0xFE .. EOF 0xEF`) & Checksum Math<br/>• MXCP Host Commands (`0x0B..0x0F`, `0x30..0x36`) & Events (`0x81..0x8F`, `0xB0..0xB7`)<br/>• UVP Vendor Opcodes (`0xC0..0xD0`) & Packed C Structures |
| **TRD-PROV-03** | **[Page 3: Remote Provisioning & Wavefront Tunneling](./trd/page3_remote_provisioning.md)** | • Model A: Ad-Hoc Transparent UVP RPR Tunneling (`0xC0..0xC3`)<br/>• Model B: Autonomous Fast Wavefront Bulk Engine Overview<br/>• End-to-End PB-ADV Packet Tunneling Sequence Diagrams |
| **TRD-PROV-04** | **[Page 4: 5-Minute Zero-ACK Key Rotation & Security Analysis](./trd/page4_key_rotation.md)** | • Zero-ACK 3-Burst Push ($O(1)$ constant RF overhead for 1,000+ nodes)<br/>• 4-Minute Dual-Key Overlap Window & NVS Atomic Pre-Commit<br/>• Company Secret Virtual Addressing (`0x8F42` via `prod_profile.yml`)<br/>• 5-Tier Reverse Engineering Defense-in-Depth & RFC 1982 Rollover Math |
| **TRD-PROV-05** | **[Page 5: Cascading Redundancy & Redundancy Claim](./trd/page5_redundancy_cascading.md)** | • Real-Time Master $\leftrightarrow$ Standby Synchronization Engine (`0xC7` Subtypes 0x01..0x04)<br/>• 15s Watchdog Failover & Dynamic $N+1$ Cascading (`0xCD`/`0xD0`)<br/>• State Freshness Vector Arbitration & Additive Bitwise Union (`OR` Merge)<br/>• Dual-Standby Deterministic Tie-Breaker ($\min(\text{Addr}, \text{UUID})$) |
| **TRD-PROV-06** | **[Page 6: Long-Term Eviction & DevKey Self-Healing](./trd/page6_eviction_healing.md)** | • Eviction Horizon (>10 Minutes Inactivity / $\Delta\text{Epoch} \ge 2$)<br/>• Autonomous Re-Healing Handshake (`0xCE`/`0xCF`) via Blind Couriers<br/>• Cryptographic HMAC-SHA256 Nonce Verification using Immutable DevKeys<br/>• Full Node Lifecycle State Machine |
| **TRD-PROV-07** | **[Page 7: Failure Modes, Resilience & Memory Analysis](./trd/page7_resilience_memory.md)** | • Comprehensive 12-Point Resilience & Failure Mitigation Matrix<br/>• In-Flight Dynamic Lease 3-State FSM & 30s Rollback<br/>• Multi-Proxy Inverse-RSSI Backoff & Early Suppression Math<br/>• Zephyr OS-Agnostic Software NVS Debounce Layer (7.5x Flash Wear Reduction)<br/>• Runtime RAM Scaling Table (< 1 KB for 1,000 nodes) & Flash Headroom (45.1% Free) |
| **TRD-PROV-08** | **[Page 8: Wavefront Bulk Engine (Configurable K) & JIT Dynamic Allocation](./trd/page8_fast_bulk_provisioning.md)** | • Configurable Branching ($K=0 \implies \infty$ Spatial Clustering vs. $K \ge 2$ Bounded Wavefront)<br/>• 100% Local Proxy Crypto (P-256 ECDH in ~35 ms)<br/>• JIT Contiguous Element Block Allocation (`0xC8`/`0xCC`)<br/>• Paged Sparse 2-Bit Bitmap (`meshx_sparse_addr_map_t`)<br/>• Dual-Phase Infill Sentry Mode (2% Micro-Duty Background Sniff) |
| **TRD-PROV-09** | **[Page 9: Node Replacement & Unicast Lease Reallocation](./trd/page9_node_replacement.md)** | • Modality A: In-Place Identical Replacement ($N_{\text{new}} = N_{\text{old}}$, Schema Match)<br/>• Modality B: In-Place Swap with Differing Models (Composition Schema Diff & `0xB7`)<br/>• Modality C1: Element Contraction ($N_{\text{new}} < N_{\text{old}}$) & 24h RPL Quarantine Table<br/>• Modality C2: Element Expansion ($N_{\text{new}} > N_{\text{old}}$) & Contiguity JIT Relocation<br/>• 3-Tier Replay Protection (RPL) SeqNum Fast-Forward & DevKey Vault Swap |

---

## 3. Subsystem Integration & Code Mapping

| Subsystem Component | Primary Implementation Files | Core Architectural Responsibilities & TRD Reference |
|---|---|---|
| **MXCP Host Interface** | `main/component/meshx/ble_mesh/common/src/meshx_mxcp.c`<br/>`main/component/meshx/ble_mesh/common/inc/meshx_mxcp.h` | Implements serial frame parsing, OOB input watchdogs, fast prov dispatch (`0x34`), and node replacement commands (`0x36`). *(TRD Pages 2, 7, 8, 9)* |
| **UVP Dispatcher** | `main/component/meshx/ble_mesh/common/src/meshx_uvp_dispatcher.cpp`<br/>`main/component/meshx/inc/meshx_uvp.h` | Dispatches vendor opcodes (`0xC0..0xD0`), executes Key Rotation state machine, Master/Standby sync, infill leasing, and re-healing. *(TRD Pages 2, 4, 5, 6, 8)* |
| **Address & Lease Allocator** | `main/component/meshx/ble_mesh/common/src/meshx_group_allocator.c`<br/>`main/component/meshx/ble_mesh/common/inc/meshx_group_allocator.h` | Manages Paged Sparse 2-Bit Bitmap (`meshx_sparse_addr_map_t`), JIT contiguous element block allocator, 30s lease watchdogs, and 24h RPL quarantine. *(TRD Pages 1, 7, 8, 9)* |
| **NVS Vault & Debounce Layer** | `main/component/meshx/src/nvs/meshx_nvs.c`<br/>`main/component/meshx/inc/nvs/meshx_nvs.h` | Manages Master DevKey NVS Vault, atomic pre-commit of `kr_epoch`, and cross-platform SAL software debounce timer (1000 ms / 10 dirty nodes). *(TRD Pages 4, 7, 8)* |
| **Platform Crypto & Hardware Acceleration** | `main/component/meshx/interface/crypto/meshx_crypto.h`<br/>`port/platform/esp/esp_idf/utils/meshx_crypto_esp.c`<br/>`port/platform/zephyr/utils/meshx_crypto_zephyr.c` | Hardware-accelerated P-256 ECDH calculation (~35 ms) on proxy nodes and HMAC-SHA256 calculation for re-healing beacons. *(TRD Pages 6, 8)* |
| **Build & Configuration Pipeline** | `tools/scripts/code_gen.py`<br/>`port/bsp/weact_c3/prod_profile.yml` | Parses company secret management virtual address (`mgmt_virtual_addr` = `0x8F42`) into generated configuration headers. *(TRD Page 4)* |

---

## 4. Verification & Validation Roadmap

Verification and regression testing will be established via automated Python bench harnesses (`tools/scripts/host_decoder.py`) and simulated multi-hop topologies as outlined in [tasks.md](./tasks.md) (`TASK-BPROV-008`).
