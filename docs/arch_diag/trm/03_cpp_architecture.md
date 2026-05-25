# Page 3 — C++ Class Architecture

> **[← Boot Sequence](./02_boot_sequence.md)** | **[← Index](../README.md)** | **[Next: Control Task →](./04_control_task.md)**

---

## 4. C++ Class Architecture

The C++ layer uses a **Flat Dispatcher Pattern** (replacing the legacy template-based multi-model hierarchy).

### 4.1 Class Diagram

```mermaid
classDiagram
    class meshXElementIF {
        <<Interface>>
        +get_element_idx() uint16_t
        +get_element_type() meshxElementType_t
        +get_element_variant() meshx_element_type_t
        +on_model_cb(payload, size, ctx) meshx_err_t
    }

    class meshXElementServer {
        <<Base Server Element>>
    }

    class meshXUVPElement {
        -meshx_element_common_ctx_t element_ctx
        -vector~unique_ptr~meshXLogicalModel~~ logical_models
        +list_ven_models() uint8_t
        +element_state_change_notify(param, size, ctx) meshx_err_t
    }

    class meshXUVPModel {
        +send(dst, type_id, payload, len, ack)
        +send_with_func_id(dst, type_id, func_id, payload, len, ack)
    }

    class meshXLogicalModel {
        <<Abstract Base>>
        #meshXElementIF* parent
        #meshXUVPModel* phys_model
        #uint16_t func_id
        +can_handle(param, size, ctx) bool
        +handle_rx(param, size, ctx)* meshx_err_t
        +handle_timeout(ctx)* meshx_err_t
    }

    class meshXRelayServerModel {
        +handle_rx() meshx_err_t
        +handle_timeout() meshx_err_t
    }

    class meshXRelayClientModel {
        +handle_rx() meshx_err_t
        +handle_timeout() meshx_err_t
    }

    class meshXLightCWWWServerModel {
        +handle_rx() meshx_err_t
        +handle_timeout() meshx_err_t
    }

    class MeshXIoInterface {
        <<Abstract C++>>
        +execute(function, args) = 0
        +setLevel(pin, level) = 0
        +setPwmDutyCycle(pin, duty) = 0
        +registerInterrupt(pin, cb) = 0
    }

    class MeshXGpioImpl {
        +execute() override
        +setLevel() override
    }

    class MeshXPwmImpl {
        +execute() override
        +setPwmDutyCycle() override
    }

    meshXElementIF <|-- meshXElementServer
    meshXElementServer <|-- meshXUVPElement
    meshXUVPElement *-- meshXUVPModel : "Physical BLE Vendor Model"
    meshXUVPElement o-- meshXLogicalModel : "Composes 1..N Logical Models"
    meshXLogicalModel <|-- meshXRelayServerModel
    meshXLogicalModel <|-- meshXRelayClientModel
    meshXLogicalModel <|-- meshXLightCWWWServerModel
    meshXRelayServerModel ..> MeshXIoInterface : "Drives via HAL"
    MeshXIoInterface <|-- MeshXGpioImpl
    MeshXIoInterface <|-- MeshXPwmImpl
```

### 4.2 Layer Responsibility Matrix

| Layer | Class | File | Responsibility |
|-------|-------|------|----------------|
| **Base Interface** | `meshXElementIF` | `meshx_fwd_decl.hpp` | Abstract interface; defines index, element type, variant, and `on_model_cb()` |
| **Element Container** | `meshXUVPElement` | `meshx_uvp_element.hpp/.cpp` | UVP container element; composes physical `meshXUVPModel` and `logical_models` vector; routes events |
| **Physical Transport** | `meshXUVPModel` | `meshx_uvp_model.hpp/.cpp` | Vendor Model transport wrapper; prepends 2-byte `func_id` prefix via `send_with_func_id()` |
| **Logical Model Base** | `meshXLogicalModel` | `meshx_uvp_logical_model.hpp` | Abstract base for functions within an element; matches `ctx->func_id`; defines `handle_rx()` and `handle_timeout()` |
| **Concrete Models** | `meshXRelayServerModel`, `meshXLightCWWWServerModel`, etc. | `meshx_uvp_logical_models.hpp/.cpp` | Encapsulates state persistence, dual-routing (ACK + group publish), and host telemetry reporting |
| **I/O Interface** | `MeshXIoInterface` | `meshx_io_interface.hpp` | Pure virtual C++ interface for all hardware GPIO & PWM operations |
| **I/O Impl** | `MeshXGpioImpl` / `MeshXPwmImpl` | `meshx_gpio_impl.cpp` / `meshx_pwm_impl.cpp` | Concrete ESP32-C3 hardware GPIO and LEDC PWM drivers |

### 4.3 IO Factory Pattern

The `MeshXIoFactory` (singleton) provides type-registered runtime creation of `MeshXIoInterface` instances. This allows BSP-level GPIO/PWM implementations to be registered without changing element logic.

| Method | Purpose |
|--------|---------|
| `getInstance()` | Singleton accessor |
| `registerType(name, factory_fn)` | Register a concrete IO type (e.g., "gpio", "pwm") |
| `create(logical_pin, type)` | Create an IO object by type name |
| `createFromYaml(config)` | Deserialize GPIO bindings from product YAML profile |
| `isTypeSupported(type)` | Query capability at runtime |

### 4.4 Why Flat Dispatcher? (Rationale)

The legacy architecture used `meshXBaseServerModel<T>` and `meshXBaseClientModel<T>` C++ templates, one per SIG model type (OnOff, Level, CTL, HSL, etc.). This generated:

- **Duplicate vtable + RTTI** overhead for each instantiated template
- **Multiple model opcode tables** registered with the BLE stack
- **Bloated `.rodata`** from repeated string literals and dispatch tables

The flat `meshXUVPDispatcher` collapses all of this into a single runtime `map<uint8_t, TagHandler>` with **one opcode** and **zero templates**. See [Decommissioning](./13_decommissioning.md) for the quantified savings.

---

> **[← Boot Sequence](./02_boot_sequence.md)** | **[← Index](../README.md)** | **[Next: Control Task →](./04_control_task.md)**
