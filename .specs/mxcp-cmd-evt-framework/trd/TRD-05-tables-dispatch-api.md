# TRD-05 — Command/Event Tables, Dispatch, and TX API

## 1. Command Table

```c
typedef void (*mxcp_cmd_handler_t)(const uint8_t *payload, uint8_t len);

typedef struct {
    uint16_t              cmd_id;        /* mxcp_cmd_id_t */
    mxcp_cmd_handler_t    handler;       /* Command handler function */
    uint8_t               payload_size;  /* Expected payload size (0 = variable) */
    uint16_t              sync_evt_id;   /* Associated sync response event (0 = none) */
    uint16_t              async_evt_id;  /* Associated async event (0 = none) */
} mxcp_cmd_entry_t;

#define MXCP_CMD_ENTRY(id, fn, psz, sync_evt, async_evt) \
    { (id), (fn), (psz), (sync_evt), (async_evt) }

static const mxcp_cmd_entry_t mxcp_cmd_table[] = {
    /* System Commands */
    MXCP_CMD_ENTRY(MXCP_CMD_HOSTED_MODE_ENABLE,  mxcp_cmd_fn_hosted_mode,       sizeof(mxcp_cmd_hosted_mode_enable_t),     MXCP_EVT_HOSTED_MODE_RSP, 0),
    MXCP_CMD_ENTRY(MXCP_CMD_NODE_RESET,           mxcp_cmd_fn_node_reset,        0,                                        MXCP_EVT_NODE_RESET_IND,  0),
    MXCP_CMD_ENTRY(MXCP_CMD_GET_COMPOSITION,      mxcp_cmd_fn_get_composition,   0,                                        MXCP_EVT_COMPOSITION_RSP, 0),
    MXCP_CMD_ENTRY(MXCP_CMD_GET_ELEMENT_STATE,    mxcp_cmd_fn_get_element_state, 0,                                        MXCP_EVT_ELEMENT_STATE_RSP, 0),
    MXCP_CMD_ENTRY(MXCP_CMD_SET_CONSOLE_ROUTING,  mxcp_cmd_fn_set_console_routing, sizeof(mxcp_cmd_set_console_routing_t), MXCP_EVT_CONSOLE_ROUTING_RSP, 0),

    /* Element Commands */
    MXCP_CMD_ENTRY(MXCP_CMD_EL_SEND,              mxcp_cmd_fn_el_send,           0, /* variable */ 0, MXCP_EVT_EL_DATA_NOTIFY),

    /* GPIO Commands */
    MXCP_CMD_ENTRY(MXCP_CMD_GPIO_SET_LEVEL,       mxcp_cmd_fn_gpio_set_level,    sizeof(mxcp_cmd_gpio_set_level_t),    MXCP_EVT_GPIO_SET_LEVEL_RSP, 0),
    MXCP_CMD_ENTRY(MXCP_CMD_GPIO_GET_LEVEL,       mxcp_cmd_fn_gpio_get_level,    sizeof(mxcp_cmd_gpio_get_level_t),    MXCP_EVT_GPIO_GET_LEVEL_RSP, 0),
    MXCP_CMD_ENTRY(MXCP_CMD_GPIO_TOGGLE,          mxcp_cmd_fn_gpio_toggle,       sizeof(mxcp_cmd_gpio_toggle_t),       MXCP_EVT_GPIO_TOGGLE_RSP,    0),
    MXCP_CMD_ENTRY(MXCP_CMD_GPIO_SET_PWM_DUTY,    mxcp_cmd_fn_gpio_set_pwm_duty, sizeof(mxcp_cmd_gpio_set_pwm_duty_t), MXCP_EVT_GPIO_SET_PWM_DUTY_RSP, 0),
    MXCP_CMD_ENTRY(MXCP_CMD_GPIO_SET_PWM_FREQ,    mxcp_cmd_fn_gpio_set_pwm_freq, sizeof(mxcp_cmd_gpio_set_pwm_freq_t), MXCP_EVT_GPIO_SET_PWM_FREQ_RSP, 0),
    MXCP_CMD_ENTRY(MXCP_CMD_GPIO_INTR_ENABLE,     mxcp_cmd_fn_gpio_intr_enable,  sizeof(mxcp_cmd_gpio_intr_enable_t),  MXCP_EVT_GPIO_INTR_ENABLE_RSP, MXCP_EVT_GPIO_ASYNC),
    MXCP_CMD_ENTRY(MXCP_CMD_GPIO_INTR_DISABLE,    mxcp_cmd_fn_gpio_intr_disable, sizeof(mxcp_cmd_gpio_intr_disable_t), MXCP_EVT_GPIO_INTR_DISABLE_RSP, 0),
    MXCP_CMD_ENTRY(MXCP_CMD_GPIO_GET_CONFIG,      mxcp_cmd_fn_gpio_get_config,   sizeof(mxcp_cmd_gpio_get_config_t),   MXCP_EVT_GPIO_GET_CONFIG_RSP, 0),
    MXCP_CMD_ENTRY(MXCP_CMD_GPIO_GET_STATE,       mxcp_cmd_fn_gpio_get_state,    sizeof(mxcp_cmd_gpio_get_state_t),    MXCP_EVT_GPIO_GET_STATE_RSP,  0),
};

#define MXCP_CMD_TABLE_SIZE (sizeof(mxcp_cmd_table) / sizeof(mxcp_cmd_table[0]))
```

## 2. Event Table

The event table is primarily used for TX-side serialization and future host-side dispatch. On the engine side, events are sent via `mxcp_send_event()` which looks up the event ID to validate it exists.

```c
typedef struct {
    uint16_t  evt_id;         /* mxcp_evt_id_t */
    uint16_t  src_cmd_id;     /* Originating command ID (0 = unsolicited) */
    uint8_t   payload_size;   /* Expected payload size (0 = variable) */
} mxcp_evt_entry_t;

static const mxcp_evt_entry_t mxcp_evt_table[] = {
    /* System Events */
    { MXCP_EVT_PROV_COMP,            0,                        sizeof(mxcp_evt_prov_comp_t) },
    { MXCP_EVT_PROV_FAILED,          0,                        sizeof(mxcp_evt_prov_failed_t) },
    { MXCP_EVT_PROV_START,           0,                        0 },
    { MXCP_EVT_IDENTIFY_START,       0,                        0 },
    { MXCP_EVT_IDENTIFY_STOP,        0,                        0 },
    { MXCP_EVT_COMPOSITION_RSP,      MXCP_CMD_GET_COMPOSITION, 0 /* variable */ },
    { MXCP_EVT_ELEMENT_STATE_RSP,    MXCP_CMD_GET_ELEMENT_STATE, 0 /* variable */ },
    { MXCP_EVT_NODE_RESET_IND,       MXCP_CMD_NODE_RESET,      0 },
    { MXCP_EVT_HOSTED_MODE_RSP,      MXCP_CMD_HOSTED_MODE_ENABLE, 0 },
    { MXCP_EVT_CONSOLE_ROUTING_RSP,  MXCP_CMD_SET_CONSOLE_ROUTING, 0 },
    { MXCP_EVT_EL_DATA_NOTIFY,       MXCP_CMD_EL_SEND,         0 /* variable */ },

    /* GPIO Sync Responses */
    { MXCP_EVT_GPIO_SET_LEVEL_RSP,    MXCP_CMD_GPIO_SET_LEVEL,    0 },
    { MXCP_EVT_GPIO_GET_LEVEL_RSP,    MXCP_CMD_GPIO_GET_LEVEL,    sizeof(mxcp_evt_gpio_rsp_t) },
    { MXCP_EVT_GPIO_TOGGLE_RSP,       MXCP_CMD_GPIO_TOGGLE,       sizeof(mxcp_evt_gpio_rsp_t) },
    { MXCP_EVT_GPIO_SET_PWM_DUTY_RSP, MXCP_CMD_GPIO_SET_PWM_DUTY, 0 },
    { MXCP_EVT_GPIO_SET_PWM_FREQ_RSP, MXCP_CMD_GPIO_SET_PWM_FREQ, 0 },
    { MXCP_EVT_GPIO_INTR_ENABLE_RSP,  MXCP_CMD_GPIO_INTR_ENABLE,  0 },
    { MXCP_EVT_GPIO_INTR_DISABLE_RSP, MXCP_CMD_GPIO_INTR_DISABLE, 0 },
    { MXCP_EVT_GPIO_GET_CONFIG_RSP,   MXCP_CMD_GPIO_GET_CONFIG,   sizeof(mxcp_evt_gpio_rsp_t) },
    { MXCP_EVT_GPIO_GET_STATE_RSP,    MXCP_CMD_GPIO_GET_STATE,    sizeof(mxcp_evt_gpio_rsp_t) },
    { MXCP_EVT_GPIO_ASYNC,            0,                          sizeof(mxcp_evt_gpio_async_t) },
    { MXCP_EVT_GPIO_ERROR,            0,                          0 },
};

#define MXCP_EVT_TABLE_SIZE (sizeof(mxcp_evt_table) / sizeof(mxcp_evt_table[0]))
```

*(REQ-002, REQ-004)*

## 3. Dispatch Architecture

### 3.1 Single-Layer Dispatch Flow

```
UART RX byte → mxcp_serial_parse_byte() → state machine (unchanged)
    → on valid frame: mxcp_dispatch_frame(type, payload, len)
        → check bit 7 of type byte
        → if CMD (bit 7 = 0): linear search mxcp_cmd_table[] for matching cmd_id → invoke handler
        → if EVT (bit 7 = 1): (future) linear search mxcp_evt_table[] for host-side event handling
```

### 3.2 Dispatch Implementation

```c
void mxcp_dispatch_frame(uint8_t type, const uint8_t *payload, uint8_t len)
{
    if (MXCP_TYPE_IS_CMD(type)) {
        uint8_t cmd_id = MXCP_TYPE_ID(type);
        for (size_t i = 0; i < MXCP_CMD_TABLE_SIZE; i++) {
            if (mxcp_cmd_table[i].cmd_id == cmd_id) {
                mxcp_cmd_table[i].handler(payload, len);
                return;
            }
        }
        MESHX_LOGW(MODULE_ID_COMMON, "Unhandled MXCP CMD: 0x%02x", cmd_id);
    } else {
        uint8_t evt_id = MXCP_TYPE_ID(type);
        MESHX_LOGW(MODULE_ID_COMMON, "Unexpected EVT on engine RX: 0x%02x", evt_id);
    }
}
```

*(REQ-005)*

## 4. TX API — Unified Event Send

### 4.1 Core API

All Engine → Host communication uses a single function:

```c
meshx_err_t mxcp_send_event(mxcp_evt_id_t evt_id, const uint8_t *payload, uint8_t len);
```

This replaces:
- `mxsp_send_ctrl_event()` → `mxcp_send_event(MXCP_EVT_PROV_COMP, ...)`
- `mxsp_send_data_event()` → `mxcp_send_event(MXCP_EVT_EL_DATA_NOTIFY, ...)`
- `mxsp_send_gpio_rsp()` → `mxcp_send_event(MXCP_EVT_GPIO_GET_LEVEL_RSP, ...)`
- `mxsp_send_gpio_evt()` → `mxcp_send_event(MXCP_EVT_GPIO_ASYNC, ...)`

### 4.2 Host-Side TX API (Convenience)

```c
meshx_err_t mxcp_send_cmd(mxcp_cmd_id_t cmd_id, const uint8_t *payload, uint8_t len);
```

*(REQ-008)*
