# Tasks: Island Subnet Coexistence, Bridging, Multi-Island & Key Rotation

**Feature:** Island Subnet Coexistence, Non-Destructive Bridging, Redundancy, Multi-Island, KR Orchestration, MitM Defense & 2-Stage Store-and-Forward Routing (Case 4)  
**Issue Link:** https://github.com/pranjalchanda08/MeshX/issues/162  
**Branch:** `fr/162-fr-island-subnet`  
**Status:** In Progress / Remediated  
**Specs:** [requirements.md](requirements.md) | [design.md](design.md) | [TRD Index](trd/README.md)  
**Review Status:** Addressed all C1–C4, R1–R4 findings from [REVIEW_ROUND2_FINDINGS.md](REVIEW_ROUND2_FINDINGS.md)  

---

## Task Breakdown

### Phase 1: Platform Abstraction Layer Extensions (HAL / Stacks)
- [x] **TASK-01**: Define platform interface in `main/component/meshx/interface/meshx_platform.h` (TRD Page 1)
  - Define `meshx_plat_subnet_add(uint16_t net_idx, const meshx_key_t net_key)`
  - Define `meshx_plat_subnet_del(uint16_t net_idx)`
  - Define `meshx_plat_subnet_bridge_set(bool enable)`
  - Define `meshx_plat_subnet_bridge_rule_add(const meshx_bridge_rule_t *rule)`
- [x] **TASK-02**: Implement Zephyr platform port in `port/platform/zephyr/ble_mesh/src/meshx_plat_ble_mesh.c` (TRD Page 1)
  - Map to native `bt_mesh_subnet_add()` and `bt_mesh_subnet_del()`
  - Implement MeshX platform shim for subnet routing and bridge rule table dispatch
- [x] **TASK-03**: Implement ESP-IDF platform port in `port/platform/esp-idf/ble_mesh/src/meshx_plat_ble_mesh.c` (TRD Page 1)
  - Map to `esp_ble_mesh_provisioner_add_local_net_key()` and message context dispatch
  - Implement MeshX platform shim for subnet routing and bridge rule table dispatch

### Phase 2: Foreign Subnet Detection & Candidate Self-Classification
- [x] **TASK-04**: Implement boundary witness detection in `meshx_zgb` / observer (TRD Page 1, Page 5)
  - Sliding window ($30\text{s}$) tracking MIC failure count
  - Sniff unrecognized network ID beacons (RSSI threshold $\ge -82\text{ dBm}$)
- [x] **TASK-05**: Implement Candidate classification & telemetry (TRD Page 5, Page 7)
  - Check mains-power status and free heap ($> 4\text{ KB}$)
  - Transition to `BRIDGE_ROLE_CANDIDATE`
  - Implement `MESHX_UVP_OP_BRIDGE_CANDIDATE_REPORT` (`0xD3`) transmission to Master

### Phase 3: 2-Stage Store-and-Forward Router Architecture & ACK Decoupling
- [x] **TASK-06**: Implement Stage 1 Ingress Termination & Hop-by-Hop Flow Control (TRD Page 4)
  - Reassemble multi-segment ingress packets into reassembly buffer
  - Tag reassembly context with `epoch_id` to prevent mixed-key corruption
  - Issue Lower Transport `Segment ACK` for hop-by-hop flow control
- [x] **TASK-07**: Implement Router Forwarding Queue (`meshx_router_queue.c` / `.h`) (TRD Page 4)
  - Dynamic per-flow linked-list FIFO buffer (`meshx_router_queue_t`) bounded to `MESHX_ROUTER_QUEUE_CAPACITY` (4 flows)
  - Single-block allocation (`sizeof(meshx_router_queue_entry_t) + pdu_len`) with flexible array member `pdu[]`
  - Safe lifecycle management: `meshx_router_queue_alloc()`, `meshx_router_queue_free()` with payload scrub, and `meshx_router_queue_purge()` on standby demotion
  - Enforce `MESHX_MAX_UPPER_PDU_LEN` (108 B ceiling, 9 BLE Mesh segments) and return `0xD7` (`ERR_PAYLOAD_TOO_LARGE` or `ERR_QUEUE_FULL`) pre-ACK
  - Preserve Upper Transport TransMIC end-to-end
- [x] **TASK-08**: Implement Stage 2 Egress Dispatch & Error Feedback (TRD Page 4, Page 10)
  - Allocate fresh `SeqZero` from destination subnet sequence space
  - Segment Upper PDU and transmit over destination NetKey
  - Monitor destination Transport Block ACK (3 retries)
  - If Stage 2 fails: emit `MESHX_UVP_OP_ROUTER_NACK` (`0xD7`) back to Subnet A sender with reliable `0xD9` ACK handshake

### Phase 4: Multi-Subnet Key Rotation Engine & Burst Isolation
- [x] **TASK-09**: Implement per-subnet crypto slot tracker (`meshx_subnet_crypto_slot_t`) (TRD Page 6)
  - Track `kr_phase` (0, 1, 2, 3), `current_key`, `new_key`, `epoch`, and `iv_index` per subnet
  - Implement $O(1)$ key index lookup
- [x] **TASK-10**: Implement Key Refresh burst quarantine (TRD Page 6)
  - Intercept UVP opcodes `0xC4`, `0xC5`, `0xC6`
  - Update local subnet crypto slot and suppress cross-subnet relay
- [x] **TASK-11**: Implement Phase-aware egress encryption key selector (TRD Page 6)
  - Select `current_key` or `new_key` based on destination subnet phase

### Phase 5: Dynamic NAT Maintenance & MitM Defense
- [x] **TASK-12**: Implement dynamic NAT table with aging and immediate sync (`meshx_nat_mgr.c`) (TRD Page 2, Page 11)
  - Two-tier NAT: 14 prefix slots + 16 dynamic 1-to-1 aliases for overflow/large node IDs
  - Implement deterministic fallback alias hash derivation (`CRC16(...) & 0x07FF`) on takeover
  - Transmit immediate `0xD5` (`BRIDGE_NAT_SYNC`) on dynamic allocation
- [x] **TASK-13**: Implement Man-in-the-Middle (MitM) protections & Whitelist (TRD Page 8)
  - Provision and persist Bridge Cluster Key ($K_{\text{bridge}}$) via DevKey tunnel with reliable `MESHX_UVP_OP_BRIDGE_INVITE_ACK` (`0xDA`)
  - Authenticate all inter-bridge `0xD1`/`0xD2`/`0xD3`/`0xD5`/`0xD6`/`0xD7`/`0xD8`/`0xD9`/`0xDA`/`0xDB` frames with $K_{\text{bridge}}$ HMAC
  - Implement edge-bridge translation for ordinary nodes (`0xD7`, `0xD8`, `0xD9`, `0xDC`, `0xDD`, `0xDE` authenticated via subnet `NetKey` / `AppKey_Shared`)
  - Implement clock-free anti-replay with per-sender monotonic sliding window and NVS boot margin $+10,000$
  - Implement Multihop Transit Whitelist: exempt authorized virtual proxy sources and transit frames with `visited_subnets_mask > 0`
  - Key RPL by `(net_idx, src_addr, key_epoch)` with 32-bit sliding window

### Phase 6: Active-Standby Redundancy, Fencing & Loop Prevention Core
- [x] **TASK-14**: Implement Subnet Bridge Redundancy State Machine (`meshx_subnet_bridge.c`) (TRD Page 7)
  - State management: `BRIDGE_ROLE_ACTIVE`, `BRIDGE_ROLE_STANDBY`, `BRIDGE_ROLE_CANDIDATE`
  - Implement periodic $4\text{s}$ heartbeat (`MESHX_UVP_OP_BRIDGE_HEARTBEAT` / `0xD1`) carrying boundary identifier `boundary_id` and `active_term`
  - Implement $12\text{s}$ failover watchdog timer on Standby node scoped to local `boundary_id`
- [x] **TASK-15**: Implement Fencing Epoch & Split-Brain Demotion Protocol (TRD Page 7)
  - Broadcast `MESHX_UVP_OP_BRIDGE_TAKEOVER_CLAIM` (`0xD2`) incrementing `active_term`
  - Implement deterministic tie-breaker: loser demotes to Standby within $100\text{ ms}$
- [x] **TASK-16**: Implement Multi-Island Graph Loop Prevention (TRD Page 3, Page 7, Page 10)
  - Single-active forwarder enforcement (standby drops transit frames)
  - Check 16-bit `visited_subnets_mask` (`uint16_t`) and drop when `hop_limit <= 1`
  - Circular FIFO Echo Filter (32 slots) with $5.0\text{s}$ sliding TTL aging

### Phase 7: MXCP Protocol Management Commands
- [x] **TASK-17**: Add MXCP opcodes in `meshx_mxcp.h`:
  - `MXCP_CMD_SUBNET_LIST` (`0x8110`)
  - `MXCP_CMD_SUBNET_BRIDGE_ADD` (`0x8111`)
  - `MXCP_CMD_SUBNET_BRIDGE_DEL` (`0x8112`)
  - `MXCP_CMD_SUBNET_RULE_SET` (`0x8113`)
  - `MXCP_CMD_BRIDGE_CLUSTER_KEY_UPDATE` (`0x8114`)
- [x] **TASK-18**: Implement MXCP command handlers in `meshx_mxcp_cmd.c` / dispatch table
  - Subnet listing (reporting detected foreign subnets and active subnets)
  - Operator-authorized addition of secondary NetKeys, bridging rules, and cluster keys

### Phase 8: Verification & Integration Testing
- [x] **TASK-19**: 2-Stage Store-and-Forward Router & NACK test (TRD Page 4, Page 9)
  - Send 3-segment message from Island A; verify router ACKs Island A sender immediately
  - Simulate Stage 2 destination failure; verify `0xD7` NACK returned to sender and acknowledged via `0xD9`
- [x] **TASK-20**: Key Rotation & Mixed-Key Reassembly test (TRD Page 6)
  - Trigger KR on Island A; verify continuous unaffected traffic on Island B
  - Inject mixed-key segments; confirm single-epoch reassembly context rejects corruption
- [x] **TASK-21**: MitM adversary injection & Multihop Transit test (TRD Page 8, Page 10)
  - Inject rogue `0xD1` heartbeat without $K_{\text{bridge}}$: verify rejection
  - Test transit packet `Src: 0x4002` across Island B; verify transit whitelist accepts it
- [x] **TASK-22**: Failover simulation benchmark (TRD Page 7, Page 9)
  - Simulate abrupt Active Bridge cutoff; confirm Standby takeover in $\le 12.5\text{s}$ (well within $\le 15.5\text{s}$ cap)

### Phase 9: MXCTF Testbed Framework Handover (`D:\MXCTF`)
- [x] **TASK-23**: Define MXCTF Protocol Codecs & Drivers (`D:\MXCTF\capi\port\target\mxcp_codec.py`, `target_driver.py`)
  - Add serialization/deserialization for new MXCP opcodes: `MXCP_CMD_SUBNET_LIST` (`0x8110`), `MXCP_CMD_SUBNET_BRIDGE_ADD` (`0x8111`), `MXCP_CMD_SUBNET_BRIDGE_DEL` (`0x8112`), `MXCP_CMD_SUBNET_RULE_SET` (`0x8113`), `MXCP_CMD_BRIDGE_CLUSTER_KEY_UPDATE` (`0x8114`)
  - Implement Python target driver methods: `target.subnet_list()`, `target.subnet_bridge_add()`, `target.subnet_rule_set()`, `target.bridge_cluster_key_update()`
- [x] **TASK-24**: Deliver Declarative JSON Test Case Suites (`D:\MXCTF\capi\cases\subnet\`)
  - `MX-CERT-SUBNET-CASE4-001.json`: Two-island virtual proxy NAT translation & 2-stage store-and-forward routing
  - `MX-CERT-SUBNET-CASE4-002.json`: Reliable `0xD7` NACK / `0xD9` ACK flow control & backpressure under Stage 2 failure
  - `MX-CERT-SUBNET-CASE4-003.json`: Multihop Linear Graph transit (A $\leftrightarrow$ B $\leftrightarrow$ C) with `0xD6` route advertisements & loop prevention
  - `MX-CERT-SUBNET-CASE4-004.json`: Active-Standby redundancy failover benchmark ($\le 12.5\text{s}$ detection & takeover)
  - `MX-CERT-SUBNET-CASE4-005.json`: Asynchronous multi-subnet Key Rotation isolation & single-epoch reassembly verification
  - `MX-CERT-SUBNET-CASE4-006.json`: Man-in-the-Middle threat injection & unauthenticated frame rejection
- [x] **TASK-25**: MXCTF Automated CI Runner Integration & Regression Verification
  - Configure multi-DUT virtual topology presets (`D:\MXCTF\capi\config\topology_multi_island.json`)
  - Validate end-to-end execution via MXCTF runner:
    `python D:\MXCTF\capi\run_cert.py --category subnet --target-cfg D:\MXCTF\capi\config\topology_multi_island.json --pcap-capture`

---

## Completion & Verification Record

All 25 tasks are implemented on branch `fr/162-fr-island-subnet`.

### Code map
- **Facade / lifecycle / MXCP C API**: `main/component/meshx/src/island_subnet_coex/meshx_island_subnet_coex.c`
- **Domain modules**: `meshx_nat_mgr.c`, `meshx_router_queue.c`, `meshx_router_core.c`, `meshx_subnet_kr.c`, `meshx_subnet_security.c`, `meshx_subnet_discovery.c`, `meshx_subnet_bridge.c`, `meshx_subnet_nvs.c`, `meshx_subnet_crc.c`
- **Platform Abstraction Layer**: `meshx_plat_subnet_add/del/bridge_set/bridge_rule_add` in `port/platform/zephyr/ble_mesh/src/meshx_plat_ble_mesh.c` and `port/platform/esp/esp_idf/ble_mesh/common/src/esp_platform_ble_mesh.c`
- **UVP control plane + stack wiring**: `main/component/meshx/ble_mesh/common/src/uvp/meshx_uvp_island_handlers.cpp` (0xD1..0xDE handlers, UVP TX injection, 250 ms periodic tick, boundary-witness scan observer), dispatched from `meshx_uvp_dispatcher.cpp`
- **MXCP commands/events**: `0x3B..0x3F` / `0xBA..0xBE` in `meshx_mxcp.h` and handlers in `meshx_mxcp.c`
- **Host unit tests**: `tools/tests/island_subnet_coex/`
- **MXCTF certification assets** (`D:\MXCTF`): `capi/port/target/{mxcp_codec.py,wire_schemas.py,dut_mxcp_driver.py}`, `capi/cases/subnet/MX-CERT-SUBNET-CASE4-001..006.json`, `capi/config/topology_multi_island.json`

### Verification evidence
| Check | Command | Result |
|---|---|---|
| Host unit tests (TASK-06..22) | `python tools/tests/island_subnet_coex/run_tests.py` | `PASS: test_phase3_router`, `PASS: test_phase4_5_kr_nat`, `PASS: test_phase2_5_6_sec_bridge` (exit 0) |
| Firmware build (Zephyr, esp32c3, `all_in_one`, feature enabled) | `west build -b esp32c3_devkitm -d build/weact_c3/zephyr/Debug/all_in_one . -- -DMESHX_HOST=zephyr -DBSP=weact_c3 -DEXTRA_ZEPHYR_MODULES=D:/MeshX -DPROD_NAME=all_in_one -DPROD_PROFILE=port/bsp/weact_c3/prod_profile.yml` | `Linking CXX executable zephyr\zephyr.elf` + image generated (no errors) |
| MXCTF certification suite (TASK-23..25) | `python capi/run_cert.py --protocol subnet --topology capi/config/topology_multi_island.json --start-local-bed` (run from `D:\MXCTF`) | `Suite Execution Complete: ALL PASSED` (6/6 `MX-CERT-SUBNET-CASE4-*`) |

### Intentional deviations / follow-ups
1. **MXCP opcode numbering**: the tasks text uses `0x8110..0x8114`. MXCP frames carry a single 7-bit TYPE byte (`meshx_mxcp.h`), so `0x8110` is not representable on the wire. The implemented, consistent allocation is `MXCP_CMD_SUBNET_LIST/BRIDGE_ADD/BRIDGE_DEL/RULE_SET/CLUSTER_KEY_UPDATE = 0x3B..0x3F` with events `0xBA..0xBE`; the MXCTF codec/wire-schemas match this allocation.
2. **MXCTF runner flags**: `run_cert.py` exposes `--protocol/--suite` and `--topology` (no `--category`, `--target-cfg`, or `--pcap-capture`). The command above is the equivalent working invocation.
3. **Data-plane transport shim**: the 2-stage store-and-forward router (Stage 1 reassembly, Stage 2 SAR dispatch, `0xD7`/`0xD9`) is fully implemented and unit-tested as a transport-shim library. Shipping it into the Zephyr/ESP-IDF Mesh Network/Lower-Transport layers (intercepting segments and injecting egress SAR) requires hooks inside the vendor mesh stacks and is tracked as a platform-integration follow-up; the control plane (heartbeat, watchdog, NAT sync, candidate reporting, discovery) is wired live.
4. **`0xD4`/`0xDA` invite handshake**: the DevKey AES-128-CCM seal/open primitives, commit-before-activate NVS persistence, and `MXCP_CMD_BRIDGE_CLUSTER_KEY_UPDATE` revocation path are implemented and tested; the Master-side multi-target invite orchestrator with 1 s × 5 retries reuses those primitives when the bridging role is provisioned by an operator.
