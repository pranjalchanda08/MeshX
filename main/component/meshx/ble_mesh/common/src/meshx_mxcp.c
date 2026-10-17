/**
 * @file meshx_mxcp.c
 * @brief MeshX Command Protocol (MXCP) Implementation
 *
 * @author Pranjal Chanda
 * @date 2026-05-19
 * @copyright Copyright 2024 - 2025 MeshX
 */

#include <string.h>
#include <stdlib.h>
#include "meshx_api.h"
#include "meshx_nvs.h"
#include "meshx_uvp.h"
#include "meshx_mxcp.h"
#include "meshx_serial.h"
#include "meshx_os_timer.h"
#include "meshx_builder_api.h"
#include "pairing/meshx_manifest.h"
#include "interface/meshx_platform.h"
#include "pairing/meshx_unpair_mgr.h"
#include "interface/gpio/meshx_gpio.h"
#include "interface/logging/meshx_log.h"
#include "interface/gpio/meshx_gpio_types.h"
#include "pairing/meshx_pair_orchestrator.h"

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
static void mxcp_cmd_fn_el_send                (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_node_reset             (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_hosted_mode            (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_gpio_toggle            (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_gpio_set_level         (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_gpio_get_level         (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_gpio_get_state         (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_get_composition        (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_gpio_get_config        (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_gpio_intr_enable       (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_gpio_set_pwm_duty      (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_get_element_state      (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_gpio_set_pwm_freq      (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_gpio_intr_disable      (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_set_console_routing    (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_set_composition        (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_discover_capabilities  (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_bind_elements          (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_unbind_elements        (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_get_link_table         (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_get_link_epoch         (const uint8_t *payload, uint8_t len);

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
    MXCP_CMD_ENTRY(MXCP_CMD_HOSTED_MODE_ENABLE,    mxcp_cmd_fn_hosted_mode,           sizeof(mxcp_cmd_hosted_mode_enable_t),    MXCP_EVT_HOSTED_MODE_RSP,        MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_NODE_RESET,            mxcp_cmd_fn_node_reset,            0,                                        MXCP_EVT_NONE,                   MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_GET_COMPOSITION,       mxcp_cmd_fn_get_composition,       0,                                        MXCP_EVT_COMPOSITION_RSP,        MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_GET_ELEMENT_STATE,     mxcp_cmd_fn_get_element_state,     0,                                        MXCP_EVT_ELEMENT_STATE_RSP,      MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_SET_CONSOLE_ROUTING,   mxcp_cmd_fn_set_console_routing,   sizeof(mxcp_cmd_set_console_routing_t),   MXCP_EVT_CONSOLE_ROUTING_RSP,    MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_SET_COMPOSITION,       mxcp_cmd_fn_set_composition,       0,                                        MXCP_EVT_SET_COMPOSITION_RSP,    MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_EL_SEND,               mxcp_cmd_fn_el_send,               sizeof(mxcp_cmd_el_send_t),               MXCP_EVT_EL_DATA_TX_NOTIFY,      MXCP_EVT_EL_DATA_RX_NOTIFY),
    MXCP_CMD_ENTRY(MXCP_CMD_GPIO_SET_LEVEL,        mxcp_cmd_fn_gpio_set_level,        sizeof(mxcp_cmd_gpio_set_level_t),        MXCP_EVT_GPIO_SET_LEVEL_RSP,     MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_GPIO_GET_LEVEL,        mxcp_cmd_fn_gpio_get_level,        sizeof(mxcp_cmd_gpio_get_level_t),        MXCP_EVT_GPIO_GET_LEVEL_RSP,     MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_GPIO_TOGGLE,           mxcp_cmd_fn_gpio_toggle,           sizeof(mxcp_cmd_gpio_toggle_t),           MXCP_EVT_GPIO_TOGGLE_RSP,        MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_GPIO_SET_PWM_DUTY,     mxcp_cmd_fn_gpio_set_pwm_duty,     sizeof(mxcp_cmd_gpio_set_pwm_duty_t),     MXCP_EVT_GPIO_SET_PWM_DUTY_RSP,  MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_GPIO_SET_PWM_FREQ,     mxcp_cmd_fn_gpio_set_pwm_freq,     sizeof(mxcp_cmd_gpio_set_pwm_freq_t),     MXCP_EVT_GPIO_SET_PWM_FREQ_RSP,  MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_GPIO_INTR_ENABLE,      mxcp_cmd_fn_gpio_intr_enable,      sizeof(mxcp_cmd_gpio_intr_enable_t),      MXCP_EVT_GPIO_INTR_ENABLE_RSP,   MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_GPIO_INTR_DISABLE,     mxcp_cmd_fn_gpio_intr_disable,     sizeof(mxcp_cmd_gpio_intr_disable_t),     MXCP_EVT_GPIO_INTR_DISABLE_RSP,  MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_GPIO_GET_CONFIG,       mxcp_cmd_fn_gpio_get_config,       sizeof(mxcp_cmd_gpio_get_config_t),       MXCP_EVT_GPIO_GET_CONFIG_RSP,    MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_GPIO_GET_STATE,        mxcp_cmd_fn_gpio_get_state,        sizeof(mxcp_cmd_gpio_get_state_t),        MXCP_EVT_GPIO_GET_STATE_RSP,     MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_DISCOVER_CAPABILITIES, mxcp_cmd_fn_discover_capabilities, sizeof(mxcp_cmd_discover_capabilities_t), MXCP_EVT_NONE,                   MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_BIND_ELEMENTS,         mxcp_cmd_fn_bind_elements,         sizeof(mxcp_cmd_bind_elements_t),         MXCP_EVT_BIND_STATUS,            MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_UNBIND_ELEMENTS,       mxcp_cmd_fn_unbind_elements,       sizeof(mxcp_cmd_unbind_elements_t),       MXCP_EVT_UNBIND_STATUS,          MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_GET_LINK_TABLE,        mxcp_cmd_fn_get_link_table,        0,                                        MXCP_EVT_LINK_TABLE_RSP,         MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_GET_LINK_EPOCH,        mxcp_cmd_fn_get_link_epoch,        0,                                        MXCP_EVT_LINK_EPOCH_RSP,         MXCP_EVT_NONE),
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
    uint8_t i = 0;

    if (payload != NULL)
    {
        for (i = 0; i < len; i++)
        {
            checksum ^= payload[i];
        }
    }
    return checksum;
}

/**
 * @brief Dispatch a received frame to the matching command handler.
 *
 * Performs a linear scan of mxcp_cmd_table for the given command ID.
 * EVT-typed frames are rejected with a warning.
 *
 * @param[in] type    TYPE byte (direction + ID).
 * @param[in] payload Payload bytes.
 * @param[in] len     Payload length in bytes.
 */
void mxcp_dispatch_frame(uint8_t type, const uint8_t *payload, uint8_t len)
{
    uint8_t cmd_id = 0;
    size_t i = 0;

    if (len > 0 && payload == NULL)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "MXCP: payload NULL with non-zero length %u", len);
        return;
    }

    if (MXCP_TYPE_IS_EVT(type))
    {
        MESHX_LOGW(MODULE_ID_COMMON, "MXCP: received unexpected EVT frame 0x%02x", type);
        return;
    }

    cmd_id = MXCP_TYPE_ID(type);

    for (i = 0; i < MXCP_CMD_TABLE_SIZE; i++)
    {
        if (mxcp_cmd_table[i].cmd_id == cmd_id)
        {
            if (mxcp_cmd_table[i].handler != NULL)
            {
                mxcp_cmd_table[i].handler(payload, len);
            }
            return;
        }
    }

    MESHX_LOGW(MODULE_ID_COMMON, "MXCP: unhandled CMD 0x%02x", cmd_id);
}

/**
 * @brief Build and transmit a variable-length MXCP frame.
 *
 * Serialises [SOF][LEN][TYPE][PAYLOAD][CHK][EOF] into a flat buffer
 * and writes it to the UART. No-op when hosted mode is disabled.
 *
 * @param[in] type    TYPE byte.
 * @param[in] payload Payload bytes (may be NULL).
 * @param[in] len     Payload length in bytes.
 *
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t mxcp_send_frame(uint8_t type, const uint8_t *payload, uint8_t len)
{
    uint8_t frame_buff[3 + MXCP_PAYLOAD_MAX_SIZE + 2];
    uint8_t ptr = 0;

    if (!mxsp_ctx.hosted_mode_enabled)
    {
        return MESHX_SUCCESS;
    }

    frame_buff[ptr++] = MXCP_SOF;
    frame_buff[ptr++] = len;
    frame_buff[ptr++] = type;
    if (len > 0 && payload != NULL)
    {
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
 * @param[in] evt_id  Event ID.
 * @param[in] payload Typed event payload.
 * @param[in] len     Payload length in bytes.
 *
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t mxcp_send_event(mxcp_evt_id_t evt_id, const uint8_t *payload, uint8_t len)
{
    uint8_t type = MXCP_MAKE_TYPE(MXCP_TYPE_DIR_EVT, evt_id);
    return mxcp_send_frame(type, payload, len);
}

/**
 * @brief Send an MXCP command (Host -> Engine).
 *
 * @param[in] cmd_id  Command ID.
 * @param[in] payload Typed command payload.
 * @param[in] len     Payload length in bytes.
 *
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t mxcp_send_cmd(mxcp_cmd_id_t cmd_id, const uint8_t *payload, uint8_t len)
{
    uint8_t type = MXCP_MAKE_TYPE(MXCP_TYPE_DIR_CMD, cmd_id);
    return mxcp_send_frame(type, payload, len);
}

/**
 * @brief Query the composition builder and send the response as an MXCP event.
 *
 * @param[in] evt_id MXCP_EVT_COMPOSITION_RSP or MXCP_EVT_ELEMENT_STATE_RSP.
 */
void mxcp_send_element_info_response(mxcp_evt_id_t evt_id)
{
    uint8_t resp_buf[MXCP_PAYLOAD_MAX_SIZE];
    size_t data_len = 0;

    if (evt_id == MXCP_EVT_COMPOSITION_RSP)
    {
        data_len = meshx_get_element_composition_data(resp_buf, sizeof(resp_buf));
    }
    else if (evt_id == MXCP_EVT_ELEMENT_STATE_RSP)
    {
        data_len = meshx_get_element_state_data(resp_buf, sizeof(resp_buf));
    }

    mxcp_send_event(evt_id, resp_buf, (uint8_t)data_len);
}

/**
 * @brief Handle MXCP_CMD_HOSTED_MODE_ENABLE.
 *
 * Enables or disables hosted mode on the engine. When disabled,
 * the TX path silently discards all outgoing frames.
 *
 * @param[in] payload Pointer to mxcp_cmd_hosted_mode_enable_t (enable != 0 to activate).
 * @param[in] len     Must be >= sizeof(mxcp_cmd_hosted_mode_enable_t).
 */
static void mxcp_cmd_fn_hosted_mode(const uint8_t *payload, uint8_t len)
{
    mxcp_cmd_hosted_mode_enable_t cmd;
    bool enabled = false;

    if (payload == NULL || len < sizeof(mxcp_cmd_hosted_mode_enable_t))
    {
        return;
    }
    memcpy(&cmd, payload, sizeof(cmd));
    enabled = (cmd.enable != 0);
    meshx_serial_set_hosted_mode(enabled);
    MESHX_LOGI(MODULE_ID_COMMON, "Hosted mode %s via MXCP", enabled ? "enabled" : "disabled");
}

/**
 * @brief Handle MXCP_CMD_NODE_RESET.
 *
 * Erases dynamic composition schema from NVS and triggers a platform-level reset.
 *
 * @param[in] payload Unused.
 * @param[in] len     Unused.
 */
static void mxcp_cmd_fn_node_reset(const uint8_t *payload, uint8_t len)
{
    (void)payload;
    (void)len;
    meshx_nvs_comp_schema_erase();
    meshx_platform_reset();
}

/**
 * @brief Software timer callback for scheduled reboot following composition change.
 *
 * @param[in] timer Pointer to the OS timer structure.
 */
static void mxcp_reboot_timer_cb(const meshx_os_timer_t *timer)
{
    (void)timer;
    MESHX_LOGI(MODULE_ID_COMMON, "Rebooting MCU to apply new composition...");
    meshx_platform_reset();
}

/**
 * @brief Handle MXCP_CMD_SET_COMPOSITION.
 *
 * Configures the element composition remotely, commits it to NVS with override flag,
 * sends response event to the host, and schedules a graceful reset.
 *
 * @param[in] payload Pointer to mxcp_cmd_set_composition_t followed by element_types array.
 * @param[in] len     Total length of the payload in bytes.
 */
static void mxcp_cmd_fn_set_composition(const uint8_t *payload, uint8_t len)
{
    uint8_t i                 = 0;
    uint16_t delay_ms         = 0;
    size_t expected_len       = 0;
    uint8_t dyn_idx           = 0;
    uint8_t cur_elem_count    = 0;
    uint8_t el                = 0xFF;
    bool mutated              = false;
    meshx_err_t err           = MESHX_SUCCESS;
    meshx_nvs_comp_schema_t schema;
    meshx_nvs_link_table_t cur_table;
    mxcp_evt_set_composition_rsp_t rsp;
    const mxcp_cmd_set_composition_t *cmd        = NULL;
    static meshx_os_timer_t *s_comp_reboot_timer = NULL;

    rsp.element_count   = 0;
    rsp.reboot_delay_ms = 0;
    rsp.status          = MXCP_COMP_STATUS_SUCCESS;

    if (payload == NULL || len < sizeof(mxcp_cmd_set_composition_t))
    {
        rsp.status = MXCP_COMP_STATUS_ERR_INVALID_TYPE;
        mxcp_send_event(MXCP_EVT_SET_COMPOSITION_RSP, (const uint8_t *)&rsp, sizeof(rsp));
        return;
    }

    cmd = (const mxcp_cmd_set_composition_t *)payload;
    if (cmd == NULL)
    {
        rsp.status = MXCP_COMP_STATUS_ERR_INVALID_TYPE;
        mxcp_send_event(MXCP_EVT_SET_COMPOSITION_RSP, (const uint8_t *)&rsp, sizeof(rsp));
        return;
    }

    if (cmd->element_count == 0 || cmd->element_count > CONFIG_MAX_ELEMENT_COUNT)
    {
        rsp.status = MXCP_COMP_STATUS_ERR_COUNT_EXCEEDED;
        rsp.element_count = cmd->element_count;
        mxcp_send_event(MXCP_EVT_SET_COMPOSITION_RSP, (const uint8_t *)&rsp, sizeof(rsp));
        return;
    }

    expected_len = sizeof(mxcp_cmd_set_composition_t) + (cmd->element_count * sizeof(uint16_t));
    if (len < expected_len)
    {
        rsp.status = MXCP_COMP_STATUS_ERR_INVALID_TYPE;
        mxcp_send_event(MXCP_EVT_SET_COMPOSITION_RSP, (const uint8_t *)&rsp, sizeof(rsp));
        return;
    }

    /* Validate element types */
    for (i = 0; i < cmd->element_count; i++)
    {
        if (cmd->element_types[i] >= MESHX_ELEMENT_TYPE_MAX)
        {
            rsp.status = MXCP_COMP_STATUS_ERR_INVALID_TYPE;
            mxcp_send_event(MXCP_EVT_SET_COMPOSITION_RSP, (const uint8_t *)&rsp, sizeof(rsp));
            return;
        }
    }

    /* Populate schema to save in NVS */
    memset(&schema, 0, sizeof(schema));
    schema.magic         = MESHX_NVS_COMP_MAGIC;
    schema.version       = MESHX_NVS_COMP_VERSION;
    schema.flags         = MESHX_COMP_FLAG_OVERRIDE_RO;
    schema.element_count = cmd->element_count;
    for (i = 0; i < cmd->element_count; i++)
    {
        schema.element_types[i] = cmd->element_types[i];
    }

    err = meshx_nvs_comp_schema_set(&schema);
    if (err != MESHX_SUCCESS)
    {
        rsp.status = MXCP_COMP_STATUS_ERR_NVS_WRITE;
        mxcp_send_event(MXCP_EVT_SET_COMPOSITION_RSP, (const uint8_t *)&rsp, sizeof(rsp));
        return;
    }

    /* Proactively unpair peers of mutated or removed elements */
    memset(&cur_table, 0, sizeof(cur_table));
    if (meshx_nvs_link_table_get(&cur_table) == MESHX_SUCCESS)
    {
        cur_elem_count = meshx_comp_get_element_count();
        for (i = 0; i < MESHX_MAX_PERSISTED_LINKS; i++)
        {
            if (cur_table.links[i].active &&
                cur_table.links[i].target_unicast_addr != MESHX_ADDR_UNASSIGNED)
            {
                el = cur_table.links[i].local_element_idx;
                mutated = false;

                if (el == 0)
                {
                    /* Root element (index 0) is static and never mutated by dynamic composition */
                    mutated = false;
                }
                else if (el >= cur_elem_count)
                {
                    /* Stale link referencing an element that exceeds current composition */
                    mutated = true;
                }
                else
                {
                    /* Dynamic elements are 1-based (el=1 corresponds to cmd->element_types[0]) */
                    dyn_idx = el - 1;
                    if (dyn_idx >= cmd->element_count)
                    {
                        /* Element removed in new composition */
                        mutated = true;
                    }
                    else if (meshx_comp_get_element_variant(el) != cmd->element_types[dyn_idx])
                    {
                        /* Element variant mutated */
                        mutated = true;
                    }
                }

                if (mutated)
                {
                    meshx_unpair_element(el,
                                         cur_table.links[i].target_unicast_addr,
                                         1,
                                         cur_table.links[i].func_id);
                }
            }
        }
    }

    delay_ms = cmd->reboot_delay_ms ? cmd->reboot_delay_ms : 500;
    rsp.status = MXCP_COMP_STATUS_SUCCESS;
    rsp.element_count = cmd->element_count;
    rsp.reboot_delay_ms = delay_ms;

    mxcp_send_event(MXCP_EVT_SET_COMPOSITION_RSP, (const uint8_t *)&rsp, sizeof(rsp));

    /* Schedule reboot timer */
    err = meshx_os_timer_create("mxcp_comp_rst", delay_ms, false, mxcp_reboot_timer_cb, &s_comp_reboot_timer);
    if (err == MESHX_SUCCESS)
    {
        meshx_os_timer_start(s_comp_reboot_timer);
    }
    else
    {
        meshx_platform_reset();
    }
}

/**
 * @brief Handle MXCP_CMD_DISCOVER_CAPABILITIES.
 */
static void mxcp_cmd_fn_discover_capabilities(const uint8_t *payload, uint8_t len)
{
    if (payload == NULL || len < sizeof(mxcp_cmd_discover_capabilities_t))
    {
        return;
    }
    const mxcp_cmd_discover_capabilities_t *cmd = (const mxcp_cmd_discover_capabilities_t *)payload;
    if (cmd->target_unicast == MESHX_ADDR_UNASSIGNED)
    {
        return;
    }
    meshx_uvp_discovery_query_t query;
    memset(&query, 0, sizeof(query));
    query.filter_role = cmd->filter_role;
    query.msg_type = MESHX_SYS_CMD_DISCOVERY_QUERY;
    meshx_comp_send_uvp(0, cmd->target_unicast, MESHX_FUNC_ID_SYS_DISCOVERY, &query, sizeof(query), false);
}

/**
 * @brief Handle MXCP_CMD_BIND_ELEMENTS.
 */
static void mxcp_cmd_fn_bind_elements(const uint8_t *payload, uint8_t len)
{
    mxcp_evt_bind_status_t evt;
    memset(&evt, 0, sizeof(evt));

    if (payload == NULL || len < sizeof(mxcp_cmd_bind_elements_t))
    {
        evt.status = MESHX_SYS_STATUS_ERR_INCOMPATIBLE;
        mxcp_send_event(MXCP_EVT_BIND_STATUS, (const uint8_t *)&evt, sizeof(evt));
        return;
    }

    const mxcp_cmd_bind_elements_t *cmd = (const mxcp_cmd_bind_elements_t *)payload;
    if (cmd->dst_addr == MESHX_ADDR_UNASSIGNED)
    {
        evt.status = MESHX_SYS_STATUS_ERR_INCOMPATIBLE;
        mxcp_send_event(MXCP_EVT_BIND_STATUS, (const uint8_t *)&evt, sizeof(evt));
        return;
    }

    meshx_err_t err = meshx_pair_initiate(cmd->src_elem, cmd->dst_addr, cmd->dst_elem, cmd->func_id);
    if (err == MESHX_SUCCESS)
    {
        evt.status = MESHX_SYS_STATUS_SUCCESS;
    }
    else if (err == MESHX_BUSY)
    {
        evt.status = MESHX_SYS_STATUS_ERR_BUSY;
    }
    else if (err == MESHX_NO_MEM)
    {
        evt.status = MESHX_SYS_STATUS_ERR_TABLE_FULL;
    }
    else
    {
        evt.status = MESHX_SYS_STATUS_ERR_INCOMPATIBLE;
    }
    mxcp_send_event(MXCP_EVT_BIND_STATUS, (const uint8_t *)&evt, sizeof(evt));
}

/**
 * @brief Handle MXCP_CMD_UNBIND_ELEMENTS.
 */
static void mxcp_cmd_fn_unbind_elements(const uint8_t *payload, uint8_t len)
{
    mxcp_evt_unbind_status_t evt;
    memset(&evt, 0, sizeof(evt));

    if (payload == NULL || len < sizeof(mxcp_cmd_unbind_elements_t))
    {
        evt.status = MESHX_SYS_STATUS_ERR_ELEMENT_MISSING;
        mxcp_send_event(MXCP_EVT_UNBIND_STATUS, (const uint8_t *)&evt, sizeof(evt));
        return;
    }

    const mxcp_cmd_unbind_elements_t *cmd = (const mxcp_cmd_unbind_elements_t *)payload;
    if (cmd->dst_addr == MESHX_ADDR_UNASSIGNED)
    {
        evt.status = MESHX_SYS_STATUS_ERR_ELEMENT_MISSING;
        mxcp_send_event(MXCP_EVT_UNBIND_STATUS, (const uint8_t *)&evt, sizeof(evt));
        return;
    }

    meshx_err_t err = meshx_unpair_element(cmd->src_elem, cmd->dst_addr, cmd->dst_elem, cmd->func_id);
    evt.status = (err == MESHX_SUCCESS) ? MESHX_SYS_STATUS_SUCCESS : MESHX_SYS_STATUS_ERR_ELEMENT_MISSING;
    mxcp_send_event(MXCP_EVT_UNBIND_STATUS, (const uint8_t *)&evt, sizeof(evt));
}

/**
 * @brief Handle MXCP_CMD_GET_LINK_TABLE.
 */
static void mxcp_cmd_fn_get_link_table(const uint8_t *payload, uint8_t len)
{
    MESHX_UNUSED(payload);
    MESHX_UNUSED(len);
    meshx_nvs_link_table_t table;
    if (meshx_nvs_link_table_get(&table) == MESHX_SUCCESS)
    {
        mxcp_send_event(MXCP_EVT_LINK_TABLE_RSP, (const uint8_t *)&table, sizeof(table));
    }
}

/**
 * @brief Handle MXCP_CMD_GET_LINK_EPOCH.
 */
static void mxcp_cmd_fn_get_link_epoch(const uint8_t *payload, uint8_t len)
{
    MESHX_UNUSED(payload);
    MESHX_UNUSED(len);
    mxcp_evt_link_epoch_rsp_t rsp;
    rsp.link_epoch = meshx_nvs_get_link_epoch();
    rsp.node_addr  = meshx_get_node_addr();
    mxcp_send_event(MXCP_EVT_LINK_EPOCH_RSP, (const uint8_t *)&rsp, sizeof(rsp));
}

/**
 * @brief Handle MXCP_CMD_GET_COMPOSITION.
 *
 * Requests both composition data and element state from the builder.
 * Sends MXCP_EVT_COMPOSITION_RSP followed by MXCP_EVT_ELEMENT_STATE_RSP.
 *
 * @param[in] payload Unused.
 * @param[in] len     Unused.
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
 * @param[in] payload Unused.
 * @param[in] len     Unused.
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
 * @param[in] payload Pointer to mxcp_cmd_set_console_routing_t (enable != 0 to route).
 * @param[in] len     Must be >= sizeof(mxcp_cmd_set_console_routing_t).
 */
static void mxcp_cmd_fn_set_console_routing(const uint8_t *payload, uint8_t len)
{
    mxcp_cmd_set_console_routing_t cmd;

    if (payload == NULL || len < sizeof(mxcp_cmd_set_console_routing_t))
    {
        return;
    }
    memcpy(&cmd, payload, sizeof(cmd));
    meshx_platform_set_mxsp_use_console(cmd.enable != 0);
}

/**
 * @brief Handle MXCP_CMD_EL_SEND.
 *
 * Forwards element-bound data from the host to the target element
 * via meshx_send_msg_to_element(). The payload beyond the header
 * is treated as the message body.
 *
 * @param[in] payload Pointer to mxcp_cmd_el_send_t header followed by message body.
 * @param[in] len     Must be >= sizeof(mxcp_cmd_el_send_t).
 */
static void mxcp_cmd_fn_el_send(const uint8_t *payload, uint8_t len)
{
    mxcp_cmd_el_send_t hdr;
    mxcp_evt_el_data_tx_notify_t tx_hdr;
    uint32_t buf_size = 0;
    uint8_t *buf = NULL;

    if (payload == NULL || len < sizeof(hdr))
    {
        return;
    }

    memcpy(&hdr, payload, sizeof(hdr));
    meshx_send_msg_to_element(hdr.element_id, hdr.element_type, hdr.func_id,
                              len - sizeof(hdr),
                              &payload[sizeof(hdr)]);

    tx_hdr.element_id = hdr.element_id;
    tx_hdr.element_type = hdr.element_type;
    tx_hdr.func_id = hdr.func_id;
    tx_hdr.msg_len = len - sizeof(hdr);

    buf_size = sizeof(tx_hdr) + tx_hdr.msg_len;
    buf = (uint8_t *)malloc(buf_size);
    if (buf)
    {
        memcpy(buf, &tx_hdr, sizeof(tx_hdr));
        if (tx_hdr.msg_len > 0)
        {
            memcpy(buf + sizeof(tx_hdr), &payload[sizeof(hdr)], tx_hdr.msg_len);
        }
        mxcp_send_event(MXCP_EVT_EL_DATA_TX_NOTIFY, buf, (uint8_t)buf_size);
        free(buf);
    }
}

/**
 * @brief Handle MXCP_CMD_GPIO_SET_LEVEL.
 *
 * Sets the output level on a GPIO pin and sends a status-only response
 * (status + logical_pin, 2 bytes).
 *
 * @param[in] payload Pointer to mxcp_cmd_gpio_set_level_t (logical_pin, level).
 * @param[in] len     Must be >= sizeof(mxcp_cmd_gpio_set_level_t).
 */
static void mxcp_cmd_fn_gpio_set_level(const uint8_t *payload, uint8_t len)
{
    mxcp_cmd_gpio_set_level_t cmd;
    mxcp_evt_gpio_rsp_t rsp;
    meshx_err_t err = MESHX_SUCCESS;

    if (payload == NULL || len < sizeof(mxcp_cmd_gpio_set_level_t))
    {
        return;
    }
    memcpy(&cmd, payload, sizeof(cmd));
    err = meshx_gpio_set_level(cmd.logical_pin, cmd.level);
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
 * @param[in] payload Pointer to mxcp_cmd_gpio_get_level_t (logical_pin).
 * @param[in] len     Must be >= sizeof(mxcp_cmd_gpio_get_level_t).
 */
static void mxcp_cmd_fn_gpio_get_level(const uint8_t *payload, uint8_t len)
{
    mxcp_cmd_gpio_get_level_t cmd;
    mxcp_evt_gpio_rsp_t rsp;
    uint8_t level = 0;
    meshx_err_t err = MESHX_SUCCESS;

    if (payload == NULL || len < sizeof(mxcp_cmd_gpio_get_level_t))
    {
        return;
    }
    memcpy(&cmd, payload, sizeof(cmd));
    err = meshx_gpio_get_level(cmd.logical_pin, &level);
    memset(&rsp, 0, sizeof(rsp));
    rsp.status = (uint8_t)err;
    rsp.logical_pin = cmd.logical_pin;
    if (err == MESHX_SUCCESS)
    {
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
 * @param[in] payload Pointer to mxcp_cmd_gpio_toggle_t (logical_pin).
 * @param[in] len     Must be >= sizeof(mxcp_cmd_gpio_toggle_t).
 */
static void mxcp_cmd_fn_gpio_toggle(const uint8_t *payload, uint8_t len)
{
    mxcp_cmd_gpio_toggle_t cmd;
    mxcp_evt_gpio_rsp_t rsp;
    uint8_t level = 0;
    meshx_err_t err = MESHX_SUCCESS;

    if (payload == NULL || len < sizeof(mxcp_cmd_gpio_toggle_t))
    {
        return;
    }
    memcpy(&cmd, payload, sizeof(cmd));
    err = meshx_gpio_toggle(cmd.logical_pin);
    memset(&rsp, 0, sizeof(rsp));
    rsp.status = (uint8_t)err;
    rsp.logical_pin = cmd.logical_pin;
    if (err == MESHX_SUCCESS)
    {
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
 * @param[in] payload Pointer to mxcp_cmd_gpio_set_pwm_duty_t (logical_pin, duty_cycle).
 * @param[in] len     Must be >= sizeof(mxcp_cmd_gpio_set_pwm_duty_t).
 */
static void mxcp_cmd_fn_gpio_set_pwm_duty(const uint8_t *payload, uint8_t len)
{
    mxcp_cmd_gpio_set_pwm_duty_t cmd;
    mxcp_evt_gpio_rsp_t rsp;
    uint32_t args[1];
    meshx_err_t err = MESHX_SUCCESS;

    if (payload == NULL || len < sizeof(mxcp_cmd_gpio_set_pwm_duty_t))
    {
        return;
    }
    memcpy(&cmd, payload, sizeof(cmd));
    args[0] = cmd.duty_cycle;
    err = meshx_gpio_execute_function(cmd.logical_pin,
                                      MESHX_IO_FUNCTION_SET_PWM_DUTY,
                                      args, 1);
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
 * @param[in] payload Pointer to mxcp_cmd_gpio_set_pwm_freq_t (logical_pin, frequency).
 * @param[in] len     Must be >= sizeof(mxcp_cmd_gpio_set_pwm_freq_t).
 */
static void mxcp_cmd_fn_gpio_set_pwm_freq(const uint8_t *payload, uint8_t len)
{
    mxcp_cmd_gpio_set_pwm_freq_t cmd;
    mxcp_evt_gpio_rsp_t rsp;
    uint32_t args[1];
    meshx_err_t err = MESHX_SUCCESS;

    if (payload == NULL || len < sizeof(mxcp_cmd_gpio_set_pwm_freq_t))
    {
        return;
    }
    memcpy(&cmd, payload, sizeof(cmd));
    args[0] = cmd.frequency;
    err = meshx_gpio_execute_function(cmd.logical_pin,
                                      MESHX_IO_FUNCTION_SET_PWM_FREQUENCY,
                                      args, 1);
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
 * @param[in] payload Pointer to mxcp_cmd_gpio_intr_enable_t (logical_pin, enable).
 * @param[in] len     Must be >= sizeof(mxcp_cmd_gpio_intr_enable_t).
 */
static void mxcp_cmd_fn_gpio_intr_enable(const uint8_t *payload, uint8_t len)
{
    mxcp_cmd_gpio_intr_enable_t cmd;
    mxcp_evt_gpio_rsp_t rsp;
    bool enable = false;
    meshx_err_t err = MESHX_SUCCESS;

    if (payload == NULL || len < sizeof(mxcp_cmd_gpio_intr_enable_t))
    {
        return;
    }
    memcpy(&cmd, payload, sizeof(cmd));
    enable = (cmd.enable != 0);
    err = meshx_gpio_intr_enable(cmd.logical_pin, enable);
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
 * @param[in] payload Pointer to mxcp_cmd_gpio_intr_disable_t (logical_pin).
 * @param[in] len     Must be >= sizeof(mxcp_cmd_gpio_intr_disable_t).
 */
static void mxcp_cmd_fn_gpio_intr_disable(const uint8_t *payload, uint8_t len)
{
    mxcp_cmd_gpio_intr_disable_t cmd;
    mxcp_evt_gpio_rsp_t rsp;
    meshx_err_t err = MESHX_SUCCESS;

    if (payload == NULL || len < sizeof(mxcp_cmd_gpio_intr_disable_t))
    {
        return;
    }
    memcpy(&cmd, payload, sizeof(cmd));
    err = meshx_gpio_intr_enable(cmd.logical_pin, false);
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
 * @param[in] payload Pointer to mxcp_cmd_gpio_get_config_t (logical_pin).
 * @param[in] len     Must be >= sizeof(mxcp_cmd_gpio_get_config_t).
 */
static void mxcp_cmd_fn_gpio_get_config(const uint8_t *payload, uint8_t len)
{
    mxcp_cmd_gpio_get_config_t cmd;
    meshx_gpio_pin_config_t config;
    mxcp_evt_gpio_rsp_t rsp;
    meshx_err_t err = MESHX_SUCCESS;

    if (payload == NULL || len < sizeof(mxcp_cmd_gpio_get_config_t))
    {
        return;
    }
    memcpy(&cmd, payload, sizeof(cmd));
    err = meshx_gpio_get_pin_config(cmd.logical_pin, &config);
    memset(&rsp, 0, sizeof(rsp));
    rsp.status = (uint8_t)err;
    rsp.logical_pin = cmd.logical_pin;
    if (err == MESHX_SUCCESS)
    {
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
 * @param[in] payload Pointer to mxcp_cmd_gpio_get_state_t (logical_pin).
 * @param[in] len     Must be >= sizeof(mxcp_cmd_gpio_get_state_t).
 */
static void mxcp_cmd_fn_gpio_get_state(const uint8_t *payload, uint8_t len)
{
    mxcp_cmd_gpio_get_state_t cmd;
    meshx_gpio_pin_state_t state;
    mxcp_evt_gpio_rsp_t rsp;
    meshx_err_t err = MESHX_SUCCESS;

    if (payload == NULL || len < sizeof(mxcp_cmd_gpio_get_state_t))
    {
        return;
    }
    memcpy(&cmd, payload, sizeof(cmd));
    err = meshx_gpio_get_pin_state(cmd.logical_pin, &state);
    memset(&rsp, 0, sizeof(rsp));
    rsp.status = (uint8_t)err;
    rsp.logical_pin = cmd.logical_pin;
    if (err == MESHX_SUCCESS)
    {
        rsp.data.get_state.current_level = state.current_level;
        rsp.data.get_state.interrupt_registered = state.interrupt_registered ? 1 : 0;
        rsp.data.get_state.intr_type = (uint8_t)state.intr_type;
    }
    mxcp_send_event(MXCP_EVT_GPIO_GET_STATE_RSP, (const uint8_t *)&rsp,
                    2 + sizeof(rsp.data.get_state));
}
