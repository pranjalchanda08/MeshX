# Page 18 — Web Console & USB CDC Multiplexing

> **[← Logical Models](./17_logical_models_testing.md)** | **[← Index](./README.md)** | **[Next: ELF Logging →](./19_elf_logging.md)**

---

## Overview

The MeshX Web Console provides a browser-based dashboard for monitoring, controlling, and testing MeshX nodes. It operates over the same USB CDC serial channel as the MXCP binary protocol by using a **dynamic stream multiplexer** that separates binary log packets, MXCP frames, and raw console text — all flowing over a single byte stream.

---

## 1. System Architecture

```mermaid
graph TD
    subgraph PC ["PC Tier"]
        UI["Vite + React UI Dashboard"]
        WS["FastAPI WebSocket Server"]
        Parser["Sliding-Window Stream Parser\n& ELF Log Resolver"]
    end

    subgraph ESP ["ESP32-C3 Tier"]
        Console["VFS Console Driver\n(USB CDC or UART)"]
        Router["Dynamic Platform Router\nesp_platform.c"]
        Shell["Interactive shell_task\nmeshx_shell.c"]
        RxTask["Binary mxsp_uart_rx_task"]
    end

    subgraph Mesh ["BLE Mesh Network"]
        UVP["UVP Engine"]
        Nodes["Remote BLE Nodes"]
    end

    UI <-->|WebSocket JSON| WS
    WS <-->|MXCP Frame over Serial| Parser
    Parser <-->|USB CDC byte stream| Console
    Console --> Router
    Router -->|Shell input| Shell
    Router -->|Binary frames| RxTask
    RxTask --> UVP
    UVP <-->|BLE Mesh air| Nodes
```

---

## 2. Dynamic Activation & Concurrency

When the host activates hosted mode, the shell task yields console ownership to the binary MXCP RX task. This prevents read collisions on the shared USB CDC port.

```mermaid
sequenceDiagram
    autonumber
    actor Dev as Developer / Web Console
    participant Shell as shell_task
    participant Platform as Platform Layer (esp_platform.c)
    participant RxTask as mxsp_uart_rx_task

    Note over Dev, Shell: Phase 1 — Boot (Debug Mode)
    Shell->>Shell: Poll console input byte-by-byte

    Dev->>Shell: "ut 8 1 1 1" (text CLI command)
    Shell->>Platform: meshx_platform_set_mxsp_use_console(true)
    Note over Platform: g_mxsp_use_console = true

    Note over Dev, RxTask: Phase 2 — Hosted Mode Initialization
    Dev->>Platform: Binary MXCP frame (0xFE 0x00 0x01 ...)
    Platform->>Platform: hosted_mode_enabled = true

    loop RX Collision Avoidance
        Shell->>Platform: get_mxsp_use_console() && is_hosted_mode_enabled()?
        Note over Shell: Both true → shell_task enters yield sleep loop
    end

    Note over Dev, RxTask: Phase 3 — Multiplexed Stream Active
    RxTask->>Platform: meshx_platform_serial_read()
    Platform-->>RxTask: Route to meshx_platform_console_read()
    Note over RxTask: Full ownership of console RX stream
```

---

## 3. Stream Demultiplexer (Host Side)

The PC-side `demux.py` parser handles three interleaved data types on the unified byte stream:

```mermaid
flowchart TD
    In["Incoming byte stream\n(USB CDC / UART)"]
    In --> ChkDE{"Buffer[0:2]\n== 0xDE 0xAD?"}
    ChkDE -->|Yes| TryLog["Parse TLV Log frame\ncheck XOR parity"]
    TryLog --> LogOK{"Parity\nvalid?"}
    LogOK -->|Yes| EmitLog["emit on_log(addr, args)\nadvance buffer"]
    LogOK -->|No| Slide1["Slide window +1 byte\nemit on_text(byte)"]

    ChkDE -->|No| ChkFE{"Buffer[0]\n== 0xFE?"}
    ChkFE -->|Yes| TryMXCP["Parse MXCP frame\ncheck CHK + 0xEF EOF"]
    TryMXCP --> MXCPok{"Frame\nvalid?"}
    MXCPok -->|Yes| EmitMXCP["emit on_mxsp(type, payload)\nadvance buffer"]
    MXCPok -->|No| Slide2["Slide window +1 byte\nemit on_text(byte)"]

    ChkFE -->|No| EmitText["emit on_text(byte)\nadvance buffer"]
```

### 3.1 Packet Signatures

| Data Type | SOF Signature | Verification |
|-----------|--------------|--------------|
| Binary TLV Log | `0xDE 0xAD` | XOR parity byte at end |
| MXCP Frame | `0xFE` | XOR checksum + `0xEF` EOF |
| Raw console text | (any other byte) | — (emitted directly) |

---

## 4. Serial Routing State Machine (Firmware)

```mermaid
stateDiagram-v2
    [*] --> UART1_Mode : Boot default

    UART1_Mode : MXCP routes to physical UART1
    UART1_Mode --> Console_Mode : ut 8 1 1 1\n(g_mxsp_use_console = true)

    Console_Mode : MXCP routes to USB CDC / Console
    Console_Mode --> UART1_Mode : ut 8 1 1 0\n(g_mxsp_use_console = false)

    Console_Mode --> Hosted_Active : Binary MXCP init frame received\n(hosted_mode_enabled = true)

    Hosted_Active : Shell yields RX\nRxTask owns console
    Hosted_Active --> Console_Mode : Hosted mode disabled\n(hosted_mode_enabled = false)
```

---

## 5. Console Channel Detection

The platform layer auto-detects whether USB CDC or physical UART is active:

```c
typedef enum {
    MESHX_PLATFORM_CONSOLE_CHANNEL_UART,
    MESHX_PLATFORM_CONSOLE_CHANNEL_USB_CDC,
} meshx_platform_console_channel_t;

meshx_platform_console_channel_t meshx_platform_get_console_channel(void);
```

This is used during startup to log which physical channel MXCP multiplexing is running over, and informs the web console which serial port to connect to.

---

## 6. Web Console UI Data Flow

```mermaid
sequenceDiagram
    participant UI as Browser UI
    participant WS as FastAPI WebSocket
    participant Demux as demux.py (parser)
    participant Serial as USB CDC Serial

    UI->>WS: JSON {"cmd": "el_send", "element_id": 1, ...}
    WS->>Demux: Build MXCP frame bytes
    Demux->>Serial: Write [0xFE][LEN][0x10][payload][CHK][0xEF]

    Serial-->>Demux: Binary byte stream
    Demux->>Demux: Feed bytes → parse frames
    alt MXCP Event (0x90 EL_DATA_NOTIFY)
        Demux->>WS: on_mxsp(type=0x90, payload)
        WS->>UI: JSON {"event": "el_notify", "element_id": 1, "state": 1}
    else Binary TLV Log (0xDEAD)
        Demux->>WS: on_log(elf_addr, args)
        WS->>UI: JSON {"event": "log", "msg": "Relay ON"}
    else Raw text
        Demux->>WS: on_text(bytes)
        WS->>UI: JSON {"event": "console", "text": "..."}
    end
```

---

> **[← Logical Models](./17_logical_models_testing.md)** | **[← Index](./README.md)** | **[Next: ELF Logging →](./19_elf_logging.md)**
