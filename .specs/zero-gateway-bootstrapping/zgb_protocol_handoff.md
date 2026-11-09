# Protocol & Testing Handoff: Zero-Gateway Bootstrapping (ZGB)

**Target Framework:** MeshX Certification Test Framework (**MXCTF**) (`D:\MXCTF`)
**Firmware Module:** `meshx_zgb`, `meshx_prov_srv`, `meshx_mxcp`, `meshx_nvs`, `meshx_group_allocator`
**Parent Specifications:** `.specs/zero-gateway-bootstrapping/`, `.specs/meshx-prov-srv-mxcp/`
**Firmware State:** Phase 1 (Case 1), Phase 2 (Case 2), and Phase 3 (Case 3) fully implemented and compiled cleanly under single-binary constraint (`CONFIG_BLE_MESH_PROVISIONER=n`, `CONFIG_BLE_MESH_PROV=y`).

---

## 1. Protocol Architecture & Wire Handshake Specifications

### 1.1 MXCP Serial Command Additions

The host interface adds support for manual commissioning overrides and window aborts:

| Opcode | Command Name | Payload Format | Description |
|---|---|---|---|
| `0x0B` | `MXCP_CMD_PROV_ENABLE` | `uint8_t bearer_mask`, `uint16_t duration_sec` | Arms one-shot TTL beacon timer (`s_prov_beacon_timer`) for unprovisioned passive nodes. |
| `0x0C` | `MXCP_CMD_PROV_DISABLE` | (empty) | Immediate kill-switch. Clears TTL timer, closes links, transitions `BEACONING -> UNPROVISIONED`. |
| `0x0D` | `MXCP_CMD_PROV_GET_STATE` | (empty) | Returns current provisioning state code and assigned unicast address. |
| `0x39` | `MXCP_CMD_BOOTSTRAP_COMMISSION` | (empty) | Forces node into anchor commissioning mode (`COMMISSIONING`, 0x04) and opens 60s window (with 10s listen phase). |
| `0x3A` | `MXCP_CMD_BOOTSTRAP_ABORT` | (empty) | Aborts active commissioning window, purges staged keys from RAM, reverts to `UNPROVISIONED`. |

### 1.2 MXCP Event Notifications

| Event Opcode | Event Name | Payload Format | Description |
|---|---|---|---|
| `0x01` | `MXCP_EVT_PROV_COMP` | `uint16_t addr` | Emitted when self-provisioning commits (`0x0001`) or external provisioning finishes. |
| `0x02` | `MXCP_EVT_PROV_FAILED` | `uint8_t reason (0x02)` | Emitted when capability validation fails (trait mask bit 5 `0x0020` vs element count mismatch). |
| `0x8C` | `MXCP_EVT_PROV_STATE_IND` | `uint16_t unicast_addr`, `uint16_t net_idx`, `uint8_t state` | Broadcasts state transitions: `0x00` (UNPROV), `0x01` (BEACONING), `0x02` (PROVISIONING), `0x03` (PROVISIONED), `0x04` (COMMISSIONING), `0x05` (SUPPRESSED). |

### 1.3 ZGB Arbitration Beacon (AB) — Wire Format

Used for Case-3 pre-commit election and post-commit island/split-brain detection:
- **AD Structure Type:** `0xFF` (Manufacturer Specific Data)
- **Company ID:** `0x02E5` (MeshX)
- **Length:** 24 bytes (2 bytes CID + 22 bytes payload)

```
+---------------+---------------+---------------+-------------------------------+-----------------------+
| CID (2 Bytes) |  Ver (1 Byte) | Flags (1 Byte)|       UUID (16 Bytes)         |  NetKey FP (4 Bytes)  |
| 0xE5, 0x02    |     0x01      |  Bitmap Flags |  Hardware/Derived Device UUID | SHA-256(NetKey)[0..3] |
+---------------+---------------+---------------+-------------------------------+-----------------------+
```

**Flags Bitmask:**
- `0x01`: `ANCHOR_ELIGIBLE`
- `0x02`: `COMMISSIONING` (Active during 10s `LISTEN` / 60s window)
- `0x04`: `PROVISIONED` (Set by provisioned anchor with valid `NetKey FP`)

---

## 2. MXCTF Test Case Suite Definitions

The following test definitions map directly to `D:\MXCTF\capi\cases\prov\`:

### 2.1 `MX-CERT-ZGB-CASE1-001.json`: Passive Provisionee & Capability Integrity
- **Objective:** Verify boot beacon gating, TTL kill-switch, and capability integrity rejection.
- **Steps:**
  1. Boot DUT; verify DUT does NOT beacon until `MXCP_CMD_PROV_ENABLE`.
  2. Send `MXCP_CMD_PROV_ENABLE` with `duration_sec = 5`.
  3. Verify unprovisioned beacon observed on sniffer (filtered by DUT UUID prefix `02E5`).
  4. Wait 6s; verify beaconing auto-terminates via TTL timer (`MXCP_EVT_PROV_STATE_IND(state=0x00)`).
  5. Open provisioning link with corrupted element count (mismatch with trait mask in UUID bytes 10..11).
  6. Verify DUT rejects link close with `MXCP_EVT_PROV_FAILED(reason=0x02)`, cleanses state, and does NOT commit to NVS.

### 2.2 `MX-CERT-ZGB-CASE2-001.json`: Single Anchor Key Birth & Self-Provisioning
- **Objective:** Verify atomic self-provisioning via physical trigger (GPIO9 hold $\ge 3\text{s}$) or `0x39`.
- **Steps:**
  1. Boot DUT in unprovisioned state.
  2. Issue `MXCP_CMD_BOOTSTRAP_COMMISSION` (`0x39`).
  3. Verify DUT emits `MXCP_EVT_PROV_STATE_IND(state=0x04)`.
  4. Sniffer verifies Arbitration Beacon transmitted (`ver=0x01, flags=0x03`).
  5. Wait 10s (`T_listen`); verify DUT performs atomic self-provisioning:
     - Assigned address: `0x0001`
     - Role: `MASTER` (1)
     - `MXCP_EVT_PROV_COMP(addr=0x0001)` emitted.
  6. Reboot DUT; verify node restores as `0x0001`, role `MASTER`, and native unprovisioned beaconing is permanently gated.

### 2.3 `MX-CERT-ZGB-CASE2-002.json`: External Provisioning Priority Guard
- **Objective:** Confirm external provisioner (SIG app/gateway) preempts self-commissioning.
- **Steps:**
  1. DUT opens commission window via button hold or `0x39`.
  2. External provisioner initiates PB-ADV link open at $t = 3\text{s}$.
  3. Verify DUT aborts self-commissioning immediately, discards staged HWRNG keys, stops `T_listen`, and accepts external credentials.
  4. Verify DUT commits external unicast address (e.g. `0x0005`), role `STANDALONE`, and does NOT claim `0x0001`.

### 2.4 `MX-CERT-ZGB-CASE3-001.json`: Multi-Anchor min-UUID Election
- **Objective:** Verify that when two anchors commission simultaneously, lowest UUID wins and higher UUID suppresses.
- **Topology:** 2 Anchor DUTs (`DUT_0` with lower UUID, `DUT_1` with higher UUID).
- **Steps:**
  1. Trigger `0x39` simultaneously on `DUT_0` and `DUT_1`.
  2. Both broadcast commissioning Arbitration Beacons during 10s listen phase.
  3. `DUT_1` observes lower UUID from `DUT_0` -> transitions to `SUPPRESSED` (`0x05`), stops AB transmission, and arms retry backoff timer (60s).
  4. `DUT_0` completes `T_listen = 10s`, wins election, commits as Master `0x0001`.
  5. `DUT_1` detects provisioned AB from `DUT_0` -> cancels retries, reverts to Case-1 passive mode, and awaits absorption.

### 2.5 `MX-CERT-ZGB-CASE3-002.json`: Dual-Anchor Split-Brain Detection & Healing
- **Objective:** Verify that two anchors provisioned independently heal when brought into radio contact.
- **Topology:** 2 Provisioned Anchors (`DUT_A` addr `0x0001`, `DUT_B` addr `0x0001`, `UUID_A < UUID_B`).
- **Steps:**
  1. `DUT_A` and `DUT_B` advertise provisioned AB with their respective 4-byte `netkey_fp`.
  2. Bring nodes into radio contact.
  3. Verify nodes detect fingerprint mismatch.
  4. After 3 stable consecutive ABs (within `T_confirm = 30s`):
     - `DUT_A` (lower UUID): Logs win, maintains `0x0001` and NetKey.
     - `DUT_B` (higher UUID): Emits pre-reset teardown broadcast `0x09` to its children, performs factory reset, wipes NVS, and restarts as unprovisioned Case-1 node.
  5. `DUT_A` absorbs `DUT_B` into unified pool `0x0002..0x7FFF`. Exactly one `0x0001` remains.

---

## 3. MXCTF Driver Update Requirements

To enable target execution from `D:\MXCTF`, add the following definitions to `D:\MXCTF\capi\port\target\mxcp_codec.py`:

```python
# ZGB Bootstrap Command Codes
MXCP_CMD_BOOTSTRAP_COMMISSION = 0x39
MXCP_CMD_BOOTSTRAP_ABORT      = 0x3A

# ZGB State Codes
PROV_STATE_UNPROVISIONED = 0x00
PROV_STATE_BEACONING     = 0x01
PROV_STATE_PROVISIONING  = 0x02
PROV_STATE_PROVISIONED   = 0x03
PROV_STATE_COMMISSIONING = 0x04
PROV_STATE_SUPPRESSED    = 0x05
PROV_STATE_SPLIT_BRAIN   = 0x06
PROV_STATE_RESETTING     = 0x07
```

In `D:\MXCTF\capi\port\target\target_driver.py`, add driver wrappers:
```python
def bootstrap_commission(self) -> Dict[str, Any]:
    return self.send_command(MXCP_CMD_BOOTSTRAP_COMMISSION, b"")

def bootstrap_abort(self) -> Dict[str, Any]:
    return self.send_command(MXCP_CMD_BOOTSTRAP_ABORT, b"")
```

---

## 4. Execution Commands

From `D:\MXCTF`:
```cmd
# Run all ZGB certification test cases
python D:\MXCTF\capi\run_cert.py --category prov --log-level INFO --pcap-capture

# Run specific Case 3 Multi-Anchor Election test
python D:\MXCTF\capi\run_cert.py --case MX-CERT-ZGB-CASE3-001 --target-cfg D:\MXCTF\capi\config\dual_anchor_c3.json
```
