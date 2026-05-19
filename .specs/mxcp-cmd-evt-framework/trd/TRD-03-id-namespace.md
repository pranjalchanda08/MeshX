# TRD-03 — Command and Event ID Namespace

## 1. Command IDs (Host → Engine, bit 7 = 0)

All commands live in a single flat enum. The raw TYPE byte on the wire is the enum value (bit 7 = 0 for CMD).

```c
typedef enum {
    /* --- System Commands (0x01 - 0x0F) --- */
    MXCP_CMD_HOSTED_MODE_ENABLE    = 0x01,
    MXCP_CMD_NODE_RESET            = 0x02,
    MXCP_CMD_GET_COMPOSITION       = 0x03,
    MXCP_CMD_GET_ELEMENT_STATE     = 0x04,
    MXCP_CMD_SET_CONSOLE_ROUTING   = 0x05,

    /* --- Element Commands (0x10 - 0x1F) --- */
    MXCP_CMD_EL_SEND               = 0x10,

    /* --- GPIO Commands (0x20 - 0x3F) --- */
    MXCP_CMD_GPIO_SET_LEVEL        = 0x21,
    MXCP_CMD_GPIO_GET_LEVEL        = 0x22,
    MXCP_CMD_GPIO_TOGGLE           = 0x23,
    MXCP_CMD_GPIO_SET_PWM_DUTY     = 0x24,
    MXCP_CMD_GPIO_SET_PWM_FREQ     = 0x25,
    MXCP_CMD_GPIO_INTR_ENABLE      = 0x26,
    MXCP_CMD_GPIO_INTR_DISABLE     = 0x27,
    MXCP_CMD_GPIO_GET_CONFIG       = 0x28,
    MXCP_CMD_GPIO_GET_STATE        = 0x29,

    MXCP_CMD_MAX
} mxcp_cmd_id_t;
```

## 2. Event IDs (Engine → Host, bit 7 = 1)

Events use the same 7-bit ID space with bit 7 set. On the wire, the TYPE byte = `0x80 | id`.

```c
typedef enum {
    /* --- System Events (0x01 - 0x0F) --- */
    MXCP_EVT_PROV_COMP             = 0x01,
    MXCP_EVT_PROV_FAILED           = 0x02,
    MXCP_EVT_PROV_START            = 0x03,
    MXCP_EVT_IDENTIFY_START        = 0x04,
    MXCP_EVT_IDENTIFY_STOP         = 0x05,
    MXCP_EVT_COMPOSITION_RSP       = 0x06,
    MXCP_EVT_ELEMENT_STATE_RSP     = 0x07,
    MXCP_EVT_NODE_RESET_IND        = 0x08,
    MXCP_EVT_HOSTED_MODE_RSP       = 0x09,
    MXCP_EVT_CONSOLE_ROUTING_RSP   = 0x0A,

    /* --- Element/Data Events (0x10 - 0x1F) --- */
    MXCP_EVT_EL_DATA_NOTIFY        = 0x10,

    /* --- GPIO Events (0x20 - 0x3F) --- */
    MXCP_EVT_GPIO_SET_LEVEL_RSP    = 0x21,
    MXCP_EVT_GPIO_GET_LEVEL_RSP    = 0x22,
    MXCP_EVT_GPIO_TOGGLE_RSP       = 0x23,
    MXCP_EVT_GPIO_SET_PWM_DUTY_RSP = 0x24,
    MXCP_EVT_GPIO_SET_PWM_FREQ_RSP = 0x25,
    MXCP_EVT_GPIO_INTR_ENABLE_RSP  = 0x26,
    MXCP_EVT_GPIO_INTR_DISABLE_RSP = 0x27,
    MXCP_EVT_GPIO_GET_CONFIG_RSP   = 0x28,
    MXCP_EVT_GPIO_GET_STATE_RSP    = 0x29,
    MXCP_EVT_GPIO_ASYNC            = 0x3E,
    MXCP_EVT_GPIO_ERROR            = 0x3F,

    MXCP_EVT_MAX
} mxcp_evt_id_t;
```

*(REQ-001, REQ-005)*
