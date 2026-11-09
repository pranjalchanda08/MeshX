# Requirements — Zero-Gateway Bootstrap (First-Node & Multi-Anchor Election)

**Module:** `ZGB`  
**Parent Document:** GitHub issue https://github.com/pranjalchanda08/MeshX/issues/160  
**Status:** Draft  
**Target Subsystems:** `meshx_prov_srv`, `meshx_uvp_dispatcher`, `meshx_mxcp`, `meshx_nvs`, `meshx_group_allocator`

---

## 1. Problem Statement

MeshX drives all advanced commissioning (Remote Provisioning tunneling `0xC0..0xC3`, Wavefront leases `0xC8/0xCC/0xC9`, Key Rotation `0xC4..0xC6`) over UVP, a standard BT Mesh vendor model (`0x0001:0x02E5`) encrypted with NetKey/AppKey. Every MeshX build intentionally keeps the native SIG Provisioner stack disabled (`CONFIG_BLE_MESH_PROVISIONER=n`) to avoid 120–150 KB of flash bloat. Instead, every device ships with the mandatory **Native Provisioning Server (`CONFIG_BLE_MESH_PROV=y`)**.

Rather than letting the compiled Native Prov Server become dead code after boot, MeshX utilizes it directly for unprovisioned onboarding while UVP orchestrates network formation and address allocation. Nothing in the spec suite previously defined how the **first** device self-initializes onto an empty mesh and becomes Master Provisioner `0x0001` without an external gateway, or how multiple candidate devices coordinate to elect a single anchor.

This spec defines three birth cases on a single unified firmware binary (`CONFIG_BLE_MESH_PROVISIONER=n`, `CONFIG_BLE_MESH_PROV=y`):

| Case | Topology | What the spec owns |
|------|----------|--------------------|
| **Case 1** | One node, standard passive role | Passive provisionee: uses mandatory native Prov Server, beacons PB-ADV/PB-GATT, waits for provisioning |
| **Case 2** | One node, anchor-eligible (trigger active) | Anchor: on-device key birth, claims `0x0001`, transitions to Master via local node provisioning APIs, absorbs Case-1 nodes using standard Prov Server handshakes driven over UVP |
| **Case 3** | Multiple anchor-eligible nodes | Anchor election: `min(UUID)` listen-before-founding, retry-on-suppression, split-brain healing |

Cases 2 and 3 reuse **existing** UVP opcodes and stack local provisioning APIs — **no new UVP opcode is introduced**, and `CONFIG_BLE_MESH_PROVISIONER` remains disabled across all builds.

---

## 2. Requirements

| ID | Title | Description | Acceptance Criteria | Priority |
|----|-------|-------------|---------------------|----------|
| ZGB-001 | Case 1: passive provisionee beaconing | An unprovisioned node without provisioner capability shall start PB-ADV/PB-GATT beaconing on boot and carry REQ-003 capability/trait flags. | `MXCP_PROV_STATE_UNPROVISIONED → BEACONING` transition occurs on fresh boot; beacon includes company ID + product trait mask. | P0 |
| ZGB-002 | Case 1: no self-provisioning | A Case-1 node shall not generate NetKey/AppKey or assign itself an address. It waits for an external provisioner. | Node remains `UNPROVISIONED` (addr `MESHX_ADDR_UNASSIGNED`) until a provisioning link closes with assigned addr. | P0 |
| ZGB-003 | Case 1: birth channels | A Case-1 node shall accept provisioning via (a) direct PB-ADV/PB-GATT from a SIG app/gateway, or (b) UVP RPR tunnel `0xC0..0xC3` relayed by an intermediate provisioned node. | Provisioning completes in each channel; node then joins UVP management plane. | P0 |
| ZGB-004 | Case 1: beaconing has a kill-switch | Beaconing without an arriving provisioner shall not persist indefinitely. | `MXCP_CMD_PROV_DISABLE` or an operator-set beaconing TTL ends `BEACONING` and returns state to `UNPROVISIONED`. OOB watchdog remains link-scoped (30s) only after a link opens. | P1 |
| ZGB-005 | Case 1: capability integrity | Unprovisioned beacon trait/role data shall not be silently trusted for provisioning decisions without validation. | Provisioner validates trait mask against actual element count at link close; mismatch aborts with `PROVISIONING_FAILED`. | P1 |
| ZGB-006 | Case 2: commission triggers | An anchor-eligible node shall enter commission mode via (a) GPIO button hold ≥ 3s, (b) `MXCP_CMD_BOOTSTRAP_COMMISSION`, or (c) post-boot timer 30s ± jitter when unprovisioned. | Each trigger independently starts commission mode; timer jitter prevents simultaneous commits among co-located anchors. | P0 |
| ZGB-007 | Case 2: on-device key birth | Anchor shall generate NetKey + AppKey + self-DevKey on-device from HWRNG (seeded from eFuse + radio entropy). Keys never leave the device over air or serial. | `meshx_get_device_uuid()` stable; NetKey/AppKey/DevKey material present only in NVS, never in UART/log output. | P0 |
| ZGB-008 | Case 2: atomic self-provision | Anchor commits NetKey, AppKey, self-DevKey, addr `0x0001`, role MASTER in a single atomic NVS commit and initializes the local mesh node without SIG Provisioner stack; on any failure the whole birth rolls back. | Post-commit: `is_provisioned() == true`, addr `0x0001`, role `MASTER`. Partial commit (DevKey with no address, or address with no key) is impossible. | P0 |
| ZGB-009 | Case 2: Case-1 absorption via Prov Server | Anchor shall absorb Case-1 nodes by driving standard PB-ADV PDUs to the target's native Prov Server (direct in-range or tunneled via UVP RPR `0xC0..0xC3` / Wavefront leases `0xC8/0xCC`). | Target native Prov Server receives standard Invite/PubKey/Data and completes onboarding; address block allocated from unified pool `0x0002..0x7FFF`; `0x0001` remains reserved for the anchor. Zero new UVP opcodes used. | P0 |
| ZGB-010 | Case 2: post-commit beacon gating | After successful self-provisioning, native unprovisioned beaconing is permanently gated; anchor no longer advertises as provisionable. | `MXCP_PROV_STATE_PROVISIONED` persists across reboots; beaconing does not re-enable on boot. | P0 |
| ZGB-011 | Case 2: external provisioning priority | If a Case-2 anchor candidate is externally provisioned (SIG app PB-ADV/PB-GATT) before or during commission mode, commissioning is aborted and the adopted credentials are kept. | `meshx_is_node_provisioned()` pre-commit guard rejects self-generation; external addr/NetKey/DevKey retained. | P1 |
| ZGB-012 | Case 3: anchor election precondition | Multiple anchor-eligible nodes commissioning simultaneously shall elect a single founder before any commits `0x0001`. | Only one node in a radio neighborhood commits `0x0001` per commission epoch; others suppress and retry or join. | P0 |
| ZGB-013 | Case 3: min-UUID listen-before-founding | Commissioning anchors shall scan ZGB Arbitration Beacons (ZGB-018) during a listen phase inside the commission window; only the anchor with the lowest UUID among commissioning peers may commit. | Suppressed anchor receives a lower-UUID commissioning AB and cancels its own founding. | P0 |
| ZGB-014 | Case 3: retry-on-suppression | An anchor suppressed by a lower-UUID peer shall retry commission after a bounded backoff window until it wins or detects an existing network. | Suppressed node re-enters commission mode every `T_retry` (default 60s) unless a provisioned network is detected; no permanent latch. | P0 |
| ZGB-015 | Case 3: split-brain detection & healing | Two anchors committing inside each other's blind window create two `0x0001` claims. Because different NetKeys cause MIC failure before UVP dispatch, detection shall compare the provisioned Arbitration Beacon `netkey_fp` over a confirmation window, not "first UVP contact". | Fingerprint mismatch confirmed by 3 stable ABs over 30 s raises `SPLIT_BRAIN`; higher-UUID anchor broadcasts node-reset to its children, factory-resets, and rejoins as Case-1; winner's NetKey survives. | P0 |
| ZGB-016 | Case 3: DevKey vault consistency | A failed anchor's orphan DevKey record shall not block legitimate re-adoption after reset. | Blacklist (`ZOMBIE_REHEAL_REJECTED`) applies only to replaced hardware UUIDs, not reset-and-rejoin anchors; re-adopt path exists via standard provisioning. | P1 |
| ZGB-017 | Universal binary & flash budget | Bootstrap logic, anchor key birth, and election state machines shall operate within a single unified firmware binary (`CONFIG_BLE_MESH_PROVISIONER=n`, `CONFIG_BLE_MESH_PROV=y`) fitting within the existing UVP flash budget (≤ 15 KB UVP suite, REQ-NF01) across all targets. | Measured total flash delta ≤ 15 KB; no dual images or anchor-class builds; `CONFIG_BLE_MESH_PROVISIONER=n` verified on all targets. | P0 |
| ZGB-018 | Case 3: ZGB Arbitration Beacon | Cases 1-3 shall use one auxiliary standard BLE advertisement for election and island detection: AD type `0xFF`, Company ID `0x02E5`, payload `ver(1) + flags(1) + uuid(16) + netkey_fp(4)`. It is not a UVP opcode and not a mesh model. | AB parses and round-trips on ESP-IDF and Zephyr; commissioning candidates advertise `anchor_eligible \| commissioning`; provisioned anchors advertise `anchor_eligible \| provisioned` plus `SHA-256(NetKey)[0..3]`; passive Case-1 nodes emit no AB. | P0 |
| ZGB-019 | Case 3: election commit timing | A commissioning anchor shall listen for `T_listen = 10 s` inside the 60 s commission window before committing `0x0001`. A commit at window expiry is insufficient. | Winner commits at `T_listen` end when it sees no lower-UUID commissioning peer; commit failure rolls back and re-listens next cycle. | P0 |
| ZGB-020 | Case 3: provisioner role state machine | `meshx_prov_role_t` shall expose `CANDIDATE` and `STALE` in addition to `STANDALONE/MASTER/STANDBY`; only `MASTER`/`STANDBY` persist to NVS. | `CANDIDATE` while listening, `MASTER` after commit, `STALE` after suppression or split-brain loss; reboot never restores `CANDIDATE`/`STALE`. | P1 |

---

## 3. Out of Scope

- Factory pre-provisioning (per-pack burned keys) — deferred manufacturing option.
- Self-provisioning without any physical/human trigger for Case 1 (impossible without shared secret).
- New MXCP command set beyond `MXCP_CMD_BOOTSTRAP_COMMISSION` / `MXCP_CMD_BOOTSTRAP_ABORT`.
- Authenticated unprovisioned beacon signing (noted as gap ZGB-005; deferred to future).
- Subnet coexistence / multi-island routing (Case 4) — deferred to a future spec focused on dual-NetKey gateway routing.

---

## 4. References

- `.specs/meshx-prov-srv-mxcp/requirements.md` (REQ-F01, REQ-F02, REQ-NF01, REQ-NF02)
- `.specs/meshx-prov-srv-mxcp/trd/page1_overview.md` (standard provisioning, zero-bloat)
- `.specs/meshx-prov-srv-mxcp/trd/page3_remote_provisioning.md` (RPR tunneling `0xC0..0xC3`)
- `.specs/meshx-prov-srv-mxcp/trd/page5_redundancy_cascading.md` (Master `0x0001`, freshness vector, roles)
- `.specs/meshx-prov-srv-mxcp/trd/page7_resilience_memory.md` (failure modes, NVS commit)
- `.specs/meshx-prov-srv-mxcp/trd/page8_fast_bulk_provisioning.md` (Wavefront `0xC8/0xCC`, unified pool)
- `.specs/meshx-prov-srv-mxcp/trd/page9_node_replacement.md` (quarantine, zombie blacklist)
- `.specs/autonomous-element-pairing/requirements.md` (REQ-003 capability advertising, deferred zero-gateway)
- `.specs/autonomous-element-pairing/design/02_uvp_discovery_protocol.md` (UVP vendor model, `0xFFFF` drop)

---

## 5. Known Implementation Gaps (spec-to-code)

| Gap | Evidence | Handling in this spec |
|-----|----------|----------------------|
| No UVP `0xC7/0xC8/0xC9/0xCC` handler code in tree | `grep 0xC8\|0xC9\|0xCC in src → empty` | Case 2/3 tasks include implementing handler stubs |
| No provisioner-role behavior behind `MASTER/STANDBY` enum | `meshx_group_allocator.h:143-147` | Case 2/3 tasks include role state machine |
| `CANDIDATE`/`STALE` roles absent from code | spec page5 vs code enum | Case 3 uses MASTER/STANDBY only; suppression retry is software-level |
| DevKey NVS logs first 4 UUID bytes | `meshx_nvs.c:822-823` | ZGB-007 demands log sanitization |
| `meshx_nvs_commit()` clears write-list head on write error | `meshx_nvs.c:283-314` | ZGB-008 atomicity depends on fixing partial commits |
| No beaconing kill-switch / TTL | `meshx_mxcp.c:857-878` | ZGB-004 adds TTL/disable path |
| `meshx_zgb_perform_self_provision()` has no caller | `grep` over `main/`, `port/` → only definition in `meshx_zgb.c:138` | ZGB-019 wires commit after `T_listen` win |
| `meshx_zgb_absorb_case1_target()` has no caller | only definition in `meshx_zgb.c:294` | ZGB-009 needs an MXCP/RPR entry point; current code allocates address + DevKey only, does not drive PB-ADV PDUs |
| No arbitration beacon TX/RX | platform layers expose only re-heal mfg-data advertising; no GAP scan receive path | ZGB-018 adds AB TX/RX on ESP-IDF and Zephyr |
| `meshx_prov_role_t` lacks `CANDIDATE`/`STALE` | `meshx_group_allocator.h:143-147` | ZGB-020 extends enum; volatile roles only |
| Split-brain "first UVP contact" is impossible | different NetKey → MIC failure, no UVP dispatch | ZGB-015 revised to AB `netkey_fp` comparison |
