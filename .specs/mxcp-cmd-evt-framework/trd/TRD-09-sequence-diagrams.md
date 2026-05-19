# TRD-09 — Sequence Diagrams

## 1. Sync Command / Response Flow (e.g. GET_COMPOSITION)

```mermaid
sequenceDiagram
    participant App as Host App
    participant FB_H as Host Frame Builder
    participant UART_H as Host UART TX
    participant UART_E as Engine UART RX
    participant SM as Engine State Machine
    participant D as mxcp_dispatch_frame
    participant T as mxcp_cmd_table[]
    participant H as mxcp_cmd_fn_get_composition
    participant TX as mxcp_send_event
    participant FB_E as Engine Frame Builder
    participant UART_ET as Engine UART TX

    App->>FB_H: mxcp_send_cmd(MXCP_CMD_GET_COMPOSITION, NULL, 0)
    Note over FB_H: Build frame: [SOF][LEN=0][0x03][CHK][EOF]<br/>TYPE=0x03 (CMD, ID=0x03)
    FB_H->>UART_H: write(frame, 6)
    UART_H->>UART_E: UART wire
    UART_E->>SM: mxcp_serial_parse_byte() × N
    Note over SM: SOF → LEN → TYPE<br/>→ CHECKSUM → EOF (LEN=0, skip PAYLOAD)
    SM->>D: mxcp_dispatch_frame(0x03, payload, 0)
    Note over D: bit 7=0 → CMD, ID=0x03
    D->>T: linear scan for cmd_id == 0x03
    T->>H: handler(payload, 0)
    Note over H: Serialize element composition<br/>+ element state data
    H->>TX: mxcp_send_event(MXCP_EVT_COMPOSITION_RSP, buf, len)
    Note over TX: Build frame: [SOF][LEN][0x86][COMP_DATA][CHK][EOF]<br/>TYPE=0x86 (EVT, ID=0x06)
    TX->>FB_E: mxcp_send_frame()
    FB_E->>UART_ET: write(frame)
    UART_ET->>UART_H: UART wire

    Note over UART_H,App: --- Sync response received ---

    UART_H->>App: mxcp_dispatch_frame(0x86, ...) → EVT COMPOSITION_RSP
    Note over App: App callback: composition data available

    App->>FB_H: mxcp_send_cmd(MXCP_CMD_GET_ELEMENT_STATE, NULL, 0)
    Note over FB_H: Build frame: [SOF][LEN=0][0x04][CHK][EOF]<br/>TYPE=0x04 (CMD, ID=0x04)
    FB_H->>UART_H: write(frame, 6)
    UART_H->>UART_E: UART wire
    UART_E->>SM: mxcp_serial_parse_byte() × N
    SM->>D: mxcp_dispatch_frame(0x04, payload, 0)
    D->>T: linear scan for cmd_id == 0x04
    T->>H: mxcp_cmd_fn_get_element_state(payload, 0)
    Note over H: Serialize current element states
    H->>TX: mxcp_send_event(MXCP_EVT_ELEMENT_STATE_RSP, buf, len)
    TX->>FB_E: mxcp_send_frame()
    FB_E->>UART_ET: write(frame)
    UART_ET->>UART_H: UART wire

    Note over UART_H,App: --- Sync response received ---

    UART_H->>App: mxcp_dispatch_frame(0x87, ...) → EVT ELEMENT_STATE_RSP
    Note over App: App callback: element state data available
```

## 2. Async Command / Event Flow (e.g. GPIO_INTR_ENABLE)

```mermaid
sequenceDiagram
    participant App as Host App
    participant FB_H as Host Frame Builder
    participant UART as UART Wire
    participant SM as Engine State Machine
    participant D as mxcp_dispatch_frame
    participant T as mxcp_cmd_table[]
    participant H as mxcp_cmd_fn_gpio_intr_enable
    participant TX as mxcp_send_event
    participant FB_E as Engine Frame Builder
    participant ISR as GPIO ISR / Callback

    rect rgb(230, 245, 255)
        Note over App,ISR: Phase 1: Sync Command + Sync Response
        App->>FB_H: mxcp_send_cmd(MXCP_CMD_GPIO_INTR_ENABLE,<br/>{pin=5, enable=1}, sizeof(...))
        Note over FB_H: Build: [SOF][LEN][0x26][pin=5,en=1][CHK][EOF]<br/>TYPE=0x26 (CMD, ID=0x26)
        FB_H->>UART: write(frame)
        UART->>SM: UART bytes
        SM->>D: mxcp_dispatch_frame(0x26, payload, len)
        D->>T: scan for cmd_id == 0x26
        T->>H: handler(payload, len)
        Note over H: meshx_gpio_intr_enable(5, true)
        H->>TX: mxcp_send_event(MXCP_EVT_GPIO_INTR_ENABLE_RSP,<br/>{status=0, pin=5}, ...)
        Note over TX: Build: [SOF][LEN][0xA6][rsp][CHK][EOF]<br/>TYPE=0xA6 (EVT, ID=0x26, sync ACK)
        TX->>FB_E: mxcp_send_frame()
        FB_E->>UART: write(frame)
        UART->>App: EVT GPIO_INTR_ENABLE_RSP (sync ACK)
        Note over App: Interrupt enabled successfully
    end

    rect rgb(255, 240, 230)
        Note over App,ISR: Phase 2: Async Event (time passes, GPIO interrupt fires)
        ISR->>ISR: GPIO ISR triggers
        ISR->>TX: mxcp_gpio_hosted_event_handler()
        Note over ISR,TX: mxcp_send_event(MXCP_EVT_GPIO_ASYNC,<br/>{type=INTR, pin=5, value=1, ts=...})
        Note over TX: Build: [SOF][LEN][0xBE][evt_data][CHK][EOF]<br/>TYPE=0xBE (EVT, ID=0x3E, async)
        TX->>FB_E: mxcp_send_frame()
        FB_E->>UART: write(frame)
        UART->>App: EVT GPIO_ASYNC (unsolicited)
        Note over App: App callback: interrupt notification pin 5
    end
```

## 3. Unsolicited Event Flow (e.g. PROV_COMP)

```mermaid
sequenceDiagram
    participant BLE as BLE Stack Callback
    participant API as meshx_send_ctrl_msg_to_app
    participant TX as mxcp_send_event
    participant FB as Engine Frame Builder
    participant UART as UART Wire
    participant SM as Host State Machine
    participant D as mxcp_dispatch_frame
    participant CB as Host App Callback

    rect rgb(245, 235, 255)
        Note over BLE,CB: Unsolicited Event: no originating command from host
        BLE->>API: provisioning complete event
        Note over API: Internal: wires to mxcp_send_event
        API->>TX: mxcp_send_event(MXCP_EVT_PROV_COMP,<br/>{net_idx, addr, uuid}, ...)
        Note over TX: Build: [SOF][LEN][0x81][prov_data][CHK][EOF]<br/>TYPE=0x81 (EVT, ID=0x01)
        TX->>FB: mxcp_send_frame()
        FB->>UART: write(frame)
        UART->>SM: UART bytes
        SM->>D: mxcp_dispatch_frame(0x81, ...)
        Note over D: EVT PROV_COMP, bit 7=1, ID=0x01 (unsolicited)
        D->>CB: meshx_app_ctrl_cb_t invoked
        Note over CB: App handles provisioning complete
    end
```

## 4. Element Data Flow (e.g. BLE state change → Host notification)

```mermaid
sequenceDiagram
    participant BLE as BLE Mesh Stack
    participant UVP as UVP Dispatcher
    participant EL as UVP Element
    participant API as meshx_send_msg_to_app
    participant TX as mxcp_send_event
    participant FB as Engine Frame Builder
    participant UART as UART Wire
    participant SM as Host State Machine
    participant D as mxcp_dispatch_frame
    participant CB as Host App Callback

    BLE->>UVP: UVP message received (over-the-air)
    UVP->>EL: element->on_model_cb()
    Note over EL: element_state_change_notify()
    Note over EL: 1. ACK to requester (BLE unicast)<br/>2. Publish to group (if pub_addr set)<br/>3. Notify host via MXCP
    EL->>API: meshx_send_msg_to_app(el_id, type, func_id,<br/>msg_len, &state_data)
    Note over API: Internal: wires to mxcp_send_event
    API->>TX: mxcp_send_event(MXCP_EVT_EL_DATA_NOTIFY,<br/>{element_id, element_type, func_id,<br/>msg_len, state_data}, ...)
    Note over TX: Build: [SOF][LEN][0x90][el_data][CHK][EOF]<br/>TYPE=0x90 (EVT, ID=0x10)
    TX->>FB: mxcp_send_frame()
    FB->>UART: write(frame)
    UART->>SM: UART bytes
    SM->>D: mxcp_dispatch_frame(0x90, ...)
    Note over D: EVT EL_DATA_NOTIFY, bit 7=1, ID=0x10
    D->>CB: meshx_app_data_cb_t invoked
    Note over CB: App receives element state change data
```

## 5. Layer-by-Layer Architecture

```mermaid
block-beta
    columns 1
    block:APP["APPLICATION LAYER"]:1
        columns 2
        HC["Host App<br/>mxcp_send_cmd()<br/>register EVT callbacks"]
        EC["Engine<br/>meshx_send_msg_to_app()<br/>ctrl callbacks"]
    end
    block:MXP["MXCP PROTOCOL LAYER"]:1
        columns 2
        CT["mxcp_send_cmd()<br/>mxcp_cmd_table[] dispatch<br/>typed payload structs"]
        ET["mxcp_send_event()<br/>mxcp_evt_table[] lookup<br/>typed payload structs"]
    end
    block:FRM["FRAME LAYER"]:1
        columns 2
        HB["Header build/parse<br/>[SOF][LEN][TYPE:8]<br/>bit7=DIR, bits6-0=ID"]
        CV["Checksum calc/verify<br/>[PAYLOAD][CHK][EOF]"]
    end
    block:TRN["TRANSPORT LAYER"]:1
        columns 2
        TXL["UART TX<br/>meshx_platform_serial_write()"]
        RXL["UART RX<br/>state machine<br/>mxcp_serial_parse_byte()"]
    end
    block:PHY["PHYSICAL LAYER"]:1
        columns 1
        W["UART / Serial Wire"]
    end

    APP --> MXP
    MXP --> FRM
    FRM --> TRN
    TRN --> PHY
```
