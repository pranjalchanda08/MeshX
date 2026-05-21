# L0 Test Plan — UVP Model Refactor
## MXCP-Driven Validation

**Spec:** `uvp_model_refactor`
**Tester:** MXCP serial protocol (web console or Python MXCP client)
**Build:** `python3 tools/scripts/meshx.py -B=xiao_c3 -N=all_in_one -b`
**Target:** xiao_c3 (ESP32-C3)

---

> [!IMPORTANT]
> All tests below are driven via **MXCP** (serial command interface).
> No unit test console (`ut`) is used. Commands are issued through the
> web console or Python MXCP client.

---

## Test Environment Setup

1. Flash `meshx_build_xiao_c3.bin` to the DUT.
2. Flash `meshx_config.mxc` to the DUT. For updated Product configuration.
3. Open MXCP serial console (web console or `mxcp_client.py`).
4. Provision the DUT into a mesh network (or use loopback addresses).

---

## L0.1 — func_id Wire Prefix: End-to-End Dispatch (REQ-004)

**Objective:** Verify the dispatcher correctly strips the 2-byte `func_id` prefix and routes to the correct logical model.

### Setup
- DUT element: `RELAY_SERVER` (index 1)
- MXCP command: send raw UVP payload with `func_id=0x0000` prepended

### Steps

| Step | MXCP Command | Expected Log |
|------|-------------|--------------|
| 1 | `uvp_send el_idx=1 func_id=0x00 data=0x01` | `[ELEMENT] RelayServer [1] func_id=0x00: cmd from 0x0001` |
| 2 | `uvp_send el_idx=1 func_id=0xFF data=0x01` | `[ELEMENT] UVP Element [1]: no model matched func_id=0x00ff` |

### Pass Criteria
- Step 1: Log shows `RelayServer` receives the packet with correct `func_id`
- Step 2: Log shows "no model matched" (unknown func_id rejected)

---

## L0.2 — Relay Client: Host-to-BLE Forward (REQ-007)

**Objective:** Verify a host command with `func_id=0x00` causes the Relay Client to forward to `pub_addr`.

### Setup
- DUT element: `RELAY_CLIENT` (index 2), `pub_addr` configured to `0x0100`
- MXCP command: send ON/OFF to element 2

### Steps

| Step | MXCP Command | Expected Behavior |
|------|-------------|-------------------|
| 1 | `app_cmd el_idx=2 func_id=0x00 data=0x01` | BLE TX observed on sniffer / TXCM log shows send to `0x0100` |
| 2 | Peer node replies with ACK | App callback `relay_client_evt` with `err_code=0, on_off=1` |

### Pass Criteria
- Step 1: `send_with_func_id` log visible, destination = `pub_addr`
- Step 2: App receives `meshx_api_relay_client_evt_t` with correct state

---

## L0.3 — Relay Server: ACK + Publish + Telemetry Pipeline (REQ-006)

**Objective:** Verify the 3-step server pipeline executes in order.

### Setup
- DUT element: `RELAY_SERVER` (index 1), `pub_addr=0x0200`, `ack_req=1`
- MXCP: simulate incoming BLE packet from `0x0150` with `ack_req=1`

### Steps

| Step | MXCP Command / Trigger | Expected Log |
|------|----------------------|--------------|
| 1 | `uvp_inject el_idx=1 src=0x0150 ack=1 func_id=0x00 data=0x01` | `RelayServer [1]: sending ACK to 0x0150` |
| 2 | (automatic) | `RelayServer [1]: publishing to 0x0200` |
| 3 | (automatic) | `RelayServer [1]: sending telemetry to app` |
| 4 | App callback fires | App receives `meshx_send_msg_to_app` notification |

### Pass Criteria
- Logs show ACK → Publish → Telemetry in order
- No double-routing or missed step

---

## L0.4 — CWWW Client: Multi-func_id Routing (REQ-002)

**Objective:** Verify two independent instances route correctly: OnOff to `func_id=0x00`, CTL to `func_id=0x01`.

### Setup
- DUT element: `LIGHT_CWWW_CLIENT` (index 3)

### Steps

| Step | MXCP Command | Expected Log |
|------|-------------|--------------|
| 1 | `app_cmd el_idx=3 func_id=0x00 data=0x01` | `CWWWClient [3] func_id=0x00: forwarding` |
| 2 | `app_cmd el_idx=3 func_id=0x01 data=<ctl_payload>` | `CWWWClient [3] func_id=0x01: forwarding` |
| 3 | `app_cmd el_idx=3 func_id=0x02 data=0x00` | `UVP Element [3]: no model matched func_id=0x0002` |

### Pass Criteria
- Steps 1 and 2: correct logical model handles each func_id independently
- Step 3: unknown func_id silently dropped with warning log

---

## L0.5 — TXCM Timeout Broadcast (REQ-008)

**Objective:** Verify that a TXCM timeout calls `handle_timeout()` on ALL logical models of a client element.

### Setup
- DUT element: `LIGHT_CWWW_CLIENT` (index 3, 2 models: func_id 0x00 and 0x01)
- Induce TXCM timeout by sending to unreachable address and waiting

### Steps

| Step | Trigger | Expected Log / Callback |
|------|---------|------------------------|
| 1 | TXCM timeout fires for element 3 | `UVP Element [3]: TXCM timeout — broadcasting to 2 model(s)` |
| 2 | (automatic) | `CWWWClient [3] func_id=0x00: TXCM timeout` |
| 3 | (automatic) | `CWWWClient [3] func_id=0x01: TXCM timeout` |
| 4 | App callback | Two `meshx_api_light_cwww_client_evt_t` events with `err_code=1` |

### Pass Criteria
- Both model instances report timeout to app
- No crash, no duplicate timeout for same func_id

---

## L0.6 — Legacy Payload Fallback (Backward Compatibility)

**Objective:** Verify that payloads without `func_id` prefix (less than 2 bytes) default to `func_id=0x00` without crashing.

### Steps

| Step | MXCP Command | Expected |
|------|-------------|----------|
| 1 | `uvp_inject el_idx=1 src=0x0100 raw=<1-byte-payload>` | `func_id=0x0000` (fallback), routed to first model |

### Pass Criteria
- No crash
- Fallback to `func_id=0` routes to Relay Server model

---

## L0.7 — Sensor Server: ACK + Publish + Telemetry Pipeline (REQ-006)

**Objective:** Verify the Sensor Server model executes the standard 3-step pipeline for sensor data.

### Setup
- DUT element: `SENSOR_SERVER` (index N), `pub_addr=0x0300`, `ack_req=1`
- MXCP: simulate incoming BLE packet from `0x0150` with `ack_req=1`

### Steps

| Step | MXCP Command / Trigger | Expected Log |
|------|----------------------|--------------|
| 1 | `uvp_inject el_idx=N src=0x0150 ack=1 func_id=0x00 data=0x2A00` | `SensorServer [N] func_id=0x00: cmd from 0x0150` |
| 2 | (automatic) | `SensorServer [N]: sending ACK to 0x0150` |
| 3 | (automatic) | `SensorServer [N]: publishing to 0x0300` |
| 4 | App callback fires | App receives `meshx_send_msg_to_app` with sensor data |

### Pass Criteria
- Logs show ACK → Publish → Telemetry in order
- Sensor data payload forwarded correctly

---

## L0.8 — Sensor Client: Host-to-BLE Forward (REQ-007)

**Objective:** Verify a host command with `func_id=0x00` causes the Sensor Client to forward to `pub_addr`.

### Setup
- DUT element: `SENSOR_CLIENT` (index N), `pub_addr` configured to `0x0300`

### Steps

| Step | MXCP Command | Expected Behavior |
|------|-------------|-------------------|
| 1 | `app_cmd el_idx=N func_id=0x00 data=0x00` | TXCM log shows send to `0x0300` |
| 2 | Peer node replies with sensor data | App callback receives `meshx_api_sensor_client_evt_t` with `err_code=0, value=<sensor_data>` |

### Pass Criteria
- Step 1: `send_with_func_id` log visible, destination = `pub_addr`
- Step 2: App receives sensor client event with correct data

---

## L0.9 — RGB/HSL Server: Multi-func_id Routing (REQ-002, REQ-006)

**Objective:** Verify two independent HSL Server instances route correctly: OnOff to `func_id=0x00`, HSL to `func_id=0x01`.

### Setup
- DUT element: `LIGHT_HSL_SERVER` (index N)

### Steps

| Step | MXCP Command | Expected Log |
|------|-------------|--------------|
| 1 | `uvp_inject el_idx=N src=0x0200 ack=1 func_id=0x00 data=0x01` | `HSLServer [N] func_id=0x00: cmd from 0x0200` |
| 2 | `uvp_inject el_idx=N src=0x0200 ack=1 func_id=0x01 data=<hsl_payload>` | `HSLServer [N] func_id=0x01: cmd from 0x0200` |
| 3 | `uvp_inject el_idx=N src=0x0200 ack=1 func_id=0x02 data=0x00` | `UVP Element [N]: no model matched func_id=0x0002` |

### Pass Criteria
- Steps 1 and 2: correct logical model handles each func_id, full ACK→Pub→Telemetry pipeline
- Step 3: unknown func_id silently dropped with warning log

---

## L0.10 — RGB/HSL Client: Multi-func_id Host Forward (REQ-002, REQ-007)

**Objective:** Verify two independent HSL Client instances route correctly via host commands.

### Setup
- DUT element: `LIGHT_HSL_CLIENT` (index N), `pub_addr` configured

### Steps

| Step | MXCP Command | Expected Log |
|------|-------------|--------------|
| 1 | `app_cmd el_idx=N func_id=0x00 data=0x01` | `HSLClient [N] func_id=0x00: forwarding` |
| 2 | `app_cmd el_idx=N func_id=0x01 data=<hsl_payload>` | `HSLClient [N] func_id=0x01: forwarding` |
| 3 | `app_cmd el_idx=N func_id=0x02 data=0x00` | `UVP Element [N]: no model matched func_id=0x0002` |

### Pass Criteria
- Steps 1 and 2: correct logical model handles each func_id independently
- Step 3: unknown func_id silently dropped with warning log

---

## L0.11 — RGB/HSL Client: TXCM Timeout Broadcast (REQ-008)

**Objective:** Verify that a TXCM timeout calls `handle_timeout()` on BOTH HSL client model instances.

### Setup
- DUT element: `LIGHT_HSL_CLIENT` (index N, 2 models: func_id 0x00 and 0x01)
- Induce TXCM timeout by sending to unreachable address and waiting

### Steps

| Step | Trigger | Expected Log / Callback |
|------|---------|------------------------|
| 1 | TXCM timeout fires for element N | `UVP Element [N]: TXCM timeout — broadcasting to 2 model(s)` |
| 2 | (automatic) | `HSLClient [N] func_id=0x00: TXCM timeout` |
| 3 | (automatic) | `HSLClient [N] func_id=0x01: TXCM timeout` |
| 4 | App callback | Two `meshx_api_light_hsl_client_evt_t` events with `err_code=1` |

### Pass Criteria
- Both model instances report timeout to app
- No crash, no duplicate timeout for same func_id

---

## L0.12 — Sensor Client: TXCM Timeout (REQ-008)

**Objective:** Verify Sensor Client timeout reports err_code=1 to app.

### Setup
- DUT element: `SENSOR_CLIENT` (index N, 1 model: func_id 0x00)
- Induce TXCM timeout

### Steps

| Step | Trigger | Expected Log / Callback |
|------|---------|------------------------|
| 1 | TXCM timeout fires for element N | `SensorClient [N] func_id=0x00: TXCM timeout` |
| 2 | App callback | `meshx_api_sensor_client_evt_t` with `err_code=1` |

### Pass Criteria
- Timeout reported to app with correct func_id
- No crash

---

## L0 Result Tracker

| Test | Status | Notes |
|------|--------|-------|
| L0.1 Wire dispatch | Pending | |
| L0.2 Relay Client host-forward | Pending | |
| L0.3 Server ACK+Pub+Telemetry | Pending | |
| L0.4 CWWW multi-func_id routing | Pending | |
| L0.5 TXCM timeout broadcast | Pending | |
| L0.6 Legacy payload fallback | Pending | |
| L0.7 Sensor Server pipeline | Pending | |
| L0.8 Sensor Client host-forward | Pending | |
| L0.9 RGB/HSL Server multi-func_id | Pending | |
| L0.10 RGB/HSL Client multi-func_id | Pending | |
| L0.11 RGB/HSL Client TXCM timeout | Pending | |
| L0.12 Sensor Client TXCM timeout | Pending | |
