# Requirements: persistant-serialised-config

This document captures the requirements for the `persistant-serialised-config` feature.

| ID | Title | Description | Acceptance Criteria | Priority |
|----|-------|-------------|---------------------|----------|
| REQ-001 | Serialized Configuration Format | Provide a way to serialize MeshX configurations (typically defined in `prod_profile.json`) into a format suitable for persistent storage. | Configuration data can be converted from JSON to a compact, serialized representation. | P0 |
| REQ-002 | Dedicated Read-Only Storage Partition | Use a separate memory section (partition) from the standard MeshX persistent storage. This section should behave like an "OTP region" (Read-Only during normal operation). | A distinct flash partition is defined and used. Normal operations do not overwrite this section. | P0 |
| REQ-003 | Compilation-Time Binary Generation | Tooling must integrate into the build system to generate the binary configuration image during the build process. | The binary image is automatically generated as part of the standard build flow (e.g., CMake). | P0 |
| REQ-004 | Boot-Time Configuration Initialization | At system boot, the firmware must read the serialized configuration from the dedicated memory section and use it to initialize models and system functionalities. | MeshX models and features are correctly configured based on the data read from the partition during boot. | P0 |
| REQ-005 | Graceful Feature Initialization | MeshX core must gracefully handle configuration for features that might be disabled or missing in the current firmware build (e.g., using stubs), preventing system crashes. | Initialization fails gracefully with appropriate logging if a configured feature is not present in the firmware. | P0 |
| REQ-006 | Forward Compatibility for OTA | The serialized configuration format and initialization logic must support firmware upgrades (OTA) without requiring re-flashing or modifying the OEM configuration binary. | A firmware update does not break compatibility with an existing configuration partition. | P0 |
| REQ-007 | Flashing and Erasing Support in `meshx.py` | The `meshx.py` tool must be updated to support flashing and erasing the dedicated configuration partition (and any associated sections) independently of the main firmware. | `meshx.py` can target the configuration partition for flash and erase operations specifically. | P1 |
