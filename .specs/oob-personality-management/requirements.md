# Requirements: Dynamic OOB Personality Management

This document captures the requirements for the `oob-personality-management` feature.

| ID | Title | Description | Acceptance Criteria | Priority |
| :--- | :--- | :--- | :--- | :--- |
| **REQ-001** | NVS Composition Schema Storage | Store the runtime element composition schema (counts, element types, and CRC) in the MeshX NVS store. | A validated `meshx_nvs_comp_schema_t` blob is persisted to NVS whenever the dynamic builder commits. | P0 |
| **REQ-002** | Autonomous Boot Reconstruction | On cold boot, if no factory RO flash partition is present, reconstruct the composition table from NVS before `meshx_element_init()`. | Node boots into its previously configured element layout without requiring host rebuild commands. | P0 |
| **REQ-003** | Fallback & CRC Guarding | If the NVS schema is corrupted or CRC mismatch occurs, safely reject the record and fall back to default root element. | Corruption does not crash the device; warnings are logged and default configuration is booted. | P0 |
| **REQ-004** | Remote Composition Configuration (MXCP) | Introduce `MXCP_CMD_SET_COMPOSITION` allowing external MCUs or gateways to configure element counts and types over UART. | Host can configure arbitrary combinations of elements up to `CONFIG_MAX_ELEMENT_COUNT`. | P0 |
| **REQ-005** | Controlled Persist-and-Reset Lifecycle | Because the BLE Mesh stack cannot hot-swap elements, applying a new composition via MXCP triggers an ACK response followed by an automatic graceful reboot. | Device returns `MXCP_EVT_SET_COMPOSITION_RSP` and restarts after a configurable delay to bake new layout. | P0 |
| **REQ-006** | Factory Node Reset Cleanup | Performing a node reset (via BLE Mesh Config Server or local reset) must clear the NVS composition schema. | Subsequent boot starts in pristine unprovisioned state with default personality. | P1 |
| **REQ-007** | Application Telemetry Completeness | Expand application data callback in `main.c` to handle all UVP logical models (Sensor Server/Client and RGB HSL Server/Client). | Inbound telemetry and state changes for all catalog models are handled without unhandled warnings. | P1 |
