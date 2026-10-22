# Requirements Document: Zephyr Multi-Target Architecture (Option C)

## Introduction

This document specifies the requirements for expanding MeshX from a single-vendor ESP-IDF ecosystem to a portable multi-target architecture supporting the **Zephyr RTOS** ecosystem alongside ESP-IDF, following **Option C: Phased Multi-Target Architecture (RECOMMENDED)** defined in `docs/plans/zephyr_expansion_and_migration_risk_assessment.md`.

MeshX must function both as an ESP-IDF component and as a standard out-of-tree **Zephyr Module**, providing non-disruptive coexistence for existing production silicon (ESP32 / ESP32-C3) while unlocking native multi-vendor target support (Nordic nRF52/nRF53/nRF54, Silicon Labs, STM32, NXP) and modern Bluetooth Mesh 1.0.1/1.1 features.

## Glossary

- **OSAL**: Operating System Abstraction Layer (`port/os/`)
- **PAL**: Platform Abstraction Layer (`port/platform/`)
- **Zephyr Module**: An external git repository containing `zephyr/module.yml` recognized and built by Zephyr's CMake build system
- **West**: Zephyr's meta-tool for repository management, building (`west build`), and flashing (`west flash`)
- **Zephyr Settings**: Zephyr's native key-value storage subsystem (`subsys/settings`)
- **Zephyr NVS**: Non-Volatile Storage subsystem in Zephyr (`subsys/fs/nvs`)
- **MCUboot**: Secure, vendor-agnostic 32-bit microcontroller bootloader used by Zephyr
- **Devicetree (DTS)**: Hardware description hierarchy defining peripheral pins and properties
- **CRTP / Static Polymorphism**: Compile-time polymorphism avoiding runtime type information (`typeid`/`dynamic_cast`)

---

## Requirements

### Requirement 1: C++ Runtime & Toolchain Portability (Phase 0)

**User Story:** As a firmware engineer, I want MeshX's C++ codebase to compile cleanly under strict embedded toolchains (`-fno-rtti`, `-fno-exceptions`), so that models and elements run deterministically without runtime crashes on Cortex-M / RISC-V targets.

#### Acceptance Criteria

1. THE MeshX C++ models and elements SHALL compile cleanly with `-fno-rtti` and `-fno-exceptions`.
2. NO C++ source (`.cpp`) or header (`.hpp`) file SHALL invoke `dynamic_cast`, `typeid`, or throw/catch exceptions.
3. ALL `.cpp` and `.hpp` files SHALL route C header inclusions strictly through `meshx_c_header.h` per workspace coding guidelines; NO direct `.h` inclusions of C headers SHALL exist in C++ sources.
4. Static constructors in C++ models and elements SHALL NOT invoke OSAL or hardware primitives before kernel scheduler initialization.
5. Dynamic memory allocations in performance paths SHALL be eliminated or routed through `meshx_rtos_malloc` / `meshx_rtos_free`.

---

### Requirement 2: Zephyr Out-of-Tree Module Skeleton & CMake Integration (Phase 1)

**User Story:** As an embedded software developer, I want MeshX to be packaged as a recognized Zephyr module, so that any Zephyr application can import MeshX via `west.yml` without modifying MeshX internals.

#### Acceptance Criteria

1. THE MeshX repository SHALL provide a `zephyr/module.yml` file declaring module name `meshx` and root CMake entry points.
2. THE MeshX build system SHALL detect when running inside a Zephyr build environment (`ZEPHYR_BASE` defined) and configure include directories, compile definitions, and source sets using Zephyr's `zephyr_library()` / `zephyr_include_directories()` APIs.
3. THE ESP-IDF CMake build flow SHALL remain completely intact and functional with zero regressions when building via `meshx.py -b`.
4. THE Zephyr module configuration SHALL provide Kconfig definitions (`Kconfig.meshx`) allowing configuration of MeshX features, buffer sizes, and logging levels.

---

### Requirement 3: Zephyr Operating System Abstraction Layer (OSAL) (Phase 1)

**User Story:** As an OSAL consumer, I want MeshX task, message queue, semaphore, timer, and utility APIs to map onto Zephyr kernel primitives, so that core MeshX components operate identically across FreeRTOS and Zephyr.

#### Acceptance Criteria

1. THE Zephyr OSAL (`port/os/zephyr/`) SHALL implement `meshx_task_*` using `k_thread` and dynamic thread allocation.
2. THE Zephyr OSAL SHALL implement `meshx_msg_q_*` using `k_msgq` (supporting ISR context via `k_is_in_isr()`).
3. THE Zephyr OSAL SHALL implement `meshx_sem_*` using `k_sem` (supporting counting and binary semantics).
4. THE Zephyr OSAL SHALL implement `meshx_rtos_timer_*` using `k_timer` and workqueue-safe callback dispatch.
5. THE Zephyr OSAL SHALL implement `meshx_rtos_utils.h` using `k_uptime_get_32()` for time and `k_malloc()` / `k_free()` for heap operations.
6. THE OSAL implementation SHALL distinguish ISR execution context using `k_is_in_isr()` and execute non-blocking ISR-safe primitives when called from interrupt context.

---

### Requirement 4: BLE Mesh Platform Adaptation & Storage (Phase 2)

**User Story:** As a MeshX protocol architect, I want MeshX model structures and element bindings to map to Zephyr's native Bluetooth Mesh stack (`subsys/bluetooth/mesh`) and Zephyr Settings, so that MeshX leverages upstream Bluetooth Mesh without duplicating storage or callback overhead.

#### Acceptance Criteria

1. THE platform layer (`port/platform/zephyr/ble_mesh/`) SHALL map `MESHX_MODEL`, `MESHX_ELEMENT`, and `MESHX_COMPOSITION` directly to Zephyr's `struct bt_mesh_model`, `struct bt_mesh_elem`, and `struct bt_mesh_comp`.
2. THE platform layer SHALL bind MeshX UVP vendor model opcodes directly to Zephyr `bt_mesh_model_op` handlers.
3. Persistent mesh state (keys, IV index, sequence numbers, subscriptions) SHALL be handled natively by Zephyr Settings (`CONFIG_BT_SETTINGS=y`).
4. THE MeshX NVS API (`meshx_nvs_*`) on Zephyr builds SHALL map directly onto Zephyr Settings (`settings_runtime` / `settings_save_one`) or Zephyr NVS (`nvs_read` / `nvs_write`), completely eliminating redundant FAL wear-leveling duplication.

---

### Requirement 5: Tooling & CLI Integration (`meshx.py` & Devicetree) (Phase 3)

**User Story:** As a developer using Windows or Linux, I want `meshx.py` to seamlessly orchestrate Zephyr builds and flash operations, so that I can develop for Nordic or STM32 targets using the familiar `meshx.py` CLI workflow.

#### Acceptance Criteria

1. `tools/scripts/meshx.py` SHALL implement a `ZephyrTarget(Target)` subclass supporting `-H zephyr`.
2. WHEN `-H zephyr` is supplied, `meshx.py` SHALL invoke `west build` with appropriate board (`-b <board>`), pristine flags, and build directory structure.
3. `meshx.py` SHALL support flashing via `west flash` supporting configurable runners (e.g., `pyocd`, `jlink`, `nrfjprog`).
4. `tools/scripts/code_gen.py` SHALL support generating Zephyr Devicetree overlays (`app.overlay`) mapping logical pins from `prod_profile.yml` to Devicetree GPIO aliases.
5. Native Windows environment validation SHALL detect `ZEPHYR_BASE` and verify toolchain availability without requiring WSL.

---

### Out of Scope: Gateway & Connectivity Realization (Phase 4)

> [!NOTE]
> Phase 4 (Gateway & Connectivity Realization: Binary MXCP over MQTT, MCUboot stream OTA, mDNS responder) is explicitly **out of scope** for this specification and will be addressed in a dedicated Gateway Feature Request (FR).

