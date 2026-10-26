# Requirements: Autonomous Re-Healing & Cascading Redundancy (N+1)

**Feature:** Autonomous DevKey Re-Healing & N+1 Cascading Provisioner Redundancy  
**Subsystems:** `meshx_uvp_dispatcher`, `meshx_mxcp`, `meshx_crypto`, `meshx_nvs`, `meshx_group_allocator`  
**Status:** In Progress  
**Parent Specs:** [.specs/meshx-prov-srv-mxcp/](../meshx-prov-srv-mxcp/trd/README.md)  

---

## 1. Executive Summary

When nodes remain offline across multiple Key Rotation cycles ($> 10\text{ minutes}$ / $\Delta\text{Epoch} \ge 2$), network keys are permanently purged from intermediate mesh relays, making standard managed flooding impossible. Additionally, when a Standby provisioner promotes to Master, network redundancy falls to zero unless a candidate node dynamically assumes the Standby role.

This specification governs the implementation of:
1. **Dynamic $N+1$ Standby Cascading**: Automatic claim (`0xCD`) and promotion of candidate nodes (`0xD0`), plus rebooting previous Masters into a non-colliding `ROLE_STALE`.
2. **Autonomous DevKey Self-Healing**: Offline node isolation detection, unencrypted BLE HMAC-SHA256 beacon request (`0xCE`), blind courier relaying through active mesh nodes, and Master DevKey-encrypted re-admission (`0xCF`).

---

## 2. Functional Requirements (REQ-F)

### REQ-F01: N+1 Standby Cascading & Election
- When the Standby node promotes to active Master on a 15-second heartbeat watchdog timeout:
  - It SHALL broadcast `MESHX_UVP_OP_REDUNDANCY_CLAIM` (`0xCD`) carrying the current `next_free_addr` watermark across the unified address space (`0x0002`..`0x7FFF`) with exponential backoff retries ($2\text{s}, 4\text{s}, 8\text{s}$).
  - Idle provisioner-capable nodes configured as `ROLE_CANDIDATE` SHALL respond with `MESHX_UVP_OP_REDUNDANCY_CLAIM_ACK` (`0xD0`) and assume `ROLE_STANDBY`.
  - If no candidate responds, the Master SHALL continue operating as Solo Master and emit periodic discovery beacons every 30 seconds without faulting.

### REQ-F02: Stale Master Reboot State Machine
- When a node with a provisioner image reboots:
  - It SHALL initialize in `ROLE_STALE` prior to radio activation.
  - While in `ROLE_STALE`, the node is strictly prohibited from allocating addresses from the unified address space or emitting heartbeats (`0xC7`).
  - Upon receiving an authoritative heartbeat (`0xC7`) from an incumbent Master, the stale node SHALL update its allocation high-water mark and demote cleanly to `ROLE_CANDIDATE`.

### REQ-F03: Cryptographic Blindness & Inactivity Eviction Detection
- When an already-provisioned device powers on:
  - If it sniffs ambient BLE Mesh packets for **15 seconds** and $100\%$ of packets fail MIC decryption (or detect $\Delta\text{Epoch} \ge 2$ via plaintext UVP metadata), it SHALL transition to `PROV_STATE_ISOLATED_HEALING`.

### REQ-F04: Autonomous Re-Healing Beacon Transmission
- While in `PROV_STATE_ISOLATED_HEALING`:
  - The node SHALL generate a 32-bit monotonic timestamp/nonce.
  - The node SHALL compute an HMAC-SHA256 signature truncated to 16 bytes using its immutable permanent `DevKey` over `[UUID || Old_Addr || Nonce]`.
  - The node SHALL broadcast `MESHX_UVP_OP_REHEAL_BEACON_REQ` (`0xCE`) over unencrypted BLE Advertisements with randomized jitter.

### REQ-F05: Zero-Trust Blind Courier Forwarding
- Any operational mesh node sniffing a valid `0xCE` advertisement:
  - SHALL NOT attempt to authenticate the DevKey HMAC locally.
  - SHALL encapsulate the raw payload into a standard UVP message encrypted using its **active $K_{new}$** and forward it to Master Provisioner `0x0001`.
  - SHALL apply a rate limiter: maximum 1 forward per unique UUID every 30 seconds.

### REQ-F06: Master DevKey Verification & Re-Admission
- Upon receiving a forwarded `0xCE` payload:
  - The Master SHALL look up the `UUID` in its NVS DevKey registry.
  - If found and the computed HMAC matches, the Master SHALL generate `MESHX_UVP_OP_REHEAL_ADMIT_RSP` (`0xCF`) encrypted with the target node's `DevKey` (AES-128-CCM).
  - The payload SHALL contain: active `NetKey`, active `Epoch`, and assigned `Unicast Address`.
  - The target node SHALL decrypt `0xCF`, commit credentials to NVS, and restore `PROV_STATE_PROVISIONED` without factory reset or physical intervention.

---

## 3. Non-Functional Requirements (REQ-NF)

### REQ-NF01: Zero Heavy Model Overhead
- All re-healing and cascading exchanges SHALL execute over lightweight UVP vendor frames (`0xCD..0xD0`). No SIG Client models (`CONFIG_BLE_MESH_CONFIG_CLIENT=n`) or SIG RPR models shall be introduced.

### REQ-NF02: Replay Attack Immunity
- The 32-bit nonce in `0xCE` SHALL be verified by the Master against cached past transaction timestamps for that UUID. Duplicate or decremented nonces SHALL be dropped silently.
