# TRD — UVP Model Refactor
## Page 2: Interface Specifications

---

## 1. `meshx_uvp_ctx_t` — Modified Structure

**File:** `main/component/meshx/inc/meshx_uvp.h`
**Requirement:** REQ-003

```c
typedef struct {
    uint16_t src_addr;  /**< Source unicast address. 0x0001 = host, MESHX_ADDR_UNASSIGNED = timeout */
    uint16_t dst_addr;  /**< Destination unicast or group address */
    uint8_t  tid;       /**< Transaction ID (0–255) */
    uint8_t  ack_req;   /**< 1 if sender requested an ACK */
    uint16_t func_id;   /**< Function ID within the element — NEW FIELD (REQ-003)
                         *   0xFFFF = broadcast sentinel for TXCM timeout */
} meshx_uvp_ctx_t;
```

**Sentinel values for `src_addr`:**

| `src_addr` | Meaning |
|---|---|
| `0x0001` | Command from host application |
| `MESHX_ADDR_UNASSIGNED (0x0000)` | TXCM transmission timeout |
| Any other unicast | BLE Mesh peer node |

**Sentinel value for `func_id`:**

| `func_id` | Meaning |
|---|---|
| `0x0000–0xFFFE` | Specific function within the element |
| `0xFFFF` | Broadcast — timeout fires for all models |

---

## 2. `meshXModel` — Base Class Interface

**File:** `main/component/meshx/ble_mesh/model/inc/meshx_uvp_logical_model.hpp` *(new)*
**Requirement:** REQ-001, REQ-002, REQ-005

```cpp
/**
 * @class meshXModel
 * @brief Abstract base class for all UVP logical models.
 *
 * A logical model encapsulates one functional capability (identified by func_id)
 * on a UVP element. Multiple logical models may share the same physical
 * meshXUVPModel transport.
 */
class meshXModel {
protected:
    meshXElementIF* parent_element; ///< Owning element
    meshXUVPModel*  physical_model; ///< Shared physical transport
    uint16_t        func_id;        ///< Registered function ID (REQ-002)

public:
    meshXModel(meshXElementIF* parent,
               meshXUVPModel*  phys_model,
               uint16_t        func_id)
        : parent_element(parent), physical_model(phys_model), func_id(func_id) {}

    virtual ~meshXModel() = default;

    /** Returns the registered function ID. */
    uint16_t get_func_id() const { return func_id; }

    /**
     * @brief Query whether this model should handle the given packet.
     *
     * Default: matches when ctx->func_id == this->func_id.
     * Subclasses MAY override for special routing (e.g., broadcast handler).
     * MUST NOT use param_size as a routing criterion (REQ-005).
     *
     * @param param       Raw payload pointer (informational only)
     * @param param_size  Payload length (informational only, NOT for routing)
     * @param ctx         UVP routing context carrying func_id
     * @return true if this model handles the packet
     */
    virtual bool can_handle(const void*           param,
                             size_t                param_size,
                             const meshx_uvp_ctx_t* ctx) const {
        (void)param;
        (void)param_size;
        return ctx->func_id == func_id;
    }

    /**
     * @brief Handle an incoming packet (BLE RX or host command).
     *
     * For client models:
     *   - If src_addr == 0x0001: forward to BLE mesh via physical_model->send_with_func_id()
     *   - Otherwise: parse response and call meshx_send_msg_to_app()
     * For server models:
     *   - Step 1: Unicast ACK to src_addr (if ctx->ack_req)
     *   - Step 2: Publish to pub_addr (if configured and != src_addr)
     *   - Step 3: Telemetry to app via meshx_send_msg_to_app()
     *
     * @param param       Payload (without func_id prefix — already stripped by dispatcher)
     * @param param_size  Payload length
     * @param ctx         UVP routing context
     */
    virtual meshx_err_t handle_rx(const void*           param,
                                  size_t                param_size,
                                  const meshx_uvp_ctx_t* ctx) = 0;

    /**
     * @brief Handle a TXCM transmission timeout.
     *
     * Called when ctx->src_addr == MESHX_ADDR_UNASSIGNED.
     * Client models: report err_code=1 telemetry to app.
     * Server models: no-op (return MESHX_SUCCESS immediately).
     *
     * @param ctx  UVP context (src_addr = MESHX_ADDR_UNASSIGNED, func_id = 0xFFFF)
     */
    virtual meshx_err_t handle_timeout(const meshx_uvp_ctx_t* ctx) = 0;
};
```

---

## 3. `meshXUVPModel::send_with_func_id()` — New Helper

**File:** `main/component/meshx/ble_mesh/model/inc/meshx_uvp_model.hpp`
**Requirement:** REQ-004

```cpp
/**
 * @brief Send a UVP message with func_id prepended to the wire payload.
 *
 * Wire format: [ func_id (2 bytes, LE) | payload (N bytes) ]
 *
 * @param dst_addr    Destination unicast or group address
 * @param type_id     Element type ID (element variant)
 * @param func_id     Function ID to embed in the wire payload prefix
 * @param payload     Application payload pointer (may be nullptr if payload_len == 0)
 * @param payload_len Length of application payload
 * @param ack_req     Whether to request ACK from destination
 * @return MESHX_SUCCESS on success, error code otherwise
 */
meshx_err_t send_with_func_id(uint16_t    dst_addr,
                               uint16_t    type_id,
                               uint16_t    func_id,
                               const void* payload,
                               uint16_t    payload_len,
                               bool        ack_req = false)
{
    // Stack-allocate wire buffer: [func_id (2B)] + [payload (N B)]
    const uint16_t wire_len = sizeof(uint16_t) + payload_len;
    // Use a small stack buffer for common cases; fall back to heap for large payloads
    uint8_t stack_buf[64];
    uint8_t* wire = (wire_len <= sizeof(stack_buf))
                        ? stack_buf
                        : static_cast<uint8_t*>(malloc(wire_len));
    if (!wire) return MESHX_NO_MEM;

    // Prepend func_id in little-endian
    wire[0] = static_cast<uint8_t>(func_id & 0xFF);
    wire[1] = static_cast<uint8_t>((func_id >> 8) & 0xFF);

    if (payload && payload_len > 0) {
        memcpy(wire + sizeof(uint16_t), payload, payload_len);
    }

    meshx_err_t err = send(dst_addr, type_id, wire, wire_len, ack_req);

    if (wire != stack_buf) free(wire);
    return err;
}
```

---

## 4. `meshXUVPElement` — Modified Members

**File:** `main/component/meshx/ble_mesh/elements/inc/variants/meshx_uvp_element.hpp`
**Requirement:** REQ-001, REQ-008

```cpp
class meshXUVPElement : public meshXElementServer {
private:
    meshx_element_common_ctx_t               element_ctx;
    std::vector<std::unique_ptr<meshXModel>> logical_models; ///< Composed logical models (NEW)

public:
    // ... existing public interface unchanged (REQ-009)
    meshx_err_t element_state_change_notify(meshx_ptr_t param,
                                            size_t param_size,
                                            const meshx_uvp_ctx_t* ctx) override;
};
```

**New `element_state_change_notify` body:**

```cpp
meshx_err_t meshXUVPElement::element_state_change_notify(
        meshx_ptr_t param, size_t param_size, const meshx_uvp_ctx_t* ctx) {
    if (!ctx) return MESHX_SUCCESS;

    const bool is_timeout = (ctx->src_addr == MESHX_ADDR_UNASSIGNED);

    for (auto& model : logical_models) {
        if (is_timeout) {
            model->handle_timeout(ctx);           // broadcast to all (REQ-008)
        } else if (model->can_handle(param, param_size, ctx)) {
            model->handle_rx(param, param_size, ctx);
            break;                                 // one model per func_id
        }
    }
    return MESHX_SUCCESS;
}
```

---

*Continued in Page 3: Sequence Diagrams*
