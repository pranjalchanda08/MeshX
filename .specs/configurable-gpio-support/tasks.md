# Implementation Plan: Configurable GPIO Support

## Overview

This implementation plan converts the GPIO subsystem design into discrete coding tasks for incremental implementation. The GPIO subsystem provides configurable GPIO support for MeshX BLE Mesh nodes with YAML-based configuration, platform abstraction, interrupt handling, PWM support, and KV Engine persistence.

The implementation follows MeshX architecture patterns: C interfaces for platform abstraction, C++ wrapper classes for element integration, and strict C/C++ boundary separation. GPIO configuration is defined in `prod_profile.yml`, validated at build time, and compiled into firmware with runtime flexibility for hosted/non-hosted mode switching.

**Important Updates Based on Design Refinement:**
1. **IO Layer Location**: IO abstraction is in `meshx/io/` directory (not in `ble_mesh/`)
2. **Abstract IO Interface**: Unified abstract IO class with function-based API instead of separate GPIO/PWM classes
3. **KV Engine Integration**: Uses `meshx_kv_engine.c` APIs for persistence (not native ESP-IDF NVS APIs)
4. **Function-based API**: Single abstract method with function flags and argument vectors for extensibility

## Tasks

### Phase 1: Core Infrastructure (Weeks 1-3)

- [x] 1. Set up GPIO infrastructure and configuration system
  - Create GPIO interface directory structure in `interface/gpio/`
  - Extend `prod_profile.yml` schema for GPIO configuration with function-based API support
  - Update `code_gen.py` to parse and validate GPIO YAML configuration
  - Generate GPIO configuration macros in `meshx_config.h`
  - _Requirements: 1.1-1.10, 6.3, 7.5-7.6, 8.4_

- [x] 2. Implement core GPIO C interfaces
  - [x] 2.1 Create GPIO interface headers (`meshx_gpio.h`, `meshx_gpio_types.h`)
    - Define GPIO enums (mode, pull, drive strength, interrupt types, IoFunction types)
    - Define core API functions (init, set_level, get_level, toggle, execute_function)
    - Add GPIO error codes to `meshx_err.h`
    - _Requirements: 2.1-2.10, 8.1-8.3_

  - [x] 2.2 Write property test for GPIO interface validation
    - **Property 2: Mode-Aware GPIO Operation Validity**
    - **Validates: Requirements 2.1-2.5, 2.8-2.10, 8.1-8.3, 8.9-8.10**

  - [x] 2.3 Create PWM interface header (`meshx_pwm.h`)
    - Define PWM API functions (init, start, stop, set_duty_cycle, set_frequency)
    - Add PWM error codes to `meshx_err.h`
    - _Requirements: 4.1-4.10_

  - [x] 2.4 Write property test for PWM subsystem correctness
    - **Property 4: PWM Subsystem Correctness**
    - **Validates: Requirements 4.1-4.10**

- [x] 3. Checkpoint - Validate interface definitions
  - Ensure all tests pass, ask the user if questions arise.

### Phase 2: Platform Abstraction & BSP (Weeks 4-6)

- [x] 4. Implement platform abstraction layer and BSP implementations
  - [x] 4.1 Create GPIO platform interface (`meshx_gpio_platform.h`)
    - Define platform-specific function signatures
    - Follow existing RTOS/NVS abstraction patterns
    - Include function-based API support
    - _Requirements: 6.1-6.8, 6.10_

  - [x] 4.2 Implement ESP-IDF GPIO BSP (`port/platform/esp/esp_idf/gpio/`)
    - Create `meshx_gpio_esp.c` using ESP-IDF GPIO API
    - Create `meshx_pwm_esp.c` using ESP-IDF LEDC API
    - Handle ESP32-specific constraints and pin mappings
    - Implement function-based API dispatch
    - _Requirements: 6.1-6.8_

  - [x]* 4.3 Write property test for BSP abstraction
    - **Property 8: BSP Abstraction and Platform Compatibility**
    - **Validates: Requirements 6.1-6.8, 6.10, 12.9**

  - [x] 4.4 Create BSP GPIO mapping files for supported boards
    - Create `gpio_pin_map.h` and `gpio_constraints.h` for xiao_c3, weact_c3, esp32_devkitC
    - Define logical to physical pin mappings per BSP
    - _Requirements: 6.1-6.8_

### Phase 3: Runtime Subsystem (Weeks 7-9)

- [ ] 5. Implement GPIO runtime subsystem
  - [x] 5.1 Create core GPIO runtime implementation (`src/gpio/meshx_gpio.c`)
    - Implement GPIO API functions with validation
    - Maintain pin state tracking and runtime validation
    - Handle logical to physical pin mapping
    - Implement function-based API dispatch
    - _Requirements: 2.1-2.10, 8.1-8.3_

  - [x] 5.2 Create PWM runtime implementation (`src/gpio/meshx_pwm.c`)
    - Implement PWM API functions with parameter validation
    - Handle hardware channel allocation and management
    - Maintain PWM state for runtime control
    - _Requirements: 4.1-4.10_

  - [ ]* 5.3 Write property test for runtime subsystem
    - **Property 1: Comprehensive Configuration Validation and Generation**
    - **Validates: Requirements 1.1-1.10, 6.3, 7.5-7.6, 8.4**

  - [x] 5.4 Implement interrupt subsystem
    - Add interrupt registration, enabling, and callback handling
    - Integrate with RTOS for task priorities and stack sizes
    - Handle interrupt nesting and reentrancy protection
    - _Requirements: 3.1-3.10, 8.7_

- [x] 6. Checkpoint - Validate runtime implementation
  - Ensure all tests pass, ask the user if questions arise.

### Phase 4: Abstract IO Interface & Element Integration (Weeks 10-12)

- [ ] 7. Implement abstract IO interface in `meshx/io/` directory
  - [x] 7.1 Create abstract IO interface (`meshx/io/inc/meshx_io_interface.hpp`)
    - Define `MeshXIoInterface` abstract base class with `execute()` method
    - Implement function-based API with `IoFunction` enum and argument vectors
    - Support extensible function types including `CUSTOM_FUNCTION`
    - _Requirements: 7.1-7.4, 7.8-7.9, 12.8_

  - [x] 7.2 Create concrete IO implementations
    - Create `meshx/io/src/meshx_gpio_impl.cpp` for GPIO operations
    - Create `meshx/io/src/meshx_pwm_impl.cpp` for PWM operations
    - Implement factory pattern for creating IO instances
    - _Requirements: 7.1-7.4, 7.8-7.9_

  - [x] 7.3 Create IO factory (`meshx/io/inc/meshx_io_factory.hpp`)
    - Implement factory for creating IO instances based on configuration
    - Support runtime creation of GPIO, PWM, and custom IO types
    - _Requirements: 7.1-7.4, 7.8-7.9_

  - [x]* 7.4 Write property test for abstract IO interface
    - **Property 6: Element-IO Integration Consistency**
    - **Validates: Requirements 7.1-7.4, 7.8-7.9, 12.8**

- [ ] 8. Implement element-IO integration
  - [x] 8.1 Update existing element types for IO integration
    - Add IO binding to relay, light, and sensor elements
    - Update element state change handlers to use `execute()` method
    - Follow existing element patterns with `std::unique_ptr` ownership
    - _Requirements: 7.1-7.4, 7.8-7.9_

  - [x] 8.2 Create IO bridge for C/C++ boundary
    - Create `meshx/io/interface/meshx_io_bridge.h`
    - Bridge between C GPIO API and C++ IO interface
    - Follow MeshX C/C++ boundary patterns
    - _Requirements: 7.1-7.4, 7.8-7.9_

### Phase 5: Hosted/Non-Hosted Mode Support (Weeks 13-15)

- [x] 9. Implement hosted/non-hosted mode support
  - [x] 9.1 Add hosted mode API TLV command handling
    - Implement mode switching via API command
    - Handle pin state transitions during mode switches
    - Maintain backward compatibility with existing hosted mode
    - _Requirements: 5.1-5.10, 7.7_

  - [x] 9.2 Implement UART transport for hosted mode GPIO events
    - Serialize GPIO events for transmission to host MCU
    - Handle event deserialization and processing in hosted mode
    - Make GPIO binding optional for elements in hosted mode
    - _Requirements: 5.1-5.10_

  - [ ]* 9.3 Write property test for mode transition safety
    - **Property 5: Hosted/Non-Hosted Mode Transition Safety**
    - **Validates: Requirements 5.1-5.10, 7.7**

- [x] 10. Checkpoint - Validate mode switching functionality
  - Ensure all tests pass, ask the user if questions arise.

### Phase 6: KV Engine Persistence (Weeks 16-17)

- [x] 11. Implement KV Engine persistence for GPIO configuration
  - [x] 11.1 Create GPIO configuration serialization format compatible with KV Engine
    - Define binary serialization structures with versioning
    - Include CRC16 checksum compatible with KV Engine's CRC16
    - Use product-specific key prefixes for KV Engine storage
    - _Requirements: 13.1-13.15, 12.5_

  - [x] 11.2 Implement KV Engine persistence API using `meshx_kv_engine_*` functions
    - Create `meshx_gpio_kv.c` using `meshx_kv_engine_init()`, `meshx_kv_engine_set()`, `meshx_kv_engine_commit()`
    - Implement save/load functions for GPIO configuration
    - Handle corruption fallback to compiled defaults
    - _Requirements: 13.1-13.15_

  - [ ]* 11.3 Write property test for KV Engine persistence
    - **Property 7: KV Engine Configuration Persistence and Integrity**
    - **Validates: Requirements 13.1-13.15, 12.5**

  - [x] 11.4 Implement configuration export/import functionality
    - Add API for exporting GPIO configuration to serialized format
    - Add API for importing configuration from serialized format
    - Support migration between devices and configuration versions
    - _Requirements: 13.14_

### Phase 7: Testing & Validation (Weeks 18-20)

- [ ] 12. Implement testing and validation infrastructure
  - [x] 12.1 Extend unit test harness for GPIO testing
    - Add GPIO test cases to `unit_test` component
    - Test all API functions with valid and invalid inputs
    - Test error conditions and recovery strategies
    - Test function-based API with various argument vectors
    - _Requirements: 10.1-10.4_

  - [x] 12.2 Create integration tests for GPIO subsystem
    - Test GPIO with element integration using abstract IO interface
    - Test hosted/non-hosted mode switching
    - Test NVS persistence and recovery using KV Engine
    - _Requirements: 10.5-10.8_

  - [~] 12.3 Extend autotest framework for GPIO validation
    - Add serial-based automated tests for GPIO functionality
    - Test on all supported BSPs (xiao_c3, weact_c3, esp32_devkitC)
    - Measure performance metrics (timing, power consumption)
    - _Requirements: 10.9-10.10_

### Phase 8: Final Integration (Weeks 21-22)

- [ ] 13. Final integration and system testing
  - [~] 13.1 Integrate GPIO subsystem into `meshx_init()` sequence
    - Add GPIO initialization to main initialization flow
    - Ensure proper initialization order with other subsystems
    - Handle GPIO deinitialization during shutdown
    - _Requirements: 2.9-2.10, 4.9-4.10_

  - [~] 13.2 Test complete system integration
    - Test GPIO with BLE mesh stack and other subsystems
    - Verify OTA update compatibility with GPIO configuration persistence using KV Engine
    - Test performance and resource usage meets requirements
    - _Requirements: 9.1-9.10, 12.1-12.10_

  - [~] 13.3 Create documentation and examples
    - Document YAML GPIO configuration schema with examples
    - Create API reference for all GPIO functions including function-based API
    - Provide step-by-step guides for common GPIO tasks
    - Document abstract IO interface usage patterns
    - _Requirements: 11.1-11.10_

- [~] 14. Final checkpoint - Complete system validation
  - Ensure all tests pass, ask the user if questions arise.

## Notes

- Tasks marked with `*` are optional and can be skipped for faster MVP
- Each task references specific requirements for traceability
- Checkpoints ensure incremental validation
- Property tests validate universal correctness properties from design document
- Unit tests validate specific examples and edge cases
- Follow MeshX C/C++ boundary patterns: include C headers via `meshx_c_header.h`
- Use existing RTOS and NVS abstraction patterns for platform independence
- **KV Engine Integration**: GPIO configuration persistence uses MeshX KV Engine (`meshx_kv_engine.c`) APIs, not native ESP-IDF NVS APIs
- **IO Layer Location**: IO abstraction is in `meshx/io/` directory (not in `ble_mesh/`)
- **Abstract IO Interface**: Use unified abstract IO class with function-based API instead of separate GPIO/PWM classes
- **Function-based API**: Single abstract `execute()` method with `IoFunction` enum and argument vectors for extensibility
- BSP implementations reside in `port/bsp/<board>/gpio/` directories
- Configuration is generated from `prod_profile.yml` via `code_gen.py`
- **BUILD VALIDATION**: After completing any code changes, run build verification:
  ```sh
  source tools/scripts/env.sh source /run/media/pchanda/workspace/wsl/esp/v5.4/esp-idf/export.sh
  ./tools/scripts/meshx.py -B xiao_c3 -N all_in_one -bc
  ```
  All compilation errors must be resolved before marking tasks complete.

## Task Dependency Graph

```json
{
  "waves": [
    { "id": 0, "tasks": ["1.1"] },
    { "id": 1, "tasks": ["2.1", "2.3", "4.1"] },
    { "id": 2, "tasks": ["2.2", "2.4", "4.2", "4.4"] },
    { "id": 3, "tasks": ["4.3", "5.1", "5.2"] },
    { "id": 4, "tasks": ["5.3", "5.4", "7.1"] },
    { "id": 5, "tasks": ["7.2", "7.3", "7.4", "8.1"] },
    { "id": 6, "tasks": ["8.2", "9.1", "9.2"] },
    { "id": 7, "tasks": ["9.3", "11.1", "11.2"] },
    { "id": 8, "tasks": ["11.3", "11.4", "12.1"] },
    { "id": 9, "tasks": ["12.2", "12.3", "13.1"] },
    { "id": 10, "tasks": ["13.2", "13.3"] }
  ]
}
```

## Key Implementation Details

### Abstract IO Interface Design

The abstract IO interface in `meshx/io/` provides a unified API for all IO operations:

```cpp
// meshx/io/inc/meshx_io_interface.hpp
class MeshXIoInterface {
public:
    virtual ~MeshXIoInterface() = default;

    // Function-based API for extensibility
    virtual meshx_err_t execute(IoFunction function,
                                const std::vector<uint32_t>& args) = 0;

    // Convenience methods (implemented in terms of execute())
    virtual meshx_err_t setLevel(uint8_t level);
    virtual meshx_err_t getLevel(uint8_t* level);
    virtual meshx_err_t toggle();
};

// IoFunction enum for extensible function types
enum class IoFunction {
    SET_LEVEL = 0,
    GET_LEVEL,
    TOGGLE,
    SET_PWM_DUTY,
    SET_PWM_FREQUENCY,
    REGISTER_INTERRUPT,
    CUSTOM_FUNCTION  // For future extensibility
};
```

### KV Engine Integration Pattern

GPIO configuration persistence uses the MeshX KV Engine APIs:

```c
// Example: Saving GPIO configuration to KV Engine
meshx_err_t meshx_gpio_save_config_to_kv(const char* product_name) {
    char key[64];
    snprintf(key, sizeof(key), "gpio_%s_config", product_name);

    // Serialize configuration
    uint8_t buffer[512];
    uint16_t size = serialize_gpio_config(buffer, sizeof(buffer));

    // Use KV Engine APIs (not native ESP-IDF NVS)
    meshx_err_t err = meshx_kv_engine_set(key, buffer, size);
    if (err != MESHX_SUCCESS) return err;

    return meshx_kv_engine_commit();  // Transactional commit
}

// Example: Loading GPIO configuration from KV Engine
meshx_err_t meshx_gpio_load_config_from_kv(const char* product_name) {
    char key[64];
    snprintf(key, sizeof(key), "gpio_%s_config", product_name);

    uint8_t buffer[512];
    uint16_t size = sizeof(buffer);

    // Use KV Engine APIs
    meshx_err_t err = meshx_kv_engine_read(key, buffer, &size);
    if (err == MESHX_SUCCESS) {
        return deserialize_gpio_config(buffer, size);
    }

    // Fall back to compiled defaults if not found
    return MESHX_NOT_FOUND;
}
```

### YAML Configuration Schema

The `prod_profile.yml` schema includes function-based configuration:

```yaml
gpio:
  pins:
    - name: "RELAY_1"
      logical_pin: 0
      physical_pin: 4
      mode: "output"
      functions:
        - type: "set_level"
          args: [0]  # Initial level
        - type: "custom_function"
          id: 123
          args: [100, 200]  # Custom function arguments

    - name: "LED_PWM"
      logical_pin: 1
      physical_pin: 5
      mode: "pwm_output"
      functions:
        - type: "set_pwm_duty"
          args: [50]  # 50% duty cycle
        - type: "set_pwm_frequency"
          args: [1000]  # 1kHz frequency
```

### File Structure

```
main/component/meshx/io/                    # Abstract IO interface layer
├── inc/
│   ├── meshx_io_interface.hpp              # Abstract IO interface
│   ├── meshx_io_factory.hpp                # IO factory
│   └── meshx_io_types.hpp                  # IO type definitions
├── src/
│   ├── meshx_io_interface.cpp              # Abstract interface implementation
│   ├── meshx_gpio_impl.cpp                 # Concrete GPIO implementation
│   ├── meshx_pwm_impl.cpp                  # Concrete PWM implementation
│   └── meshx_io_factory.cpp                # Factory implementation
└── interface/
    ├── meshx_io_platform.h                 # IO platform interface (C)
    └── meshx_io_bridge.h                   # C/C++ bridge header

interface/gpio/                             # C interface layer
├── meshx_gpio.h
├── meshx_gpio_types.h
├── meshx_pwm.h
└── meshx_gpio_platform.h

port/bsp/<board>/gpio/                      # BSP implementations
├── meshx_gpio_bsp.c
├── meshx_pwm_bsp.c
├── gpio_pin_map.h
└── gpio_constraints.h

port/platform/esp/esp_idf/gpio/             # ESP-IDF platform implementation
├── meshx_gpio_esp.c
└── meshx_pwm_esp.c
```
