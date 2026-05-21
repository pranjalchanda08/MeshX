# Page 20 — UVP Routing & ACK Architecture

> **[← ELF Logging](./19_elf_logging.md)** | **[← Index](./README.md)**

---

## Overview

This page describes how the UVP (Unified Vendor Protocol) layer propagates routing context from the BLE Mesh stack all the way to the application-level element callback. It also covers the **dual-routing response pattern**: when a client sends an acknowledged request, the server responds with both a unicast ACK and a group publish to keep the mesh state synchronized.

---

## 1. Routing Context: `meshx_uvp_ctx_t`

Every element callback receives a populated context struct:

```c
typedef struct {
    uint16_t src_addr;   /* BLE Mesh source address of the incoming message */
    uint16_t dst_addr;   /* BLE Mesh destination address (unicast or group) */
    uint8_t  tid;        /* Transaction ID for duplicate suppression */
    bool     ack_req;    /* Client set ACK_REQ bit in UVP header */
    uint16_t func_id;    /* Function within the element (e.g., 0x0000 = OnOff) */
} meshx_uvp_ctx_t;
```

> **Internal type only.** `meshx_uvp_ctx_t` is firmware-internal. The public app API (`meshx_api.h`) remains binary-compatible and unchanged.

---

## 2. Inbound Routing Flow

```mermaid
sequenceDiagram
    participant STK as BLE Mesh Stack
    participant Port as "esp_ven_srv_model.c (Port Layer)"
    participant CT as Control Task Queue
    participant Disp as "UVP Dispatcher (meshx_uvp_dispatcher.cpp)"
    participant REG as "Element Registry (C++)"
    participant El as "meshXUVPElement (Relay Server)"
    participant API as meshx_api.c
    participant App as "App Layer (main.c)"

    STK->>Port: Rx PDU (src_addr, dst_addr, payload)
    Port->>Port: rx_el_id = model element_idx
    Note over Port: Resolve local element index from<br/>BLE model instance
    Port->>CT: control_task_msg_publish_uvp(meta, payload)
    CT->>Disp: Dequeue event (meta + payload)
    Disp->>Disp: Parse UVP header (TID, ACK_REQ, TYPE_ID)
    Disp->>Disp: Strip 2-byte func_id prefix from payload
    Disp->>Disp: Populate meshx_uvp_ctx_t (src, dst, tid, ack_req, func_id)
    Disp->>REG: find_element(rx_el_id)
    REG-->>Disp: element pointer

    loop For each logical model in element
        Disp->>El: model.can_handle(payload, len, ctx)
        opt func_id matches model
            El->>El: handle_rx(payload, len, ctx)
        end
    end

    El->>API: meshx_send_msg_to_app(el_type, func_id, data)
    API->>CT: control_task_msg_publish(CODE_TO_APP)
    CT->>App: Invoke meshx_app_data_cb(el_type, func_id, payload)
```

---

## 3. Dual-Routing Response Pattern

When a client requests an acknowledgment (`ack_req == true`), the server performs two independent sends:

```mermaid
sequenceDiagram
    participant Client as Requesting Client Node
    participant Server as "meshXUVPElement (Server)"
    participant TXCM as TXCM Module
    participant Group as Group Subscribers

    Client->>Server: UVP Frame ACK_REQ=1 src=0x0010 dst=0xC000
    Server->>Server: Process state change

    opt ack_req == true
        Server->>TXCM: Send unicast ACK to src_addr 0x0010
        TXCM->>Client: Status PDU unicast
    end

    opt pub_addr configured AND pub_addr != src_addr
        Server->>TXCM: Publish state to pub_addr group
        TXCM->>Group: Status PDU group publish
    end
```

### 3.1 Dual-Routing Policy (Code Level)

```c
// Inside meshXRelayServerModel::handle_rx()
if (ctx->ack_req) {
    // Step 1: Unicast ACK directly to requesting client
    physical_model->send(ctx->src_addr, status_payload, len, /*ack=*/false);
}

uint16_t pub_addr = parent_element->get_pub_addr();
if (pub_addr != MESHX_ADDR_UNASSIGNED && pub_addr != ctx->src_addr) {
    // Step 2: Group publish to keep network state synchronized
    physical_model->send(pub_addr, status_payload, len, /*ack=*/false);
}

// Step 3: App telemetry
meshx_send_msg_to_app(el_type, get_func_id(), &data, sizeof(data));
```

---

## 4. Client-Originating Command Flow

When the **host** sends a command to a client element, `src_addr == 0x0001` (host sentinel):

```mermaid
sequenceDiagram
    participant Host as Host / MXCP
    participant Disp as "UVP Dispatcher (App Command CB)"
    participant El as "meshXUVPElement (Client)"
    participant TXCM as TXCM Module
    participant Server as Remote Server Node

    Host->>Disp: MXCP_CMD_EL_SEND element_id func_id payload
    Disp->>Disp: ctx.src_addr = 0x0001 host sentinel
    Disp->>Disp: ctx.func_id = from MXCP header
    Disp->>El: element on_model_cb(payload, len, ctx)

    El->>El: model.can_handle(ctx) func_id match
    El->>El: handle_rx() ctx.src_addr == 0x0001 host command

    alt pub_addr configured
        El->>TXCM: send(pub_addr, payload, ack_req=true)
        TXCM->>Server: UVP Frame over BLE Mesh
        Server-->>TXCM: ACK
        TXCM-->>El: Delivery confirmed
    else pub_addr unassigned
        El->>El: Log warning no send
    end
```

---

## 5. Timeout Broadcast (TXCM Expiry)

When a TXCM timeout fires for an element, **all** registered logical models are notified:

```mermaid
sequenceDiagram
    participant TXCM as TXCM Module
    participant CT as Control Task
    participant El as meshXUVPElement
    participant Models as "Logical Models (all)"
    participant App as App Layer

    TXCM->>CT: publish(TXCM_MSG_TIMEOUT, element_id)
    CT->>El: element_state_change_notify is_timeout=true ctx
    Note over El: ctx.src_addr == MESHX_ADDR_UNASSIGNED<br/>timeout signal

    loop For each logical model in element
        El->>Models: model.handle_timeout(ctx)
        alt Server model
            Models->>Models: no-op servers never initiate TX
        else Client model
            Models->>App: meshx_send_msg_to_app(func_id, err_code=1)
        end
    end
```

---

## 6. `func_id` Wire Encoding

For BLE mesh RX, the dispatcher has no per-function identifier in the UVP header beyond `TYPE_ID`. The **sender** (client model) prepends a 2-byte `func_id` prefix to every UVP payload:

```mermaid
flowchart LR
    App["Client model<br/>handle_tx()"]
    Prefix["Prepend 2-byte func_id"]
    Send["send_with_func_id<br/>pub_addr, func_id, body"]
    BLE["meshx_uvp_send()<br/>func_id_lo, func_id_hi, body..."]
    RX["uvp_unified_dispatcher_cb()"]
    Strip["Read first 2 bytes into ctx.func_id<br/>Advance payload pointer +2"]
    CB["element on_model_cb<br/>stripped_payload, ctx"]

    App --> Prefix --> Send --> BLE -->|over air| RX --> Strip --> CB
```

> **Payload budget:** The 2-byte `func_id` prefix is counted against `MESHX_UVP_MAX_PAYLOAD`. Effective application payload = 377 − 2 = **375 bytes** maximum.

---

## 7. Key Requirements Summary

| REQ | Title | Status |
|-----|-------|--------|
| REQ-001 | Propagate `dst_addr` to Control Task | ✅ Implemented |
| REQ-002 | `meshx_uvp_ctx_t` passed to element callback | ✅ Implemented |
| REQ-003 | Unicast ACK routing via `ctx->src_addr` | ✅ Implemented |
| REQ-004 | `ack_req` flag in UVP header | ✅ Implemented |
| REQ-005 | Dual-Routing: unicast ACK + group publish | ✅ Implemented |
| REQ-006 (model refactor) | `func_id` in `meshx_uvp_ctx_t` | ✅ Implemented |
| REQ-007 (model refactor) | `can_handle()` uses `func_id` match only | ✅ Implemented |
| REQ-009 (model refactor) | Public app API (`meshx_api.h`) unchanged | ✅ Verified |

---

> **[← ELF Logging](./19_elf_logging.md)** | **[← Index](./README.md)**
