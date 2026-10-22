# Architecture & Design Document: Zephyr Multi-Target Architecture (Option C)

## 1. Architectural Philosophy & Strategy

The primary architectural goal is to expand MeshX from a single-vendor ESP-IDF framework into a **portable, multi-target BLE Mesh framework** without disrupting current production firmware, automated HIL testing, or developer workflows.

Per the strategic analysis in `docs/plans/zephyr_expansion_and_migration_risk_assessment.md`, **Option C: Phased Multi-Target Architecture** establishes MeshX as:
1. **Core Portable Framework:** Vendor-neutral protocol models, element routing, and application interfaces in `main/component/meshx/`.
2. **ESP-IDF Target Port:** Preserved intact under `port/platform/esp/` and `port/os/FreeRTOS/`.
3. **Zephyr Out-of-Tree Module:** Packaged via `zephyr/module.yml` with native OSAL under `port/os/zephyr/` and platform adapter under `port/platform/zephyr/`.
4. **Unified Developer Experience:** Cohesive orchestration in `tools/scripts/meshx.py` supporting both `-H esp` and `-H zephyr`.

```
+-------------------------------------------------------------------------+
|                       MeshX Application & Models                        |
|   (Element Dispatcher, Logical Models, Base Classes, Builder APIs)      |
+------------------------------------+------------------------------------+
                                     |
                                     v
+------------------------------------+------------------------------------+
|                  MeshX OSAL & Platform Interfaces                       |
|   (meshx_task, meshx_msg_q, meshx_sem, meshx_rtos_timer, meshx_nvs)     |
+-----------------+-----------------------------------+-------------------+
                  |                                   |
                  v                                   v
+-----------------+-----------------+   +-------------+-------------------+
|            ESP-IDF Port           |   |            Zephyr Port          |
| - OSAL: FreeRTOS                  |   | - OSAL: Zephyr Kernel (k_thread)|
| - BLE Mesh: esp_ble_mesh          |   | - BLE Mesh: subsys/bluetooth    |
| - Storage: NVS Flash              |   | - Storage: Zephyr Settings/NVS  |
| - Bootloader: 2-stage ESP         |   | - Bootloader: MCUboot           |
+-----------------------------------+   +---------------------------------+
```

---

## 2. Directory Layout & Module Structure

```
MeshX/
├── zephyr/
│   ├── module.yml                    # Zephyr out-of-tree module definition
│   ├── CMakeLists.txt                # Zephyr module build entry
│   └── Kconfig                       # Zephyr module Kconfig symbols
├── port/
│   ├── os/
│   │   ├── FreeRTOS/                 # Existing FreeRTOS OSAL
│   │   ├── ThreadX/
│   │   └── zephyr/                   # NEW: Zephyr kernel OSAL
│   │       ├── meshx_task.c          # Mapped to k_thread
│   │       ├── meshx_msg_q.c         # Mapped to k_msgq
│   │       ├── meshx_sem.c           # Mapped to k_sem
│   │       ├── meshx_timer.c         # Mapped to k_timer
│   │       └── meshx_utils.c         # Mapped to k_uptime & k_malloc
│   ├── platform/
│   │   ├── esp/                      # Existing ESP-IDF platform port
│   │   └── zephyr/                   # NEW: Zephyr platform port
│   │       ├── ble_mesh/
│   │       │   ├── inc/
│   │       │   │   └── meshx_platform_ble_mesh.h # Mapped to bt_mesh_*
│   │       │   └── src/
│   │       │       ├── meshx_plat_ble_mesh.c
│   │       │       └── meshx_plat_ven_srv.c
│   │       ├── nvs/
│   │       │   └── meshx_nvs_plat_zephyr.c       # Mapped to Zephyr Settings
│   │       └── zephyr.cmake
│   └── bsp/
│       ├── weact_c3/                 # Existing ESP32-C3 board
│       └── nrf52840dk/               # Reference Zephyr target board
├── tools/
│   └── scripts/
│       ├── meshx.py                  # Added ZephyrTarget CLI integration
│       └── code_gen.py               # Added Devicetree overlay generation
└── .specs/
    └── zephyr-multi-target-architecture/
```

---

## 3. C++ Runtime Isolation & Header Audit (Phase 0)

Zephyr projects typically compile with `-fno-rtti` and `-fno-exceptions`. To guarantee seamless cross-compilation across Cortex-M, RISC-V, and Xtensa:

### 3.1 Coding Rule Enforcement
1. **Zero RTTI & Exceptions:** No `dynamic_cast`, no `typeid`, no `throw`, no `catch`.
2. **C Header Discipline:** All `.cpp` and `.hpp` files MUST include C headers only through `meshx_c_header.h`. Direct `#include <*.h>` in C++ files is strictly prohibited.
3. **Static Constructor Safety:** Model classes must not perform hardware or OSAL calls during static construction; initialization is deferred to element/model `.init()` lifecycles.

---

## 4. Zephyr OSAL Implementation Details (Phase 1)

### 4.1 Task Management (`meshx_task.c`)
- Uses `k_thread_create()` with a heap-allocated `struct k_thread` and stack buffer (`k_malloc()`).
- Supports `k_thread_abort()`, `k_thread_suspend()`, and `k_thread_resume()`.
- Sleep and delay map to `k_msleep(delay_ms)`.

### 4.2 Message Queue (`meshx_msg_q.c`)
- Uses Zephyr's dynamic message queue API: `k_msgq_alloc_init(msgq, msg_size, max_msgs)`.
- Cleanup invokes `k_msgq_cleanup()`.
- In ISR context (`k_is_in_isr()`), timeouts are clamped to `K_NO_WAIT`.
- Peeking uses `k_msgq_peek()`. Count uses `k_msgq_num_used_get()`.

### 4.3 Semaphore (`meshx_sem.c`)
- Uses `k_sem_init(sem, initial_count, limit_count)`.
- Take uses `k_sem_take(sem, K_MSEC(delay_ms))`; in ISR context, `K_NO_WAIT` is enforced.
- Give uses `k_sem_give(sem)`.

### 4.4 RTOS Timer (`meshx_timer.c`)
- Uses `k_timer_init()`, `k_timer_start()`, `k_timer_stop()`.
- Zephyr timer expiry runs in ISR context. The timer callback schedules work on the system workqueue (`k_system_work_q`) if user callbacks require thread context.

### 4.5 RTOS Utilities (`meshx_utils.c`)
- System time: `*millis = k_uptime_get_32()`.
- Memory: `k_malloc()`, `k_calloc()`, and `k_free()` backed by `CONFIG_HEAP_MEM_POOL_SIZE`.
- Task ID: `*task_id = (uint32_t)(uintptr_t)k_current_get()`.

---

## 5. BLE Mesh Platform Adaptation & Storage (Phase 2)

### 5.1 BLE Mesh Type Mapping (`meshx_platform_ble_mesh.h`)
In `port/platform/zephyr/ble_mesh/inc/meshx_platform_ble_mesh.h`:
```c
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/mesh.h>

#define MESHX_COMPOSITION       struct bt_mesh_comp
#define MESHX_ELEMENT           struct bt_mesh_elem
#define MESHX_MODEL             struct bt_mesh_model
#define MESHX_MODEL_PUB         struct bt_mesh_model_pub
```

### 5.2 Storage Subsystem Mapping (`meshx_nvs_plat_zephyr.c`)
- Avoid introducing a redundant Flash Abstraction Layer (FAL) wear-leveling engine.
- Bluetooth Mesh internal state is managed directly by `CONFIG_BT_SETTINGS=y`.
- MeshX application-level NVS calls (`meshx_nvs_set_*`, `meshx_nvs_get_*`) are routed directly to Zephyr Settings key-value pairs (`settings_save_one()` / `settings_runtime`) or Zephyr NVS (`subsys/fs/nvs`).

---

## 6. Tooling Integration (`meshx.py` & Devicetree) (Phase 3)

### 6.1 `ZephyrTarget` in `meshx.py`
```python
class ZephyrTarget(Target):
    def __init__(self, port, baud=115200):
        super().__init__("zephyr", port, baud)

    def toolcheck(self):
        check_tool_installed("west")
        check_tool_installed("cmake")
        check_tool_installed("ninja")

    def configure(self, build_root, args=None):
        build_dir = os.path.join(build_root, args.prod_name[0])
        subprocess.run(["west", "build", "-t", "menuconfig", "-d", build_dir], check=True)

    def build(self, args, build_root="build"):
        build_dir = os.path.join(build_root, args.prod_name[0])
        board = args.bsp  # e.g., nrf52840dk/nrf52840
        cmd = ["west", "build", "-b", board, "-d", build_dir, "-p", "auto", "."]
        subprocess.run(cmd, check=True)

    def flash(self, args, build_root="build"):
        build_dir = os.path.join(build_root, args.prod_name[0])
        cmd = ["west", "flash", "-d", build_dir]
        subprocess.run(cmd, check=True)
```

### 6.2 Devicetree Generation in `code_gen.py`
- Emits `build/<prod>/app.overlay` when target host is Zephyr.
- Maps GPIO entries from `prod_profile.yml` into Devicetree aliases and node configurations.

---

## 7. Out of Scope

> [!NOTE]
> **Phase 4: Gateway & Connectivity Realization** (Binary MXCP over Zephyr MQTT client, MCUboot dual-slot stream OTA flashing, tuned mDNS service responder) is explicitly **out of scope** for this feature specification. It will be implemented as part of a dedicated Gateway Feature Request (FR).
