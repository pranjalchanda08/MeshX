# TRD-04 — Typed Payload Structures

## 1. System Command Payloads

```c
#pragma pack(push, 1)

/* CMD: HOSTED_MODE_ENABLE */
typedef struct {
    uint8_t enable;            /* 0=disable, non-zero=enable */
} mxcp_cmd_hosted_mode_enable_t;

/* CMD: SET_CONSOLE_ROUTING */
typedef struct {
    uint8_t enable;
} mxcp_cmd_set_console_routing_t;

/* CMD: EL_SEND */
typedef struct {
    uint16_t element_id;
    uint16_t element_type;
    uint16_t func_id;
    uint16_t msg_len;
    /* followed by msg_len bytes of element-specific data */
} mxcp_cmd_el_send_t;

/* CMD: NODE_RESET, GET_COMPOSITION, GET_ELEMENT_STATE — no payload */
```

## 2. GPIO Command Payloads

```c
/* CMD: GPIO_SET_LEVEL */
typedef struct {
    uint8_t logical_pin;
    uint8_t level;
} mxcp_cmd_gpio_set_level_t;

/* CMD: GPIO_GET_LEVEL */
typedef struct {
    uint8_t logical_pin;
} mxcp_cmd_gpio_get_level_t;

/* CMD: GPIO_TOGGLE */
typedef struct {
    uint8_t logical_pin;
} mxcp_cmd_gpio_toggle_t;

/* CMD: GPIO_SET_PWM_DUTY */
typedef struct {
    uint8_t  logical_pin;
    uint8_t  duty_cycle;
} mxcp_cmd_gpio_set_pwm_duty_t;

/* CMD: GPIO_SET_PWM_FREQ */
typedef struct {
    uint8_t  logical_pin;
    uint32_t frequency;
} mxcp_cmd_gpio_set_pwm_freq_t;

/* CMD: GPIO_INTR_ENABLE */
typedef struct {
    uint8_t logical_pin;
    uint8_t enable;
} mxcp_cmd_gpio_intr_enable_t;

/* CMD: GPIO_INTR_DISABLE */
typedef struct {
    uint8_t logical_pin;
} mxcp_cmd_gpio_intr_disable_t;

/* CMD: GPIO_GET_CONFIG */
typedef struct {
    uint8_t logical_pin;
} mxcp_cmd_gpio_get_config_t;

/* CMD: GPIO_GET_STATE */
typedef struct {
    uint8_t logical_pin;
} mxcp_cmd_gpio_get_state_t;
```

## 3. System Event Payloads

```c
/* EVT: PROV_COMP */
typedef struct {
    uint16_t net_idx;
    uint16_t addr;
    uint8_t  device_uuid[16];
} mxcp_evt_prov_comp_t;

/* EVT: PROV_FAILED */
typedef struct {
    uint8_t reason;
} mxcp_evt_prov_failed_t;

/* EVT: COMPOSITION_RSP */
typedef struct {
    uint8_t element_count;
    /* followed by element_count entries:
       { uint16_t idx; uint16_t variant; uint16_t type; char name[]; } */
} mxcp_evt_composition_rsp_t;

/* EVT: ELEMENT_STATE_RSP */
typedef struct {
    uint8_t element_count;
    /* followed by element_count entries:
       { uint16_t idx; uint16_t variant; uint16_t ctx_size; uint8_t ctx[]; } */
} mxcp_evt_element_state_rsp_t;

/* EVT: EL_DATA_NOTIFY */
typedef struct {
    uint16_t element_id;
    uint16_t element_type;
    uint16_t func_id;
    uint16_t msg_len;
    /* followed by msg_len bytes of element-specific data */
} mxcp_evt_el_data_notify_t;
```

## 4. GPIO Event Payloads

```c
/* EVT: GPIO Generic Response (used for all GPIO sync responses) */
typedef struct {
    uint8_t  status;           /* 0=success, error code otherwise */
    uint8_t  logical_pin;
    uint8_t  response_len;
    uint8_t  response[8];      /* Typed response data, interpretation depends on event ID */
} mxcp_evt_gpio_rsp_t;

/* EVT: GPIO_ASYNC (interrupts, level changes) */
typedef struct {
    uint8_t  event_type;       /* MXCP_GPIO_EVT_LEVEL_CHANGE, etc. */
    uint8_t  logical_pin;
    uint8_t  value;
    uint32_t timestamp;
} mxcp_evt_gpio_async_t;
```

```c
#pragma pack(pop)
```

*(REQ-002, REQ-003)*
