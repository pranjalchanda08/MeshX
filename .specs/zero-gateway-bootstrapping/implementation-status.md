# Implementation Status — Zero-Gateway Bootstrap

**Checked against:** uncommitted working tree, 2026-11-06.  
**Legend:** DONE = code exists and matches requirement; PARTIAL = code exists but path is unreachable or incomplete; MISSING = no code.

| Req | Feature | Status | Evidence | Gap |
|-----|---------|--------|----------|-----|
| ZGB-001 | Case-1 beaconing | DONE | native Prov Server enabled on boot when unprovisioned (`esp_platform_ble_mesh.c`, `meshx_plat_ble_mesh.c`) | — |
| ZGB-002 | Case-1 no self-provision | DONE | `meshx_zgb_open_commission_window()` rejects provisioned node; passive nodes never enter commit | — |
| ZGB-003 | Dual birth channels | PARTIAL | direct PB-ADV works via native Prov Server; RPR tunnel handlers are stubs (`meshx_uvp_rpr_handlers.cpp`) | RPR PDU injection not implemented |
| ZGB-004 | Beaconing kill-switch | DONE | `duration_sec` TTL timer + `MXCP_CMD_PROV_DISABLE` in `meshx_mxcp.c` | — |
| ZGB-005 | Capability integrity | DONE | trait mask `uuid[10..11]` vs element count at `CONTROL_TASK_MSG_EVT_PROVISION_STOP` (`meshx_prov_srv.cpp`) | mask is still injected, not signed |
| ZGB-006 | Commission triggers | DONE | button (`meshx_gpio.c`), MXCP 0x39 (`meshx_mxcp.c`), `T_boot` 25-35 s (`meshx_zgb.c`) | — |
| ZGB-007 | On-device key birth | DONE | `meshx_crypto_random_fill()` on both platforms; staged keys zeroed; no key logging | — |
| ZGB-008 | Atomic self-provision | PARTIAL | `meshx_nvs_self_provision_commit()` + `meshx_plat_self_provision()` exist | `meshx_zgb_perform_self_provision()` has no caller, so commit never runs |
| ZGB-009 | Case-1 absorption | PARTIAL | `meshx_zgb_absorb_case1_target()` allocates address + DevKey, validates traits | no caller; no PB-ADV PDU driving; no RPR tunnel |
| ZGB-010 | Post-commit beacon gating | DONE | boot gate in both platform init paths; beacon disable after self-provision | — |
| ZGB-011 | External priority | DONE | `meshx_zgb_on_external_prov_start()` on link-open event | — |
| ZGB-012 | Election precondition | MISSING | — | no election yet |
| ZGB-013 | Min-UUID election | MISSING | — | no AB TX/RX, no LISTEN phase |
| ZGB-014 | Retry-on-suppression | MISSING | — | no `SUPPRESSED`/`T_retry` |
| ZGB-015 | Split-brain | MISSING | — | old "UVP contact" model impossible; AB `netkey_fp` not implemented |
| ZGB-016 | DevKey vault consistency | PARTIAL | `meshx_nvs_set_device_key` / `meshx_nvs_erase_device_key` exist | no blacklist distinction for replaced vs reset hardware |
| ZGB-017 | Unified binary / flash | PARTIAL | `CONFIG_BLE_MESH_PROVISIONER=n` unchanged in diff | no measured flash delta |
| ZGB-018 | Arbitration Beacon | MISSING | — | no platform TX/RX, no frame codec |
| ZGB-019 | Election commit timing | MISSING | — | commission timer aborts; it never commits |
| ZGB-020 | Role state machine | MISSING | enum still `STANDALONE/MASTER/STANDBY` (`meshx_group_allocator.h:143-147`) | `CANDIDATE`/`STALE` absent |

## Verified defects to fix first

1. **Anchor never commits.** `zgb_commission_timer_cb()` calls `meshx_zgb_abort_commissioning()`. `meshx_zgb_perform_self_provision()` has no caller. Case 2 cannot complete today.
2. **Absorption is unreachable.** `meshx_zgb_absorb_case1_target()` has no caller in `main/` or `port/`.
3. **Election is unimplemented.** No ABA TX/RX, no LISTEN, no `SUPPRESSED`, no `T_retry`.
4. **Split-brain detection model was wrong.** Different NetKeys cause MIC failure at the mesh layer; no UVP frame reaches the dispatcher. Detection must use AB `netkey_fp`.
5. **Role enum drift.** `PROJECT.md` lists `CANDIDATE/STALE`; code does not have them.

## Test status

`tools/scripts/test_zgb_e2e_harness.py`: 240/240 simulator tests pass. The harness models zgb_sim.py, not firmware. It does not exercise defect 1 or 2 because those code paths are unreachable.
