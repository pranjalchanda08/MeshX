# Technical Reference Design: Dynamic USB CDC Multiplexing Index

This document serves as the master architectural reference and index for the **Dynamic USB CDC Multiplexing & Premium Web-Console** feature. The complete technical implementation spec is divided into five logical design pages to ensure comprehensive coverage of each tier.

---

## 1. System Architecture Diagram

The system comprises three physical tiers linked via a single unified serial stream over USB CDC or physical UART.

```mermaid
graph TD
    subgraph PC_Tier ["1. PC Client & Gateway Tier"]
        UI["Vite + React UI Dashboard (Glassmorphic)"]
        WS["FastAPI WebSocket Server"]
        Parser["Sliding-Window Stream Parser & ELF Resolver"]
    end

    subgraph Platform_Tier ["2. Platform & OS Tier (ESP32)"]
        Console["VFS Console Driver (JTAG/CDC or UART)"]
        Router["Dynamic Platform Router (esp_platform.c)"]
        Shell["Interactive Text shell_task (meshx_shell.c)"]
        MXSP_RX["Binary mxsp_uart_rx_task"]
    end

    subgraph BLE_Mesh_Tier ["3. Bluetooth Mesh Network"]
        UVP["Unified Vendor Protocol Engine"]
        Radio["Target BLE Nodes"]
    end

    %% Downstream flow
    UI -->|WebSocket JSON Events| WS
    WS -->|MXSP Frame over Serial| Console
    Console -->|Dynamic Abstraction Router| Router

    %% Upstream flow & Routing
    Router -->|1. Shell Input Yielded| Shell
    Router -->|2. Exclusive Read| MXSP_RX
    MXSP_RX -->|UVP Opcode Broadcast| UVP
    UVP -->|Over-the-air Radio| Radio
```

---

## 2. Dynamic Activation & Concurrency Sequence

The dynamic transition from interactive shell mode to Hosted multiplexed mode, avoiding read collisions on the unified console port, operates as follows:

```mermaid
sequenceDiagram
    autonumber
    actor Dev as Developer / Web Console
    participant Shell as Interactive shell_task
    participant Plt as Platform Layer (esp_platform.c)
    participant RxTask as mxsp_uart_rx_task

    Note over Dev, Shell: [Phase 1: Boot Debug Mode]
    Note over Shell: Polls console input byte-by-byte
    Dev->>Shell: Send CLI Command: "ut 8 1 1 1" [REQ-002]
    Shell->>Plt: meshx_platform_set_mxsp_use_console(true) [REQ-001]
    Note over Plt: Routing state updated (g_mxsp_use_console = True)

    Note over Dev, RxTask: [Phase 2: Hosted Mode Initialization]
    Dev->>Plt: Send binary MXSP Hosted Init Frame (0xFE 0x03 0x01 ...)
    Note over Plt: Parser sets hosted_mode_enabled = True

    loop RX Collision Avoidance [REQ-003]
        Shell->>Plt: Check state: get_mxsp_use_console() && is_hosted_mode_enabled()
        Note over Shell: Both are True: shell_task enters yield sleep loop
    end

    Note over Dev, RxTask: [Phase 3: Multiplexed Stream Active]
    RxTask->>Plt: meshx_platform_serial_read()
    Plt-->>RxTask: Route directly to meshx_platform_console_read()
    Note over RxTask: Full ownership of console RX stream
```

---

## 3. Logical Design Pages Reference

To review the specific low-level implementations, APIs, and algorithms, navigate to the respective design pages:

| Logical Component | Design Sheet | Addressed Requirements |
| :--- | :--- | :--- |
| **Platform Routing Layer** | [01_platform_routing.md](./design/01_platform_routing.md) | REQ-001, REQ-002, REQ-007 |
| **Concurrency Coordination** | [02_shell_concurrency.md](./design/02_shell_concurrency.md) | REQ-003 |
| **Legacy Decommissioning** | [03_decommissioning.md](./design/03_decommissioning.md) | REQ-008 |
| **Host Stream Parser** | [04_host_demux.md](./design/04_host_demux.md) | REQ-004, REQ-005 |
| **Premium UI/UX Dashboard** | [05_web_console_ui.md](./design/05_web_console_ui.md) | REQ-006 |
