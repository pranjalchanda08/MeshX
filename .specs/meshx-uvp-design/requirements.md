# Requirements: MeshX Unified Vendor Protocol (UVP)

## 1. Overview
The MeshX Unified Vendor Protocol (UVP) aims to replace standard Bluetooth Mesh SIG models with a custom, highly efficient vendor-specific communication protocol. This is required to reclaim flash memory and RAM for high-level gateway services.

## 2. Functional Requirements

### 2.1 Unified Model Support
- **REQ-F01**: The system must support a single Vendor Model per element.
- **REQ-F02**: The model must handle all functional states (OnOff, Level, Color, Sensors, etc.).
- **REQ-F03**: The protocol must support multi-state updates in a single radio message (Atomic Updates).

### 2.2 TLV Payload Engine
- **REQ-F04**: The payload must use a Type-Length-Value (TLV) format for extensibility.
- **REQ-F05**: The system must support at least 256 unique Tags (1-byte Tag ID).
- **REQ-F06**: The system must support dynamic registration of Tag handlers.

### 2.3 Backward Compatibility
- **REQ-F07**: The node must remain compatible with standard BLE Mesh **Configuration Server** and **Health Server** models.
- **REQ-F08**: The node must support standard BLE Mesh **Provisioning** (PB-ADV/PB-GATT).

### 2.4 Gateway Integration
- **REQ-F09**: The protocol must provide a reliable status mechanism for the Gateway to track node states.
- **REQ-F10**: The status messages must be grouped to minimize radio air-time.

### 2.5 Application API & Architecture
- **REQ-F11**: The system must provide a unified Application API that abstracts TLV serialization from the developer.
- **REQ-F12**: The API must support both "Push" (set state) and "Pull" (request status) operations for any registered Tag.
- **REQ-F13**: Provide a lightweight compatibility layer to map legacy model-specific calls to the new Unified API.
- **REQ-F14**: The system must support **Standard Multi-Element Addressing** to maintain compatibility with Mesh Group/Virtual address subscriptions.
- **REQ-F15**: Each application element must host exactly one instance of the **MeshX UVP Vendor Model**.
- **REQ-F16**: The state machine shall be **Target-Status based**, with optional TLV tags for `Transition Time` and `Delay`.
- **REQ-F17**: Implement a lightweight **Transaction ID (TID)** mechanism within the TLV header to ensure message idempotency.
- **REQ-F18**: The UVP layer must utilize **TXCM (Transmission Control Module)** for all command and status transmissions to ensure message reliability, ordered queuing, and application-level retries.
- **REQ-F19**: Underlying IO management (GPIO, PWM, ADC) and hardware drivers must remain unchanged; the UVP layer will interface with existing platform-specific hardware handlers.
- **REQ-F20**: The system must maintain **Hosted Mode** support, allowing the UVP Dispatcher to forward incoming BLE Mesh commands to a Serial Host via **MXSP (MeshX Serial Protocol)**.
- **REQ-F21**: The **MXSP** frame format must be extended to include a **UVP/TLV Pass-through** message type for Host-Controller interaction.
- **REQ-F22**: The protocol must support **Global Element Type IDs (EL_TYPE_ID)** via a dedicated TLV Tag to facilitate precise Client-Server pairing and roles-based discovery.
- **REQ-F23**: Nodes must report their unique **EL_TYPE_ID** (e.g. Generic OnOff, RGB Light, PIR Sensor) to ensure type-safe networking and prevent mismatched pairings.
- **REQ-F24**: The protocol shall utilize a **Global Tag Namespace**, ensuring each Tag ID has a fixed definition and data type across the ecosystem.
- **REQ-F25**: Elements shall dynamically register only the subset of Tags relevant to their specific **EL_TYPE_ID**.

## 3. Non-Functional Requirements

### 3.1 Performance & Size
- **REQ-NF01**: The UVP implementation must reduce the compiled Flash footprint by at least 150 KB compared to the full SIG model stack.
- **REQ-NF02**: Message processing latency must be < 50ms (local).

### 3.2 Reliability
- **REQ-NF03**: The TLV parser must be robust against malformed or truncated packets.
- **REQ-NF04**: The system must handle packet segmentation for large TLV streams (> 11 bytes).

### 3.3 Ecosystem
- **REQ-NF05**: (Walled Garden) The protocol is intentionally private to the MeshX ecosystem.

## 5. Decommissioning Scope

### 5.1 SIG Application Models
- **REQ-D01**: All SIG-standard Generic Client and Server models (OnOff, Level, Default Transition Time, Power OnOff, Power Level, Battery, Location, Property) will be decommissioned.
- **REQ-D02**: All SIG-standard Lighting Client and Server models (Lightness, CTL, HSL, XYL, LC) will be decommissioned.
- **REQ-D03**: All SIG-standard Sensor, Time, Scene, and Scheduler models will be decommissioned.

### 5.2 Code Infrastructure
- **REQ-D04**: The complex C++ template-based model hierarchy (`meshXBaseServerModel`, `meshXBaseClientModel` with multiple instantiations) will be decommissioned and replaced by a flat, non-template unified class.
- **REQ-D05**: Standard message dispatchers for the above models will be removed to reclaim Flash.

## 6. Constraints
- **Hardware**: Must run on ESP32-C3 with 4MB Flash and 400KB RAM.
- **Software**: Must integrate with ESP-BLE-MESH (ESP-IDF v5.4).
