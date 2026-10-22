# Implementation Plan: Zephyr Multi-Target Architecture (Option C)

## Overview

This implementation plan organizes the tasks required to implement **Option C: Phased Multi-Target Architecture** from `docs/plans/zephyr_expansion_and_migration_risk_assessment.md`. It establishes MeshX as a portable framework capable of targeting both ESP-IDF and Zephyr RTOS without disrupting existing ESP32-C3 builds.

> [!NOTE]
> Per user specification, **Phase 4: Gateway & Connectivity Realization** is explicitly **out of scope** for this plan and deferred to a dedicated Gateway Feature Request (FR).

---

## Tasks

### Phase 0: Prerequisites & Architectural Clean-Up (Weeks 1-2)

- [ ] 1. Enforce C++ runtime and C header inclusion discipline
  - [x] 1.1 Fix direct C header inclusions in C++ files per workspace coding guidelines
    - Update `meshx_uvp_logical_model.hpp`, `meshx_uvp_logical_models.hpp`, `meshx_uvp_model.hpp`
    - Update `meshx_uvp_logical_models.cpp`, `meshx_uvp_element.cpp`
    - Verify all C headers route strictly through `meshx_c_header.h`
    - _Requirements: 1.3_
  - [x] 1.2 Audit codebase for dynamic casting, RTTI, and exception handling
    - Verify zero `dynamic_cast`, `typeid`, or throw/catch keywords in `.cpp`/`.hpp`
    - _Requirements: 1.1, 1.2_
  - [ ] 1.3 Verify ESP-IDF clean build after header clean-up
    - Ensure zero regressions to existing build and tests
    - _Requirements: 1.1, 2.3_

---

### Phase 1: Zephyr Module Skeleton & OSAL Port (Weeks 3-4)

- [x] 2. Establish Zephyr Module skeleton and build infrastructure
  - [x] 2.1 Create `zephyr/module.yml` declaring `meshx` module name and CMake settings
    - _Requirements: 2.1, 2.2_
  - [x] 2.2 Create `zephyr/CMakeLists.txt` and `zephyr/Kconfig` for out-of-tree module integration
    - _Requirements: 2.1, 2.4_
  - [x] 2.3 Update `port/os/osal.cmake` to support `OS=zephyr`
    - _Requirements: 2.2, 3.1_

- [x] 3. Implement Zephyr OSAL (`port/os/zephyr/`)
  - [x] 3.1 Implement `port/os/zephyr/meshx_task.c`
    - Map `meshx_task_create`, `meshx_task_delete`, `meshx_task_suspend`, `meshx_task_resume`, `meshx_task_delay` to `k_thread` and `k_msleep`
    - _Requirements: 3.1, 3.6_
  - [x] 3.2 Implement `port/os/zephyr/meshx_msg_q.c`
    - Map `meshx_msg_q_*` to `k_msgq_alloc_init`, `k_msgq_put`, `k_msgq_get`, `k_msgq_peek`
    - Support ISR-safe checks via `k_is_in_isr()`
    - _Requirements: 3.2, 3.6_
  - [x] 3.3 Implement `port/os/zephyr/meshx_sem.c`
    - Map `meshx_sem_*` to `k_sem_init`, `k_sem_take`, `k_sem_give`
    - _Requirements: 3.3, 3.6_
  - [x] 3.4 Implement `port/os/zephyr/meshx_timer.c`
    - Map `meshx_rtos_timer_*` to `k_timer_init`, `k_timer_start`, `k_timer_stop`
    - _Requirements: 3.4_
  - [x] 3.5 Implement `port/os/zephyr/meshx_utils.c`
    - Map `meshx_rtos_get_sys_time` to `k_uptime_get_32()`
    - Map `meshx_rtos_malloc`, `meshx_rtos_calloc`, `meshx_rtos_free` to `k_malloc`, `k_calloc`, `k_free`
    - Map `meshx_rtos_get_curr_task_id_prio` to `k_current_get()`
    - _Requirements: 3.5_

---

### Phase 2: BLE Mesh Subsystem & Storage Mapping (Weeks 5-6)

- [x] 4. Create Zephyr BLE Mesh Platform Abstraction (`port/platform/zephyr/ble_mesh/`)
  - [x] 4.1 Define `port/platform/zephyr/ble_mesh/inc/meshx_platform_ble_mesh.h`
    - Map `MESHX_MODEL`, `MESHX_ELEMENT`, `MESHX_COMPOSITION` to `struct bt_mesh_model`, `struct bt_mesh_elem`, `struct bt_mesh_comp`
    - _Requirements: 4.1_
  - [x] 4.2 Implement platform vendor model opcode binding and initialization
    - Implement `meshx_plat_ven_srv_init`, `meshx_plat_bind_uvp_opcodes`, `meshx_plat_model_sub_add`, `meshx_plat_model_sub_del`
    - _Requirements: 4.2_
  - [x] 4.3 Implement Zephyr persistent storage adapter (`port/platform/zephyr/nvs/meshx_nvs_plat_zephyr.c`)
    - Map `meshx_nvs_*` to Zephyr Settings / Zephyr NVS without duplicate FAL
    - _Requirements: 4.3, 4.4_

---

### Phase 3: Tooling & Hardware Configuration Integration (Week 7)

- [x] 5. CLI Tooling & Devicetree generation
  - [x] 5.1 Implement `ZephyrTarget` in `tools/scripts/meshx.py`
    - Support `-H zephyr` CLI flag with `west build`, `west flash`, `west config`
    - _Requirements: 5.1, 5.2, 5.3_
  - [x] 5.2 Extend `tools/scripts/code_gen.py` for Devicetree overlay emission
    - Generate `app.overlay` from `prod_profile.yml` logical GPIO configurations
    - _Requirements: 5.4_
  - [x] 5.3 Provide Zephyr workspace environment helper
    - Script or tooling setup instructions for native Windows workspace
    - _Requirements: 5.5_

---

### Phase 4: Gateway & Connectivity Realization (DEFERRED / OUT OF SCOPE)

> [!NOTE]
> Phase 4 (Binary MXCP over Zephyr MQTT client, MCUboot dual-slot stream OTA flashing, tuned mDNS responder) is deferred to a dedicated Gateway Feature Request (FR).
