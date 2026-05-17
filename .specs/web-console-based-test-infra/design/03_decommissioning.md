# Design Page 03: Decommissioning Obsolete Subsystem UT Commands

This page outlines the decommission plan, codebase purification, and file removal checklist to eradicate obsolete GPIO/PWM unit test commands while preserving the core CLI unit test infrastructure for routing control.

---

## 1. Decommissioning Rationale

*   **Subsystem Status:** Subsystem tests (GPIO toggling, PWM sweep validation, dynamic integration checks) inside the `unit_test` component are obsolete.
*   **Verification Strategy:** In the modern MeshX architecture, all functional peripheral verification is driven dynamically using Hosted Mode binary commands and MXSP telemetry packages. This is significantly more precise, avoids compile-time code bloat, and aligns with production hardware testing pipelines.
*   **Safety Isolation:** We retain `unit_test.c` and its core registration mechanism to handle dynamic port switching (e.g. `ut 8 1 1 <0|1>`), but completely purge the underlying sub-test components.

---

## 2. Codebase Purification File Checklist

The following 14 obsolete source and header files will be permanently decommissioned and deleted from the workspace during the implementation phase:

### 2.1 Source Directory (`main/component/unit_test/src/`)
*   `gpio_unit_test.c` (GPIO peripheral baseline validations)
*   `gpio_property_test.c` (Property-based constraint validations)
*   `gpio_integration_test.c` (GPIO-event subsystem integrations)
*   `pwm_property_test.c` (PWM registers sweep validations)
*   `gpio_platform_property_test.c` (Platform-specific hardware boundary checks)
*   `gpio_integration_property_test.c` (GPIO-interrupt loop property validations)
*   `gpio_test_registry.c` (Legacy test list registry)

### 2.2 Include Directory (`main/component/unit_test/inc/`)
*   `gpio_unit_test.h`
*   `gpio_property_test.h`
*   `gpio_integration_test.h`
*   `pwm_property_test.h`
*   `gpio_platform_property_test.h`
*   `gpio_integration_property_test.h`
*   `gpio_test_registry.h`

---

## 3. Entrypoint Refactoring (`unit_test.c`)

We refactor `unit_test.c` to sever references to the decommissioned code blocks.

```diff
- #include "gpio_test_registry.h"

  meshx_err_t init_unit_test_console() {
      meshx_err_t err = meshx_shell_init();
      if (err != MESHX_SUCCESS) {
          return err;
      }
  
      err = register_ut_command();
      if (err != MESHX_SUCCESS) {
          return err;
      }
  
-     // Register all GPIO subsystem tests
-     err = register_all_gpio_tests();
-     if (err != MESHX_SUCCESS) {
-         return err;
-     }
  
+     // Register routing control under Module ID Common (8)
+     err = register_unit_test(MODULE_ID_COMMON, common_ut_callback);
+     if (err != MESHX_SUCCESS) {
+         return err;
+     }

      return meshx_shell_start();
  }
```

---

## 4. Build Safety Verification

*   **CMake Compliance:** Because `/home/pchanda/MeshX/main/CMakeLists.txt` uses `file(GLOB UNIT_TEST_SRC ...)` to list files under `unit_test/src/`, removing files from the filesystem automatically deletes them from subsequent compilation units without requiring complex file filter alterations.
*   **Compile Test:** A full clean compile (`rm -rf build && idf.py build`) must be executed post-purification to guarantee no orphaned symbol headers remain in references.
