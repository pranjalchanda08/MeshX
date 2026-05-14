# Requirements Document: Configurable GPIO Support

## Introduction

This document specifies the requirements for configurable GPIO (General Purpose Input/Output) support in MeshX, a portable BLE Mesh node stack for embedded microcontrollers. The GPIO subsystem enables product developers to define GPIO pin assignments, modes, and behaviors through the existing `prod_profile.yml` configuration system, supporting both hosted and non-hosted deployment modes.

The system provides a unified, platform-abstracted API for digital I/O, interrupts, and PWM (Pulse Width Modulation) while maintaining MeshX's strict separation between hardware abstraction and application logic. GPIO configuration is defined in YAML, validated at build time, and compiled into firmware, ensuring runtime safety and determinism.

## Glossary

- **GPIO**: General Purpose Input/Output - programmable digital pins on a microcontroller
- **Logical Pin**: Abstract pin identifier (0-255) used by application code
- **Physical Pin**: Actual hardware GPIO pin number specific to a BSP
- **BSP**: Board Support Package - hardware-specific implementation layer
- **Hosted Mode**: MeshX runs on a separate MCU/CPU communicating with a host MCU via UART transport
- **Non-Hosted Mode**: MeshX runs directly on the application MCU with direct GPIO access
- **TLV**: Type-Length-Value - binary message format used in MeshX API commands
- **PWM**: Pulse Width Modulation - technique for controlling analog devices with digital signals
- **ISR**: Interrupt Service Routine - function called when a GPIO interrupt occurs
- **Element**: MeshX component that manages state and coordinates models, potentially with GPIO bindings
- **prod_profile.yml**: YAML configuration file defining product composition and GPIO settings

## Requirements

### Requirement 1: GPIO Configuration System

**User Story:** As a product developer, I want to define GPIO pin configurations in YAML, so that I can easily configure hardware interfaces without modifying source code.

#### Acceptance Criteria

1. WHEN a developer defines GPIO pins in `prod_profile.yml`, THE Code_Generator SHALL parse and validate the configuration at build time
2. WHEN GPIO configuration contains invalid values, THE Code_Generator SHALL fail the build with descriptive error messages
3. THE GPIO_Configuration SHALL support logical pin numbers (0-255) that map to physical BSP-specific pins
4. THE GPIO_Configuration SHALL support multiple pin modes: input, output, open-drain output, input/output, and open-drain input/output
5. THE GPIO_Configuration SHALL support pull resistor settings: none, pull-up, pull-down, and pull-up/down
6. THE GPIO_Configuration SHALL support drive strength settings: weak, medium, strong, and maximum strong
7. THE GPIO_Configuration SHALL support signal inversion for active-low devices
8. THE GPIO_Configuration SHALL support initial output level specification for output pins
9. THE GPIO_Configuration SHALL generate compile-time configuration macros in `meshx_config.h`
10. THE GPIO_Configuration SHALL generate static initialization data structures for runtime use

### Requirement 2: GPIO Runtime API

**User Story:** As an element developer, I want a unified GPIO API to control hardware pins, so that my code works across different BSPs without modification.

#### Acceptance Criteria

1. THE GPIO_API SHALL provide functions to set, get, and toggle GPIO pin levels
2. THE GPIO_API SHALL validate pin numbers and modes before performing operations
3. THE GPIO_API SHALL return standardized error codes for invalid operations
4. WHEN a pin is configured as input, THE GPIO_API SHALL allow reading its current level
5. WHEN a pin is configured as output, THE GPIO_API SHALL allow setting its level
6. THE GPIO_API SHALL be implemented in C for platform abstraction
7. THE GPIO_API SHALL have C++ wrapper classes for type-safe element integration
8. THE GPIO_API SHALL maintain pin state tracking for runtime validation
9. THE GPIO_API SHALL initialize all configured pins during system startup
10. THE GPIO_API SHALL deinitialize pins and free resources during system shutdown

### Requirement 3: GPIO Interrupt Support

**User Story:** As a developer creating interactive devices, I want to handle GPIO interrupts for buttons and sensors, so that I can respond immediately to hardware events.

#### Acceptance Criteria

1. THE GPIO_Configuration SHALL support interrupt trigger types: disabled, positive edge, negative edge, any edge, low level, and high level
2. THE GPIO_API SHALL allow registration of interrupt callback functions with user context data
3. THE GPIO_API SHALL enable and disable interrupts for specific pins
4. THE GPIO_API SHALL unregister interrupt handlers when no longer needed
5. WHEN an interrupt occurs, THE GPIO_Subsystem SHALL invoke the registered callback with the logical pin number
6. THE GPIO_Subsystem SHALL handle interrupt nesting and priority according to RTOS capabilities
7. THE GPIO_Subsystem SHALL provide interrupt task configuration (priority, stack size) in YAML
8. THE GPIO_Subsystem SHALL protect against interrupt handler reentrancy issues
9. THE GPIO_Subsystem SHALL log interrupt events for debugging purposes
10. THE GPIO_Subsystem SHALL handle interrupt errors gracefully without system crashes

### Requirement 4: PWM Support

**User Story:** As a developer creating lighting or motor control products, I want PWM support for dimmable LEDs and motor speed control, so that I can create smooth analog-like effects.

#### Acceptance Criteria

1. THE GPIO_Configuration SHALL support PWM pin definitions with frequency, duty cycle, and resolution
2. THE PWM_API SHALL initialize PWM subsystems based on YAML configuration
3. THE PWM_API SHALL start and stop PWM output on configured pins
4. THE PWM_API SHALL set and get PWM duty cycle (0-100%)
5. THE PWM_API SHALL set and get PWM frequency (in Hz)
6. THE PWM_API SHALL support PWM signal inversion for active-low devices
7. THE PWM_API SHALL validate PWM parameters (frequency range, duty cycle limits)
8. THE PWM_API SHALL handle hardware PWM channel allocation and management
9. THE PWM_API SHALL deinitialize PWM subsystems and free resources during shutdown
10. THE PWM_Subsystem SHALL maintain PWM state for runtime control and monitoring

### Requirement 5: Hosted and Non-Hosted Mode Support

**User Story:** As a system architect, I want MeshX to support both hosted and non-hosted deployment modes, so that I can choose the appropriate architecture for my product requirements.

#### Acceptance Criteria

1. WHEN MeshX receives the Hosted Mode Enable API TLV command, THE System SHALL switch to hosted mode operation
2. WHEN in hosted mode, THE GPIO_Binding SHALL be optional for elements
3. WHEN in hosted mode and GPIO is not bound, THE Element SHALL send GPIO events to the host via UART transport
4. WHEN in non-hosted mode, THE Element SHALL generally bind to GPIO pins for direct hardware control
5. THE Mode_Selection SHALL be dynamic via API command, not compile-time configuration
6. WHEN the hosted mode API TLV is received at any time, THE System SHALL begin operating in hosted mode
7. THE GPIO_Subsystem SHALL detect the current mode and adapt its behavior accordingly
8. WHEN switching modes, THE GPIO_Subsystem SHALL handle pin state transitions safely
9. THE System SHALL maintain backward compatibility with existing hosted mode implementations
10. THE Documentation SHALL clearly explain the differences between hosted and non-hosted GPIO usage

### Requirement 6: BSP Abstraction Layer

**User Story:** As a board support package developer, I want a clean abstraction layer for GPIO implementations, so that I can support new hardware without modifying core MeshX code.

#### Acceptance Criteria

1. THE BSP_GPIO_Implementation SHALL reside in `port/bsp/<board>/gpio/` directories
2. THE BSP_GPIO_Implementation SHALL implement platform-specific GPIO operations
3. THE BSP_GPIO_Implementation SHALL map logical pins to physical pins using generated configuration
4. THE Platform_Abstraction_Layer SHALL provide a unified interface to BSP implementations
5. THE Platform_Abstraction_Layer SHALL select the appropriate BSP implementation at compile time
6. THE BSP_GPIO_Implementation SHALL use the underlying SDK's GPIO APIs (ESP-IDF, etc.)
7. THE BSP_GPIO_Implementation SHALL handle board-specific constraints and limitations
8. THE BSP_GPIO_Implementation SHALL be tested on all supported BSPs (xiao_c3, weact_c3, esp32_devkitC)
9. THE BSP_Template SHALL provide a starting point for new BSP implementations
10. THE Documentation SHALL provide guidelines for adding GPIO support to new BSPs

### Requirement 7: Element-GPIO Integration

**User Story:** As an element developer, I want clean integration between elements and GPIO pins, so that I can control hardware while maintaining separation of concerns.

#### Acceptance Criteria

1. THE Element_GPIO_Bridge SHALL provide C++ wrapper classes around the C GPIO API
2. THE Element_GPIO_Bridge SHALL use logical pin references, not physical pin numbers
3. WHEN an element state changes, THE Element SHALL update bound GPIO pins through the bridge
4. WHEN a GPIO interrupt occurs, THE Element SHALL receive notifications through registered callbacks
5. THE Element_Configuration SHALL reference GPIO pins by logical name from YAML
6. THE Code_Generator SHALL validate element-GPIO bindings at build time
7. WHEN in hosted mode without GPIO binding, THE Element SHALL serialize events for UART transport
8. THE Element_GPIO_Integration SHALL follow MeshX's C/C++ boundary patterns
9. THE Element_GPIO_Integration SHALL support existing element types (relay, light, sensor)
10. THE Documentation SHALL provide examples of element-GPIO integration patterns

### Requirement 8: Error Handling and Safety

**User Story:** As a safety-conscious developer, I want robust error handling and safety features in the GPIO subsystem, so that my products are reliable and safe to use.

#### Acceptance Criteria

1. THE GPIO_API SHALL return specific error codes for different failure conditions
2. WHEN an invalid pin number is provided, THE GPIO_API SHALL return MESHX_ERR_GPIO_INVALID_PIN
3. WHEN an operation is attempted on a pin with wrong mode, THE GPIO_API SHALL return MESHX_ERR_GPIO_INVALID_MODE
4. THE GPIO_Subsystem SHALL validate all configuration at build time to prevent runtime errors
5. THE GPIO_Subsystem SHALL include electrical safety considerations in documentation
6. THE GPIO_Subsystem SHALL recommend current limiting and ESD protection in hardware designs
7. THE Interrupt_Handlers SHALL run with appropriate RTOS priorities to avoid blocking critical functions
8. THE GPIO_Subsystem SHALL integrate with system watchdog to prevent hangs
9. WHEN errors occur, THE GPIO_Subsystem SHALL log warnings and continue operation when possible
10. THE GPIO_Subsystem SHALL provide error recovery strategies for common failure scenarios

### Requirement 9: Performance and Resource Management

**User Story:** As an embedded developer working with constrained resources, I want efficient GPIO operations with minimal memory and CPU overhead, so that my application has resources for other tasks.

#### Acceptance Criteria

1. THE GPIO_Read_Write_Operations SHALL complete in less than 10 microseconds
2. THE Interrupt_Latency SHALL be less than 50 microseconds
3. THE PWM_Update_Operations SHALL complete in less than 100 microseconds
4. THE GPIO_Initialization SHALL complete in less than 5 milliseconds
5. THE Static_Configuration_Memory SHALL use approximately 4 bytes per GPIO pin
6. THE Runtime_State_Memory SHALL use approximately 8 bytes per GPIO pin
7. THE Interrupt_Handler_Memory SHALL use approximately 16 bytes per interrupt registration
8. THE PWM_Channel_Memory SHALL use approximately 32 bytes per PWM channel
9. THE Total_Memory_Estimate SHALL be approximately 1KB for typical configurations
10. THE Power_Consumption SHALL be minimized for input pins and configurable for output pins

### Requirement 10: Testing and Validation

**User Story:** As a quality assurance engineer, I want comprehensive testing for the GPIO subsystem, so that I can ensure reliable operation across all supported hardware.

#### Acceptance Criteria

1. THE Unit_Tests SHALL validate all GPIO API functions with valid and invalid inputs
2. THE Unit_Tests SHALL test each GPIO mode (input, output, open-drain, etc.)
3. THE Unit_Tests SHALL test interrupt registration, triggering, and handling
4. THE Unit_Tests SHALL test PWM frequency, duty cycle, and start/stop operations
5. THE Integration_Tests SHALL test GPIO on each supported BSP (xiao_c3, weact_c3, esp32_devkitC)
6. THE Integration_Tests SHALL test GPIO usage from elements (relay, light, sensor)
7. THE Integration_Tests SHALL test YAML configuration parsing and code generation
8. THE Integration_Tests SHALL test GPIO with other MeshX subsystems (BLE, NVS, logging)
9. THE Hardware_Tests SHALL verify each configured pin behaves as expected on actual hardware
10. THE Hardware_Tests SHALL measure performance metrics (timing, power consumption)

### Requirement 11: Documentation and Examples

**User Story:** As a new MeshX developer, I want clear documentation and working examples for GPIO configuration, so that I can quickly understand and use the system.

#### Acceptance Criteria

1. THE Documentation SHALL explain the YAML GPIO configuration schema with examples
2. THE Documentation SHALL provide API reference for all GPIO functions
3. THE Documentation SHALL explain the differences between hosted and non-hosted modes
4. THE Documentation SHALL provide step-by-step guides for common GPIO tasks
5. THE Examples SHALL include complete product profiles with GPIO configurations
6. THE Examples SHALL demonstrate element-GPIO integration patterns
7. THE Examples SHALL show interrupt handling for button inputs
8. THE Examples SHALL show PWM control for dimmable LEDs
9. THE Documentation SHALL include troubleshooting guides for common GPIO issues
10. THE Documentation SHALL be generated from source code comments using Doxygen

### Requirement 12: Future Compatibility and Extensibility

**User Story:** As a forward-thinking architect, I want the GPIO subsystem to be extensible for future features, so that MeshX can evolve with new hardware capabilities.

#### Acceptance Criteria

1. THE GPIO_Architecture SHALL accommodate future ADC (Analog-to-Digital Converter) support
2. THE GPIO_Architecture SHALL accommodate future DAC (Digital-to-Analog Converter) support
3. THE GPIO_Architecture SHALL accommodate future touch sensor support
4. THE GPIO_Architecture SHALL support dynamic power management for unused pins
5. THE GPIO_Architecture SHALL support state persistence across reboots
6. THE GPIO_Architecture SHALL support remote configuration via BLE mesh
7. THE GPIO_API SHALL maintain backward compatibility across minor versions
8. THE GPIO_Configuration_Schema SHALL be extensible for new pin modes and features
9. THE GPIO_Subsystem SHALL support non-ESP32 platforms through the abstraction layer
10. THE Migration_Path SHALL be documented for future enhancements


### Requirement 13: NVS Persistence for GPIO Configuration

**User Story:** As a product maintainer, I want GPIO configuration to be stored in NVS (Non-Volatile Storage) in a serialized format, so that product-specific IO configurations are preserved during OTA (Over-The-Air) updates of the core MeshX stack.

#### Acceptance Criteria

1. THE GPIO_Configuration SHALL be stored in NVS flash memory in a serialized binary format
2. FOR EACH product, THE NVS SHALL have a dedicated section/namespace for GPIO configuration storage
3. THE GPIO_Configuration_Data SHALL include all pin definitions, modes, pull settings, and initial states
4. WHEN the system boots, THE GPIO_Subsystem SHALL read configuration from NVS if available, otherwise use compiled defaults
5. THE NVS_Storage SHALL use wear-leveling to prevent flash memory degradation
6. THE GPIO_Configuration SHALL be versioned to support migration between different configuration formats
7. WHEN OTA update occurs, THE NVS_GPIO_Section SHALL remain intact and unaffected by core stack updates
8. THE GPIO_Subsystem SHALL provide API functions to read and write GPIO configuration to/from NVS
9. THE Serialized_Format SHALL be compact and efficient to minimize flash usage
10. THE NVS_Storage SHALL include CRC (Cyclic Redundancy Check) or checksum for data integrity validation
11. WHEN NVS data is corrupted, THE GPIO_Subsystem SHALL fall back to compiled defaults and log error
12. THE GPIO_Configuration SHALL support dynamic updates at runtime via API while preserving in NVS
13. THE NVS_Storage SHALL be compatible with existing MeshX NVS abstraction layer
14. THE GPIO_Configuration SHALL be exportable/importable between devices via serialized format
15. THE Documentation SHALL explain NVS storage layout and migration procedures for GPIO configuration


## GitNexus Analysis of Current MeshX Codebase

### **Current State Analysis (Based on GitNexus MCP Investigation)**

1. **No Existing GPIO Abstraction**: The MeshX codebase currently has **no GPIO abstraction layer**. The only GPIO-related code found is UART pin definitions in `port/platform/esp/esp_idf/utils/esp_platform.c` (CONFIG_MXSP_UART_TX_PIN, CONFIG_MXSP_UART_RX_PIN).

2. **Well-Established Abstraction Patterns**: MeshX has strong, proven abstraction patterns:
   - **RTOS Abstraction**: Complete interface in `main/component/meshx/interface/rtos/` (tasks, semaphores, timers, message queues)
   - **NVS Abstraction**: Complete storage interface in `main/component/meshx/interface/utils/meshx_nvs_interface.h`
   - **Platform Abstraction**: Interface defined in `main/component/meshx/interface/meshx_platform.h`

3. **Configuration System**:
   - **YAML-based**: `prod_profile.yml` defines product compositions (found in each BSP directory)
   - **Code Generation**: `tools/scripts/code_gen.py` generates `meshx_config.h` with compile-time configuration
   - **BSP-specific**: Each BSP (`port/bsp/xiao_c3/`, `port/bsp/weact_c3/`, `port/bsp/esp32_devkitC/`) has its own configuration

4. **NVS Storage System**:
   - **Complete Implementation**: NVS abstraction with platform-specific implementations in `port/platform/esp/esp_idf/utils/meshx_nvs_plat_esp.c`
   - **Key-Value Storage**: Uses namespaces and keys for data organization
   - **Element State Persistence**: Elements already use NVS for state restoration (`restore_nvs_context` method)

5. **Architecture Patterns**:
   - **C/C++ Boundary**: Strict separation with C interfaces and C++ implementations
   - **Interface/Implementation**: Clear separation between interfaces and platform implementations
   - **BSP Abstraction**: Board-specific code isolated in `port/bsp/<board>/`

### **Phase Implementation Plan**

Based on the requirements analysis and current codebase investigation, here is a phased implementation plan:

#### **Phase 1: Core GPIO Infrastructure (Foundation)**
**Duration**: 2-3 weeks | **Priority**: High

1. **GPIO Interface Design** (`interface/gpio/`):
   - Create `meshx_gpio.h` - Core GPIO API (set_level, get_level, toggle, etc.)
   - Create `meshx_gpio_types.h` - Data structures and enums
   - Create `meshx_pwm.h` - PWM-specific API
   - Follow existing RTOS abstraction patterns

2. **YAML Schema Extension**:
   - Extend `prod_profile.yml` schema to include GPIO configuration
   - Define GPIO pin definitions, modes, pull settings, etc.
   - Add validation rules in `code_gen.py`

3. **Code Generation Extension**:
   - Extend `code_gen.py` to parse GPIO configuration
   - Generate GPIO configuration macros in `meshx_config.h`
   - Generate initialization data structures

#### **Phase 2: BSP GPIO Implementations**
**Duration**: 2-3 weeks | **Priority**: High

1. **ESP32 Platform Implementation** (`port/platform/esp/esp_idf/gpio/`):
   - Implement `meshx_gpio_esp.c` using ESP-IDF GPIO API
   - Implement `meshx_pwm_esp.c` using ESP-IDF LEDC API
   - Handle ESP32-specific constraints

2. **BSP GPIO Mapping** (`port/bsp/<board>/gpio/`):
   - Create BSP-specific GPIO mapping files
   - Define logical to physical pin mappings
   - Handle board-specific constraints (available pins, conflicts)

3. **Platform Abstraction Layer**:
   - Create `meshx_gpio_platform.c` to dispatch to BSP implementations
   - Follow existing NVS abstraction pattern

#### **Phase 3: Runtime API and Element Integration**
**Duration**: 2-3 weeks | **Priority**: Medium

1. **GPIO Runtime Implementation** (`src/gpio/`):
   - Implement `meshx_gpio.c` - Core GPIO subsystem
   - Implement `meshx_pwm.c` - PWM subsystem
   - Add error handling and validation

2. **Element-GPIO Bridge**:
   - Create C++ wrapper classes in elements layer
   - Add GPIO binding to existing element types
   - Follow C/C++ boundary patterns

3. **Initialization System**:
   - Integrate GPIO initialization into `meshx_init()`
   - Add GPIO deinitialization

#### **Phase 4: Advanced Features**
**Duration**: 2-3 weeks | **Priority**: Medium

1. **Interrupt Support**:
   - Extend GPIO interface for interrupt registration
   - Implement interrupt handling with RTOS integration
   - Add debouncing and edge detection

2. **Hosted/Non-Hosted Mode Support**:
   - Implement mode detection via API TLV command
   - Add UART transport for hosted mode GPIO events
   - Handle mode switching safely

#### **Phase 5: NVS Persistence Integration**
**Duration**: 1-2 weeks | **Priority**: High

1. **NVS Storage for GPIO Configuration**:
   - Extend NVS interface for GPIO configuration storage
   - Create serialization format for GPIO configuration
   - Add versioning for schema migration

2. **Runtime Configuration Updates**:
   - Implement API for dynamic GPIO configuration changes
   - Add NVS persistence for runtime changes
   - Handle configuration validation

#### **Phase 6: Testing and Validation**
**Duration**: 2-3 weeks | **Priority**: Medium

1. **Unit Tests**:
   - Test all GPIO API functions
   - Test error conditions and validation
   - Test interrupt handling

2. **Integration Tests**:
   - Test GPIO with element integration
   - Test hosted/non-hosted mode switching
   - Test NVS persistence and recovery

3. **Hardware Tests**:
   - Test on all supported BSPs (xiao_c3, weact_c3, esp32_devkitC)
   - Validate timing and performance requirements
   - Test OTA update compatibility

#### **Phase 7: Documentation and Examples**
**Duration**: 1-2 weeks | **Priority**: Low

1. **API Documentation**:
   - Document all GPIO APIs
   - Create usage examples
   - Add to Doxygen generation

2. **Configuration Guide**:
   - Document YAML schema for GPIO configuration
   - Create example product profiles
   - Add troubleshooting guide

### **Key Design Decisions Based on Current Codebase**

1. **Follow Existing Patterns**:
   - Use same interface/implementation pattern as RTOS and NVS
   - Maintain C/C++ boundary with `meshx_c_header.h` inclusion
   - Use same error code system (`MESHX_ERR_GPIO_*`)

2. **Leverage Existing Infrastructure**:
   - Extend `code_gen.py` rather than creating new tool
   - Use existing NVS abstraction for persistence
   - Integrate with existing `meshx_init()` system

3. **BSP-Centric Design**:
   - GPIO configuration per BSP in `prod_profile.yml`
   - BSP-specific implementations in `port/bsp/<board>/gpio/`
   - Logical to physical pin mapping at BSP level

4. **OTA Compatibility**:
   - Store GPIO configuration in NVS with product-specific namespaces
   - Preserve configuration during core stack OTA updates
   - Support dynamic configuration updates

### **Risk Mitigation Strategies**

1. **Incremental Implementation**: Each phase builds on previous, reducing integration risk
2. **Pattern Consistency**: Following established patterns reduces design risk
3. **Early Testing**: Unit tests from Phase 1, integration tests from Phase 3
4. **Backward Compatibility**: Maintain existing element interfaces, add GPIO as optional feature

### **Dependencies and Prerequisites**

1. **Phase 1 must complete before Phase 2** (interface before implementation)
2. **Phase 2 must complete before Phase 3** (BSP implementations before runtime)
3. **Phase 5 depends on Phase 1-4** (NVS persistence needs full GPIO system)
4. **All phases depend on existing MeshX build system and tooling**

### **Implementation Notes**

- **Total Estimated Duration**: 12-18 weeks for complete implementation
- **Critical Path**: Phases 1, 2, 5 (Core infrastructure, BSP implementations, NVS persistence)
- **Risk Areas**: Interrupt handling (real-time constraints), NVS persistence (flash wear-leveling)
- **Success Metrics**: All 13 requirements met with working implementations on all supported BSPs
