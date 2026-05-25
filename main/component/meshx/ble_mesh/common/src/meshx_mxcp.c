/**
 * @file meshx_mxcp.c
 * @brief MeshX Command Protocol (MXCP) Implementation
 *
 * @author Pranjal Chanda
 * @date 2026-05-19
 * @copyright Copyright 2024 - 2025 MeshX
 */

#include "meshx_mxcp.h"
#include "meshx_serial.h"
#include "meshx_builder_api.h"
#include "meshx_api.h"
#include "interface/logging/meshx_log.h"
#include "interface/meshx_platform.h"
#include "interface/gpio/meshx_gpio.h"
#include "interface/gpio/meshx_gpio_types.h"
#include <string.h>
#include <stdlib.h>

extern void meshx_platform_reset(void);
extern void meshx_platform_set_mxsp_use_console(bool enable);
extern void meshx_platform_serial_write(const uint8_t *data, uint16_t len);


/**
 * @brief Forward declarations for MXCP command handler functions.
 *
 * Each handler receives the raw payload and length extracted from
 * an incoming MXCP frame after table-driven dispatch.
 *
 * @param payload  Pointer to payload bytes from the frame.
 * @param len      Payload length in bytes.
 */
static void mxcp_cmd_fn_el_send             (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_node_reset          (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_hosted_mode         (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_gpio_toggle         (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_gpio_set_level      (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_gpio_get_level      (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_gpio_get_state      (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_get_composition     (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_gpio_get_config     (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_gpio_intr_enable    (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_gpio_set_pwm_duty   (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_get_element_state   (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_gpio_set_pwm_freq   (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_gpio_intr_disable   (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_set_console_routing (const uint8_t *payload, uint8_t len);

/**
 * @brief Command dispatch table (Host -> Engine).
 *
 * Linear-scan lookup table mapping each MXCP_CMD_xxx ID to its handler,
 * expected payload size, synchronous response EVT ID, and optional
 * asynchronous notification EVT ID.
 *
 * Fields per entry:
 *   cmd_id       — MXCP command ID from mxcp_cmd_id_t
 *   handler      — Command handler function
 *   payload_size — Expected payload size (0 = variable/no payload)
 *   sync_evt_id  — Response EVT sent immediately after handling (0 = none)
 *   async_evt_id — Async notification EVT (0 = none)
 */
static const mxcp_cmd_entry_t mxcp_cmd_table[] = {
    MXCP_CMD_ENTRY(MXCP_CMD_HOSTED_MODE_ENABLE,  mxcp_cmd_fn_hosted_mode,         sizeof(mxcp_cmd_hosted_mode_enable_t),  MXCP_EVT_HOSTED_MODE_RSP,      MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_NODE_RESET,          mxcp_cmd_fn_node_reset,          0,                                      0,                             MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_GET_COMPOSITION,     mxcp_cmd_fn_get_composition,     0,                                      MXCP_EVT_COMPOSITION_RSP,      MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_GET_ELEMENT_STATE,   mxcp_cmd_fn_get_element_state,   0,                                      MXCP_EVT_ELEMENT_STATE_RSP,    MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_SET_CONSOLE_ROUTING, mxcp_cmd_fn_set_console_routing, sizeof(mxcp_cmd_set_console_routing_t), MXCP_EVT_CONSOLE_ROUTING_RSP,  MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_EL_SEND,             mxcp_cmd_fn_el_send,             sizeof(mxcp_cmd_el_send_t),             MXCP_EVT_EL_DATA_TX_NOTIFY,    MXCP_EVT_EL_DATA_RX_NOTIFY),
    MXCP_CMD_ENTRY(MXCP_CMD_GPIO_SET_LEVEL,      mxcp_cmd_fn_gpio_set_level,      sizeof(mxcp_cmd_gpio_set_level_t),      MXCP_EVT_GPIO_SET_LEVEL_RSP,   MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_GPIO_GET_LEVEL,      mxcp_cmd_fn_gpio_get_level,      sizeof(mxcp_cmd_gpio_get_level_t),      MXCP_EVT_GPIO_GET_LEVEL_RSP,   MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_GPIO_TOGGLE,         mxcp_cmd_fn_gpio_toggle,         sizeof(mxcp_cmd_gpio_toggle_t),         MXCP_EVT_GPIO_TOGGLE_RSP,      MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_GPIO_SET_PWM_DUTY,   mxcp_cmd_fn_gpio_set_pwm_duty,   sizeof(mxcp_cmd_gpio_set_pwm_duty_t),   MXCP_EVT_GPIO_SET_PWM_DUTY_RSP,MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_GPIO_SET_PWM_FREQ,   mxcp_cmd_fn_gpio_set_pwm_freq,   sizeof(mxcp_cmd_gpio_set_pwm_freq_t),   MXCP_EVT_GPIO_SET_PWM_FREQ_RSP,MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_GPIO_INTR_ENABLE,    mxcp_cmd_fn_gpio_intr_enable,    sizeof(mxcp_cmd_gpio_intr_enable_t),    MXCP_EVT_GPIO_INTR_ENABLE_RSP, MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_GPIO_INTR_DISABLE,   mxcp_cmd_fn_gpio_intr_disable,   sizeof(mxcp_cmd_gpio_intr_disable_t),   MXCP_EVT_GPIO_INTR_DISABLE_RSP,MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_GPIO_GET_CONFIG,     mxcp_cmd_fn_gpio_get_config,     sizeof(mxcp_cmd_gpio_get_config_t),     MXCP_EVT_GPIO_GET_CONFIG_RSP,  MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_GPIO_GET_STATE,      mxcp_cmd_fn_gpio_get_state,      sizeof(mxcp_cmd_gpio_get_state_t),      MXCP_EVT_GPIO_GET_STATE_RSP,   MXCP_EVT_NONE),
};

#define MXCP_CMD_TABLE_SIZE (sizeof(mxcp_cmd_table) / sizeof(mxcp_cmd_table[0]))

/**
 * @brief Compute XOR checksum over LEN, TYPE, and PAYLOAD fields.
 *
 * @param len     Payload length byte.
 * @param type    TYPE byte (direction + ID).
 * @param payload Payload bytes (may be NULL if len == 0).
 * @return Single-byte XOR checksum.
 */
static uint8_t mxcp_calculate_checksum(uint8_t len, uint8_t type, const uint8_t *payload)
{
    uint8_t checksum = len ^ type;
    for (uint8_t i = 0; i < len; i++) {
        checksum ^= payload[i];
    }
    return checksum;
}

/**
 * @brief Dispatch a received frame to the matching command handler.
 *
 * Performs a linear scan of mxcp_cmd_table for the given command ID.
 * EVT-typed frames are rejected with a warning.
 *
 * @param type    TYPE byte (direction + ID).
 * @param payload Payload bytes.
 * @param len     Payload length.
 */
void mxcp_dispatch_frame(uint8_t type, const uint8_t *payload, uint8_t len)
{
    if (MXCP_TYPE_IS_EVT(type)) {
        MESHX_LOGW(MODULE_ID_COMMON, "MXCP: received unexpected EVT frame 0x%02x", type);
        return;
    }

    uint8_t cmd_id = MXCP_TYPE_ID(type);

    for (size_t i = 0; i < MXCP_CMD_TABLE_SIZE; i++) {
        if (mxcp_cmd_table[i].cmd_id == cmd_id) {
            mxcp_cmd_table[i].handler(payload, len);
            return;
        }
    }

    MESHX_LOGW(MODULE_ID_COMMON, "MXCP: unhandled CMD 0x%02x", cmd_id);
}

/**
 * @brief Build and transmit a variable-length MXCP frame.
 *
 * Serialises [SOF][LEN][TYPE][PAYLOAD][CHK][EOF] into a flat buffer
 * and writes it to the UART.  No-op when hosted mode is disabled.
 *
 * @param type    TYPE byte.
 * @param payload Payload bytes (may be NULL).
 * @param len     Payload length.
 * @return MESHX_SUCCESS.
 */
meshx_err_t mxcp_send_frame(uint8_t type, const uint8_t *payload, uint8_t len)
{
    if (!mxsp_ctx.hosted_mode_enabled) {
        return MESHX_SUCCESS;
    }

    uint8_t frame_buff[3 + MXCP_PAYLOAD_MAX_SIZE + 2];
    uint8_t ptr = 0;

    frame_buff[ptr++] = MXCP_SOF;
    frame_buff[ptr++] = len;
    frame_buff[ptr++] = type;
    if (len > 0 && payload != NULL) {
        memcpy(&frame_buff[ptr], payload, len);
        ptr += len;
    }
    frame_buff[ptr++] = mxcp_calculate_checksum(len, type, payload);
    frame_buff[ptr++] = MXCP_EOF;

    meshx_platform_serial_write(frame_buff, ptr);
    return MESHX_SUCCESS;
}

/**
 * @brief Send an MXCP event (Engine -> Host).
 *
 * @param evt_id  Event ID.
 * @param payload Typed event payload.
 * @param len     Payload length.
 * @return MESHX_SUCCESS.
 */
meshx_err_t mxcp_send_event(mxcp_evt_id_t evt_id, const uint8_t *payload, uint8_t len)
{
    uint8_t type = MXCP_MAKE_TYPE(MXCP_TYPE_DIR_EVT, evt_id);
    return mxcp_send_frame(type, payload, len);
}

/**
 * @brief Send an MXCP command (Host -> Engine).
 *
 * @param cmd_id  Command ID.
 * @param payload Typed command payload.
 * @param len     Payload length.
 * @return MESHX_SUCCESS.
 */
meshx_err_t mxcp_send_cmd(mxcp_cmd_id_t cmd_id, const uint8_t *payload, uint8_t len)
{
    uint8_t type = MXCP_MAKE_TYPE(MXCP_TYPE_DIR_CMD, cmd_id);
    return mxcp_send_frame(type, payload, len);
}

/**
 * @brief Query the composition builder and send the response as an MXCP event.
 *
 * @param evt_id MXCP_EVT_COMPOSITION_RSP or MXCP_EVT_ELEMENT_STATE_RSP.
 */
void mxcp_send_element_info_response(mxcp_evt_id_t evt_id)
{
    uint8_t resp_buf[MXCP_PAYLOAD_MAX_SIZE];
    size_t data_len = 0;

    if (evt_id == MXCP_EVT_COMPOSITION_RSP) {
        data_len = meshx_get_element_composition_data(resp_buf, sizeof(resp_buf));
    } else if (evt_id == MXCP_EVT_ELEMENT_STATE_RSP) {
        data_len = meshx_get_element_state_data(resp_buf, sizeof(resp_buf));
    }

    mxcp_send_event(evt_id, resp_buf, (uint8_t)data_len);
}

/**
 * @brief Handle MXCP_CMD_HOSTED_MODE_ENABLE.
 *
 * Enables or disables hosted mode on the engine.  When disabled,
 * the TX path silently discards all outgoing frames.
 *
 * @param payload  mxcp_cmd_hosted_mode_enable_t (enable != 0 to activate).
 * @param len      Must be >= sizeof(mxcp_cmd_hosted_mode_enable_t).
 */
static void mxcp_cmd_fn_hosted_mode(const uint8_t *payload, uint8_t len)
{
    if (len < sizeof(mxcp_cmd_hosted_mode_enable_t)) {
        return;
    }
    mxcp_cmd_hosted_mode_enable_t cmd;
    memcpy(&cmd, payload, sizeof(cmd));
    bool enabled = (cmd.enable != 0);
    meshx_serial_set_hosted_mode(enabled);
    MESHX_LOGI(MODULE_ID_COMMON, "Hosted mode %s via MXCP", enabled ? "enabled" : "disabled");
}

/**
 * @brief Handle MXCP_CMD_NODE_RESET.
 *
 * Triggers a platform-level reset of the engine.  This function
 * does not return if the reset is immediate.
 *
 * @param payload  Unused.
 * @param len      Unused.
 */
static void mxcp_cmd_fn_node_reset(const uint8_t *payload, uint8_t len)
{
    (void)payload;
    (void)len;
    meshx_platform_reset();
}

/**
 * @brief Handle MXCP_CMD_GET_COMPOSITION.
 *
 * Requests both composition data and element state from the builder.
 * Sends MXCP_EVT_COMPOSITION_RSP followed by MXCP_EVT_ELEMENT_STATE_RSP.
 *
 * @param payload  Unused.
 * @param len      Unused.
 */
static void mxcp_cmd_fn_get_composition(const uint8_t *payload, uint8_t len)
{
    (void)payload;
    (void)len;
    mxcp_send_element_info_response(MXCP_EVT_COMPOSITION_RSP);
    mxcp_send_element_info_response(MXCP_EVT_ELEMENT_STATE_RSP);
}

/**
 * @brief Handle MXCP_CMD_GET_ELEMENT_STATE.
 *
 * Requests element state data from the builder and sends
 * MXCP_EVT_ELEMENT_STATE_RSP to the host.
 *
 * @param payload  Unused.
 * @param len      Unused.
 */
static void mxcp_cmd_fn_get_element_state(const uint8_t *payload, uint8_t len)
{
    (void)payload;
    (void)len;
    mxcp_send_element_info_response(MXCP_EVT_ELEMENT_STATE_RSP);
}

/**
 * @brief Handle MXCP_CMD_SET_CONSOLE_ROUTING.
 *
 * Enables or disables console routing for MXSP/MXCP debug output.
 *
 * @param payload  mxcp_cmd_set_console_routing_t (enable != 0 to route).
 * @param len      Must be >= sizeof(mxcp_cmd_set_console_routing_t).
 */
static void mxcp_cmd_fn_set_console_routing(const uint8_t *payload, uint8_t len)
{
    if (len < sizeof(mxcp_cmd_set_console_routing_t)) {
        return;
    }
    mxcp_cmd_set_console_routing_t cmd;
    memcpy(&cmd, payload, sizeof(cmd));
    meshx_platform_set_mxsp_use_console(cmd.enable != 0);
}

/**
 * @brief Handle MXCP_CMD_EL_SEND.
 *
 * Forwards element-bound data from the host to the target element
 * via meshx_send_msg_to_element().  The payload beyond the header
 * is treated as the message body.
 *
 * @param payload  mxcp_cmd_el_send_t header followed by message body.
 * @param len      Must be >= sizeof(mxcp_cmd_el_send_t).
 */
static void mxcp_cmd_fn_el_send(const uint8_t *payload, uint8_t len)
{
    mxcp_cmd_el_send_t hdr;
    if (len >= sizeof(hdr)) {
        memcpy(&hdr, payload, sizeof(hdr));
        meshx_send_msg_to_element(hdr.element_id, hdr.element_type, hdr.func_id,
                                  len - sizeof(hdr),
                                  &payload[sizeof(hdr)]);

        mxcp_evt_el_data_tx_notify_t tx_hdr;
        tx_hdr.element_id = hdr.element_id;
        tx_hdr.element_type = hdr.element_type;
        tx_hdr.func_id = hdr.func_id;
        tx_hdr.msg_len = len - sizeof(hdr);

        uint32_t buf_size = sizeof(tx_hdr) + tx_hdr.msg_len;
        uint8_t *buf = (uint8_t *)malloc(buf_size);
        if (buf) {
            memcpy(buf, &tx_hdr, sizeof(tx_hdr));
            if (tx_hdr.msg_len > 0) {
                memcpy(buf + sizeof(tx_hdr), &payload[sizeof(hdr)], tx_hdr.msg_len);
            }
            mxcp_send_event(MXCP_EVT_EL_DATA_TX_NOTIFY, buf, (uint8_t)buf_size);
            free(buf);
        }
    }
}

/**
 * @brief Handle MXCP_CMD_GPIO_SET_LEVEL.
 *
 * Sets the output level on a GPIO pin and sends a status-only response
 * (status + logical_pin, 2 bytes).
 *
 * @param payload  mxcp_cmd_gpio_set_level_t (logical_pin, level).
 * @param len      Must be >= sizeof(mxcp_cmd_gpio_set_level_t).
 */
static void mxcp_cmd_fn_gpio_set_level(const uint8_t *payload, uint8_t len)
{
    if (len < sizeof(mxcp_cmd_gpio_set_level_t)) {
        return;
    }
    mxcp_cmd_gpio_set_level_t cmd;
    memcpy(&cmd, payload, sizeof(cmd));
    meshx_err_t err = meshx_gpio_set_level(cmd.logical_pin, cmd.level);
    mxcp_evt_gpio_rsp_t rsp;
    memset(&rsp, 0, sizeof(rsp));
    rsp.status = (uint8_t)err;
    rsp.logical_pin = cmd.logical_pin;
    mxcp_send_event(MXCP_EVT_GPIO_SET_LEVEL_RSP, (const uint8_t *)&rsp,
                    2 + sizeof(rsp.data.get_level));
}

/**
 * @brief Handle MXCP_CMD_GPIO_GET_LEVEL.
 *
 * Reads the current level of a GPIO pin and sends a typed response
 * (status + logical_pin + level, 3 bytes).
 *
 * @param payload  mxcp_cmd_gpio_get_level_t (logical_pin).
 * @param len      Must be >= sizeof(mxcp_cmd_gpio_get_level_t).
 */
static void mxcp_cmd_fn_gpio_get_level(const uint8_t *payload, uint8_t len)
{
    if (len < sizeof(mxcp_cmd_gpio_get_level_t)) {
        return;
    }
    mxcp_cmd_gpio_get_level_t cmd;
    memcpy(&cmd, payload, sizeof(cmd));
    uint8_t level = 0;
    meshx_err_t err = meshx_gpio_get_level(cmd.logical_pin, &level);
    mxcp_evt_gpio_rsp_t rsp;
    memset(&rsp, 0, sizeof(rsp));
    rsp.status = (uint8_t)err;
    rsp.logical_pin = cmd.logical_pin;
    if (err == MESHX_SUCCESS) {
        rsp.data.get_level.level = level;
    }
    mxcp_send_event(MXCP_EVT_GPIO_GET_LEVEL_RSP, (const uint8_t *)&rsp,
                    2 + sizeof(rsp.data.get_level));
}

/**
 * @brief Handle MXCP_CMD_GPIO_TOGGLE.
 *
 * Toggles the output level of a GPIO pin, reads back the new level,
 * and sends a typed response (status + logical_pin + level, 3 bytes).
 *
 * @param payload  mxcp_cmd_gpio_toggle_t (logical_pin).
 * @param len      Must be >= sizeof(mxcp_cmd_gpio_toggle_t).
 */
static void mxcp_cmd_fn_gpio_toggle(const uint8_t *payload, uint8_t len)
{
    if (len < sizeof(mxcp_cmd_gpio_toggle_t)) {
        return;
    }
    mxcp_cmd_gpio_toggle_t cmd;
    memcpy(&cmd, payload, sizeof(cmd));
    meshx_err_t err = meshx_gpio_toggle(cmd.logical_pin);
    mxcp_evt_gpio_rsp_t rsp;
    memset(&rsp, 0, sizeof(rsp));
    rsp.status = (uint8_t)err;
    rsp.logical_pin = cmd.logical_pin;
    if (err == MESHX_SUCCESS) {
        uint8_t level = 0;
        meshx_gpio_get_level(cmd.logical_pin, &level);
        rsp.data.toggle.level = level;
    }
    mxcp_send_event(MXCP_EVT_GPIO_TOGGLE_RSP, (const uint8_t *)&rsp,
                    2 + sizeof(rsp.data.toggle));
}

/**
 * @brief Handle MXCP_CMD_GPIO_SET_PWM_DUTY.
 *
 * Sets the PWM duty cycle on a GPIO pin via meshx_gpio_execute_function()
 * and sends a status-only response (status + logical_pin, 2 bytes).
 *
 * @param payload  mxcp_cmd_gpio_set_pwm_duty_t (logical_pin, duty_cycle).
 * @param len      Must be >= sizeof(mxcp_cmd_gpio_set_pwm_duty_t).
 */
static void mxcp_cmd_fn_gpio_set_pwm_duty(const uint8_t *payload, uint8_t len)
{
    if (len < sizeof(mxcp_cmd_gpio_set_pwm_duty_t)) {
        return;
    }
    mxcp_cmd_gpio_set_pwm_duty_t cmd;
    memcpy(&cmd, payload, sizeof(cmd));
    uint32_t args[1] = { cmd.duty_cycle };
    meshx_err_t err = meshx_gpio_execute_function(cmd.logical_pin,
                                                   MESHX_IO_FUNCTION_SET_PWM_DUTY,
                                                   args, 1);
    mxcp_evt_gpio_rsp_t rsp;
    memset(&rsp, 0, sizeof(rsp));
    rsp.status = (uint8_t)err;
    rsp.logical_pin = cmd.logical_pin;
    mxcp_send_event(MXCP_EVT_GPIO_SET_PWM_DUTY_RSP, (const uint8_t *)&rsp, 2);
}

/**
 * @brief Handle MXCP_CMD_GPIO_SET_PWM_FREQ.
 *
 * Sets the PWM frequency on a GPIO pin via meshx_gpio_execute_function()
 * and sends a status-only response (status + logical_pin, 2 bytes).
 *
 * @param payload  mxcp_cmd_gpio_set_pwm_freq_t (logical_pin, frequency).
 * @param len      Must be >= sizeof(mxcp_cmd_gpio_set_pwm_freq_t).
 */
static void mxcp_cmd_fn_gpio_set_pwm_freq(const uint8_t *payload, uint8_t len)
{
    if (len < sizeof(mxcp_cmd_gpio_set_pwm_freq_t)) {
        return;
    }
    mxcp_cmd_gpio_set_pwm_freq_t cmd;
    memcpy(&cmd, payload, sizeof(cmd));
    uint32_t args[1] = { cmd.frequency };
    meshx_err_t err = meshx_gpio_execute_function(cmd.logical_pin,
                                                   MESHX_IO_FUNCTION_SET_PWM_FREQUENCY,
                                                   args, 1);
    mxcp_evt_gpio_rsp_t rsp;
    memset(&rsp, 0, sizeof(rsp));
    rsp.status = (uint8_t)err;
    rsp.logical_pin = cmd.logical_pin;
    mxcp_send_event(MXCP_EVT_GPIO_SET_PWM_FREQ_RSP, (const uint8_t *)&rsp, 2);
}

/**
 * @brief Handle MXCP_CMD_GPIO_INTR_ENABLE.
 *
 * Enables or disables interrupt registration on a GPIO pin and sends
 * a status-only response (status + logical_pin, 2 bytes).
 *
 * @param payload  mxcp_cmd_gpio_intr_enable_t (logical_pin, enable).
 * @param len      Must be >= sizeof(mxcp_cmd_gpio_intr_enable_t).
 */
static void mxcp_cmd_fn_gpio_intr_enable(const uint8_t *payload, uint8_t len)
{
    if (len < sizeof(mxcp_cmd_gpio_intr_enable_t)) {
        return;
    }
    mxcp_cmd_gpio_intr_enable_t cmd;
    memcpy(&cmd, payload, sizeof(cmd));
    bool enable = (cmd.enable != 0);
    meshx_err_t err = meshx_gpio_intr_enable(cmd.logical_pin, enable);
    mxcp_evt_gpio_rsp_t rsp;
    memset(&rsp, 0, sizeof(rsp));
    rsp.status = (uint8_t)err;
    rsp.logical_pin = cmd.logical_pin;
    mxcp_send_event(MXCP_EVT_GPIO_INTR_ENABLE_RSP, (const uint8_t *)&rsp, 2);
}

/**
 * @brief Handle MXCP_CMD_GPIO_INTR_DISABLE.
 *
 * Disables interrupt registration on a GPIO pin (always passes false)
 * and sends a status-only response (status + logical_pin, 2 bytes).
 *
 * @param payload  mxcp_cmd_gpio_intr_disable_t (logical_pin).
 * @param len      Must be >= sizeof(mxcp_cmd_gpio_intr_disable_t).
 */
static void mxcp_cmd_fn_gpio_intr_disable(const uint8_t *payload, uint8_t len)
{
    if (len < sizeof(mxcp_cmd_gpio_intr_disable_t)) {
        return;
    }
    mxcp_cmd_gpio_intr_disable_t cmd;
    memcpy(&cmd, payload, sizeof(cmd));
    meshx_err_t err = meshx_gpio_intr_enable(cmd.logical_pin, false);
    mxcp_evt_gpio_rsp_t rsp;
    memset(&rsp, 0, sizeof(rsp));
    rsp.status = (uint8_t)err;
    rsp.logical_pin = cmd.logical_pin;
    mxcp_send_event(MXCP_EVT_GPIO_INTR_DISABLE_RSP, (const uint8_t *)&rsp, 2);
}

/**
 * @brief Handle MXCP_CMD_GPIO_GET_CONFIG.
 *
 * Retrieves the pin configuration (mode, pull, drive strength,
 * initial level, signal inversion) and sends a typed response
 * (status + logical_pin + 5 config bytes = 7 bytes).
 *
 * @param payload  mxcp_cmd_gpio_get_config_t (logical_pin).
 * @param len      Must be >= sizeof(mxcp_cmd_gpio_get_config_t).
 */
static void mxcp_cmd_fn_gpio_get_config(const uint8_t *payload, uint8_t len)
{
    if (len < sizeof(mxcp_cmd_gpio_get_config_t)) {
        return;
    }
    mxcp_cmd_gpio_get_config_t cmd;
    memcpy(&cmd, payload, sizeof(cmd));
    meshx_gpio_pin_config_t config;
    meshx_err_t err = meshx_gpio_get_pin_config(cmd.logical_pin, &config);
    mxcp_evt_gpio_rsp_t rsp;
    memset(&rsp, 0, sizeof(rsp));
    rsp.status = (uint8_t)err;
    rsp.logical_pin = cmd.logical_pin;
    if (err == MESHX_SUCCESS) {
        rsp.data.get_config.mode = config.mode;
        rsp.data.get_config.pull = config.pull;
        rsp.data.get_config.drive_strength = config.drive_strength;
        rsp.data.get_config.initial_level = config.initial_level;
        rsp.data.get_config.signal_inversion = config.signal_inversion ? 1 : 0;
    }
    mxcp_send_event(MXCP_EVT_GPIO_GET_CONFIG_RSP, (const uint8_t *)&rsp,
                    2 + sizeof(rsp.data.get_config));
}

/**
 * @brief Handle MXCP_CMD_GPIO_GET_STATE.
 *
 * Retrieves the runtime pin state (current level, interrupt registered,
 * interrupt type) and sends a typed response
 * (status + logical_pin + 3 state bytes = 5 bytes).
 *
 * @param payload  mxcp_cmd_gpio_get_state_t (logical_pin).
 * @param len      Must be >= sizeof(mxcp_cmd_gpio_get_state_t).
 */
static void mxcp_cmd_fn_gpio_get_state(const uint8_t *payload, uint8_t len)
{
    if (len < sizeof(mxcp_cmd_gpio_get_state_t)) {
        return;
    }
    mxcp_cmd_gpio_get_state_t cmd;
    memcpy(&cmd, payload, sizeof(cmd));
    meshx_gpio_pin_state_t state;
    meshx_err_t err = meshx_gpio_get_pin_state(cmd.logical_pin, &state);
    mxcp_evt_gpio_rsp_t rsp;
    memset(&rsp, 0, sizeof(rsp));
    rsp.status = (uint8_t)err;
    rsp.logical_pin = cmd.logical_pin;
    if (err == MESHX_SUCCESS) {
        rsp.data.get_state.current_level = state.current_level;
        rsp.data.get_state.interrupt_registered = state.interrupt_registered ? 1 : 0;
        rsp.data.get_state.intr_type = (uint8_t)state.intr_type;
    }
    mxcp_send_event(MXCP_EVT_GPIO_GET_STATE_RSP, (const uint8_t *)&rsp,
                    2 + sizeof(rsp.data.get_state));
}
