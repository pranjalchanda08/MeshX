# TRD-06 — Handler Migration and File Structure

## 1. Handler Mapping — Old → New

| Old Handler | New Handler | Command ID |
|-------------|-------------|------------|
| `mxsp_cmd_el_cmd_send()` | `mxcp_cmd_fn_el_send()` | `MXCP_CMD_EL_SEND` |
| `mxsp_cmd_sys_dispatch()` → `mxsp_sys_node_reset()` | `mxcp_cmd_fn_node_reset()` | `MXCP_CMD_NODE_RESET` |
| `mxsp_cmd_sys_dispatch()` → `mxsp_sys_get_composition()` | `mxcp_cmd_fn_get_composition()` | `MXCP_CMD_GET_COMPOSITION` |
| `mxsp_cmd_sys_dispatch()` → `mxsp_sys_get_element_state()` | `mxcp_cmd_fn_get_element_state()` | `MXCP_CMD_GET_ELEMENT_STATE` |
| `mxsp_cmd_sys_dispatch()` → `mxsp_sys_set_console_routing()` | `mxcp_cmd_fn_set_console_routing()` | `MXCP_CMD_SET_CONSOLE_ROUTING` |
| `mxsp_cmd_hosted_mode()` | `mxcp_cmd_fn_hosted_mode()` | `MXCP_CMD_HOSTED_MODE_ENABLE` |
| `mxsp_handle_gpio_cmd()` → case SET_LEVEL | `mxcp_cmd_fn_gpio_set_level()` | `MXCP_CMD_GPIO_SET_LEVEL` |
| `mxsp_handle_gpio_cmd()` → case GET_LEVEL | `mxcp_cmd_fn_gpio_get_level()` | `MXCP_CMD_GPIO_GET_LEVEL` |
| `mxsp_handle_gpio_cmd()` → case TOGGLE | `mxcp_cmd_fn_gpio_toggle()` | `MXCP_CMD_GPIO_TOGGLE` |
| ... (each GPIO case) | individual handler | individual CMD ID |

*(REQ-005, REQ-007)*

## 2. File Structure Changes

### 2.1 New Files

| File | Purpose |
|------|---------|
| `common/inc/meshx_mxcp.h` | MXCP frame format, header macros, command/event ID enums, payload structures, table types, API declarations |
| `common/src/meshx_mxcp.c` | Command/event table definitions, `mxcp_dispatch_frame()`, `mxcp_send_event()`, `mxcp_send_cmd()`, individual command handlers |

### 2.2 Modified Files

| File | Changes |
|------|---------|
| `common/inc/meshx_serial.h` | Rename to `meshx_mxcp_serial.h` or refactor: remove `mxsp_msg_type_t`, `mxsp_gpio_cmd_t`, `mxsp_gpio_evt_t`, old GPIO payload structs, old MXSP frame struct. Replace with `#include "meshx_mxcp.h"`. Retain `meshx_serial_init()`, `meshx_serial_set_hosted_mode()`, hosted-mode state. |
| `common/src/meshx_serial.c` | Update state machine to 2-byte header parsing, update checksum, update TX frame builder to use `mxcp_frame_t`, update `mxsp_uart_rx_task`, update GPIO hosted event callback to use `mxcp_send_event(MXCP_EVT_GPIO_ASYNC, ...)`. |
| `common/inc/meshx_mxsp_cmd.h` | Removed (replaced by `meshx_mxcp.h`). |
| `common/src/meshx_mxsp_cmd.c` | Removed (replaced by `meshx_mxcp.c`). |
| `elements/src/meshx_composition_builder.cpp` | Update `meshx_get_element_composition_data()` and `meshx_get_element_state_data()` to use new MXCP payload struct names (`mxcp_evt_composition_rsp_t` header, etc.) |
| `inc/meshx_api.h` | Keep existing `meshx_send_msg_to_app()` and `meshx_send_ctrl_msg_to_app()` but have them internally call `mxcp_send_event()` instead of `mxsp_send_data_event()` / `mxsp_send_ctrl_event()`. The public API (`meshx_app_data_cb_t`, `meshx_app_ctrl_cb_t`) remains unchanged for backward compatibility. |

### 2.3 Removed Files

| File | Reason |
|------|--------|
| `common/inc/meshx_mxsp_cmd.h` | Replaced by `meshx_mxcp.h` |
| `common/src/meshx_mxsp_cmd.c` | Replaced by `meshx_mxcp.c` |

*(REQ-006, REQ-009)*
