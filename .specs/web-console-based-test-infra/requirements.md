# Functional and Non-Functional Requirements: Dynamic USB CDC Multiplexing

This document outlines the detailed requirements for implementing dynamic USB CDC multiplexing on the MeshX platform. These requirements serve as the foundation for design and implementation stages under the **Requirement First** specs workflow.

---

## 1. Functional Requirements

### REQ-001: Dynamic Runtime Serial Routing Abstraction
*   **Description:** The platform layer must abstract serial read and write operations to allow routing the binary MeshX Serial Protocol (MXSP) stream dynamically between the physical `UART1` port and the designated active BSP log/console channel (either USB CDC or standard UART).
*   **Acceptance Criteria:** 
    *   Introduce state variable `g_mxsp_use_console` in platform drivers.
    *   `meshx_platform_serial_write` must dynamically write to UART1 (if false) or the active console channel (if true).
    *   `meshx_platform_serial_read` must dynamically read from UART1 (if false) or the active console channel (if true).
*   **Priority:** P0

### REQ-002: CLI Unit Test Command Interface
*   **Description:** Provide a text-based command under the existing Unit Test (`ut`) CLI engine to toggle the active serial transport interface at runtime.
*   **Acceptance Criteria:** 
    *   Register a command handler under `MODULE_ID_COMMON` (`0x08`) with Command ID `1`.
    *   Executing `ut 8 1 1 1` dynamically switches MXSP routing to the active console log channel.
    *   Executing `ut 8 1 1 0` dynamically switches MXSP routing back to physical `UART1`.
*   **Priority:** P0

### REQ-007: BSP Logging/Console Channel Query API
*   **Description:** The platform abstraction layer must expose a query API to detect and return which physical channel the active BSP is using for logging/console output (e.g. USB CDC vs. Standard UART).
*   **Acceptance Criteria:**
    *   Define `meshx_platform_console_channel_t` enumeration supporting `MESHX_PLATFORM_CONSOLE_CHANNEL_UART` and `MESHX_PLATFORM_CONSOLE_CHANNEL_USB_CDC`.
    *   Expose `meshx_platform_get_console_channel(void)` in `meshx_platform.h`.
    *   ESP platform implementation must return the channel type based on defined config symbols (`CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG` vs `CONFIG_ESP_CONSOLE_UART`).
    *   Dynamic routing initialization must query this API to log diagnostic info specifying whether multiplexing runs over USB CDC or standard UART.
*   **Priority:** P0


### REQ-003: Shell RX Collision Prevention
*   **Description:** Prevent competing reads on the USB CDC console port between the interactive text `shell_task` and the binary `mxsp_uart_rx_task`.
*   **Acceptance Criteria:**
    *   `shell_task` must evaluate the current routing state and stack mode.
    *   If `g_mxsp_use_console` is active AND `hosted_mode_enabled` is true, the shell task must yield console input reading completely, leaving the channel clear for binary frames.
    *   If hosted mode is deactivated, shell input reading must instantly resume, enabling fallback CLI recovery.
*   **Priority:** P0

### REQ-004: Multi-Stage Stream Demultiplexing on Host
*   **Description:** The PC-side gateway worker must uniquely distinguish and demultiplex dynamic TLV logs, MXSP frames, and raw text output over the unified serial channel.
*   **Acceptance Criteria:**
    *   Extract TLV logs matching sync signature `0xDEAD` and validate parity byte.
    *   Extract MXSP frames matching SOF `0xFE`, valid enum types, and EOF `0xEF`.
    *   Verify frame checksums to validate event integrity.
    *   Route fallback text characters to the console standard out.
*   **Priority:** P0

### REQ-008: Decommission Obsolete Subsystem UT Command Registrations
*   **Description:** Clean up the codebase and reduce compile-time code bloat by decommissioning all obsolete legacy GPIO and PWM unit and property test source/header files. All future peripheral subsystem validation will be conducted dynamically via Hosted Mode binary MXSP packets rather than CLI macros.
*   **Acceptance Criteria:**
    *   Delete obsolete source files (`gpio_unit_test.c`, `gpio_property_test.c`, `gpio_integration_test.c`, `pwm_property_test.c`, `gpio_platform_property_test.c`, `gpio_integration_property_test.c`, `gpio_test_registry.c`) from `main/component/unit_test/src/`.
    *   Delete obsolete header files (`gpio_integration_property_test.h`, `gpio_integration_test.h`, `gpio_platform_property_test.h`, `gpio_property_test.h`, `gpio_test_registry.h`, `gpio_unit_test.h`, `pwm_property_test.h`) from `main/component/unit_test/inc/`.
    *   Update `unit_test.c` to prune the `#include "gpio_test_registry.h"` statement and remove the call to `register_all_gpio_tests()` inside `init_unit_test_console()`.
    *   Retain `unit_test.c` and the core `ut` shell infrastructure to handle our dynamic serial switching commands.
*   **Priority:** P0

---


## 2. Non-Functional Requirements

### REQ-005: Zero-Overhead Fail-Safe Recovery (Self-Healing)
*   **Description:** The stream parser must automatically recover and resynchronize without dropping the connection if incomplete packets are received due to board resets or serial glitches.
*   **Acceptance Criteria:**
    *   A sliding-window parsing cursor must discard corrupt start bytes and advance 1-byte at a time when validation checks fail.
*   **Priority:** P1

### REQ-006: Multi-Instance Premium Web-Console UI/UX
*   **Description:** The user interface must support simultaneous testing of multiple nodes (e.g. Node A on COM3, Node B on COM4) from a single computer via isolated, responsive browser windows or tabs, adhering to the standard three-tier Web-Console Architecture.
*   **Acceptance Criteria:**
    *   **Visual Design System (Glassmorphic Dark Mode):** 
        *   Implement a premium dark-themed interface (`#0B0F19` background) using a CSS Glassmorphic design (`backdrop-filter: blur(12px)`, semi-transparent panels).
        *   Provide color-coded neon accents for node states (e.g., active green for ON, amber for color temperature, dynamic purple and HSL wheel pickers).
        *   Incorporate smooth micro-animations for real-time status transitions.
    *   **Telemetry Dashboard Layout & Components:**
        *   **Live Mesh Topography (Node Grid):** Render active card widgets for each discovered node, displaying element types (Config, Relay, CWWW, HSL) and bound controls (sliders, toggles).
        *   **GPIO Status Pipeline:** Provide real-time high-fidelity pin diagrams indicating digital state (HIGH/LOW), pin direction (IN/OUT), and analog duty cycle meters (PWM %).
        *   **Packet Dissector Timeline:** Stream and dissect outbound/inbound binary frames (identifying SOF, length, type, checksum, EOF, and raw bytes) in a scrollable diagnostic timeline.
        *   **Virtualized Decoded Log Terminal:** Stream ELF-decoded logging frames dynamically into a high-performance virtual list. Support level filters (Error, Warn, Info, Debug, Verbose) and tag coloring.
    *   **Parameterized Multi-Port Routing Middleware:**
        *   Expose parameterized FastAPI REST API endpoints and WebSocket channels (e.g. `/ws/events?port=/dev/ttyUSB0`) to isolate port traffic.
        *   Spin up active asynchronous serial worker loops per COM port without lock contention or thread collisions.
        *   Implement local state caching on the host server to hydrate new browser instances instantly on connection.
*   **Priority:** P1


