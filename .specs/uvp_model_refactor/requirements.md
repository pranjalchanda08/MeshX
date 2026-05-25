# Requirements: UVP Model Refactor

**Spec:** `uvp_model_refactor`
**Flow:** Requirement First
**Status:** Draft — Pending Sign-off

---

## REQ-001 — Modular Logical Model Architecture

**Priority:** P0
**Title:** Replace monolithic `element_state_change_notify` with modular `meshXModel` handlers

**Description:**
The current `meshXUVPElement::element_state_change_notify` uses a chain of `if-else` blocks keyed on `get_element_variant()` to decide which telemetry structure to populate and dispatch. Any new element type or functional variation requires direct modification of this function, violating the Open-Closed Principle.

A base class `meshXModel` shall be introduced. Each element variant (Relay Client, CWWW Client, Relay Server, CWWW Server, etc.) shall have a corresponding concrete `meshXModel` subclass encapsulating its own RX handling, timeout handling, and app telemetry dispatch.

**Acceptance Criteria:**
- `meshXModel` base class exists with pure-virtual `handle_rx()`, `handle_timeout()`, and `can_handle()` methods.
- `meshXUVPElement` stores a `std::vector<std::unique_ptr<meshXModel>>` instead of variant-switch logic.
- Adding a new element variant requires only a new `meshXModel` subclass — no changes to `meshXUVPElement`.
- All existing variant behaviors (Relay Client, Relay Server, CWWW Client, CWWW Server) are preserved post-refactor.

---

## REQ-002 — Explicit `func_id` Registration at Model Construction

**Priority:** P0
**Title:** Each `meshXModel` instance must be constructed with an explicit, registered `func_id`

**Description:**
Currently, `func_id` is computed at runtime using payload size heuristics:
```cpp
uint16_t func_id = (param_size <= 1) ? 0x00 : 0x01;
```
This is fragile — any element can have multi-byte payloads at `func_id = 0x00`, and it creates an implicit binding that is invisible at the composition site.

Each `meshXModel` subclass instance shall accept `func_id` as a constructor parameter, establishing an explicit, traceable binding at element composition time.

**Acceptance Criteria:**
- `meshXModel` base class constructor signature: `meshXModel(meshXElementIF* parent, meshXUVPModel* phys_model, uint16_t func_id)`.
- `func_id` is stored and accessible via `get_func_id()` on the base class.
- All `meshx_send_msg_to_app()` calls within model handlers use `get_func_id()` — no hardcoded `0x00` / `0x01`.
- The `func_id` defines are in `meshx_api.h` (e.g. `MESHX_ELEMENT_FUNC_ID_RELAY_SERVER_ON_OFF`) and are used at composition time.

---

## REQ-003 — `func_id` Propagation via `meshx_uvp_ctx_t`

**Priority:** P0
**Title:** Add `func_id` field to `meshx_uvp_ctx_t` so it reaches the element callback

**Description:**
`meshx_uvp_ctx_t` currently carries: `src_addr`, `dst_addr`, `tid`, `ack_req`. It does **not** carry a `func_id`. As a result:

- For **host→element** commands (`uvp_app_command_cb`): the `func_id` exists in `p_msg->msg_type_u.element_msg.func_id` but is **silently dropped** — it is never forwarded to the element callback.
- For **BLE RX packets** (`uvp_unified_dispatcher_cb`): there is no per-function identifier in the UVP wire payload beyond `type_id` (which identifies the element type, not the function within the element).

`func_id` must be added to `meshx_uvp_ctx_t` and populated correctly at both sources.

**Acceptance Criteria:**
- `meshx_uvp_ctx_t` gains a `uint16_t func_id` field.
- `uvp_app_command_cb` populates `ctx.func_id` from `p_msg->msg_type_u.element_msg.func_id`.
- The BLE RX path (`uvp_unified_dispatcher_cb`) populates `ctx.func_id` from the wire payload (see REQ-004).
- The timeout context sets `func_id` to a sentinel value (e.g. `0xFFFF`) indicating a broadcast timeout.

---

## REQ-004 — `func_id` Encoding in UVP Wire Payload

**Priority:** P0
**Title:** Prepend `func_id` (2 bytes) to the UVP TLV wire payload for BLE RX demultiplexing

**Description:**
For BLE mesh RX packets, the only per-message identifiers available at the dispatcher level are `type_id` (element variant, in the UVP header) and the raw TLV payload. There is no field in the UVP header that carries a per-function ID within an element.

To enable reliable `func_id` routing on the receiver side, the **sender** (client `meshXModel`) shall prepend a 2-byte `func_id` field at the start of every UVP wire payload. The receiver dispatcher (`uvp_unified_dispatcher_cb`) shall strip this prefix, populate `ctx.func_id`, and pass the remaining payload to the element callback.

**Acceptance Criteria:**
- `meshXUVPModel::send()` (or a wrapper called by model `handle_tx`) prepends the 2-byte `func_id` before calling `meshx_uvp_send()`.
- `uvp_unified_dispatcher_cb` reads the first 2 bytes of the received payload as `func_id`, sets `ctx.func_id`, and advances the payload pointer by 2.
- The stripped payload (without the `func_id` prefix) is what is passed to `element->on_model_cb()`.
- `MESHX_UVP_MAX_PAYLOAD` accounting is updated to reflect the 2-byte prefix overhead.

---

## REQ-005 — `can_handle()` Based on `func_id` Match, Not Payload Size

**Priority:** P0
**Title:** `can_handle()` must use `ctx->func_id == get_func_id()` as the sole routing criterion

**Description:**
The architectural design document (`uvp_model_architecture.md`) proposed using payload size to implement `can_handle()`. This is logically incorrect: a model at `func_id = 0x00` can legitimately have a multi-byte payload (e.g., a relay server receiving a struct), and a model at `func_id = 0x01` can have a single-byte payload.

The correct and sole routing criterion is: does the incoming `ctx->func_id` match the model's registered `func_id`?

**Acceptance Criteria:**
- `can_handle()` signature: `virtual bool can_handle(const void* param, size_t param_size, const meshx_uvp_ctx_t* ctx) const = 0`
- Default implementation in base class: `return ctx->func_id == get_func_id();`
- All concrete model subclasses inherit this default unless they have a documented reason to override it.
- No reference to `param_size` as a routing criterion exists in any `can_handle()` implementation.

---

## REQ-006 — Server Model: ACK, Publish, and App Telemetry Pipeline

**Priority:** P1
**Title:** Server `meshXModel` subclasses must implement the standard 3-step pipeline

**Description:**
All server-side logical models share the same response pipeline upon receiving a command:
1. **Unicast ACK** back to `ctx->src_addr` if `ctx->ack_req == true`.
2. **Publish** updated state to `element_ctx.pub_addr` (if assigned and different from `src_addr`).
3. **App telemetry** via `meshx_send_msg_to_app()` with the correct `func_id`.

This pipeline is currently duplicated inline within `element_state_change_notify`. It must be encapsulated within each server `meshXModel` subclass.

**Acceptance Criteria:**
- Each server model (`meshXRelayServerModel`, `meshXLightCWWWServerModel`, etc.) implements steps 1–3 within its `handle_rx()`.
- `handle_timeout()` for server models returns `MESHX_SUCCESS` immediately (servers do not initiate transmissions).
- No server pipeline logic remains in `meshXUVPElement::element_state_change_notify`.

---

## REQ-007 — Client Model: Host Command Forwarding

**Priority:** P1
**Title:** Client `meshXModel` subclasses must handle host-originated commands (src_addr == 0x0001)

**Description:**
When the host application issues a command to a client element, `uvp_app_command_cb` populates `ctx->src_addr = 0x0001`. The client element must forward the payload over the BLE mesh to its configured `pub_addr`, with ACK requested.

This logic is currently inline in `element_state_change_notify`. It must move into the client `meshXModel` subclass.

**Acceptance Criteria:**
- Client model `handle_rx()` checks if `ctx->src_addr == 0x0001` (host command) and calls `physical_model->send(pub_addr, ..., ack_req=true)`.
- If `pub_addr` is `MESHX_ADDR_UNASSIGNED`, the model logs a warning and returns without sending.
- For BLE RX responses (non-host src), the client model populates the app telemetry struct with `err_code = 0` and calls `meshx_send_msg_to_app()`.

---

## REQ-008 — Timeout Broadcast to All Client Models

**Priority:** P1
**Title:** TXCM timeout must trigger `handle_timeout()` on all registered logical models for the affected element

**Description:**
When a TXCM timeout fires for an element, the current code checks the variant and constructs a single telemetry event. Under the new architecture, the element must iterate over all registered `meshXModel` instances and call `handle_timeout(ctx)` on each one. This ensures any model that sent a request can report the failure to the app layer.

**Acceptance Criteria:**
- `element_state_change_notify` detects `is_timeout` (when `ctx->src_addr == MESHX_ADDR_UNASSIGNED`) and calls `handle_timeout(ctx)` on **all** registered logical models.
- Each client model's `handle_timeout()` sends a telemetry event with `err_code = 1` using its own `get_func_id()`.
- Server model `handle_timeout()` is a no-op returning `MESHX_SUCCESS`.

---

## REQ-009 — Backward Compatibility: Existing App API Unchanged

**Priority:** P0
**Title:** The `meshx_app_element_msg_header_t` and app callback signatures must remain unchanged

**Description:**
The host application communicates with the firmware via `meshx_app_data_cb_t`. The `meshx_app_element_msg_header_t` struct (containing `element_id`, `element_type`, `func_id`, `msg_len`) and the payload structures in `meshx_data_payload_t` must remain binary-compatible. No changes to `meshx_api.h` public types (except adding `func_id` to `meshx_uvp_ctx_t` which is an internal type).

**Acceptance Criteria:**
- `meshx_api.h` public types (`meshx_app_element_msg_header_t`, `meshx_data_payload_t`, `meshx_app_data_cb_t`) are unchanged.
- `meshx_uvp_ctx_t` gains a `func_id` field (this is an internal firmware type, not part of the public app API).
- Existing `main.c` application callback code compiles without modification.

---

*End of Requirements Draft*
