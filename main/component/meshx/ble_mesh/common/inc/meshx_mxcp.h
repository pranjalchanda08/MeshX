/**
 * @file meshx_mxcp.h
 * @brief MeshX Command Protocol (MXCP) Definitions
 *
 * Unified, single-layer, table-driven command/event protocol for
 * communication between the MeshX Engine and an external Host MCU.
 *
 * Frame Format (identical structure to legacy MXSP):
 *   [SOF][LEN][TYPE][PAYLOAD][CHK][EOF]
 *
 * TYPE byte encoding:
 *   Bit 7:     Direction (0 = CMD Host→Engine, 1 = EVT Engine→Host)
 *   Bits 6-0:  Command/Event ID
 *
 * @author Pranjal Chanda
 * @date 2026-05-19
 * @copyright Copyright 2024 - 2025 MeshX
 */

#ifndef __MESHX_MXCP_H__
#define __MESHX_MXCP_H__

#include "meshx_common.h"
#include "meshx_api.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MXCP_SOF               0xFE
#define MXCP_EOF               0xEF
#define MXCP_PAYLOAD_MAX_SIZE  255

#define MXCP_TYPE_DIR_CMD  0x00
#define MXCP_TYPE_DIR_EVT  0x80
#define MXCP_TYPE_ID_MASK  0x7F

#define MXCP_MAKE_TYPE(dir, id)  ((uint8_t)((dir) | ((id) & MXCP_TYPE_ID_MASK)))
#define MXCP_TYPE_IS_CMD(t)      (((t) & 0x80) == 0)
#define MXCP_TYPE_IS_EVT(t)      (((t) & 0x80) != 0)
#define MXCP_TYPE_ID(t)          ((t) & MXCP_TYPE_ID_MASK)

/**
 * @brief Command ID namespace (Host -> Engine, bit 7 = 0).
 */
typedef enum {
    MXCP_CMD_HOSTED_MODE_ENABLE    = 0x01,
    MXCP_CMD_NODE_RESET            = 0x02,
    MXCP_CMD_GET_COMPOSITION       = 0x03,
    MXCP_CMD_GET_ELEMENT_STATE     = 0x04,
    MXCP_CMD_SET_CONSOLE_ROUTING   = 0x05,

    MXCP_CMD_EL_SEND               = 0x10,

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

/**
 * @brief Event ID namespace (Engine -> Host, bit 7 = 1).
 */
typedef enum {
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

    MXCP_EVT_EL_DATA_NOTIFY        = 0x10,

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

#pragma pack(push, 1)

typedef struct {
    uint8_t enable;
} mxcp_cmd_hosted_mode_enable_t;

typedef struct {
    uint8_t enable;
} mxcp_cmd_set_console_routing_t;

typedef struct {
    uint16_t element_id;
    uint16_t element_type;
    uint16_t func_id;
    uint16_t msg_len;
} mxcp_cmd_el_send_t;

typedef struct {
    uint8_t logical_pin;
    uint8_t level;
} mxcp_cmd_gpio_set_level_t;

typedef struct {
    uint8_t logical_pin;
} mxcp_cmd_gpio_get_level_t;

typedef struct {
    uint8_t logical_pin;
} mxcp_cmd_gpio_toggle_t;

typedef struct {
    uint8_t  logical_pin;
    uint8_t  duty_cycle;
} mxcp_cmd_gpio_set_pwm_duty_t;

typedef struct {
    uint8_t  logical_pin;
    uint32_t frequency;
} mxcp_cmd_gpio_set_pwm_freq_t;

typedef struct {
    uint8_t logical_pin;
    uint8_t enable;
} mxcp_cmd_gpio_intr_enable_t;

typedef struct {
    uint8_t logical_pin;
} mxcp_cmd_gpio_intr_disable_t;

typedef struct {
    uint8_t logical_pin;
} mxcp_cmd_gpio_get_config_t;

typedef struct {
    uint8_t logical_pin;
} mxcp_cmd_gpio_get_state_t;

typedef struct {
    uint16_t net_idx;
    uint16_t addr;
    uint8_t  device_uuid[16];
} mxcp_evt_prov_comp_t;

typedef struct {
    uint8_t reason;
} mxcp_evt_prov_failed_t;

typedef struct {
    uint8_t element_count;
} mxcp_evt_composition_rsp_t;

typedef struct {
    uint8_t element_count;
} mxcp_evt_element_state_rsp_t;

typedef struct {
    uint16_t element_id;
    uint16_t element_type;
    uint16_t func_id;
    uint16_t msg_len;
} mxcp_evt_el_data_notify_t;

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
} mxcp_evt_gpio_rsp_t;

typedef struct {
    uint8_t  event_type;
    uint8_t  logical_pin;
    uint8_t  value;
    uint32_t timestamp;
} mxcp_evt_gpio_async_t;

typedef struct {
    uint16_t idx;
    uint16_t variant;
    uint16_t type;
} mxcp_comp_entry_header_t;

typedef struct {
    uint16_t idx;
    uint16_t variant;
    uint16_t ctx_size;
} mxcp_state_entry_header_t;

#pragma pack(pop)

/**
 * @brief MXCP wire frame for RX-side parsing.
 *
 * Layout: [SOF][LEN][TYPE][PAYLOAD...][CHK][EOF]
 * NOTE: Used for RX-side parsing only. TX uses flat-buffer
 *       serialisation for variable-length wire efficiency.
 */
typedef struct {
    uint8_t len;
    uint8_t type;
    uint8_t payload[MXCP_PAYLOAD_MAX_SIZE];
    uint8_t checksum;
} mxcp_frame_t;

/**
 * @brief MXCP command handler function signature.
 *
 * @param payload  Pointer to payload bytes from the frame.
 * @param len      Payload length in bytes.
 */
typedef void (*mxcp_cmd_handler_t)(const uint8_t *payload, uint8_t len);

typedef struct {
    uint8_t              cmd_id;
    mxcp_cmd_handler_t   handler;
    uint8_t              payload_size;
    uint8_t              sync_evt_id;
    uint8_t              async_evt_id;
} mxcp_cmd_entry_t;

typedef struct {
    uint8_t  evt_id;
    uint8_t  src_cmd_id;
    uint8_t  payload_size;
} mxcp_evt_entry_t;

#define MXCP_CMD_ENTRY(id, fn, psz, sync_evt, async_evt) \
    { (id), (fn), (psz), (sync_evt), (async_evt) }

/**
 * @brief Dispatch a received MXCP frame to the matching command handler.
 *
 * @param type    TYPE byte from the frame (direction + ID).
 * @param payload Pointer to the payload bytes.
 * @param len     Payload length in bytes.
 */
void mxcp_dispatch_frame(uint8_t type, const uint8_t *payload, uint8_t len);

/**
 * @brief Build and transmit an MXCP frame over the serial interface.
 *
 * Frames are only sent when hosted mode is enabled.
 *
 * @param type    TYPE byte (direction + ID).
 * @param payload Pointer to payload bytes (may be NULL if len == 0).
 * @param len     Payload length in bytes.
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t mxcp_send_frame(uint8_t type, const uint8_t *payload, uint8_t len);

/**
 * @brief Send an MXCP event (Engine -> Host).
 *
 * @param evt_id  Event ID from @ref mxcp_evt_id_t.
 * @param payload Pointer to the typed event payload.
 * @param len     Payload length in bytes.
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t mxcp_send_event(mxcp_evt_id_t evt_id, const uint8_t *payload, uint8_t len);

/**
 * @brief Send an MXCP command (Host -> Engine).
 *
 * @param cmd_id  Command ID from @ref mxcp_cmd_id_t.
 * @param payload Pointer to the typed command payload.
 * @param len     Payload length in bytes.
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t mxcp_send_cmd(mxcp_cmd_id_t cmd_id, const uint8_t *payload, uint8_t len);

/**
 * @brief Send a composition or element-state response to the host.
 *
 * Queries the composition builder for the requested data and sends it
 * as an MXCP event.
 *
 * @param evt_id  MXCP_EVT_COMPOSITION_RSP or MXCP_EVT_ELEMENT_STATE_RSP.
 */
void mxcp_send_element_info_response(mxcp_evt_id_t evt_id);

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_MXCP_H__ */
