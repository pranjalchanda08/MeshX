/**
 * @file meshx_serial.h
 * @brief MeshX Serial Protocol (MXSP) Definitions
 *
 * This file defines the frame structure and APIs for the MeshX Serial Protocol,
 * used for communication between the Mesh engine and an external Host MCU.
 */

#ifndef __MESHX_SERIAL_H__
#define __MESHX_SERIAL_H__

#include "meshx_common.h"
#include "meshx_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MXSP_SOF 0xFE
#define MXSP_EOF 0xEF

#define MXSP_PAYLOAD_MAX_SIZE 255

typedef enum {
    MXSP_MSG_TYPE_SYS_EVT_NOTIFY  = 0xB1, // Engine -> Host (Control Path)
    MXSP_MSG_TYPE_DATA_EVT_NOTIFY = 0xB2, // Engine -> Host (Data Path)
    MXSP_MSG_TYPE_EL_CMD_SEND     = 0xC1, // Host -> Engine (Command Path)
    MXSP_MSG_TYPE_SYS_CMD_SEND    = 0xC2, // Host -> Engine (System Command)
    MXSP_MSG_TYPE_HOSTED_MODE     = 0x03, // Internal/Setup
    /* GPIO Hosted Mode Message Types */
    MXSP_MSG_TYPE_GPIO_CMD        = 0xD1, // Host -> Engine (GPIO Command)
    MXSP_MSG_TYPE_GPIO_RSP        = 0xD2, // Engine -> Host (GPIO Response)
    MXSP_MSG_TYPE_GPIO_EVT        = 0xD3, // Engine -> Host (GPIO Event - async)
} mxsp_msg_type_t;

/**
 * @brief Set the hosted mode state
 *
 * @param enabled True to enable, false to disable
 */
void meshx_serial_set_hosted_mode(bool enabled);

/**
 * @brief Check if hosted mode is enabled
 *
 * @return true if enabled, false otherwise
 */
bool meshx_serial_is_hosted_mode_enabled(void);

/**
 * @brief GPIO Command Types (Host -> Engine)
 */
typedef enum {
    MXSP_GPIO_CMD_SET_LEVEL       = 0x01, /**< Set GPIO pin level */
    MXSP_GPIO_CMD_GET_LEVEL       = 0x02, /**< Get GPIO pin level */
    MXSP_GPIO_CMD_TOGGLE          = 0x03, /**< Toggle GPIO pin */
    MXSP_GPIO_CMD_SET_PWM_DUTY    = 0x04, /**< Set PWM duty cycle */
    MXSP_GPIO_CMD_SET_PWM_FREQ    = 0x05, /**< Set PWM frequency */
    MXSP_GPIO_CMD_INTR_ENABLE     = 0x06, /**< Enable/disable interrupt */
    MXSP_GPIO_CMD_INTR_DISABLE    = 0x07, /**< Disable interrupt */
    MXSP_GPIO_CMD_GET_CONFIG      = 0x08, /**< Get pin configuration */
    MXSP_GPIO_CMD_GET_STATE       = 0x09, /**< Get pin state */
} mxsp_gpio_cmd_t;

/**
 * @brief GPIO Event Types (Engine -> Host, async)
 */
typedef enum {
    MXSP_GPIO_EVT_LEVEL_CHANGE    = 0x01, /**< Pin level changed */
    MXSP_GPIO_EVT_INTERRUPT       = 0x02, /**< Interrupt triggered */
    MXSP_GPIO_EVT_MODE_CHANGE     = 0x03, /**< Mode changed */
    MXSP_GPIO_EVT_ERROR           = 0xFF, /**< Error event */
} mxsp_gpio_evt_t;

/**
 * @brief GPIO Command Payload (Host -> Engine)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t cmd;           /**< Command type (mxsp_gpio_cmd_t) */
    uint8_t logical_pin;   /**< Logical pin number (0-255) */
    uint8_t reserved;      /**< Reserved for alignment */
    uint8_t payload_len;   /**< Payload length (0-8 bytes) */
    uint8_t payload[8];    /**< Command-specific payload */
} mxsp_gpio_cmd_payload_t;

/**
 * @brief GPIO Response Payload (Engine -> Host)
 */
typedef struct {
    uint8_t cmd;           /**< Original command type */
    uint8_t logical_pin;   /**< Logical pin number */
    uint8_t status;        /**< Status (0 = success, error code otherwise) */
    uint8_t response_len;  /**< Response data length */
    uint8_t response[8];   /**< Response data */
} mxsp_gpio_rsp_payload_t;

/**
 * @brief GPIO Event Payload (Engine -> Host, async)
 */
typedef struct {
    uint8_t event_type;    /**< Event type (mxsp_gpio_evt_t) */
    uint8_t logical_pin;   /**< Logical pin number */
    uint8_t value;         /**< Event value */
    uint8_t reserved;      /**< Reserved for alignment */
    uint32_t timestamp;    /**< Event timestamp (ms since boot) */
} mxsp_gpio_evt_payload_t;
#pragma pack(pop)

/**
 * @brief MXSP Frame structure
 */
typedef struct {
    uint8_t sof;
    uint8_t len;
    uint8_t type;
    uint8_t payload[MXSP_PAYLOAD_MAX_SIZE];
    uint8_t checksum;
    uint8_t eof;
} mxsp_frame_t;

/**
 * @brief Initialize the MeshX Serial Protocol handler
 *
 * @return meshx_err_t MESHX_SUCCESS on success
 */
meshx_err_t meshx_serial_init(void);

/**
 * @brief Parse incoming byte stream for MXSP frames
 *
 * @param data Byte to parse
 */
void meshx_serial_parse_byte(uint8_t data);

/**
 * @brief Send a control event via MXSP
 *
 * @param evt_header Control message header
 * @param payload Control message payload
 * @return meshx_err_t MESHX_SUCCESS on success
 */
meshx_err_t mxsp_send_ctrl_event(const meshx_ctrl_msg_header_t *evt_header, const meshx_ctrl_payload_t *payload);

/**
 * @brief Send data event via MXSP
 *
 * @param msg_hdr Data message header
 * @param payload Data message payload
 * @return meshx_err_t MESHX_SUCCESS on success
 */
meshx_err_t mxsp_send_data_event(const meshx_app_element_msg_header_t *msg_hdr, const meshx_data_payload_t *payload);

/**
 * @brief Send GPIO response via MXSP
 *
 * @param cmd Original command type
 * @param logical_pin Logical pin number
 * @param status Status code (0 = success)
 * @param response Response data
 * @param response_len Response data length
 * @return meshx_err_t MESHX_SUCCESS on success
 */
meshx_err_t mxsp_send_gpio_rsp(uint8_t cmd, uint8_t logical_pin, uint8_t status,
                               const uint8_t *response, uint8_t response_len);

/**
 * @brief Send GPIO async event via MXSP
 *
 * @param event_type Event type (mxsp_gpio_evt_t)
 * @param logical_pin Logical pin number
 * @param value Event value
 * @return meshx_err_t MESHX_SUCCESS on success
 */
meshx_err_t mxsp_send_gpio_evt(uint8_t event_type, uint8_t logical_pin, uint8_t value);

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_SERIAL_H__ */
