# Implementation Tasks: UVP-Driven Provisioning & Security Management with MXCP Capability

**Status:** Approved Architectural Implementation Plan  
**Target Subsystems:** `meshx_prov_srv`, `meshx_uvp_dispatcher`, `meshx_mxcp`, `meshx_api`, `meshx_nvs`, `tools/scripts`  
**Parent Requirements:** [requirements.md](./requirements.md)  
**System Architecture:** [design.md](./design.md)  
**Master TRD Index:** 👉 **[Technical Reference Manual (TRD Index)](./trd/README.md)**

---

## 1. Task Dependency Overview & Critical Path

The tasks are partitioned into functional milestones aligned with the Zero-Bloat UVP Architecture across TRD Pages 1 through 9.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ Phase 1: Core Foundation & Dynamic Address Space (TRD Pages 1, 2, 7, 8)     │
│ [TASK-BPROV-001] Deprecate Dual-Pool & Unified 15-Bit Address Space         │
│ [TASK-BPROV-002] Paged Sparse 2-Bit Bitmap Allocator & 30s Lease Engine     │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │
┌──────────────────────────────────────▼──────────────────────────────────────┐
│ Phase 2: Host Interface & Autonomous Bulk Wavefront Engine (TRD Pages 2,3,8)│
│ [TASK-BPROV-003] MXCP Host Commands & Local Hardware P-256 Crypto Engine    │
│ [TASK-BPROV-004] JIT Contiguous Block Leasing & Wavefront Dispatcher        │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │
┌──────────────────────────────────────▼──────────────────────────────────────┐
│ Phase 3: Security, Rotation & Redundancy Engine (TRD Pages 4, 5, 6)         │
│ [TASK-KROT-001]  5-Minute Zero-ACK Key Rotation & NVS Atomic Pre-Commit     │
│ [TASK-REDUND-001] Active/Standby Sync, Freshness Vector & Watchdog Failover  │
│ [TASK-HEAL-001]  Autonomous DevKey Re-Healing & HMAC-SHA256 Handshake       │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │
┌──────────────────────────────────────▼──────────────────────────────────────┐
│ Phase 4: Supervised Node Replacement & Storage Hardening (TRD Pages 7, 9)   │
│ [TASK-REPLACE-001] Node Replacement Engine, RPL Fast-Forward & Quarantine   │
│ [TASK-FLASH-001]   Cross-Platform SAL NVS Debounce Layer (7.5x Wear Red.)   │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │
┌──────────────────────────────────────▼──────────────────────────────────────┐
│ Phase 5: Tooling, Host Decoders & Verification (TRD Suite)                  │
│ [TASK-TOOL-001]   Host Decoder Updates (Python CLI, Framing & Dissectors)   │
│ [TASK-BPROV-008]  End-to-End Regression Test Suite & Multi-Hop Validation   │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │
┌──────────────────────────────────────▼──────────────────────────────────────┐
│ Phase 6: Architectural Audit & Code vs TRD Compliance Review                │
│ [TASK-REV-001]    Comprehensive Codebase vs TRD Suite Compliance Audit      │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Phase 1: Core Foundation & Dynamic Address Space

### `[TASK-BPROV-001]` Unified 15-Bit Address Space & Legacy Pool Deprecation
- [x] **Goal:** Completely eliminate legacy partitioned pools (`Pool A: 0x0100..0x01FF`, `Pool B: 0x0200..0x03FF`) in favor of a single contiguous 15-bit unicast address space (`0x0002..0x7FFF`).
- **TRD Reference:** [TRD Page 1: Section 2](./trd/page1_overview.md#2-unified-15-bit-address-space-architecture), [TRD Page 7: Section 5.1](./trd/page7_resilience_memory.md#51-paged-sparse-bitmap-vs-flat-allocation)
- **Target Files:**
  - `main/component/meshx/inc/pairing/meshx_group_allocator.h`
  - `main/component/meshx/src/pairing/meshx_group_allocator.c`
- **Subtasks:**
  - [x] 1. Remove legacy macro constants `MESHX_PROV_POOL_A_START`, `MESHX_PROV_POOL_A_END`, `MESHX_PROV_POOL_B_START`, `MESHX_PROV_POOL_B_END` in favor of unified range.
  - [x] 2. Define `MESHX_UNICAST_ADDR_START` (`0x0002`) and `MESHX_UNICAST_ADDR_END` (`0x7FFF`).
  - [x] 3. Ensure all allocation APIs reject requests for addresses outside this valid range or within reserved bounds (`0x0000`, `0x0001`, `0x8000..0xFFFF`).
  - [x] 4. Verify backward compatibility with existing unit tests referencing group and address bounds.

### `[TASK-BPROV-002]` Paged Sparse 2-Bit Bitmap Allocator & In-Flight Lease Lifecycle
- [x] **Goal:** Implement the memory-efficient 2-bit state tracking allocator (`STATE_FREE=00b`, `STATE_IN_FLIGHT=01b`, `STATE_ASSIGNED=10b`, `STATE_QUARANTINED=11b`) with dynamic page chunks and a 30s auto-rollback watchdog.
- **TRD Reference:** [TRD Page 7: Section 2](./trd/page7_resilience_memory.md#2-in-flight-dynamic-lease-lifecycle--watchdog-state-machine), [Section 5.2](./trd/page7_resilience_memory.md#52-data-structure-definitions), [TRD Page 8: Section 5](./trd/page8_fast_bulk_provisioning.md#5-address-allocation--concurrency-control-infill-algorithm)
- **Target Files:**
  - `main/component/meshx/inc/pairing/meshx_group_allocator.h`
  - `main/component/meshx/src/pairing/meshx_group_allocator.c`
- **Subtasks:**
  - [x] 1. Define C packed structures `meshx_sparse_addr_map_t`, `meshx_addr_chunk_t` (2-bit state tracking per address), and `meshx_lease_table_t`.
  - [x] 2. Implement `meshx_lease_allocate_block(uint8_t count, uint16_t proxy_addr, const uint8_t target_uuid[16], uint16_t *out_base_addr, uint32_t *out_lease_id)` to locate $N$ contiguous free slots and mark them `STATE_IN_FLIGHT`.
  - [x] 3. Implement `meshx_lease_commit(uint32_t lease_id)` to transition slots to `STATE_ASSIGNED`.
  - [x] 4. Implement `meshx_lease_rollback(uint32_t lease_id)` to reset unconfirmed slots to `STATE_FREE`.
  - [x] 5. Implement 30-second watchdog routine `meshx_lease_watchdog_poll()` executing rollover checks.

---

## 3. Phase 2: Host Interface & Autonomous Bulk Wavefront Engine

### `[TASK-BPROV-003]` MXCP Host Interface & Local Hardware P-256 Crypto Engine
- [x] **Goal:** Add host control commands and asynchronous events for bulk provisioning and configure local proxy P-256 ECDH calculation to eliminate host roundtrips.
- **TRD Reference:** [TRD Page 2: Section 3 & 4](./trd/page2_protocol_definitions.md#3-mxcp-host-interface-opcodes-and-frames), [TRD Page 8: Section 4](./trd/page8_fast_bulk_provisioning.md#4-cryptographic-acceleration--local-p-256-ecdh)
- **Target Files:**
  - `main/component/meshx/ble_mesh/common/inc/meshx_mxcp.h`
  - `main/component/meshx/ble_mesh/common/src/meshx_mxcp.c`
  - `port/platform/esp/esp_idf/utils/meshx_crypto_esp.c`
  - `port/platform/zephyr/utils/meshx_crypto_zephyr.c`
- **Subtasks:**
  - [x] 1. Define MXCP command `MXCP_CMD_FAST_PROV_START` (`0x34`) with payload struct `mxcp_cmd_fast_prov_start_t` (Wavefront limit $K$, timeout, auth mask).
  - [x] 2. Define MXCP command `MXCP_CMD_FAST_PROV_CANCEL` (`0x35`).
  - [x] 3. Define MXCP events `MXCP_EVT_FAST_PROV_PROGRESS` (`0xB4`) and `MXCP_EVT_FAST_PROV_COMPLETE` (`0xB5`).
  - [x] 4. Implement hardware-accelerated ECDH public/private keypair generation and shared secret computation using PSA / MbedTLS / ESP hardware crypto driver (~35 ms execution time).
  - [x] 5. Implement out-of-band input watchdog handling in `meshx_mxcp.c`.

### `[TASK-BPROV-004]` JIT Contiguous Block Leasing & Wavefront Dispatcher
- [x] **Goal:** Implement the autonomous wavefront provisioning pipeline (`0xC8` / `0xCC` / `0xC9`) and multi-proxy inverse-RSSI backoff election.
- **TRD Reference:** [TRD Page 3: Section 3](./trd/page3_remote_provisioning.md#3-autonomous-fast-wavefront-engine-model-b), [TRD Page 7: Section 3](./trd/page7_resilience_memory.md#3-multi-proxy-inv-rssi-backoff-math--early-suppression), [TRD Page 8: Section 2, 3 & 6](./trd/page8_fast_bulk_provisioning.md#2-wavefront-branching-architecture-configurable-k)
- **Target Files:**
  - `main/component/meshx/ble_mesh/common/inc/meshx_uvp.h`
  - `main/component/meshx/ble_mesh/common/src/meshx_uvp_dispatcher.cpp`
  - `main/component/meshx/ble_mesh/common/src/meshx_prov_srv.cpp`
- **Subtasks:**
  - [x] 1. Define UVP opcodes `OP_UVP_INFILL_LEASE_REQ` (`0xC8`), `OP_UVP_INFILL_LEASE_RSP` (`0xCC`), and `OP_UVP_INFILL_POST_CLOSE` (`0xC9`).
  - [x] 2. Implement Proxy inverse-RSSI backoff calculation: $T_{\text{backoff}} = \max(20\text{ ms}, ((-50 - \text{RSSI}) \times 8) + \text{rand}(0, 40)\text{ ms})$.
  - [x] 3. Implement JIT dynamic lease request transmission (`0xC8`) with element count $N$ upon reading device capabilities.
  - [x] 4. Implement Master allocator handler processing `0xC8` and dispatching unicast response `0xCC` carrying `lease_id` and `allocated_start_addr`.
  - [x] 5. Implement local PB-ADV completion on Proxy, followed by DevKey and element metadata delivery to Master via `0xC9` (`OP_UVP_INFILL_POST_CLOSE`).
  - [x] 6. Implement 2% Sentry Mode background unprovisioned beacon sniffing duty cycle.

---

## 4. Phase 3: Security, Rotation & Redundancy Engine

### `[TASK-KROT-001]` 5-Minute Zero-ACK Key Rotation Engine & Atomic Commit
- [x] **Goal:** Implement the 5-minute zero-ACK Key Rotation state machine broadcasting across company secret virtual address `0x8F42` with 4-minute dual-key overlap and NVS atomic pre-commit.
- **TRD Reference:** [TRD Page 2: Section 5.2](./trd/page2_protocol_definitions.md#52-5-minute-key-rotation-opcodes-0xc40xc6), [TRD Page 4: Full Specification](./trd/page4_key_rotation.md)
- **Target Files:**
  - `main/component/meshx/ble_mesh/common/inc/meshx_uvp.h`
  - `main/component/meshx/ble_mesh/common/src/meshx_uvp_dispatcher.cpp`
  - `main/component/meshx/src/nvs/meshx_nvs.c`
  - `port/bsp/weact_c3/prod_profile.yml`
  - `tools/scripts/code_gen.py`
- **Subtasks:**
  - [x] 1. Update `code_gen.py` and `prod_profile.yml` to inject `MESHX_MGMT_VIRTUAL_ADDR 0x8F42` and `MESHX_MGMT_UUID` into build config headers.
  - [x] 2. Define wire structs `uvp_key_phase1_prepare_t` (`0xC4`), `uvp_key_phase2_activate_t` (`0xC5`), and `uvp_key_phase3_revoke_t` (`0xC6`).
  - [x] 3. Implement Master rotation state machine: Phase 1 broadcast (3 bursts, 200 ms interval) $\rightarrow$ 60s soak $\rightarrow$ Phase 2 broadcast (3 bursts) $\rightarrow$ 240s dual-RX overlap $\rightarrow$ Phase 3 broadcast (3 bursts).
  - [x] 4. Implement atomic NVS pre-commit of `kr_epoch` prior to RF broadcast to survive in-flight reboots.
  - [x] 5. Implement RFC 1982 modular signed distance window: $((\text{new\_epoch} - \text{curr\_epoch}) \pmod{256}) < 128$.
  - [x] 6. Enforce 5-tier reverse engineering defense-in-depth: virtual address secrecy, replay protection, AES-128 NetKey encryption, DevKey isolation, and flash read-back protection.

### `[TASK-REDUND-001]` Provisioner Redundancy, State Freshness Vector & Watchdog Failover
- [x] **Goal:** Implement real-time Master $\leftrightarrow$ Standby state replication (`0xC7`), State Freshness Vector comparison, additive bitwise union (`OR` merge), and 15s watchdog promotion.
- **TRD Reference:** [TRD Page 2: Section 5.3](./trd/page2_protocol_definitions.md#53-redundancy--cascading-opcodes-0xc7-0xcd-0xd0), [TRD Page 5: Full Specification](./trd/page5_redundancy_cascading.md)
- **Target Files:**
  - `main/component/meshx/ble_mesh/common/inc/meshx_uvp.h`
  - `main/component/meshx/ble_mesh/common/src/meshx_uvp_dispatcher.cpp`
  - `main/component/meshx/ble_mesh/common/src/meshx_group_allocator.c`
- **Subtasks:**
  - [x] 1. Define UVP opcode `OP_UVP_REDUND_SYNC` (`0xC7`) with subtypes: `0x01` (Heartbeat / Freshness Vector), `0x02` (Addr Map Delta), `0x03` (DevKey Mirror), `0x04` (Bulk Snapshot).
  - [x] 2. Implement Master 5000 ms periodic heartbeat broadcast emitting `(kr_epoch, sync_epoch, next_free_addr, -Addr, -UUID)`.
  - [x] 3. Implement Standby 15-second watchdog timer: if no Master heartbeat is received within 15s, execute deterministic self-promotion.
  - [x] 4. Implement dual-standby arbitration: if multiple standbys detect Master loss, evaluate $\min(\text{UnicastAddr}, \text{UUID})$ to elect single active Master.
  - [x] 5. Implement 3-way multi-master collision reconciliation using lexicographical State Freshness Vector comparison.
  - [x] 6. Implement Additive Bitwise Union (`OR` merge) for sparse address bitmaps to prevent slot collision upon partitioned mesh merge.
  - [x] 7. Implement dynamic $N+1$ cascading claim frames (`OP_UVP_REDUND_CLAIM_ROLE` `0xCD` and `OP_UVP_REDUND_REJECT` `0xD0`).

### `[TASK-HEAL-001]` Autonomous DevKey Re-Healing Engine & Blind Courier Handshake
- [x] **Goal:** Implement automated re-healing for nodes partitioned during key rotation using DevKey HMAC-SHA256 authentication and blind courier encapsulation.
- **TRD Reference:** [TRD Page 2: Section 5.4](./trd/page2_protocol_definitions.md#54-eviction-and-devkey-re-healing-opcodes-0xce-0xcf), [TRD Page 6: Full Specification](./trd/page6_eviction_healing.md)
- **Target Files:**
  - `main/component/meshx/ble_mesh/common/inc/meshx_uvp.h`
  - `main/component/meshx/ble_mesh/common/src/meshx_uvp_dispatcher.cpp`
- **Subtasks:**
  - [x] 1. Define wire structs `uvp_reheal_beacon_t` (`0xCE`) and `uvp_reheal_admit_t` (`0xCF`).
  - [x] 2. Implement isolated node partition detection ($\Delta\text{Epoch} \ge 2$ or 10-minute loss of valid network traffic).
  - [x] 3. Implement unencrypted re-healing beacon emission (`0xCE`) on advertising channels carrying `HMAC-SHA256(DevKey, Nonce || Addr)`.
  - [x] 4. Implement neighbor node Blind Courier forwarding of `0xCE` to active Master over mesh backbone.
  - [x] 5. Implement Master DevKey lookup, HMAC verification, and dispatch of encrypted admission envelope `0xCF` delivering current NetKey and phase state.

---

## 5. Phase 4: Supervised Node Replacement & Storage Hardening

### `[TASK-REPLACE-001]` Supervised Node Replacement, RPL Fast-Forward & Quarantine
- [x] **Goal:** Implement end-to-end Node Replacement handling Modalities A, B, C1, and C2, sequence number fast-forwarding, DevKey vault swap, and 24-hour RPL quarantine.
- **TRD Reference:** [TRD Page 2: Section 3 & 4](./trd/page2_protocol_definitions.md#3-mxcp-host-interface-opcodes-and-frames), [TRD Page 7: Section 5.2](./trd/page7_resilience_memory.md#52-data-structure-definitions), [TRD Page 9: Full Specification](./trd/page9_node_replacement.md)
- **Target Files:**
  - `main/component/meshx/ble_mesh/common/inc/meshx_mxcp.h`
  - `main/component/meshx/ble_mesh/common/src/meshx_mxcp.c`
  - `main/component/meshx/ble_mesh/common/src/meshx_group_allocator.c`
  - `main/component/meshx/ble_mesh/common/src/meshx_uvp_dispatcher.cpp`
- **Subtasks:**
  - [x] 1. Define MXCP command `MXCP_CMD_NODE_REPLACE_START` (`0x36`) with payload struct `mxcp_cmd_node_replace_start_t`.
  - [x] 2. Define MXCP events `MXCP_EVT_NODE_REPLACE_STATUS` (`0xB6`) and `MXCP_EVT_NODE_REPLACE_SCHEMA_DIFF` (`0xB7`).
  - [x] 3. Implement `REPLACEMENT_HOLD` state in allocator locking target unicast address range during replacement.
  - [x] 4. Implement Composition Data Page 0 validation against old node profile:
     - **Modality A:** Exact match $\rightarrow$ proceed with in-place provisioning.
     - **Modality B:** Element count match, model divergence $\rightarrow$ emit `0xB7` event to host MCU for model mapping confirmation.
     - **Modality C1 ($N_{\text{new}} < N_{\text{old}}$):** Assign base elements in-place; decommission surplus trailing addresses into 24-hour RPL Quarantine Table.
     - **Modality C2 ($N_{\text{new}} > N_{\text{old}}$):** Check adjacent contiguity; if colliding, invoke JIT block relocation allocating $N_{\text{new}}$ contiguous slots and placing entire old block into 24-hour quarantine.
  - [x] 5. Implement Sequence Number Fast-Forward: $\text{SeqNum}_{\text{init}} = \text{SeqNum}_{\text{last}} + 1000$ to prevent neighbor RPL dropping.
  - [x] 6. Implement Master DevKey Vault swap: overwrite DevKey of target unicast address with replacement node DevKey.
  - [x] 7. Implement hardware UUID permanent anti-zombie blacklisting for decommissioned devices.

### `[TASK-FLASH-001]` Cross-Platform SAL NVS Debounce Layer (7.5x Flash Wear Reduction)
- [x] **Goal:** Implement the OS-agnostic software flash debounce layer buffering allocation mutations and flushing to non-volatile storage every 1000 ms or 10 dirty mutations.
- **TRD Reference:** [TRD Page 7: Section 4](./trd/page7_resilience_memory.md#4-zephyr-os-agnostic-software-nvs-debounce-layer)
- **Target Files:**
  - `main/component/meshx/src/nvs/meshx_nvs.c`
  - `main/component/meshx/inc/nvs/meshx_nvs.h`
- **Subtasks:**
  - [x] 1. Define C struct `meshx_nvs_debounce_ctx_t` with RAM cache, dirty counter, and 1000 ms FreeRTOS software timer.
  - [x] 2. Implement `meshx_nvs_dirty_alloc(uint16_t addr, uint8_t count)`: update RAM cache, increment dirty counter.
  - [x] 3. Implement debounce flush trigger: execute physical write if `dirty_count >= 10` or timer expires (1000 ms).
  - [x] 4. Ensure immediate atomic flush for critical security events (Key Rotation epoch commits, DevKey vault updates).
  - [x] 5. Validate flash endurance: verify $\ge 7.5\times$ wear reduction during burst onboarding (400 writes reduced to ~40 writes for 100 nodes).

---

## 6. Phase 5: Tooling, Host Decoders & Verification

### `[TASK-TOOL-001]` Python Host Decoder & Diagnostic Frame Dissectors
- [x] **Goal:** Update `host_decoder.py` with serial packet decoders for all new MXCP commands (`0x34..0x36`) and events (`0xB4..0xB7`).
- **TRD Reference:** [TRD Page 2: Section 3 & 4](./trd/page2_protocol_definitions.md#3-mxcp-host-interface-opcodes-and-frames), [TRD Page 9: Section 3](./trd/page9_node_replacement.md#3-mxcp-host-interface-extensions)
- **Target Files:**
  - `tools/scripts/host_decoder.py`
- **Subtasks:**
  - [x] 1. Add packet dissectors for `MXCP_CMD_FAST_PROV_START` (`0x34`) and `MXCP_CMD_FAST_PROV_CANCEL` (`0x35`).
  - [x] 2. Add packet dissectors for `MXCP_EVT_FAST_PROV_PROGRESS` (`0xB4`) and `MXCP_EVT_FAST_PROV_COMPLETE` (`0xB5`).
  - [x] 3. Add packet dissectors for `MXCP_CMD_NODE_REPLACE_START` (`0x36`), `MXCP_EVT_NODE_REPLACE_STATUS` (`0xB6`), and `MXCP_EVT_NODE_REPLACE_SCHEMA_DIFF` (`0xB7`).
  - [x] 4. Implement human-readable log formatting for replacement modalities and schema diffs.

### `[TASK-BPROV-008]` Automated End-to-End Regression & Stress Validation Suite
- [x] **Goal:** Build automated test harnesses verifying bulk wavefront throughput, key rotation soak, standby failover arbitration, and node replacement modalities.
- **TRD Reference:** [TRD Page 7: Section 1](./trd/page7_resilience_memory.md#1-resilience-and-failure-mitigation-matrix), [TRD Page 8: Section 8](./trd/page8_fast_bulk_provisioning.md#8-verification--stress-testing-matrix), [TRD Page 9: Section 8](./trd/page9_node_replacement.md#8-verification--stress-testing-matrix)
- **Target Files:**
  - `tools/scripts/test_fast_prov_harness.py` (New test script)
  - `tools/scripts/test_node_replace_harness.py` (New test script)
- **Subtasks:**
  - [x] 1. Test Wavefront throughput: verify 100-node simulated onboarding completes within $15\text{ s} \pm 2\text{ s}$ under $K=0$.
  - [x] 2. Test In-flight rollback: simulate proxy crash during step 4; verify unconfirmed lease rolls back to `STATE_FREE` at $T=30\text{ s}$.
  - [x] 3. Test Key Rotation: execute full 3-phase cycle across 100 nodes; verify 100% reception and zero packet loss during 4-min dual-key overlap.
  - [x] 4. Test Redundancy: cut Master power; verify Standby promotes at $T=15.0\text{ s}$ and processes subsequent infill requests seamlessly.
  - [x] 5. Test Node Replacement Modalities A, B, C1, and C2: verify SeqNum fast-forward ($+1000$), schema diff event emission (`0xB7`), and 24h quarantine enforcement.

---

## 7. Phase 6: Architectural Audit & Code vs TRD Compliance Review

### `[TASK-REV-001]` Comprehensive Codebase vs TRD Suite Compliance Audit
- [x] **Goal:** Conduct a rigorous line-by-line static and architectural audit comparing the active firmware implementation against all 9 Technical Reference Documents to ensure zero code-spec drift, strict rule adherence, and clean memory bounds.
- **TRD Reference:** 👉 **[TRD Documentation Index](./trd/README.md)** (Pages 1 through 9)
- **Target Files:**
  - `main/component/meshx/ble_mesh/common/src/meshx_mxcp.c`
  - `main/component/meshx/ble_mesh/common/src/meshx_uvp_dispatcher.cpp`
  - `main/component/meshx/ble_mesh/common/src/meshx_group_allocator.c`
  - `main/component/meshx/ble_mesh/common/src/meshx_prov_srv.cpp`
  - `main/component/meshx/src/nvs/meshx_nvs.c`
- **Audit Checklist & Verification Items:**
  - [x] 1. **Zero C `.h` direct inclusion in `.cpp`/`.hpp`:** Verify strict adherence to [coding_guideline.md](file:///d:/MeshX/.agents/rules/coding_guideline.md). Ensure all C headers are linked solely via `meshx_c_header.h` and no circular dependency regressions exist.
  - [x] 2. **Wire Framing & Packed Struct Parity (TRD Page 2):** Audit all MXCP frame payloads (`0x0B..0x0F`, `0x30..0x36`, `0x81..0x8F`, `0xB0..0xB7`) and UVP vendor payloads (`0xC0..0xD0`) against TRD Page 2 definitions. Verify `__attribute__((packed))` on all network structures to prevent compiler padding mismatches.
  - [x] 3. **Address Pool Range Integrity (TRD Page 1 & 7):** Ensure complete removal of legacy `Pool A` / `Pool B` macros. Confirm the allocator strictly operates on `0x0002..0x7FFF` and correctly flags `STATE_FREE`, `STATE_IN_FLIGHT`, `STATE_ASSIGNED`, and `STATE_QUARANTINED`.
  - [x] 4. **Key Rotation Timing & State Machine Parity (TRD Page 4):** Verify the 5-minute schedule: zero-ACK 3-burst push, 60s soak, 240s dual-RX overlap, RFC 1982 modular distance window, and pre-broadcast NVS commit.
  - [x] 5. **Redundancy & Multi-Master Resolution (TRD Page 5):** Verify the 5-tuple State Freshness Vector comparison $(\mathbf{kr\_epoch}, \mathbf{sync\_epoch}, \mathbf{next\_free\_addr}, -\text{Addr}, -\text{UUID})$, additive bitwise `OR` merge on address bitmaps, and 15s watchdog failover timeout.

  - [x] 6. **Re-Healing Cryptographic Nonce & HMAC (TRD Page 6):** Verify HMAC-SHA256 nonce validation over DevKey for unencrypted beacon admission (`0xCE`/`0xCF`).
  - [x] 7. **Memory Footprint & Flash Debounce Endurance (TRD Page 7):** Verify RAM allocations stay under the `< 1 KB` budget for 1,000 nodes. Verify the cross-platform SAL software debounce layer commits dirty allocations only after 1000 ms or $\ge 10$ dirty updates.
  - [x] 8. **Wavefront JIT Leasing & Proxy Crypto (TRD Page 8):** Verify proxy nodes perform local P-256 ECDH (~35 ms) without tunneling raw public keys to Master. Verify JIT contiguous block requests (`0xC8`/`0xCC`) and 30s auto-rollback timeout.
  - [x] 9. **Node Replacement Modalities & Invariants (TRD Page 9):** Audit replacement workflows for Modalities A, B, C1, and C2. Verify Sequence Number fast-forward ($\text{SeqNum}_{\text{init}} = \text{SeqNum}_{\text{last}} + 1000$), DevKey vault swap, 24-hour quarantine table enforcement for surplus/relocated addresses, and hardware UUID blacklisting.

---

## 8. Traceability Matrix: Requirements to Tasks & TRD Pages

| Requirement ID | Requirement Summary | Implementation Task | Primary TRD Reference |
|---|---|---|---|
| **REQ-F01** | Virgin Node Onboarding (Native Prov Server) | Baseline System | [TRD Page 1](./trd/page1_overview.md) |
| **REQ-F02** | UVP Remote Provisioning (RPR) Tunneling | `TASK-BPROV-003`, `TASK-BPROV-004` | [TRD Page 3](./trd/page3_remote_provisioning.md) |
| **REQ-F03** | Zero-ACK 5-Minute Key Rotation Pipeline | `TASK-KROT-001` | [TRD Page 4](./trd/page4_key_rotation.md) |
| **REQ-F04** | Provisioner Redundancy (Active/Standby) | `TASK-REDUND-001` | [TRD Page 5](./trd/page5_redundancy_cascading.md) |
| **REQ-F05** | Cascading Failover ($N+1$) & Watchdog | `TASK-REDUND-001` | [TRD Page 5](./trd/page5_redundancy_cascading.md) |
| **REQ-F06** | Dynamic Infill Leasing & Paged Sparse Bitmap | `TASK-BPROV-001`, `TASK-BPROV-002` | [TRD Page 1](./trd/page1_overview.md), [TRD Page 7](./trd/page7_resilience_memory.md) |
| **REQ-F07** | Wavefront Infill Provisioning (Configurable $K$) | `TASK-BPROV-003`, `TASK-BPROV-004` | [TRD Page 8](./trd/page8_fast_bulk_provisioning.md) |
| **REQ-F08** | Multi-Master Collision & State Freshness Vector | `TASK-REDUND-001` | [TRD Page 5](./trd/page5_redundancy_cascading.md) |
| **REQ-F09** | Long-Term Eviction & DevKey Re-Healing | `TASK-HEAL-001` | [TRD Page 6](./trd/page6_eviction_healing.md) |
| **REQ-F10** | Supervised Node Replacement & Quarantine | `TASK-REPLACE-001` | [TRD Page 9](./trd/page9_node_replacement.md) |
| **REQ-NF01** | Zero-Bloat Flash & RAM Footprint Budget | All Tasks, `TASK-REV-001` | [TRD Page 1](./trd/page1_overview.md), [TRD Page 7](./trd/page7_resilience_memory.md) |
| **REQ-NF02** | Rapid Onboarding Throughput Budget | `TASK-BPROV-003`, `TASK-BPROV-004` | [TRD Page 8](./trd/page8_fast_bulk_provisioning.md) |
| **REQ-NF03** | Zero-ACK Key Rotation Low RF Overhead | `TASK-KROT-001` | [TRD Page 4](./trd/page4_key_rotation.md) |
| **REQ-NF04** | Flash NVS Wear Endurance (Debounce Layer) | `TASK-FLASH-001` | [TRD Page 7](./trd/page7_resilience_memory.md) |
| **ALL-SPECS** | Full Code vs TRD Architectural Compliance | `TASK-REV-001` | [TRD Pages 1-9](./trd/README.md) |
