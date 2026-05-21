# TRD — UVP Model Refactor
## Page 4: Wire Format & Concrete Model Implementations

---

## 1. UVP Wire Payload Format

**Requirement:** REQ-004

Every UVP message sent via `send_with_func_id()` carries a fixed 2-byte `func_id` prefix before the application payload. The receiver dispatcher strips this prefix before passing the payload to the element.

```
┌────────────────────────────────────────────────────────────┐
│                   UVP Frame (BLE Mesh PDU)                 │
├────────────────────────┬───────────────────────────────────┤
│   UVP Header (4 B)     │   UVP Payload (up to 375 B)       │
│   tid | ack_req | rfu  │                                   │
│   type_id (2 B)        │  ┌──────────┬───────────────────┐ │
│                        │  │ func_id  │ App Payload       │ │
│                        │  │ (2 B LE) │ (0–373 B)         │ │
│                        │  └──────────┴───────────────────┘ │
└────────────────────────┴───────────────────────────────────┘
```

**Field encoding:**

| Field | Size | Encoding | Notes |
|---|---|---|---|
| `func_id` | 2 bytes | Little-endian | Stripped by dispatcher before element callback |
| App payload | 0–373 bytes | As defined by each logical model | Original application data |

**Effective max payload budget:**

| Constant | Old | New |
|---|---|---|
| `MESHX_UVP_MAX_PAYLOAD` | 377 B | 377 B (unchanged — total) |
| App payload budget | 377 B | **375 B** (minus 2 B `func_id` prefix) |

Add a comment to `meshx_uvp.h`:
```c
#define MESHX_UVP_MAX_PAYLOAD       377  /**< Max total payload (header excl.) */
#define MESHX_UVP_FUNC_ID_PREFIX_SZ 2    /**< Bytes reserved for func_id prefix (REQ-004) */
#define MESHX_UVP_MAX_APP_PAYLOAD   (MESHX_UVP_MAX_PAYLOAD - MESHX_UVP_FUNC_ID_PREFIX_SZ) /**< 375 B */
```

---

## 2. Concrete Model: `meshXRelayClientModel`

```cpp
class meshXRelayClientModel : public meshXModel {
public:
    using meshXModel::meshXModel;

    meshx_err_t handle_rx(const void* param, size_t param_size,
                          const meshx_uvp_ctx_t* ctx) override
    {
        if (ctx->src_addr == 0x0001) {
            // Host command path: forward to BLE mesh
            const auto* el_ctx = static_cast<const meshx_element_common_ctx_t*>(
                parent_element->get_element_ctx());
            if (!el_ctx || el_ctx->pub_addr == MESHX_ADDR_UNASSIGNED) {
                MESHX_LOGW(MODULE_ID_BLE_MESH_ELEMENT,
                    "RelayClient [%d]: no pub_addr configured",
                    parent_element->get_element_idx());
                return MESHX_INVALID_STATE;
            }
            return physical_model->send_with_func_id(
                el_ctx->pub_addr,
                (uint16_t)parent_element->get_element_variant(),
                get_func_id(),
                param, (uint16_t)param_size,
                true /* ack_req */);
        }

        // BLE response path: report success to app
        meshx_api_relay_client_evt_t evt = {};
        evt.err_code = 0;
        if (param && param_size > 0)
            evt.on_off = *(static_cast<const uint8_t*>(param));

        return meshx_send_msg_to_app(
            parent_element->get_element_idx(),
            (uint16_t)parent_element->get_element_variant(),
            get_func_id(), sizeof(evt), &evt);
    }

    meshx_err_t handle_timeout(const meshx_uvp_ctx_t* ctx) override
    {
        meshx_api_relay_client_evt_t evt = {};
        evt.err_code = 1; // Timeout
        return meshx_send_msg_to_app(
            parent_element->get_element_idx(),
            (uint16_t)parent_element->get_element_variant(),
            get_func_id(), sizeof(evt), &evt);
    }
};
```

---

## 3. Concrete Model: `meshXRelayServerModel`

```cpp
class meshXRelayServerModel : public meshXModel {
public:
    using meshXModel::meshXModel;

    meshx_err_t handle_rx(const void* param, size_t param_size,
                          const meshx_uvp_ctx_t* ctx) override
    {
        const auto* el_ctx = static_cast<const meshx_element_common_ctx_t*>(
            parent_element->get_element_ctx());
        const uint16_t type_id = (uint16_t)parent_element->get_element_variant();

        // Step 1: Unicast ACK
        if (ctx->ack_req) {
            physical_model->send_with_func_id(
                ctx->src_addr, type_id, get_func_id(),
                param, (uint16_t)param_size, false);
        }

        // Step 2: Publish
        if (el_ctx && el_ctx->pub_addr != MESHX_ADDR_UNASSIGNED
                   && el_ctx->pub_addr != ctx->src_addr) {
            physical_model->send_with_func_id(
                el_ctx->pub_addr, type_id, get_func_id(),
                param, (uint16_t)param_size, false);
        }

        // Step 3: App telemetry
        return meshx_send_msg_to_app(
            parent_element->get_element_idx(), type_id,
            get_func_id(), (uint16_t)param_size, param);
    }

    meshx_err_t handle_timeout(const meshx_uvp_ctx_t*) override {
        return MESHX_SUCCESS; // Servers never initiate TX — no timeout possible
    }
};
```

---

## 4. Concrete Model: `meshXLightCWWWClientModel`

*Handles both OnOff (func_id=0x00) and CTL (func_id=0x01) via separate instances.*

```cpp
class meshXLightCWWWClientModel : public meshXModel {
public:
    using meshXModel::meshXModel;

    meshx_err_t handle_rx(const void* param, size_t param_size,
                          const meshx_uvp_ctx_t* ctx) override
    {
        if (ctx->src_addr == 0x0001) {
            // Forward host command to BLE mesh
            const auto* el_ctx = static_cast<const meshx_element_common_ctx_t*>(
                parent_element->get_element_ctx());
            if (!el_ctx || el_ctx->pub_addr == MESHX_ADDR_UNASSIGNED)
                return MESHX_INVALID_STATE;
            return physical_model->send_with_func_id(
                el_ctx->pub_addr,
                (uint16_t)parent_element->get_element_variant(),
                get_func_id(), param, (uint16_t)param_size, true);
        }

        // BLE response: parse and report to app
        meshx_api_light_cwww_client_evt_t evt = {};
        evt.err_code = 0;
        if (param && param_size > 0) {
            size_t copy = std::min(param_size, sizeof(evt.state_change));
            std::memcpy(&evt.state_change, param, copy);
        }
        return meshx_send_msg_to_app(
            parent_element->get_element_idx(),
            (uint16_t)parent_element->get_element_variant(),
            get_func_id(), sizeof(evt), &evt);
    }

    meshx_err_t handle_timeout(const meshx_uvp_ctx_t*) override
    {
        meshx_api_light_cwww_client_evt_t evt = {};
        evt.err_code = 1; // Timeout
        return meshx_send_msg_to_app(
            parent_element->get_element_idx(),
            (uint16_t)parent_element->get_element_variant(),
            get_func_id(), sizeof(evt), &evt);
    }
};
```

---

## 5. Concrete Model: `meshXLightCWWWServerModel`

```cpp
class meshXLightCWWWServerModel : public meshXModel {
public:
    using meshXModel::meshXModel;

    meshx_err_t handle_rx(const void* param, size_t param_size,
                          const meshx_uvp_ctx_t* ctx) override
    {
        const auto* el_ctx = static_cast<const meshx_element_common_ctx_t*>(
            parent_element->get_element_ctx());
        const uint16_t type_id = (uint16_t)parent_element->get_element_variant();

        if (ctx->ack_req)
            physical_model->send_with_func_id(
                ctx->src_addr, type_id, get_func_id(),
                param, (uint16_t)param_size, false);

        if (el_ctx && el_ctx->pub_addr != MESHX_ADDR_UNASSIGNED
                   && el_ctx->pub_addr != ctx->src_addr)
            physical_model->send_with_func_id(
                el_ctx->pub_addr, type_id, get_func_id(),
                param, (uint16_t)param_size, false);

        return meshx_send_msg_to_app(
            parent_element->get_element_idx(), type_id,
            get_func_id(), (uint16_t)param_size, param);
    }

    meshx_err_t handle_timeout(const meshx_uvp_ctx_t*) override {
        return MESHX_SUCCESS;
    }
};
```

---

*Continued in Page 5: Task Breakdown & File Change Matrix*
