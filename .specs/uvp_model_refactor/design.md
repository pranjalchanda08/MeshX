# Technical Design: UVP Model Refactor — TRD Index

**Spec:** `uvp_model_refactor`
**Flow:** Requirement First — Stage 2
**Status:** Draft — Pending Sign-off

---

## Overview

This document is the **index and executive summary** for the UVP Model Refactor Technical Requirements Document (TRD). The full TRD is split across six pages in the `trd/` directory.

### Problem Statement

The `meshXUVPElement::element_state_change_notify()` function uses a monolithic `if-else` chain on `get_element_variant()` to route incoming BLE Mesh and host commands. The `func_id` delivered to the app is derived from `param_size` — a fragile heuristic. The `func_id` from host commands is silently dropped by the dispatcher.

### Solution Summary

Introduce a `meshXModel` base class layer between the physical `meshXUVPModel` (transport) and the `meshXUVPElement` (container). Each logical function within an element is a concrete `meshXModel` subclass registered with an explicit `func_id`. The `func_id` is propagated end-to-end via `meshx_uvp_ctx_t` and encoded as a 2-byte wire prefix on all UVP payloads.

---

## TRD Pages

| Page | File | Contents |
|------|------|----------|
| **1** | [trd/page1_overview.md](trd/page1_overview.md) | Purpose, scope, system context diagram, key entities, `func_id` lifecycle |
| **2** | [trd/page2_interfaces.md](trd/page2_interfaces.md) | `meshx_uvp_ctx_t` change, `meshXModel` base class API, `send_with_func_id`, `meshXUVPElement` members |
| **3** | [trd/page3_sequence_diagrams.md](trd/page3_sequence_diagrams.md) | 5 Mermaid sequence diagrams: Host TX, Server RX, Client ACK, TXCM Timeout, CWWW demux |
| **4** | [trd/page4_implementations.md](trd/page4_implementations.md) | UVP wire format, all 4 concrete model implementations |
| **5** | [trd/page5_tasks.md](trd/page5_tasks.md) | Wave task dependency graph, 7 task definitions, requirements traceability matrix |
| **6** | [trd/page6_io_binding.md](trd/page6_io_binding.md) | Element↔IO binding chain (RO config → Builder → App callback → GPIO/PWM), boot sequence diagram |
| **7** | [trd/page7_memory_impact.md](trd/page7_memory_impact.md) | Struct sizes, heap/flash/stack delta, wire overhead, measurement commands |

---

## Architecture at a Glance

```
HOST APPLICATION
      │  meshx_send_msg_to_element(el_id, func_id, ...)
      ▼
UVP Dispatcher  ──────────────────────────────────────────────
  uvp_app_command_cb()    uvp_unified_dispatcher_cb()
  ctx.func_id = msg.func_id   ctx.func_id = wire[0..1]
──────────────────────────────────────────────────────────────
      │  element->on_model_cb(payload, size, &ctx)
      ▼
meshXUVPElement
  for each model in logical_models:
    is_timeout  → model->handle_timeout(ctx)   [all models]
    can_handle  → model->handle_rx(...)        [first match]
      │
      ├─ meshXRelayClientModel    (func_id=0x00)
      ├─ meshXRelayServerModel    (func_id=0x00)
      ├─ meshXLightCWWWClientModel (func_id=0x00 OnOff / 0x01 CTL)
      └─ meshXLightCWWWServerModel (func_id=0x00 OnOff / 0x01 CTL)
           │  send_with_func_id([func_id|payload])
           ▼
      meshXUVPModel (Physical Transport)
           │  meshx_uvp_send()
           ▼
      ESP BLE Mesh Stack
```

---

## Key Design Decisions

| Decision | Rationale |
|----------|-----------|
| `func_id` as 2-byte wire prefix (not UVP header extension) | No BLE Mesh stack changes; backward compatible |
| `can_handle()` default: `ctx->func_id == get_func_id()` | Explicit, type-safe; no payload-size heuristic |
| Logical models composed in `list_ven_models()` | Physical model pointer must be valid (post-bake) before logical models are constructed |
| `break` after first `can_handle()` match | Each `func_id` maps to exactly one model; prevents duplicate dispatch |
| Server `handle_timeout()` is a no-op | Servers never initiate TX; no outstanding TXCM entries |
| App callback unchanged | REQ-009: binary compatibility with host application |

---

## File Change Summary

| File | Change |
|------|--------|
| `meshx_uvp.h` | Add `func_id` to `meshx_uvp_ctx_t` + payload budget macros |
| `meshx_uvp_logical_model.hpp` | **NEW** — `meshXModel` base class |
| `meshx_uvp_logical_models.hpp/.cpp` | **NEW** — 4 concrete model implementations |
| `meshx_uvp_model.hpp` | Add `send_with_func_id()` helper |
| `meshx_uvp_dispatcher.cpp` | Populate `ctx.func_id` in all 3 callbacks |
| `meshx_uvp_element.hpp/.cpp` | Add `logical_models` vector; rewrite notify |
| `CMakeLists.txt` | Add new source file |

---

## Build Command

```bash
. /run/media/pchanda/workspace/wsl/esp/v5.4/esp-idf/export.sh && \
python3 tools/scripts/meshx.py -B=xiao_c3 -N=all_in_one -b
```

---

## Requirements Reference

| REQ | Title | Priority | TRD Page |
|-----|-------|----------|----------|
| REQ-001 | Modular `meshXModel` architecture | P0 | 1, 2, 4 |
| REQ-002 | Explicit `func_id` at construction | P0 | 2 |
| REQ-003 | `func_id` in `meshx_uvp_ctx_t` | P0 | 2, 3 |
| REQ-004 | `func_id` wire prefix (2 bytes) | P0 | 3, 4 |
| REQ-005 | `can_handle()` by `func_id` only | P0 | 2 |
| REQ-006 | Server ACK→Pub→App pipeline | P1 | 3, 4 |
| REQ-007 | Client host fwd + BLE response | P1 | 3, 4 |
| REQ-008 | Timeout broadcasts to all models | P1 | 3 |
| REQ-009 | App API unchanged | P0 | 6 |

Full requirements: [requirements.md](requirements.md)
