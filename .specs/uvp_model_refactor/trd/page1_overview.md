# TRD — UVP Model Refactor
## Page 1: System Overview & Architecture

**Document:** Technical Requirements Document (TRD)
**Spec:** `uvp_model_refactor`
**Revision:** 1.0
**Status:** Draft

---

## 1. Purpose

This document describes the technical requirements and design for refactoring the `meshXUVPElement` notification path from a monolithic, variant-keyed `if-else` handler into a modular, composition-based `meshXModel` architecture.

It is intended for firmware engineers implementing or reviewing the change.

---

## 2. Scope

| In Scope | Out of Scope |
|---|---|
| `meshXUVPElement::element_state_change_notify` refactor | BLE Mesh stack internals |
| `meshx_uvp_ctx_t` `func_id` field addition | TXCM retry algorithm changes |
| UVP wire payload `func_id` prefix (2 bytes) | NVS / persistence logic |
| New `meshXModel` base class and concrete subclasses | Host application code (`main.c`) |
| Dispatcher `func_id` propagation (all 3 paths) | SIG model layer |

---

## 3. System Context

```mermaid
graph TD
    APP["Host Application\n(main.c)"]
    MXCP["MXCP Serial Layer"]
    CTRL["Control Task\n(FreeRTOS)"]
    DISP["UVP Dispatcher\n(meshx_uvp_dispatcher.cpp)"]
    ELEM["meshXUVPElement\n(Container)"]
    PHYS["meshXUVPModel\n(Physical BLE Vendor Model)"]
    LM["meshXModel\n(Logical Models)"]
    BLE["ESP BLE Mesh Stack"]
    NET["BLE Mesh Network"]

    APP -->|"meshx_send_msg_to_element(func_id)"| MXCP
    MXCP --> CTRL
    CTRL -->|"CONTROL_TASK_MSG_CODE_TO_MESHX"| DISP
    BLE -->|"Vendor Model Callback"| CTRL
    CTRL -->|"CONTROL_TASK_MSG_CODE_FRM_BLE"| DISP
    DISP -->|"on_model_cb(payload, size, ctx)"| ELEM
    ELEM -->|"can_handle / handle_rx / handle_timeout"| LM
    LM -->|"send_with_func_id(...)"| PHYS
    PHYS -->|"meshx_uvp_send()"| BLE
    BLE <-->|"BLE Mesh RF"| NET
    LM -->|"meshx_send_msg_to_app(func_id,...)"| CTRL
    CTRL --> MXCP
    MXCP --> APP
```

---

## 4. Key Entities

| Entity | Role | File |
|---|---|---|
| `meshXUVPElement` | Container: composes logical models, routes events | `meshx_uvp_element.cpp` |
| `meshXModel` | Abstract base: `handle_rx`, `handle_timeout`, `can_handle` | `meshx_uvp_logical_model.hpp` *(new)* |
| `meshXRelayClientModel` | Client logic: fwd host cmd to BLE, report ACK to app | `meshx_uvp_logical_models.cpp` *(new)* |
| `meshXRelayServerModel` | Server logic: ACK sender + publish + app telemetry | `meshx_uvp_logical_models.cpp` *(new)* |
| `meshXLightCWWWClientModel` | Client logic for OnOff (F0) and CTL (F1) | `meshx_uvp_logical_models.cpp` *(new)* |
| `meshXLightCWWWServerModel` | Server logic for OnOff (F0) and CTL (F1) | `meshx_uvp_logical_models.cpp` *(new)* |
| `meshXUVPModel` | Physical transport: wraps `meshx_uvp_send()` | `meshx_uvp_model.hpp` |
| `meshx_uvp_ctx_t` | Routing context passed element → model | `meshx_uvp.h` |
| UVP Dispatcher | Three callbacks: BLE RX / Host CMD / TXCM Timeout | `meshx_uvp_dispatcher.cpp` |

---

## 5. `func_id` Lifecycle

The core insight of this refactor is that `func_id` must be an **explicit, first-class field** that travels from the host application to the receiving BLE node and back — end-to-end.

```mermaid
journey
    title func_id lifecycle
    section Host → BLE
        Host sets func_id in API call: 5: Host
        uvp_app_command_cb reads func_id: 5: Dispatcher
        ctx.func_id populated: 5: Dispatcher
        can_handle() matches logical model: 5: Element
        send_with_func_id() prepends to wire: 5: Logical Model
    section BLE Wire
        2-byte prefix on wire payload: 5: meshXUVPModel
    section BLE → Host
        uvp_unified_dispatcher_cb strips prefix: 5: Dispatcher
        ctx.func_id populated from wire: 5: Dispatcher
        can_handle() matches logical model: 5: Element
        meshx_send_msg_to_app(func_id): 5: Logical Model
        Host receives correct func_id: 5: Host
```

---

*Continued in Page 2: Interface Specifications*
