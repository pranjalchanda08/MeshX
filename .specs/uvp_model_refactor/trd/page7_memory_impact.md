# TRD — UVP Model Refactor
## Page 7: Memory Impact Analysis

**Target Platform:** ESP32-C3 (RISC-V 32-bit, 4 MB Flash, 400 KB SRAM)
**Compiler:** Xtensa/RISC-V GCC via ESP-IDF v5.4

> [!NOTE]
> All sizes below are **estimates** based on struct layout, vtable analysis, and typical GCC code generation for the target. Exact figures require a post-build `idf.py size-components` comparison. Section 5 provides the command to measure actuals.

---

## 1. Struct Size Changes

### 1.1 `meshx_uvp_ctx_t` — Modified (REQ-003)

| Field | Type | Size |
|-------|------|------|
| `src_addr` | `uint16_t` | 2 B |
| `dst_addr` | `uint16_t` | 2 B |
| `tid` | `uint8_t` | 1 B |
| `ack_req` | `uint8_t` | 1 B |
| *(padding)* | — | 2 B |
| `func_id` *(NEW)* | `uint16_t` | 2 B |
| **Total** | | **Before: 6 B → After: 8 B** |

`meshx_uvp_ctx_t` is a **stack-allocated** local variable in the dispatcher — not stored on the heap. The delta is +2 bytes of stack depth per dispatcher call. No heap impact.

### 1.2 `meshXModel` Base Class — New (REQ-001, REQ-002)

Layout on ESP32-C3 (4-byte pointer, packed vtable):

| Member | Type | Size |
|--------|------|------|
| vtable pointer | `void*` | 4 B |
| `parent_element` | `meshXElementIF*` | 4 B |
| `physical_model` | `meshXUVPModel*` | 4 B |
| `func_id` | `uint16_t` | 2 B |
| *(padding)* | — | 2 B |
| **Total** | | **16 B per instance** |

### 1.3 `meshXUVPElement` — Added `logical_models` vector

`std::vector<std::unique_ptr<meshXModel>>` on a 32-bit system:

| Component | Size |
|-----------|------|
| `vector` internal (pointer + size + capacity) | 12 B |
| Each `unique_ptr<meshXModel>` slot on heap | 4 B (pointer storage) |
| **Overhead per element (static in object)** | **12 B** |

The vector is embedded inside `meshXUVPElement`. Since `meshXUVPElement` already exists in heap (via `make_unique` in the composition), the 12 B vector header is folded into the element's existing heap allocation.

---

## 2. Heap (RAM) Impact per Element Type

Each logical model instance is separately heap-allocated by `std::make_unique<meshXModel>`. The concrete class adds no additional data members beyond the base class in the current design.

| Element Type | Logical Models Added | Per-Model Heap | Total New Heap |
|---|---|---|---|
| `RELAY_SERVER` | 1 × `meshXRelayServerModel` | 16 B | **16 B** |
| `RELAY_CLIENT` | 1 × `meshXRelayClientModel` | 16 B | **16 B** |
| `CWWW_SERVER` | 2 × `meshXLightCWWWServerModel` (F0, F1) | 16 B each | **32 B** |
| `CWWW_CLIENT` | 2 × `meshXLightCWWWClientModel` (F0, F1) | 16 B each | **32 B** |
| `Root` (index 0) | 0 | — | **0 B** |

**ESP heap allocator overhead:** Each `malloc`/`new` allocation on the ESP-IDF heap carries a ~8 B header (linked-list node). This is platform overhead, not object size.

| Element Type | Allocations | Allocator Overhead | Total RAM (data + overhead) |
|---|---|---|---|
| `RELAY_SERVER` | 1 | 8 B | **24 B** |
| `RELAY_CLIENT` | 1 | 8 B | **24 B** |
| `CWWW_SERVER` | 2 | 16 B | **48 B** |
| `CWWW_CLIENT` | 2 | 16 B | **48 B** |

**Typical `all_in_one` node** (1 Relay Client + 1 CWWW Client + 1 Root):

```
24 B (RelayClient) + 48 B (CWWWClient) + 0 B (Root) = 72 B new heap
+ 12 B × 2 vector headers in element objects
= ~96 B total heap increase
```

---

## 3. Flash (.text + .rodata) Impact

### 3.1 Code Additions

| File / Component | Estimated .text Delta | Notes |
|---|---|---|
| `meshx_uvp_logical_models.cpp` — 4 × `handle_rx` + `handle_timeout` | **+1 100 B** | ~8 functions × ~140 B avg per function |
| `meshXModel::can_handle()` default (inline, emitted once per TU) | **+40 B** | Single non-virtual call site |
| `send_with_func_id()` inline (emitted in logical_models.cpp TU) | **+100 B** | Stack buffer + memcpy + `send()` call |
| Dispatcher changes (3 ctx.func_id assignments) | **+24 B** | 3 × ~8 B (load + store + 2-byte offset) |

### 3.2 Code Removals

| File / Component | Estimated .text Delta | Notes |
|---|---|---|
| `element_state_change_notify` — old if-else removed | **−250 B** | ~5 branches × ~50 B each |
| `list_ven_models` — simplified (no per-variant returns) | **−40 B** | Old: multiple return paths |

### 3.3 `.rodata` (vtables)

Each concrete class with virtual methods gets a vtable. For ESP-IDF GCC:

| Vtable | Entries | Size |
|--------|---------|------|
| `meshXRelayClientModel` vtable | 5 (dtor×2 + can_handle + handle_rx + handle_timeout) | 20 B |
| `meshXRelayServerModel` vtable | 5 | 20 B |
| `meshXLightCWWWClientModel` vtable | 5 | 20 B |
| `meshXLightCWWWServerModel` vtable | 5 | 20 B |
| `meshXModel` base vtable | 5 | 20 B |
| **Total vtable .rodata** | | **+100 B** |

### 3.4 Flash Summary

| Category | Delta |
|----------|-------|
| New .text (logical models + helpers + dispatcher) | +1 264 B |
| Removed .text (old notify + list_ven_models) | −290 B |
| New .rodata (vtables) | +100 B |
| **Net Flash Impact** | **≈ +1 074 B (~1.05 KB)** |

---

## 4. Stack Impact

### 4.1 `send_with_func_id()` Stack Frame

```cpp
uint8_t stack_buf[64];   // 64 B on stack (small payload fast path)
uint16_t wire_len;        // 2 B
uint8_t* wire;            // 4 B
meshx_err_t err;          // 4 B
```

Stack depth during `send_with_func_id()`: **~80 B** (including saved registers).  
This is a leaf call — it does not recurse and completes before returning.

### 4.2 `element_state_change_notify()` Stack Frame

Before and after the refactor, the function is a loop dispatcher. The new version is **shallower** — no local telemetry structs built on the stack (they move into `handle_rx()`).

| Scenario | Before (bytes) | After (bytes) |
|---|---|---|
| `element_state_change_notify` frame | ~48 B | ~24 B |
| `handle_rx` frame (inner call) | N/A (inline) | ~64 B |
| Net peak stack depth | ~48 B | ~88 B |

> [!NOTE]
> The peak stack increases by ~40 B due to the additional call frame for `handle_rx`. This is well within the FreeRTOS task stack allocation (typically 4 KB for the control task).

---

## 5. Wire Overhead

Every UVP message now carries a 2-byte `func_id` prefix (REQ-004). This affects all UVP transmissions.

| Metric | Before | After |
|--------|--------|-------|
| Effective app payload budget | 377 B | **375 B** |
| Overhead per message | 0 B | **2 B** |
| BLE Mesh PDU count impact | — | None (377 B fits in 3 segments; 375 B also fits in 3 segments) |

The BLE Mesh segmented access PDU carries up to 380 bytes in a 3-segment message. Both 377 B and 375 B fit within the same 3-segment envelope — **no additional BLE advertisements required**.

---

## 6. Total Memory Budget Summary

```
┌────────────────────────────────────────────────────────────┐
│             Memory Impact (all_in_one node)                │
├──────────────────┬──────────────────┬──────────────────────┤
│ Memory Type      │ Before           │ After (Delta)        │
├──────────────────┼──────────────────┼──────────────────────┤
│ Flash (.text)    │ baseline         │ + ~974 B             │
│ Flash (.rodata)  │ baseline         │ + ~100 B (vtables)   │
│ Heap (DRAM)      │ baseline         │ + ~96 B              │
│ Stack (peak)     │ ~48 B frame      │ ~88 B frame (+40 B)  │
│ Wire payload     │ 377 B max        │ 375 B max (-2 B)     │
├──────────────────┼──────────────────┼──────────────────────┤
│ TOTAL FLASH      │                  │ ≈ +1.05 KB           │
│ TOTAL RAM        │                  │ ≈ +96 B heap         │
└──────────────────┴──────────────────┴──────────────────────┘
```

**Verdict:** The memory impact is minimal and well within the ESP32-C3 budget (4 MB flash, 400 KB SRAM). The +1 KB flash cost buys correct `func_id` routing, modular extensibility, and elimination of a fragile heuristic.

---

## 7. How to Measure Actuals Post-Build

```bash
# Build first
. /run/media/pchanda/workspace/wsl/esp/v5.4/esp-idf/export.sh
python3 tools/scripts/meshx.py -B=xiao_c3 -N=all_in_one -b

# Flash size breakdown by component
idf.py size-components

# Detailed section sizes (text, data, rodata, bss)
idf.py size-files

# Heap usage at runtime (add to app startup)
# MESHX_LOGI(..., "Heap free: %d", esp_get_free_heap_size());
# Compare before/after applying the refactor patch
```

Add `esp_get_free_heap_size()` log lines before and after the composition bake call in `meshx_element_init()` to get precise runtime heap delta.

---

*End of Page 7*
