# TRD — UVP Model Refactor
## Page 3: Sequence Diagrams

---

## SD-01 — Host Command → Client Element → BLE Mesh TX

**Scenario:** Host application sends an ON/OFF command to Relay Client element (func_id=0x00).

```mermaid
sequenceDiagram
    participant App as Host Application<br/>(main.c)
    participant API as meshx_api<br/>(meshx_api.c)
    participant CT as Control Task<br/>(FreeRTOS Queue)
    participant DISP as UVP Dispatcher<br/>(uvp_app_command_cb)
    participant ELEM as meshXUVPElement
    participant LM as meshXRelayClientModel<br/>(func_id=0x00)
    participant PHYS as meshXUVPModel<br/>(Physical Transport)
    participant BLE as ESP BLE Mesh Stack

    App->>API: meshx_send_msg_to_element(el=1, type=RELAY_CLIENT,<br/>func_id=0x00, data=[ON])
    API->>CT: control_task_msg_publish(CODE_TO_MESHX, EVT_DATA, msg)
    CT->>DISP: uvp_app_command_cb(params=meshx_app_api_msg_t)

    Note over DISP: Read func_id from element_msg.func_id<br/>(was previously DROPPED — now fixed)
    DISP->>DISP: ctx = {src=0x0001, func_id=0x00}
    DISP->>ELEM: element->on_model_cb(data=[ON], size=1, &ctx)
    ELEM->>ELEM: element_state_change_notify(param, size, ctx)

    loop for each model in logical_models
        ELEM->>LM: can_handle(param, size, ctx)<br/>ctx.func_id(0x00) == model.func_id(0x00) → true
    end

    ELEM->>LM: handle_rx([ON], 1, ctx)
    Note over LM: ctx.src_addr == 0x0001 → host command path
    LM->>PHYS: send_with_func_id(pub_addr, type_id=RELAY_CLIENT,<br/>func_id=0x00, [ON], 1, ack_req=true)
    Note over PHYS: Wire buffer: [0x00, 0x00, ON] (func_id LE + payload)
    PHYS->>BLE: meshx_uvp_send(p_model, pub_addr, type_id, wire, 3, true)
    BLE-->>App: (async — ACK/timeout arrives later via SD-02 or SD-04)
```

---

## SD-02 — BLE Mesh RX → Server Element → ACK + Publish + App Telemetry

**Scenario:** Server element receives ON/OFF command from BLE mesh. It ACKs, publishes, and notifies the host.

```mermaid
sequenceDiagram
    participant NET as BLE Mesh Network
    participant BLE as ESP BLE Mesh Stack
    participant CT as Control Task<br/>(FreeRTOS Queue)
    participant DISP as UVP Dispatcher<br/>(uvp_unified_dispatcher_cb)
    participant ELEM as meshXUVPElement<br/>(Relay Server, el=2)
    participant LM as meshXRelayServerModel<br/>(func_id=0x00)
    participant PHYS as meshXUVPModel
    participant API as meshx_api
    participant App as Host Application

    NET->>BLE: BLE ADV/GATT: UVP vendor msg<br/>type_id=RELAY_SERVER, wire=[0x00,0x00,ON]
    BLE->>CT: Vendor model callback → control_task_msg_publish(CODE_FRM_BLE)
    CT->>DISP: uvp_unified_dispatcher_cb(params)

    Note over DISP: Strip 2-byte prefix from wire payload
    DISP->>DISP: func_id = wire[0..1] = 0x0000
    DISP->>DISP: payload = wire+2 = [ON], payload_len = 1
    DISP->>DISP: ctx = {src=CLIENT_ADDR, ack_req=1, func_id=0x00}
    DISP->>ELEM: element->on_model_cb([ON], 1, &ctx)
    ELEM->>ELEM: element_state_change_notify()

    loop for each model in logical_models
        ELEM->>LM: can_handle(ctx)<br/>ctx.func_id(0x00) == model.func_id(0x00) → true
    end

    ELEM->>LM: handle_rx([ON], 1, ctx)

    rect rgb(220, 240, 255)
        Note over LM: Step 1 — Unicast ACK (ctx.ack_req == 1)
        LM->>PHYS: send_with_func_id(src_addr=CLIENT_ADDR,<br/>type_id=RELAY_SERVER, func_id=0x00, [ON], 1, ack_req=false)
        PHYS->>BLE: meshx_uvp_send(...)
        BLE->>NET: Unicast ACK/Status to CLIENT_ADDR
    end

    rect rgb(220, 255, 220)
        Note over LM: Step 2 — Publish to group/pub address
        LM->>PHYS: send_with_func_id(pub_addr, type_id, func_id=0x00, [ON], 1, false)
        PHYS->>BLE: meshx_uvp_send(...)
        BLE->>NET: Publish to pub_addr (group)
    end

    rect rgb(255, 245, 220)
        Note over LM: Step 3 — App telemetry
        LM->>API: meshx_send_msg_to_app(el=2, type=RELAY_SERVER,<br/>func_id=get_func_id()=0x00, data=[ON])
        API->>CT: control_task_msg_publish(CODE_TO_APP)
        CT->>App: app_data_cb(msg_hdr, payload)
    end
```

---

## SD-03 — BLE Mesh RX → Client Element → App Telemetry (ACK Received)

**Scenario:** Relay Client receives status/ACK from the Relay Server. Reports success to host.

```mermaid
sequenceDiagram
    participant NET as BLE Mesh Network
    participant BLE as ESP BLE Mesh Stack
    participant CT as Control Task
    participant DISP as UVP Dispatcher
    participant ELEM as meshXUVPElement<br/>(Relay Client, el=1)
    participant LM as meshXRelayClientModel<br/>(func_id=0x00)
    participant API as meshx_api
    participant App as Host Application

    NET->>BLE: UVP status from SERVER_ADDR<br/>wire=[0x00,0x00,ON]
    BLE->>CT: Vendor model callback
    CT->>DISP: uvp_unified_dispatcher_cb()
    DISP->>DISP: func_id=0x00, payload=[ON], ctx.src_addr=SERVER_ADDR
    DISP->>ELEM: on_model_cb([ON], 1, &ctx)
    ELEM->>LM: can_handle → true (func_id match)
    ELEM->>LM: handle_rx([ON], 1, ctx)

    Note over LM: ctx.src_addr != 0x0001 → BLE response path
    LM->>LM: Build meshx_api_relay_client_evt_t<br/>err_code=0, on_off=ON
    LM->>API: meshx_send_msg_to_app(el=1, type=RELAY_CLIENT,<br/>func_id=0x00, sizeof(evt), &evt)
    API->>CT: control_task_msg_publish(CODE_TO_APP)
    CT->>App: app_data_cb → {err_code=0, on_off=ON}
```

---

## SD-04 — TXCM Timeout → All Client Models → App Telemetry (Error)

**Scenario:** TXCM determines the server did not ACK within the retry window. Timeout fires.

```mermaid
sequenceDiagram
    participant TXCM as TXCM Module<br/>(meshx_txcm.c)
    participant CT as Control Task
    participant DISP as UVP Dispatcher<br/>(uvp_txcm_timeout_cb)
    participant ELEM as meshXUVPElement<br/>(Relay Client, el=1)
    participant LM0 as meshXRelayClientModel<br/>(func_id=0x00)
    participant API as meshx_api
    participant App as Host Application

    TXCM->>CT: control_task_msg_publish(CODE_TXCM, EVT_TXCM_MSG_TIMEOUT)
    CT->>DISP: uvp_txcm_timeout_cb(params=uvp_txcm_param_t)
    DISP->>DISP: Lookup element by p_model pointer
    DISP->>DISP: ctx = {src=MESHX_ADDR_UNASSIGNED,<br/>func_id=0xFFFF (broadcast sentinel)}
    DISP->>ELEM: element->on_model_cb(nullptr, 0, &ctx)
    ELEM->>ELEM: is_timeout = true

    loop for EVERY model in logical_models
        ELEM->>LM0: handle_timeout(ctx)
        LM0->>LM0: Build relay_client_evt_t<br/>err_code=1, on_off=0
        LM0->>API: meshx_send_msg_to_app(el=1, type=RELAY_CLIENT,<br/>func_id=get_func_id()=0x00, ...)
        API->>CT: control_task_msg_publish(CODE_TO_APP)
        CT->>App: app_data_cb → {err_code=1, on_off=0}
    end
```

---

## SD-05 — CWWW Client: Two Functions on One Element (func_id Demux)

**Scenario:** CWWW Server sends back a CTL status (func_id=0x01). The element must route it to the CTL model, not the OnOff model.

```mermaid
sequenceDiagram
    participant NET as BLE Mesh Network
    participant DISP as UVP Dispatcher
    participant ELEM as meshXUVPElement<br/>(CWWW Client, el=3)
    participant LM0 as meshXLightCWWWClientModel<br/>(func_id=0x00 — OnOff)
    participant LM1 as meshXLightCWWWClientModel<br/>(func_id=0x01 — CTL)
    participant App as Host Application

    NET->>DISP: UVP wire: type_id=CWWW_CLIENT<br/>payload=[0x01, 0x00, lightness_lo, lightness_hi, temp_lo, temp_hi, ...]
    DISP->>DISP: Strip prefix: func_id=0x0001
    DISP->>DISP: payload=[lightness_lo, lightness_hi, ...], ctx.func_id=0x01
    DISP->>ELEM: on_model_cb(ctl_payload, ctl_size, &ctx)
    ELEM->>ELEM: element_state_change_notify()

    ELEM->>LM0: can_handle(ctx)<br/>ctx.func_id(0x01) == LM0.func_id(0x00) → FALSE
    ELEM->>LM1: can_handle(ctx)<br/>ctx.func_id(0x01) == LM1.func_id(0x01) → TRUE
    ELEM->>LM1: handle_rx(ctl_payload, ctl_size, ctx)
    LM1->>App: meshx_send_msg_to_app(el=3, type=CWWW_CLIENT,<br/>func_id=0x01, sizeof(evt), &ctl_evt)

    Note over LM0: NOT called — clean demux by func_id
```

---

*Continued in Page 4: Wire Format & Concrete Model Implementations*
