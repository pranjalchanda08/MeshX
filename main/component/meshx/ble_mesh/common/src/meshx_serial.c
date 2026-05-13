/**
 * @file meshx_serial.c
 * @brief MeshX Serial Protocol (MXSP) Implementation
 *
 * The MeshX Serial Protocol (MXSP) is a lightweight binary protocol used for communication
 * between a MeshX Controller and a MeshX Engine over a serial interface.
 *
 * Frame Format:
 *   Byte | Field
 *   -----|------
 *   0    | Start Of Frame (SOF)
 *   1    | Length
 *   2    | Type
 *   3..N | Payload
 *   N+1  | Checksum
 *   N+2  | End Of Frame (EOF)
 *
 * @author Pranjal Chanda
 * @date 2026-05-04
 */

#include "meshx_serial.h"
#include <string.h>
#include "interface/rtos/meshx_task.h"
#include "interface/rtos/meshx_rtos_utils.h"
#include "interface/logging/meshx_log.h"
#include "interface/gpio/meshx_gpio.h"
#include "interface/gpio/meshx_gpio_types.h"

#define MESHX_UART_RX_TASK_STACK_SIZE  2048

/**
 * @brief Priority level for the UART RX task.
 */
#define MESHX_UART_RX_TASK_PRIO        5

/* State machine context */
static struct {
    uint8_t state;
    mxsp_frame_t rx_frame;
    uint8_t rx_ptr;
    bool hosted_mode_enabled;
} mxsp_ctx;

enum {
    STATE_SOF,
    STATE_LEN,
    STATE_TYPE,
    STATE_PAYLOAD,
    STATE_CHECKSUM,
    STATE_EOF
};

/* Internal function to calculate checksum */
/**
 * @brief Calculate checksum for a given frame.
 *
 * @param len Length of the payload.
 * @param type Type of the frame.
 * @param payload Payload of the frame.
 * @return uint8_t Checksum of the frame.
 */
static uint8_t calculate_checksum(uint8_t len, uint8_t type, const uint8_t *payload)
{
    uint8_t checksum = len ^ type;
    for (uint8_t i = 0; i < len; i++) {
        checksum ^= payload[i];
    }
    return checksum;
}

/* Platform-dependent serial write - to be implemented by platform layer or main.c */
extern void meshx_platform_serial_write(const uint8_t *data, uint16_t len);

/* Platform-dependent serial read - to be implemented by platform layer or main.c */
extern int32_t meshx_platform_serial_read(uint8_t *data, uint16_t len);

/* Platform-dependent reset - to be implemented by platform layer */
extern void meshx_platform_reset(void);

/* Forward declaration for GPIO command handler */
static void mxsp_handle_gpio_cmd(const uint8_t *payload, uint8_t len);

/* Forward declaration for GPIO event handler (from host) */
static void mxsp_handle_gpio_evt_from_host(const uint8_t *payload, uint8_t len);

/**
 * @brief Get system time in milliseconds
 *
 * @return uint32_t System time in milliseconds
 */
static uint32_t meshx_get_sys_time_ms(void)
{
    unsigned int millis = 0;
    meshx_rtos_get_sys_time(&millis);
    return (uint32_t)millis;
}

meshx_err_t mxsp_send_frame(mxsp_msg_type_t type, const uint8_t *payload, uint8_t len)
{
    uint8_t frame_buff[260];
    uint8_t ptr = 0;

    frame_buff[ptr++] = MXSP_SOF;
    frame_buff[ptr++] = len;
    frame_buff[ptr++] = (uint8_t)type;
    if (len > 0 && payload != NULL) {
        memcpy(&frame_buff[ptr], payload, len);
        ptr += len;
    }
    frame_buff[ptr++] = calculate_checksum(len, (uint8_t)type, payload);
    frame_buff[ptr++] = MXSP_EOF;

    meshx_platform_serial_write(frame_buff, ptr);
    return MESHX_SUCCESS;
}

meshx_err_t mxsp_send_ctrl_event(const meshx_ctrl_msg_header_t *evt_header, const meshx_ctrl_payload_t *payload)
{
    if (!mxsp_ctx.hosted_mode_enabled) {
        return MESHX_SUCCESS;
    }
    uint8_t buff[sizeof(meshx_ctrl_msg_header_t) + sizeof(meshx_ctrl_payload_t)];
    uint8_t len = 0;

    memcpy(&buff[len], evt_header, sizeof(meshx_ctrl_msg_header_t));
    len += sizeof(meshx_ctrl_msg_header_t);

    // For simplicity, we send the whole payload union. In optimization, we could send only active part.
    memcpy(&buff[len], payload, sizeof(meshx_ctrl_payload_t));
    len += sizeof(meshx_ctrl_payload_t);

    return mxsp_send_frame(MXSP_MSG_TYPE_SYS_EVT_NOTIFY, buff, len);
}

meshx_err_t mxsp_send_data_event(const meshx_app_element_msg_header_t *msg_hdr, const meshx_data_payload_t *payload)
{
    if (!mxsp_ctx.hosted_mode_enabled) {
        return MESHX_SUCCESS;
    }
    uint8_t buff[sizeof(meshx_app_element_msg_header_t) + sizeof(meshx_data_payload_t)];
    uint8_t len = 0;

    memcpy(&buff[len], msg_hdr, sizeof(meshx_app_element_msg_header_t));
    len += sizeof(meshx_app_element_msg_header_t);

    memcpy(&buff[len], payload, sizeof(meshx_data_payload_t));
    len += sizeof(meshx_data_payload_t);

    return mxsp_send_frame(MXSP_MSG_TYPE_DATA_EVT_NOTIFY, buff, len);
}

meshx_err_t mxsp_send_gpio_rsp(uint8_t cmd, uint8_t logical_pin, uint8_t status,
                               const uint8_t *response, uint8_t response_len)
{
    if (!mxsp_ctx.hosted_mode_enabled) {
        return MESHX_SUCCESS;
    }

    mxsp_gpio_rsp_payload_t rsp;
    memset(&rsp, 0, sizeof(rsp));

    rsp.cmd = cmd;
    rsp.logical_pin = logical_pin;
    rsp.status = status;
    rsp.response_len = (response_len > 8) ? 8 : response_len;
    if (response != NULL && rsp.response_len > 0) {
        memcpy(rsp.response, response, rsp.response_len);
    }

    return mxsp_send_frame(MXSP_MSG_TYPE_GPIO_RSP, (const uint8_t*)&rsp, sizeof(rsp));
}

meshx_err_t mxsp_send_gpio_evt(uint8_t event_type, uint8_t logical_pin, uint8_t value)
{
    if (!mxsp_ctx.hosted_mode_enabled) {
        return MESHX_SUCCESS;
    }

    mxsp_gpio_evt_payload_t evt;
    memset(&evt, 0, sizeof(evt));

    evt.event_type = event_type;
    evt.logical_pin = logical_pin;
    evt.value = value;
    evt.reserved = 0;
    evt.timestamp = meshx_get_sys_time_ms();

    return mxsp_send_frame(MXSP_MSG_TYPE_GPIO_EVT, (const uint8_t*)&evt, sizeof(evt));
}

/**
 * @brief Handle GPIO command from host
 *
 * @param payload Command payload
 * @param len Payload length
 */
static void mxsp_handle_gpio_cmd(const uint8_t *payload, uint8_t len)
{
    if (payload == NULL || len < sizeof(mxsp_gpio_cmd_payload_t)) {
        MESHX_LOGE(MODULE_ID_COMMON, "Invalid GPIO command payload");
        return;
    }

    const mxsp_gpio_cmd_payload_t *cmd = (const mxsp_gpio_cmd_payload_t *)payload;
    meshx_err_t err = MESHX_SUCCESS;
    uint8_t response[8] = {0};
    uint8_t response_len = 0;

    MESHX_LOGD(MODULE_ID_COMMON, "GPIO cmd received: cmd=%d, pin=%u", cmd->cmd, cmd->logical_pin);

    switch (cmd->cmd) {
        case MXSP_GPIO_CMD_SET_LEVEL:
            /* Set GPIO pin level */
            if (cmd->payload_len >= 1) {
                uint8_t level = cmd->payload[0];
                err = meshx_gpio_set_level(cmd->logical_pin, level);
                if (err == MESHX_SUCCESS) {
                    MESHX_LOGD(MODULE_ID_COMMON, "GPIO pin %u set to %u (hosted cmd)",
                              cmd->logical_pin, level);
                }
            } else {
                err = MESHX_INVALID_ARG;
            }
            break;

        case MXSP_GPIO_CMD_GET_LEVEL:
            /* Get GPIO pin level */
            {
                uint8_t level = 0;
                err = meshx_gpio_get_level(cmd->logical_pin, &level);
                if (err == MESHX_SUCCESS) {
                    response[0] = level;
                    response_len = 1;
                }
            }
            break;

        case MXSP_GPIO_CMD_TOGGLE:
            /* Toggle GPIO pin */
            err = meshx_gpio_toggle(cmd->logical_pin);
            if (err == MESHX_SUCCESS) {
                /* Get the new level after toggle */
                uint8_t level = 0;
                meshx_gpio_get_level(cmd->logical_pin, &level);
                response[0] = level;
                response_len = 1;
            }
            break;

        case MXSP_GPIO_CMD_SET_PWM_DUTY:
            /* Set PWM duty cycle - using execute_function */
            if (cmd->payload_len >= 1) {
                uint32_t args[1] = { cmd->payload[0] };
                err = meshx_gpio_execute_function(cmd->logical_pin,
                                                  MESHX_IO_FUNCTION_SET_PWM_DUTY,
                                                  args, 1);
            } else {
                err = MESHX_INVALID_ARG;
            }
            break;

        case MXSP_GPIO_CMD_SET_PWM_FREQ:
            /* Set PWM frequency - using execute_function */
            if (cmd->payload_len >= 4) {
                uint32_t frequency = (cmd->payload[0]) |
                                    (cmd->payload[1] << 8) |
                                    (cmd->payload[2] << 16) |
                                    (cmd->payload[3] << 24);
                uint32_t args[1] = { frequency };
                err = meshx_gpio_execute_function(cmd->logical_pin,
                                                  MESHX_IO_FUNCTION_SET_PWM_FREQUENCY,
                                                  args, 1);
            } else {
                err = MESHX_INVALID_ARG;
            }
            break;

        case MXSP_GPIO_CMD_INTR_ENABLE:
            /* Enable GPIO interrupt */
            if (cmd->payload_len >= 1) {
                bool enable = (cmd->payload[0] != 0);
                err = meshx_gpio_intr_enable(cmd->logical_pin, enable);
            } else {
                err = MESHX_INVALID_ARG;
            }
            break;

        case MXSP_GPIO_CMD_INTR_DISABLE:
            /* Disable GPIO interrupt */
            err = meshx_gpio_intr_enable(cmd->logical_pin, false);
            break;

        case MXSP_GPIO_CMD_GET_CONFIG:
            /* Get pin configuration */
            {
                meshx_gpio_pin_config_t config;
                err = meshx_gpio_get_pin_config(cmd->logical_pin, &config);
                if (err == MESHX_SUCCESS) {
                    /* Serialize config into response */
                    response[0] = config.mode;
                    response[1] = config.pull;
                    response[2] = config.drive_strength;
                    response[3] = config.initial_level;
                    response[4] = config.signal_inversion ? 1 : 0;
                    response_len = 5;
                }
            }
            break;

        case MXSP_GPIO_CMD_GET_STATE:
            /* Get pin state */
            {
                meshx_gpio_pin_state_t state;
                err = meshx_gpio_get_pin_state(cmd->logical_pin, &state);
                if (err == MESHX_SUCCESS) {
                    /* Serialize state into response */
                    response[0] = state.current_level;
                    response[1] = state.interrupt_registered ? 1 : 0;
                    response[2] = state.intr_type;
                    response_len = 3;
                }
            }
            break;

        default:
            MESHX_LOGW(MODULE_ID_COMMON, "Unknown GPIO command: %d", cmd->cmd);
            err = MESHX_INVALID_ARG;
            break;
    }

    /* Send response back to host */
    mxsp_send_gpio_rsp(cmd->cmd, cmd->logical_pin, (uint8_t)err, response, response_len);
}

/**
 * @brief Handle GPIO event from host (interrupt notification)
 *
 * In some configurations, the host MCU can send GPIO interrupt events
 * to the engine when an interrupt occurs on the host side.
 *
 * @param payload Event payload
 * @param len Payload length
 */
static void mxsp_handle_gpio_evt_from_host(const uint8_t *payload, uint8_t len)
{
    if (payload == NULL || len < sizeof(mxsp_gpio_evt_payload_t)) {
        MESHX_LOGE(MODULE_ID_COMMON, "Invalid GPIO event payload from host");
        return;
    }

    const mxsp_gpio_evt_payload_t *evt = (const mxsp_gpio_evt_payload_t *)payload;

    /* Handle interrupt event from host */
    if (evt->event_type == MXSP_GPIO_EVT_INTERRUPT) {
        meshx_gpio_process_hosted_interrupt(evt->logical_pin, evt->value);
    } else {
        MESHX_LOGW(MODULE_ID_COMMON, "Unexpected GPIO event from host: type=%d", evt->event_type);
    }
}

void meshx_serial_parse_byte(uint8_t data)
{
    switch (mxsp_ctx.state) {
        case STATE_SOF:
            if (data == MXSP_SOF) {
                mxsp_ctx.state = STATE_LEN;
                mxsp_ctx.rx_ptr = 0;
            }
            break;
        case STATE_LEN:
            mxsp_ctx.rx_frame.len = data;
            mxsp_ctx.state = STATE_TYPE;
            break;
        case STATE_TYPE:
            mxsp_ctx.rx_frame.type = data;
            if (mxsp_ctx.rx_frame.len == 0) {
                mxsp_ctx.state = STATE_CHECKSUM;
            } else {
                mxsp_ctx.state = STATE_PAYLOAD;
            }
            break;
        case STATE_PAYLOAD:
            mxsp_ctx.rx_frame.payload[mxsp_ctx.rx_ptr++] = data;
            if (mxsp_ctx.rx_ptr >= mxsp_ctx.rx_frame.len) {
                mxsp_ctx.state = STATE_CHECKSUM;
            }
            break;
        case STATE_CHECKSUM:
            mxsp_ctx.rx_frame.checksum = data;
            mxsp_ctx.state = STATE_EOF;
            break;
        case STATE_EOF:
            if (data == MXSP_EOF) {
                if (calculate_checksum(mxsp_ctx.rx_frame.len, mxsp_ctx.rx_frame.type, mxsp_ctx.rx_frame.payload) == mxsp_ctx.rx_frame.checksum) {
                    if (mxsp_ctx.rx_frame.type == MXSP_MSG_TYPE_EL_CMD_SEND) {
                        meshx_app_element_msg_header_t hdr;
                        if (mxsp_ctx.rx_frame.len >= sizeof(hdr)) {
                            memcpy(&hdr, mxsp_ctx.rx_frame.payload, sizeof(hdr));
                            meshx_send_msg_to_element(hdr.element_id, hdr.element_type, hdr.func_id,
                                                    mxsp_ctx.rx_frame.len - sizeof(hdr),
                                                    &mxsp_ctx.rx_frame.payload[sizeof(hdr)]);
                        }
                    } else if (mxsp_ctx.rx_frame.type == MXSP_MSG_TYPE_SYS_CMD_SEND) {
                        meshx_ctrl_msg_header_t hdr;
                        if (mxsp_ctx.rx_frame.len >= sizeof(hdr)) {
                            memcpy(&hdr, mxsp_ctx.rx_frame.payload, sizeof(hdr));
                            if (hdr.evt_id == MESHX_CTRL_EVT_NODE_RESET) {
                                meshx_platform_reset();
                            }
                        }
                    } else if (mxsp_ctx.rx_frame.type == MXSP_MSG_TYPE_HOSTED_MODE) {
                        if (mxsp_ctx.rx_frame.len == 1) {
                            bool enabled = (mxsp_ctx.rx_frame.payload[0] != 0);
                            mxsp_ctx.hosted_mode_enabled = enabled;

                            /* Notify GPIO subsystem about hosted mode change */
                            meshx_gpio_hosted_mode_t gpio_mode = enabled ?
                                MESHX_GPIO_MODE_HOSTED : MESHX_GPIO_MODE_NON_HOSTED;
                            meshx_err_t err = meshx_gpio_set_hosted_mode(gpio_mode);
                            if (err != MESHX_SUCCESS) {
                                MESHX_LOGE(MODULE_ID_COMMON, "Failed to set GPIO hosted mode: %d", err);
                            }

                            MESHX_LOGI(MODULE_ID_COMMON, "Hosted mode %s via API TLV command",
                                      enabled ? "enabled" : "disabled");
                        }
                    } else if (mxsp_ctx.rx_frame.type == MXSP_MSG_TYPE_GPIO_CMD) {
                        /* Handle GPIO command from host */
                        mxsp_handle_gpio_cmd(mxsp_ctx.rx_frame.payload, mxsp_ctx.rx_frame.len);
                    } else if (mxsp_ctx.rx_frame.type == MXSP_MSG_TYPE_GPIO_EVT) {
                        /* Handle GPIO event from host (interrupt notification) */
                        mxsp_handle_gpio_evt_from_host(mxsp_ctx.rx_frame.payload, mxsp_ctx.rx_frame.len);
                    }
                } else {
                    MESHX_LOGE(MODULE_ID_COMMON, "Invalid MXSP frame checksum");
                }
            }
            mxsp_ctx.state = STATE_SOF;
            break;
    }
}

void meshx_serial_set_hosted_mode(bool enabled)
{
    mxsp_ctx.hosted_mode_enabled = enabled;

    /* Also update GPIO subsystem hosted mode */
    meshx_gpio_hosted_mode_t gpio_mode = enabled ?
        MESHX_GPIO_MODE_HOSTED : MESHX_GPIO_MODE_NON_HOSTED;
    meshx_gpio_set_hosted_mode(gpio_mode);
}

bool meshx_serial_is_hosted_mode_enabled(void)
{
    return mxsp_ctx.hosted_mode_enabled;
}

/**
 * @brief UART RX task to parse incoming MXSP frames
 */
static void mxsp_uart_rx_task(void *pvParameters)
{
    uint8_t data;
    while (1) {
        int32_t len = meshx_platform_serial_read(&data, 1);
        if (len > 0) {
            meshx_serial_parse_byte(data);
        }
    }
}

/**
 * @brief GPIO hosted mode event callback
 *
 * This callback is invoked by the GPIO subsystem when events need to be
 * sent to the host MCU in hosted mode.
 *
 * @param event GPIO hosted mode event
 */
static void meshx_gpio_hosted_event_handler(const meshx_gpio_hosted_event_t *event)
{
    if (!mxsp_ctx.hosted_mode_enabled) {
        return;
    }

    if (event == NULL) {
        MESHX_LOGE(MODULE_ID_COMMON, "NULL GPIO hosted event");
        return;
    }

    /* Map GPIO event type to MXSP event type */
    uint8_t mxsp_evt_type;
    switch (event->event_type) {
        case 0:  /* Level change */
            mxsp_evt_type = MXSP_GPIO_EVT_LEVEL_CHANGE;
            break;
        case 1:  /* Interrupt */
            mxsp_evt_type = MXSP_GPIO_EVT_INTERRUPT;
            break;
        default:
            mxsp_evt_type = event->event_type;
            break;
    }

    /* Send GPIO event via MXSP using the dedicated GPIO event message type */
    mxsp_send_gpio_evt(mxsp_evt_type, event->logical_pin, event->value);

    MESHX_LOGD(MODULE_ID_COMMON, "GPIO hosted event sent: type=%d, pin=%u, value=%u",
              event->event_type, event->logical_pin, event->value);
}

/**
 * @brief Initialize serial communication
 * @return MESHX_SUCCESS on success, error code otherwise
 */
meshx_err_t meshx_serial_init(void)
{
    memset(&mxsp_ctx, 0, sizeof(mxsp_ctx));
    mxsp_ctx.state = STATE_SOF;
    mxsp_ctx.hosted_mode_enabled = false;

    meshx_task_t uart_rx_task;
    uart_rx_task.task_cb        = mxsp_uart_rx_task;
    uart_rx_task.task_name      = "mxsp_uart_rx";
    uart_rx_task.stack_size     = MESHX_UART_RX_TASK_STACK_SIZE;
    uart_rx_task.priority       = MESHX_UART_RX_TASK_PRIO;
    uart_rx_task.arg            = NULL;
    /* Create UART RX Task */
    meshx_task_create(&uart_rx_task);

    /* Register GPIO hosted event callback */
    meshx_gpio_register_hosted_event_cb(meshx_gpio_hosted_event_handler);

    return MESHX_SUCCESS;
}
