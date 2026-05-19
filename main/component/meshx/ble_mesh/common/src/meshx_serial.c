/**
 * @file meshx_serial.c
 * @brief MeshX Serial Transport Layer — UART RX State Machine and Hosted Mode
 *
 * Implements the byte-level frame parser, hosted-mode GPIO bridge,
 * and serial initialisation.  Valid frames are forwarded to the
 * MXCP single-layer dispatch via mxcp_dispatch_frame().
 *
 * Frame Format (MXCP / MXSP compatible):
 *   [SOF 0xFE][LEN][TYPE][PAYLOAD...][CHK][EOF 0xEF]
 *
 * @author Pranjal Chanda
 * @date 2026-05-04
 * @copyright Copyright 2024 - 2025 MeshX
 */

#include "meshx_serial.h"
#include "meshx_mxcp.h"
#include <string.h>
#include "interface/rtos/meshx_task.h"
#include "interface/rtos/meshx_rtos_utils.h"
#include "interface/logging/meshx_log.h"
#include "interface/gpio/meshx_gpio.h"
#include "interface/gpio/meshx_gpio_types.h"


#define MESHX_UART_RX_TASK_STACK_SIZE  2048

#define MESHX_UART_RX_TASK_PRIO        5

mxsp_ctx_t mxsp_ctx;

enum {
    STATE_SOF,
    STATE_LEN,
    STATE_TYPE,
    STATE_PAYLOAD,
    STATE_CHECKSUM,
    STATE_EOF
};

extern void meshx_platform_serial_write(const uint8_t *data, uint16_t len);
extern int32_t meshx_platform_serial_read(uint8_t *data, uint16_t len);
extern void meshx_platform_reset(void);
extern void meshx_platform_set_mxsp_use_console(bool enable);

/**
 * @brief Feed a single byte into the MXCP frame parser state machine.
 *
 * @param data Byte received from the UART.
 */
void meshx_serial_parse_byte(uint8_t data)
{
    switch (mxsp_ctx.state) {
        case STATE_SOF:
            if (data == MXCP_SOF) {
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
            if (data == MXCP_EOF) {
                uint8_t calc = mxsp_ctx.rx_frame.len ^ mxsp_ctx.rx_frame.type;
                for (uint8_t i = 0; i < mxsp_ctx.rx_frame.len; i++) {
                    calc ^= mxsp_ctx.rx_frame.payload[i];
                }
                if (calc == mxsp_ctx.rx_frame.checksum) {
                    mxcp_dispatch_frame(mxsp_ctx.rx_frame.type, mxsp_ctx.rx_frame.payload, mxsp_ctx.rx_frame.len);
                } else {
                    MESHX_LOGE(MODULE_ID_COMMON, "Invalid MXSP frame checksum");
                }
            }
            mxsp_ctx.state = STATE_SOF;
            break;
    }
}

/**
 * @brief Enable or disable hosted (external-MCU) mode.
 *
 * When enabled, MXCP frames are transmitted over the serial port and
 * GPIO interrupts are forwarded to the host as async events.
 *
 * @param enabled True to enable, false to disable.
 */
void meshx_serial_set_hosted_mode(bool enabled)
{
    mxsp_ctx.hosted_mode_enabled = enabled;

    meshx_gpio_hosted_mode_t gpio_mode = enabled ?
        MESHX_GPIO_MODE_HOSTED : MESHX_GPIO_MODE_NON_HOSTED;
    meshx_gpio_set_hosted_mode(gpio_mode);
}

/**
 * @brief Check whether hosted mode is currently active.
 *
 * @return true if hosted mode is enabled.
 */
bool meshx_serial_is_hosted_mode_enabled(void)
{
    return mxsp_ctx.hosted_mode_enabled;
}

/**
 * @brief FreeRTOS task that continuously reads bytes from the UART
 *        and feeds them into the frame parser.
 *
 * @param pvParameters Unused.
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
 * @brief Callback invoked by the GPIO subsystem when an interrupt or
 *        level-change event occurs in hosted mode.
 *
 * Packs the event into an mxcp_evt_gpio_async_t and transmits it to
 * the host via mxcp_send_event().
 *
 * @param event Pointer to the GPIO hosted event data.
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

    mxcp_evt_gpio_async_t async_evt;
    async_evt.event_type = event->event_type;
    async_evt.logical_pin = event->logical_pin;
    async_evt.value = event->value;
    unsigned int millis = 0;
    meshx_rtos_get_sys_time(&millis);
    async_evt.timestamp = (uint32_t)millis;
    mxcp_send_event(MXCP_EVT_GPIO_ASYNC, (const uint8_t *)&async_evt, sizeof(async_evt));

    MESHX_LOGD(MODULE_ID_COMMON, "GPIO hosted event sent: type=%d, pin=%u, value=%u",
               event->event_type, event->logical_pin, event->value);
}

/**
 * @brief Initialise the serial transport layer.
 *
 * Resets the RX state machine, spawns the UART read task, and
 * registers the GPIO hosted-event callback.
 *
 * @return MESHX_SUCCESS on success.
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
    meshx_task_create(&uart_rx_task);

    meshx_gpio_register_hosted_event_cb(meshx_gpio_hosted_event_handler);

    return MESHX_SUCCESS;
}
