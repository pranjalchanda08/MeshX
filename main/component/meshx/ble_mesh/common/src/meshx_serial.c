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
#include "interface/logging/meshx_log.h"

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

    return mxsp_send_frame(MXSP_MSG_TYPE_CTRL, buff, len);
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

    return mxsp_send_frame(MXSP_MSG_TYPE_DATA, buff, len);
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
                    if (mxsp_ctx.rx_frame.type == MXSP_MSG_TYPE_DATA) {
                        meshx_app_element_msg_header_t hdr;
                        if (mxsp_ctx.rx_frame.len >= sizeof(hdr)) {
                            memcpy(&hdr, mxsp_ctx.rx_frame.payload, sizeof(hdr));
                            meshx_send_msg_to_element(hdr.element_id, hdr.element_type, hdr.func_id,
                                                    mxsp_ctx.rx_frame.len - sizeof(hdr),
                                                    &mxsp_ctx.rx_frame.payload[sizeof(hdr)]);
                        }
                    } else if (mxsp_ctx.rx_frame.type == MXSP_MSG_TYPE_CTRL) {
                        // Handle control commands from Host to Engine
                        // e.g. Trigger Reset
                        meshx_ctrl_msg_header_t hdr;
                        if (mxsp_ctx.rx_frame.len >= sizeof(hdr)) {
                            memcpy(&hdr, mxsp_ctx.rx_frame.payload, sizeof(hdr));
                            if (hdr.evt_id == MESHX_CTRL_EVT_NODE_RESET) {
                                meshx_platform_reset();
                            }
                        }
                    } else if (mxsp_ctx.rx_frame.type == MXSP_MSG_TYPE_HOSTED_MODE) {
                        if (mxsp_ctx.rx_frame.len == 1) {
                            mxsp_ctx.hosted_mode_enabled = (mxsp_ctx.rx_frame.payload[0] != 0);
                        }
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

    return MESHX_SUCCESS;
}
