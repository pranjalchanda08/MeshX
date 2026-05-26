/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_api.h
 * @brief This file contains the API definitions for the MeshX application.
 */

#ifndef __MESHX_API_H__
#define __MESHX_API_H__

#include "unit_test.h"
#include <meshx_common.h>
#include <meshx_control_task.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MESHX_APP_API_MSG_MAX_SIZE  377 /* Size of the maximum payload in MeshX */

/* MeshX Function ID Relay Server */
#define MESHX_ELEMENT_FUNC_ID_RELAY_SERVER_ONN_OFF          0x00

/* MeshX Function ID Light CWWW Server */
#define MESHX_ELEMENT_FUNC_ID_LIGHT_CWWW_SERVER_ONN_OFF     0x00
#define MESHX_ELEMENT_FUNC_ID_LIGHT_CWWW_SERVER_CTL         0x01

/* MeshX Function ID Light HSL Server */
#define MESHX_ELEMENT_FUNC_ID_LIGHT_HSL_SERVER_ONN_OFF      0x00
#define MESHX_ELEMENT_FUNC_ID_LIGHT_HSL_SERVER_HSL          0x01

/* MeshX Function ID Light CWWW Client */
#define MESHX_ELEMENT_FUNC_ID_LIGHT_CWWW_CLIENT_ONN_OFF     0x00
#define MESHX_ELEMENT_FUNC_ID_LIGHT_CWWW_CLIENT_CTL         0x01

/* MeshX Function ID Sensor Server */
#define MESHX_ELEMENT_FUNC_ID_SENSOR_SERVER_DATA            0x00

/* MeshX Function ID Sensor Client */
#define MESHX_ELEMENT_FUNC_ID_SENSOR_CLIENT_DATA            0x00

/* MeshX Function ID Light HSL Client */
#define MESHX_ELEMENT_FUNC_ID_LIGHT_HSL_CLIENT_ONN_OFF      0x00
#define MESHX_ELEMENT_FUNC_ID_LIGHT_HSL_CLIENT_HSL          0x01

/**
 * @brief Enumeration of Client Logical Model error codes.
 */
typedef enum meshx_api_client_err
{
    MESHX_CLIENT_ERR_SUCCESS = 0x00,          /**< Operation successful */
    MESHX_CLIENT_ERR_TIMEOUT = 0x01,          /**< TXCM timeout */
    MESHX_CLIENT_ERR_UNCONFIGURED = 0x02,     /**< No pub_addr configured */
} meshx_api_client_err_t;

/* Unified Direction Flags (matches MXCP TYPE bit 15) */
#define MESHX_MSG_DIR_CMD 0x0000
#define MESHX_MSG_DIR_EVT 0x8000

/**
 * @brief Unified Data Path Message IDs
 */
typedef enum {
    MESHX_MSG_DATA_CMD_SEND       = 0x10 | MESHX_MSG_DIR_CMD,
    MESHX_MSG_DATA_EVT_RX_NOTIFY  = 0x10 | MESHX_MSG_DIR_EVT,
    MESHX_MSG_DATA_EVT_TX_NOTIFY  = 0x11 | MESHX_MSG_DIR_EVT,
} meshx_msg_data_id_t;

/**
 * @brief Unified Control Path Command IDs
 */
typedef enum {
    MESHX_MSG_CTRL_CMD_HOSTED_MODE_ENABLE    = 0x01 | MESHX_MSG_DIR_CMD,
    MESHX_MSG_CTRL_CMD_NODE_RESET            = 0x02 | MESHX_MSG_DIR_CMD,
    MESHX_MSG_CTRL_CMD_GET_COMPOSITION       = 0x03 | MESHX_MSG_DIR_CMD,
    MESHX_MSG_CTRL_CMD_GET_ELEMENT_STATE     = 0x04 | MESHX_MSG_DIR_CMD,
    MESHX_MSG_CTRL_CMD_SET_CONSOLE_ROUTING   = 0x05 | MESHX_MSG_DIR_CMD,

#ifdef CONFIG_MESHX_ENABLE_GPIO_TEST_API
    MESHX_MSG_CTRL_CMD_GPIO_SET_LEVEL        = 0x21 | MESHX_MSG_DIR_CMD,
    MESHX_MSG_CTRL_CMD_GPIO_GET_LEVEL        = 0x22 | MESHX_MSG_DIR_CMD,
    MESHX_MSG_CTRL_CMD_GPIO_TOGGLE           = 0x23 | MESHX_MSG_DIR_CMD,
    MESHX_MSG_CTRL_CMD_GPIO_SET_PWM_DUTY     = 0x24 | MESHX_MSG_DIR_CMD,
    MESHX_MSG_CTRL_CMD_GPIO_SET_PWM_FREQ     = 0x25 | MESHX_MSG_DIR_CMD,
    MESHX_MSG_CTRL_CMD_GPIO_INTR_ENABLE      = 0x26 | MESHX_MSG_DIR_CMD,
    MESHX_MSG_CTRL_CMD_GPIO_INTR_DISABLE     = 0x27 | MESHX_MSG_DIR_CMD,
    MESHX_MSG_CTRL_CMD_GPIO_GET_CONFIG       = 0x28 | MESHX_MSG_DIR_CMD,
    MESHX_MSG_CTRL_CMD_GPIO_GET_STATE        = 0x29 | MESHX_MSG_DIR_CMD,
#endif
} meshx_msg_ctrl_cmd_t;

/**
 * @brief Unified Control Path Event IDs
 */
typedef enum {
    MESHX_MSG_CTRL_EVT_NONE                  = 0x00,
    MESHX_MSG_CTRL_EVT_PROV_COMP             = 0x01 | MESHX_MSG_DIR_EVT,
    MESHX_MSG_CTRL_EVT_PROV_FAILED           = 0x02 | MESHX_MSG_DIR_EVT,
    MESHX_MSG_CTRL_EVT_IDENTIFY_START        = 0x03 | MESHX_MSG_DIR_EVT,
    MESHX_MSG_CTRL_EVT_IDENTIFY_STOP         = 0x04 | MESHX_MSG_DIR_EVT,
    MESHX_MSG_CTRL_EVT_COMPOSITION_RSP       = 0x05 | MESHX_MSG_DIR_EVT,
    MESHX_MSG_CTRL_EVT_ELEMENT_STATE_RSP     = 0x06 | MESHX_MSG_DIR_EVT,
    MESHX_MSG_CTRL_EVT_NODE_RESET_IND        = 0x07 | MESHX_MSG_DIR_EVT,
    MESHX_MSG_CTRL_EVT_HOSTED_MODE_RSP       = 0x08 | MESHX_MSG_DIR_EVT,
    MESHX_MSG_CTRL_EVT_CONSOLE_ROUTING_RSP   = 0x09 | MESHX_MSG_DIR_EVT,

#ifdef CONFIG_MESHX_ENABLE_GPIO_TEST_API
    MESHX_MSG_CTRL_EVT_GPIO_SET_LEVEL_RSP    = 0x21 | MESHX_MSG_DIR_EVT,
    MESHX_MSG_CTRL_EVT_GPIO_GET_LEVEL_RSP    = 0x22 | MESHX_MSG_DIR_EVT,
    MESHX_MSG_CTRL_EVT_GPIO_TOGGLE_RSP       = 0x23 | MESHX_MSG_DIR_EVT,
    MESHX_MSG_CTRL_EVT_GPIO_SET_PWM_DUTY_RSP = 0x24 | MESHX_MSG_DIR_EVT,
    MESHX_MSG_CTRL_EVT_GPIO_SET_PWM_FREQ_RSP = 0x25 | MESHX_MSG_DIR_EVT,
    MESHX_MSG_CTRL_EVT_GPIO_INTR_ENABLE_RSP  = 0x26 | MESHX_MSG_DIR_EVT,
    MESHX_MSG_CTRL_EVT_GPIO_INTR_DISABLE_RSP = 0x27 | MESHX_MSG_DIR_EVT,
    MESHX_MSG_CTRL_EVT_GPIO_GET_CONFIG_RSP   = 0x28 | MESHX_MSG_DIR_EVT,
    MESHX_MSG_CTRL_EVT_GPIO_GET_STATE_RSP    = 0x29 | MESHX_MSG_DIR_EVT,
    MESHX_MSG_CTRL_EVT_GPIO_ASYNC            = 0x3E | MESHX_MSG_DIR_EVT,
    MESHX_MSG_CTRL_EVT_GPIO_ERROR            = 0x3F | MESHX_MSG_DIR_EVT,
#endif
} meshx_msg_ctrl_evt_t;

#pragma pack(push, 1)

/**
 * @brief Unified Data Path Message Structure
 */
typedef struct {
    uint16_t msg_id;          /* meshx_msg_data_id_t */
    uint16_t element_id;
    uint16_t element_type;
    uint16_t func_id;
    uint16_t payload_len;     /* Excludes this header */
    uint8_t  payload[];       /* Variable-length payload */
} meshx_msg_data_t;

/**
 * @brief Unified Control Path Message Structure
 */
typedef struct {
    uint16_t msg_id;          /* meshx_msg_ctrl_cmd_t or meshx_msg_ctrl_evt_t */
    uint16_t payload_len;     /* Excludes this header */
    uint8_t  payload[];       /* Variable-length payload */
} meshx_msg_ctrl_t;

/* --- Unified Control Payloads --- */

typedef struct {
    uint8_t enable;
} meshx_msg_hosted_mode_enable_t;

typedef struct {
    uint8_t enable;
} meshx_msg_set_console_routing_t;

#ifdef CONFIG_MESHX_ENABLE_GPIO_TEST_API
typedef struct {
    uint8_t logical_pin;
    uint8_t level;
} meshx_msg_gpio_set_level_t;

typedef struct {
    uint8_t logical_pin;
} meshx_msg_gpio_get_level_t;

typedef struct {
    uint8_t logical_pin;
} meshx_msg_gpio_toggle_t;

typedef struct {
    uint8_t  logical_pin;
    uint8_t  duty_cycle;
} meshx_msg_gpio_set_pwm_duty_t;

typedef struct {
    uint8_t  logical_pin;
    uint32_t frequency;
} meshx_msg_gpio_set_pwm_freq_t;

typedef struct {
    uint8_t logical_pin;
    uint8_t enable;
} meshx_msg_gpio_intr_enable_t;

typedef struct {
    uint8_t logical_pin;
} meshx_msg_gpio_intr_disable_t;

typedef struct {
    uint8_t logical_pin;
} meshx_msg_gpio_get_config_t;

typedef struct {
    uint8_t logical_pin;
} meshx_msg_gpio_get_state_t;

typedef struct {
    uint8_t  status;
    uint8_t  logical_pin;
    union {
        struct {
            uint8_t level;
        } get_level;
        struct {
            uint8_t level;
        } toggle;
        struct {
            uint8_t mode;
            uint8_t pull;
            uint8_t drive_strength;
            uint8_t initial_level;
            uint8_t signal_inversion;
        } get_config;
        struct {
            uint8_t current_level;
            uint8_t interrupt_registered;
            uint8_t intr_type;
        } get_state;
    } data;
} meshx_msg_gpio_rsp_t;

typedef struct {
    uint8_t  event_type;
    uint8_t  logical_pin;
    uint8_t  value;
    uint32_t timestamp;
} meshx_msg_gpio_async_t;
#endif

typedef struct {
    uint16_t net_idx;
    uint16_t addr;
    uint8_t  device_uuid[16];
} meshx_msg_prov_comp_t;

typedef struct {
    uint8_t reason;
} meshx_msg_prov_failed_t;

typedef struct {
    uint8_t element_count;
} meshx_msg_composition_rsp_t;

typedef struct {
    uint8_t element_count;
} meshx_msg_element_state_rsp_t;

typedef struct {
    uint16_t idx;
    uint16_t variant;
    uint16_t type;
} meshx_comp_entry_header_t;

typedef struct {
    uint16_t idx;
    uint16_t variant;
    uint16_t ctx_size;
    uint16_t telemetry_size;
} meshx_state_entry_header_t;


/* --- Application Data Payloads --- */

typedef struct meshx_api_relay_server_evt {
   uint8_t on_off;
} meshx_api_relay_server_evt_t;

typedef struct meshx_api_light_cwww_server_evt {
    union {
        struct { uint8_t state; } on_off;
        struct {
            uint16_t lightness;
            uint16_t temperature;
            uint16_t delta_uv;
            uint16_t temp_range_min;
            uint16_t temp_range_max;
        } ctl;
    } state_change;
} meshx_api_light_cwww_server_evt_t;

typedef struct meshx_api_light_hsl_server_evt {
    union {
        struct { uint8_t state; } on_off;
        struct {
            uint16_t lightness;
            uint16_t hue;
            uint16_t saturation;
        } hsl;
    } state_change;
} meshx_api_light_hsl_server_evt_t;

typedef struct meshx_api_sensor_server_evt {
    union {
        struct { uint16_t value; } data;
    } state_change;
} meshx_api_sensor_server_evt_t;

typedef struct meshx_api_light_hsl_client_evt {
   uint8_t err_code;
    union {
        struct { uint8_t state; } on_off;
        struct {
            uint16_t lightness;
            uint16_t hue;
            uint16_t saturation;
        } hsl;
    } state_change;
} meshx_api_light_hsl_client_evt_t;

typedef struct meshx_api_sensor_client_evt {
   uint8_t err_code;
    union {
        struct { uint16_t value; } data;
    } state_change;
} meshx_api_sensor_client_evt_t;

typedef struct meshx_api_relay_client_state {
   uint8_t err_code;
   uint8_t on_off;
} meshx_api_relay_client_evt_t;

typedef struct meshx_api_light_cwww_client_evt {
   uint8_t err_code;
   union {
        struct { uint8_t state; } on_off;
        struct {
            uint16_t lightness;
            uint16_t temperature;
            uint16_t delta_uv;
            uint16_t temp_range_min;
            uint16_t temp_range_max;
        } ctl;
    } state_change;
} meshx_api_light_cwww_client_evt_t;

#pragma pack(pop)

/**
 * @brief Unified Receive Callbacks
 */
typedef void (*meshx_api_data_cb_t)(const meshx_msg_data_t *msg);
typedef void (*meshx_api_ctrl_cb_t)(const meshx_msg_ctrl_t *msg);

/**
 * @brief Unified Send APIs
 */
meshx_err_t meshx_api_data_send(const meshx_msg_data_t *msg);
meshx_err_t meshx_api_ctrl_send(const meshx_msg_ctrl_t *msg);
meshx_err_t meshx_api_ctrl_send_with_payload(uint16_t msg_id, const void *payload, uint16_t payload_len);

/**
 * @brief Register Unified Receive Callbacks
 */
meshx_err_t meshx_api_register_data_cb(meshx_api_data_cb_t cb);
meshx_err_t meshx_api_register_ctrl_cb(meshx_api_ctrl_cb_t cb);

/**
 * @brief Get the network key identifier.
 * @return uint16_t The network key identifier.
 */
uint16_t meshx_get_net_key_id(void);

/**
 * @brief Get the node's primary unicast address.
 * @return uint16_t The node's primary unicast address, or 0x0000 if unprovisioned.
 */
uint16_t meshx_get_node_addr(void);

/**
 * @brief Get the total number of elements on this node.
 * @return size_t The element count.
 */
size_t meshx_get_element_count(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __MESHX_API_H__ */
