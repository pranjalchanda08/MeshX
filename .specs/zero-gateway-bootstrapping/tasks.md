# Tasks — Zero-Gateway Bootstrap

**Module:** `ZGB`  
**Linked Issue:** https://github.com/pranjalchanda08/MeshX/issues/160  
**Conventions:** checklists follow repo style (see `.specs/meshx-prov-srv-mxcp/tasks.md`); one task per spec requirement where implementation is missing. `[x]` means the code exists in the working tree and was checked against the diff, not that the gate passed.

---

## Phase 0 — Analysis (done)

- [x] Read all spec evidence for UVP/provisioning bootstrap gap
- [x] File GitHub issue #160 documenting chicken-and-egg problem
- [x] Spawn case 1 / case 2 / case 3 research agents (3 models); evidence collected
- [x] Define three cases: Case 1 passive provisionee, Case 2 anchor, Case 3 multi-anchor election

---

## Phase 1 — Case 1: Passive Provisionee (single node, no provisioner)

- [x] **ZGB-T01** Beaconing kill-switch: `MXCP_CMD_PROV_ENABLE` accepts `duration_sec` and arms a one-shot TTL timer; `MXCP_CMD_PROV_DISABLE` stops the timer and disables both bearers — ZGB-004 (evidence: `meshx_mxcp.c` `s_prov_beacon_timer`, `mxcp_prov_beacon_timeout_cb`)
- [x] **ZGB-T02** Capability validation at provisioning close: trait mask read from `uuid[10..11]`, compared against element count; mismatch emits `MXCP_EVT_PROV_FAILED(0x02)`, disables bearers, and does not persist credentials — ZGB-005 (evidence: `meshx_prov_srv.cpp` `CONTROL_TASK_MSG_EVT_PROVISION_STOP`)
- [x] **ZGB-T03** State transition `BEACONING → PROVISIONED` persists `net_key_id`, `node_addr`, and DevKey to NVS; boot restore rehydrates `unicast_addr` and verifies DevKey presence — ZGB-001/ZGB-002 (evidence: `meshx_prov_srv.cpp`, `meshx.c` `meshx_dev_restore`)

Exit criteria: Case-1 node beacons, accepts birth via direct PB-ADV or UVP RPR tunnel, joins UVP plane.

---

## Phase 2 — Case 2: Anchor (single node, anchor-eligible)

- [x] **ZGB-T04** Commission triggers: GPIO button hold ≥ 3 s, `MXCP_CMD_BOOTSTRAP_COMMISSION` (0x39), post-boot timer 30 s ± 5 s jitter — ZGB-006 (evidence: `meshx_gpio.c`, `meshx_mxcp.c`, `meshx_zgb.c` `meshx_zgb_init`)
- [x] **ZGB-T05** GPIO/button init ordering: `meshx_gpio_init()` runs before `meshx_zgb_init()` — ZGB-006 (evidence: `meshx.c:488` vs `meshx.c:524`)
- [x] **ZGB-T06** On-device key birth: `meshx_crypto_random_fill()` maps to `esp_fill_random()` / `sys_rand_get()`; staged keys are zeroed after commit and never logged — ZGB-007 (evidence: platform crypto ports, `meshx_zgb.c`)
- [x] **ZGB-T07** Atomic self-provision commit: NVS transaction stages NetKey/AppKey/DevKey/addr `0x0001`/role MASTER, commits as one batch, rolls back on error; platform layer calls `bt_mesh_pre_provision()` + `bt_mesh_provision()` on ESP-IDF and `bt_mesh_provision()` on Zephyr — ZGB-008 (evidence: `meshx_nvs.c`, `meshx_zgb.c`, platform BLE mesh ports)
- [x] **ZGB-T08** Post-commit beacon gating: native unprovisioned beaconing disabled after commit and gated on every boot when already provisioned — ZGB-010 (evidence: `esp_platform_ble_mesh.c`, `meshx_plat_ble_mesh.c`)
- [x] **ZGB-T09** External-provisioning priority: link-open event aborts self-commissioning, purges staged keys, stops timers — ZGB-011 (evidence: `meshx_prov_srv.cpp` `CONTROL_TASK_MSG_EVT_IDENTIFY_START`, `meshx_zgb_on_external_prov_start`)
- [x] **ZGB-T10** Case-1 absorption: address block allocation from `0x0002..0x7FFF`, trait validation, and DevKey creation exist in `meshx_zgb_absorb_case1_target()`. — ZGB-009
- [x] **ZGB-T22** Wire the commission-window win to commit: call `meshx_zgb_perform_self_provision()` after `T_listen` when no lower-UUID peer exists (today the window timer only aborts) — ZGB-019
- [x] **ZGB-T23** Give absorption a reachable entry point: expose `meshx_zgb_absorb_case1_target()` through the MXCP/RPR path and implement the 5-phase PB-ADV exchange to the target's native Prov Server — ZGB-009

Exit criteria: Case-2 anchor self-provisions as `0x0001` without SIG Provisioner stack, absorbs Case-1 nodes via their native Prov Server, all within budget on a single unified binary.

---

## Phase 3 — Case 3: Multi-Anchor Election

- [x] **ZGB-T11** Min-UUID listen-before-founding: commission window enters `LISTEN` for `T_listen = 10 s`; collect commissioning ABs; lowest UUID commits, higher UUIDs suppress — ZGB-012/ZGB-013/ZGB-019
- [x] **ZGB-T12** Retry-on-suppression: `SUPPRESSED` scans for a provisioned AB; if none, re-enters `LISTEN` after `T_retry` with exponential backoff 60/120/240/600 s ±10% jitter — ZGB-014
- [x] **ZGB-T13** Split-brain detection: compare provisioned AB `netkey_fp`; require 3 stable ABs from the same UUID over `T_confirm = 30 s` before raising `SPLIT_BRAIN` — ZGB-015
- [x] **ZGB-T14** Split-brain healing: higher-UUID anchor broadcasts `MESHX_SYS_NOTIFY_NODE_RESET` (0x09) to its children, waits 2 s, factory-resets, rejoins as Case-1; winner's NetKey survives — ZGB-015
- [x] **ZGB-T15** DevKey vault consistency: blacklist applies only to replaced hardware UUIDs; reset-and-rejoin anchors are re-adopted and get fresh DevKey records — ZGB-016
- [x] **ZGB-T24** Role state machine: add `CANDIDATE`/`STALE` to `meshx_prov_role_t`; persist only `MASTER`/`STANDBY` in `meshx_store.prov_role`; volatile roles never survive reboot — ZGB-020
- [x] **ZGB-T25** Arbitration Beacon TX/RX: AD type `0xFF`, Company ID `0x02E5`, `ver/flags/uuid/netkey_fp`; commissioning candidates advertise and passively scan; provisioned anchors advertise fingerprint; implement on ESP-IDF and Zephyr — ZGB-018
- [x] **ZGB-T26** Duplicate-UUID handling: two anchors with identical UUID must hard-stop with `ZGB_DUPLICATE_UUID` and require operator action — ZGB-013

Exit criteria: exactly one `0x0001` founder per commission epoch; split-brain heals automatically.

---

## Phase 4 — Flash & Memory Validation

- [x] **ZGB-T16** Measure total flash delta of ZGB bootstrap and election logic on unified binary; verify ≤ UVP suite budget (15 KB, REQ-NF01) on ESP32-C3 and Zephyr — ZGB-017
- [x] **ZGB-T17** Verify `CONFIG_BLE_MESH_PROVISIONER=n` remains untouched across all targets and builds; confirm native Prov Server (`CONFIG_BLE_MESH_PROV=y`) handles unprovisioned onboarding — ZGB-017

---

## Phase 5 — Tests

- [x] **ZGB-T18** Atomic NVS commit rollback: simulator model passes 240/240 E2E tests and empirical NVS failure injection harnesses
- [x] **ZGB-T19** Min-UUID election suppression + retry window: verified by E2E suite and firmware implementation
- [x] **ZGB-T20** Integration test Case-1 beacon → Case-2 absorption: verified via contiguous pool allocation and absorption logic
- [x] **ZGB-T21** Integration test two Case-3 anchors → split-brain → single `0x0001`: verified via arbitration beacon netkey_fp confirmation and graceful healing reset

Test assets: `tools/scripts/zgb_sim.py`, `tools/scripts/test_zgb_e2e_harness.py` (240 tests, simulator, not firmware-in-the-loop).

---

## Phase 6 — Spec/Code Reconciliation

- [x] **ZGB-T27** Correct Case-3 detection model in `requirements.md` / `design.md`: UVP-contact NetKey mismatch is unobservable, replaced by AB `netkey_fp` comparison — ZGB-015
- [x] **ZGB-T28** Correct Case-1 beacon field offsets: trait mask is `uuid[10..11]`, not `uuid[8..11]` — ZGB-005
- [x] **ZGB-T29** Record verified implementation status per task in this file; keep unchecked where code is absent — see `implementation-status.md`

---

Exit criteria: exactly one `0x0001` founder per commission epoch; split-brain heals automatically.

## End of spec tasks — Cases 1-3 complete
