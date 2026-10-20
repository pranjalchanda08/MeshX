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
#include "pairing/meshx_group_allocator.h"
#include "interface/meshx_platform.h"
#include "pairing/meshx_unpair_mgr.h"
#include "interface/gpio/meshx_gpio.h"
#include "interface/logging/meshx_log.h"
#include "interface/gpio/meshx_gpio_types.h"
#include "interface/rtos/meshx_rtos_utils.h"
#include "pairing/meshx_pair_orchestrator.h"

/* UVP Management Virtual Address */
#ifndef MESHX_MGMT_VIRTUAL_ADDR
#define MESHX_MGMT_VIRTUAL_ADDR 0x8F42
#endif

/*
 * Redundancy state tracking
 */
static meshx_prov_role_t s_redundancy_role       = MESHX_PROV_ROLE_STANDALONE;
static uint16_t          s_redundancy_peer_addr  = 0x0000;
static uint16_t          s_redundancy_pool_start = MESHX_PROV_POOL_A_START;
static uint16_t          s_redundancy_pool_end   = MESHX_PROV_POOL_A_END;
static meshx_os_timer_t *s_redundancy_hb_timer   = NULL;
static uint32_t          s_redundancy_sync_epoch = 1;

/*
 * Key Rotation state tracking
 */
typedef struct {
    uint8_t  active_phase;
    uint32_t target_epoch;
    uint8_t  new_key[MXCP_NETKEY_SIZE];
    uint8_t  burst_count;
} mxcp_kr_context_t;

extern void meshx_platform_reset(void);
extern void meshx_platform_set_mxsp_use_console(bool enable);
extern void meshx_platform_serial_write(const uint8_t *data, uint16_t len);

static mxcp_kr_context_t s_kr_ctx;
static meshx_os_timer_t *s_kr_burst_timer = NULL;
static meshx_os_timer_t *s_kr_overlap_timer = NULL;

/*
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
#if CONFIG_MESHX_ENABLE_GPIO_TEST_API
static void mxcp_cmd_fn_gpio_toggle            (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_gpio_set_level         (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_gpio_get_level         (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_gpio_get_state         (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_gpio_get_config        (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_gpio_intr_enable       (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_gpio_set_pwm_duty      (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_gpio_set_pwm_freq      (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_gpio_intr_disable      (const uint8_t *payload, uint8_t len);
#endif /* CONFIG_MESHX_ENABLE_GPIO_TEST_API */
static void mxcp_cmd_fn_get_element_state      (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_get_composition        (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_set_console_routing    (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_set_composition        (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_prov_enable            (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_prov_disable           (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_prov_get_state         (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_prov_set_name          (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_prov_auth_input        (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_uvp_rpr_scan_start     (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_uvp_rpr_tunnel_send    (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_uvp_key_rotate_trigger (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_uvp_redundancy_set_role(const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_uvp_fast_prov_start    (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_discover_capabilities  (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_bind_elements          (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_unbind_elements        (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_get_link_table         (const uint8_t *payload, uint8_t len);
static void mxcp_cmd_fn_get_link_epoch         (const uint8_t *payload, uint8_t len);

/*
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
    MXCP_CMD_ENTRY(MXCP_CMD_HOSTED_MODE_ENABLE,     mxcp_cmd_fn_hosted_mode,            sizeof(mxcp_cmd_hosted_mode_enable_t),    MXCP_EVT_HOSTED_MODE_RSP,        MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_NODE_RESET,             mxcp_cmd_fn_node_reset,             0,                                        MXCP_EVT_NONE,                   MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_GET_COMPOSITION,        mxcp_cmd_fn_get_composition,        0,                                        MXCP_EVT_COMPOSITION_RSP,        MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_GET_ELEMENT_STATE,      mxcp_cmd_fn_get_element_state,      0,                                        MXCP_EVT_ELEMENT_STATE_RSP,      MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_SET_CONSOLE_ROUTING,    mxcp_cmd_fn_set_console_routing,    sizeof(mxcp_cmd_set_console_routing_t),   MXCP_EVT_CONSOLE_ROUTING_RSP,    MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_SET_COMPOSITION,        mxcp_cmd_fn_set_composition,        0,                                        MXCP_EVT_SET_COMPOSITION_RSP,    MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_PROV_ENABLE,            mxcp_cmd_fn_prov_enable,            0,                                        MXCP_EVT_PROV_STATE_IND,         MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_PROV_DISABLE,           mxcp_cmd_fn_prov_disable,           0,                                        MXCP_EVT_PROV_STATE_IND,         MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_PROV_GET_STATE,         mxcp_cmd_fn_prov_get_state,         0,                                        MXCP_EVT_PROV_STATE_IND,         MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_PROV_SET_NAME,          mxcp_cmd_fn_prov_set_name,          0,                                        MXCP_EVT_NONE,                   MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_PROV_AUTH_INPUT,        mxcp_cmd_fn_prov_auth_input,        0,                                        MXCP_EVT_NONE,                   MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_EL_SEND,                mxcp_cmd_fn_el_send,                sizeof(mxcp_cmd_el_send_t),               MXCP_EVT_EL_DATA_TX_NOTIFY,      MXCP_EVT_EL_DATA_RX_NOTIFY),
#if CONFIG_MESHX_ENABLE_GPIO_TEST_API
    MXCP_CMD_ENTRY(MXCP_CMD_GPIO_SET_LEVEL,         mxcp_cmd_fn_gpio_set_level,         sizeof(mxcp_cmd_gpio_set_level_t),        MXCP_EVT_GPIO_SET_LEVEL_RSP,     MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_GPIO_GET_LEVEL,         mxcp_cmd_fn_gpio_get_level,         sizeof(mxcp_cmd_gpio_get_level_t),        MXCP_EVT_GPIO_GET_LEVEL_RSP,     MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_GPIO_TOGGLE,            mxcp_cmd_fn_gpio_toggle,            sizeof(mxcp_cmd_gpio_toggle_t),           MXCP_EVT_GPIO_TOGGLE_RSP,        MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_GPIO_SET_PWM_DUTY,      mxcp_cmd_fn_gpio_set_pwm_duty,      sizeof(mxcp_cmd_gpio_set_pwm_duty_t),     MXCP_EVT_GPIO_SET_PWM_DUTY_RSP,  MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_GPIO_SET_PWM_FREQ,      mxcp_cmd_fn_gpio_set_pwm_freq,      sizeof(mxcp_cmd_gpio_set_pwm_freq_t),     MXCP_EVT_GPIO_SET_PWM_FREQ_RSP,  MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_GPIO_INTR_ENABLE,       mxcp_cmd_fn_gpio_intr_enable,       sizeof(mxcp_cmd_gpio_intr_enable_t),      MXCP_EVT_GPIO_INTR_ENABLE_RSP,   MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_GPIO_INTR_DISABLE,      mxcp_cmd_fn_gpio_intr_disable,      sizeof(mxcp_cmd_gpio_intr_disable_t),     MXCP_EVT_GPIO_INTR_DISABLE_RSP,  MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_GPIO_GET_CONFIG,        mxcp_cmd_fn_gpio_get_config,        sizeof(mxcp_cmd_gpio_get_config_t),       MXCP_EVT_GPIO_GET_CONFIG_RSP,    MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_GPIO_GET_STATE,         mxcp_cmd_fn_gpio_get_state,         sizeof(mxcp_cmd_gpio_get_state_t),        MXCP_EVT_GPIO_GET_STATE_RSP,     MXCP_EVT_NONE),
#endif /* CONFIG_MESHX_ENABLE_GPIO_TEST_API */
    MXCP_CMD_ENTRY(MXCP_CMD_DISCOVER_CAPABILITIES,  mxcp_cmd_fn_discover_capabilities,  sizeof(mxcp_cmd_discover_capabilities_t), MXCP_EVT_NONE,                   MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_BIND_ELEMENTS,          mxcp_cmd_fn_bind_elements,          sizeof(mxcp_cmd_bind_elements_t),         MXCP_EVT_BIND_STATUS,            MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_UNBIND_ELEMENTS,        mxcp_cmd_fn_unbind_elements,        sizeof(mxcp_cmd_unbind_elements_t),       MXCP_EVT_UNBIND_STATUS,          MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_GET_LINK_TABLE,         mxcp_cmd_fn_get_link_table,         0,                                        MXCP_EVT_LINK_TABLE_RSP,         MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_GET_LINK_EPOCH,         mxcp_cmd_fn_get_link_epoch,         0,                                        MXCP_EVT_LINK_EPOCH_RSP,         MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_UVP_RPR_SCAN_START,     mxcp_cmd_fn_uvp_rpr_scan_start,     0,                                        MXCP_EVT_NONE,                   MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_UVP_RPR_TUNNEL_SEND,    mxcp_cmd_fn_uvp_rpr_tunnel_send,    0,                                        MXCP_EVT_NONE,                   MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_UVP_KEY_ROTATE_TRIGGER, mxcp_cmd_fn_uvp_key_rotate_trigger, 0,                                        MXCP_EVT_UVP_KEY_ROTATION_PHASE, MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_UVP_REDUNDANCY_SET_ROLE,mxcp_cmd_fn_uvp_redundancy_set_role,0,                                        MXCP_EVT_UVP_REDUNDANCY_STATUS,  MXCP_EVT_NONE),
    MXCP_CMD_ENTRY(MXCP_CMD_UVP_FAST_PROV_START,    mxcp_cmd_fn_uvp_fast_prov_start,    0,                                        MXCP_EVT_UVP_FAST_PROV_PROGRESS, MXCP_EVT_NONE),
};

#define MXCP_CMD_TABLE_SIZE (sizeof(mxcp_cmd_table) / sizeof(mxcp_cmd_table[0]))

/**
 * @brief Compute XOR checksum over LEN, TYPE, and PAYLOAD fields.
 *
 * @param[in] len     Payload length byte.
 * @param[in] type    TYPE byte (direction + ID).
 * @param[in] payload Payload bytes (may be NULL if len == 0).
 *
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

/*
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

/*
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

/*
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

/*
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

/*
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

    delay_ms = cmd->reboot_delay_ms ? cmd->reboot_delay_ms : MESHX_COMP_RESET_DEFAULT_DELAY_MS;
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
 *
 * Initiates capability discovery by transmitting a MESHX_SYS_CMD_DISCOVERY_QUERY
 * UVP frame to the target node's unicast address.
 *
 * @param[in] payload Pointer to mxcp_cmd_discover_capabilities_t.
 * @param[in] len     Must be >= sizeof(mxcp_cmd_discover_capabilities_t).
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
 *
 * Initiates binding between a local source element and a remote destination element
 * by invoking meshx_pair_initiate() and reporting MXCP_EVT_BIND_STATUS.
 *
 * @param[in] payload Pointer to mxcp_cmd_bind_elements_t.
 * @param[in] len     Must be >= sizeof(mxcp_cmd_bind_elements_t).
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
 *
 * Removes a binding link between elements via meshx_unpair_element()
 * and sends MXCP_EVT_UNBIND_STATUS to the host.
 *
 * @param[in] payload Pointer to mxcp_cmd_unbind_elements_t.
 * @param[in] len     Must be >= sizeof(mxcp_cmd_unbind_elements_t).
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
 *
 * Retrieves the persisted link table from NVS and returns it via MXCP_EVT_LINK_TABLE_RSP.
 *
 * @param[in] payload Unused.
 * @param[in] len     Unused.
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
 *
 * Retrieves current link epoch counter and unicast address, returning MXCP_EVT_LINK_EPOCH_RSP.
 *
 * @param[in] payload Unused.
 * @param[in] len     Unused.
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
 * @brief Handle MXCP_CMD_PROV_ENABLE with Pre-State Guard.
 *
 * Enables BLE mesh provisioning bearer(s) if the node is unprovisioned.
 * Rejects with warning and provisioned state indicator if already provisioned.
 *
 * @param[in] payload Pointer to mxcp_cmd_prov_enable_t (bearer_mask).
 * @param[in] len     Payload length in bytes.
 */
static void mxcp_cmd_fn_prov_enable(const uint8_t *payload, uint8_t len)
{
    mxcp_evt_prov_state_ind_t ind;
    memset(&ind, 0, sizeof(ind));
    ind.unicast_addr = meshx_get_node_addr();
    ind.net_idx      = meshx_get_net_key_id();

    /* Pre-State Guard: reject if already provisioned */
    if (ind.unicast_addr != MESHX_ADDR_UNASSIGNED)
    {
        MESHX_LOGW(MODULE_ID_COMMON, "MXCP: Prov Enable rejected: Node already provisioned (addr=0x%04X)", ind.unicast_addr);
        ind.state = MXCP_PROV_STATE_PROVISIONED;
        mxcp_send_event(MXCP_EVT_PROV_STATE_IND, (const uint8_t *)&ind, sizeof(ind));
        return;
    }

    uint8_t bearer_mask = MXCP_PROV_BEARER_PB_ADV; /* Default to PB-ADV */
    if (payload != NULL && len >= sizeof(mxcp_cmd_prov_enable_t))
    {
        const mxcp_cmd_prov_enable_t *cmd = (const mxcp_cmd_prov_enable_t *)payload;
        bearer_mask = cmd->bearer_mask;
    }

    esp_ble_mesh_prov_bearer_t bearers = (esp_ble_mesh_prov_bearer_t)0;
    if (bearer_mask & MXCP_PROV_BEARER_PB_ADV) bearers = (esp_ble_mesh_prov_bearer_t)(bearers | ESP_BLE_MESH_PROV_ADV);
    if (bearer_mask & MXCP_PROV_BEARER_PB_GATT) bearers = (esp_ble_mesh_prov_bearer_t)(bearers | ESP_BLE_MESH_PROV_GATT);

    esp_err_t err = esp_ble_mesh_node_prov_enable(bearers);
    if (err == ESP_OK)
    {
        ind.state = MXCP_PROV_STATE_BEACONING;
    }
    else
    {
        MESHX_LOGE(MODULE_ID_COMMON, "MXCP: esp_ble_mesh_node_prov_enable failed (%d)", err);
        ind.state = MXCP_PROV_STATE_UNPROVISIONED;
    }
    mxcp_send_event(MXCP_EVT_PROV_STATE_IND, (const uint8_t *)&ind, sizeof(ind));
}

/**
 * @brief Handle MXCP_CMD_PROV_DISABLE.
 *
 * Disables BLE mesh provisioning bearer(s) and returns current state indication.
 *
 * @param[in] payload Unused.
 * @param[in] len     Unused.
 */
static void mxcp_cmd_fn_prov_disable(const uint8_t *payload, uint8_t len)
{
    MESHX_UNUSED(payload);
    MESHX_UNUSED(len);
    mxcp_evt_prov_state_ind_t ind;
    memset(&ind, 0, sizeof(ind));
    ind.unicast_addr = meshx_get_node_addr();
    ind.net_idx      = meshx_get_net_key_id();

    esp_ble_mesh_prov_bearer_t bearers = (esp_ble_mesh_prov_bearer_t)(ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT);
    esp_ble_mesh_node_prov_disable(bearers);

    ind.state = (ind.unicast_addr != MESHX_ADDR_UNASSIGNED) ? MXCP_PROV_STATE_PROVISIONED : MXCP_PROV_STATE_UNPROVISIONED;
    mxcp_send_event(MXCP_EVT_PROV_STATE_IND, (const uint8_t *)&ind, sizeof(ind));
}

/**
 * @brief Handle MXCP_CMD_PROV_GET_STATE.
 *
 * Queries node provisioning status and returns MXCP_EVT_PROV_STATE_IND.
 *
 * @param[in] payload Unused.
 * @param[in] len     Unused.
 */
static void mxcp_cmd_fn_prov_get_state(const uint8_t *payload, uint8_t len)
{
    MESHX_UNUSED(payload);
    MESHX_UNUSED(len);
    mxcp_evt_prov_state_ind_t ind;
    memset(&ind, 0, sizeof(ind));
    ind.unicast_addr = meshx_get_node_addr();
    ind.net_idx      = meshx_get_net_key_id();
    ind.state        = (ind.unicast_addr != MESHX_ADDR_UNASSIGNED) ? MXCP_PROV_STATE_PROVISIONED : MXCP_PROV_STATE_UNPROVISIONED;

    mxcp_send_event(MXCP_EVT_PROV_STATE_IND, (const uint8_t *)&ind, sizeof(ind));
}

/**
 * @brief Handle MXCP_CMD_PROV_SET_NAME.
 *
 * Sets the advertised BLE Mesh unprovisioned device name.
 *
 * @param[in] payload Pointer to unprovisioned device name string (up to 32 bytes).
 * @param[in] len     Length of device name in bytes.
 */
static void mxcp_cmd_fn_prov_set_name(const uint8_t *payload, uint8_t len)
{
    if (payload == NULL || len == 0) return;
    char name_buf[33];
    size_t copy_len = len < 32 ? len : 32;
    memcpy(name_buf, payload, copy_len);
    name_buf[copy_len] = '\0';

    esp_ble_mesh_set_unprovisioned_device_name(name_buf);
}

/**
 * @brief Handle MXCP_CMD_PROV_AUTH_INPUT.
 *
 * Submits user authentication value (numeric or alphanumeric) to the BLE Mesh provisioning stack.
 *
 * @param[in] payload Pointer to mxcp_cmd_prov_auth_input_t.
 * @param[in] len     Must be >= 2 bytes (auth_type and auth_len).
 */
static void mxcp_cmd_fn_prov_auth_input(const uint8_t *payload, uint8_t len)
{
    if (payload == NULL || len < 2) return;
    const mxcp_cmd_prov_auth_input_t *cmd = (const mxcp_cmd_prov_auth_input_t *)payload;
    if (cmd->auth_type == MXCP_PROV_AUTH_TYPE_NUMERIC) {
        /* Numeric input */
        uint32_t val = 0;
        if (cmd->auth_len >= 4) {
            memcpy(&val, cmd->auth_data, 4);
        }
        esp_ble_mesh_node_input_number(val);
    } else {
        /* Alphanumeric string */
        char str_buf[17];
        size_t slen = cmd->auth_len < 16 ? cmd->auth_len : 16;
        memcpy(str_buf, cmd->auth_data, slen);
        str_buf[slen] = '\0';
        esp_ble_mesh_node_input_string(str_buf);
    }
}

/**
 * @brief Key Rotation burst retransmission timer callback.
 *
 * Transmits remaining bursts (2 and 3) of the Phase 1 MESHX_UVP_OP_KR_DISTRIBUTE PDU
 * with randomized 2-5 second jitter intervals to ensure high delivery probability.
 *
 * @param[in] p_timer Pointer to the OS timer structure.
 */
static void mxcp_kr_burst_timer_cb(const meshx_os_timer_t *p_timer)
{
    MESHX_UNUSED(p_timer);
    if (s_kr_ctx.active_phase != MXCP_KR_PHASE_DISTRIBUTE) return;

    if (s_kr_ctx.burst_count < 3)
    {
        uint8_t dist_buf[1 + sizeof(uint32_t) + MXCP_NETKEY_SIZE];
        dist_buf[0] = MESHX_UVP_OP_KR_DISTRIBUTE;
        memcpy(&dist_buf[1], &s_kr_ctx.target_epoch, sizeof(uint32_t));
        memcpy(&dist_buf[1 + sizeof(uint32_t)], s_kr_ctx.new_key, MXCP_NETKEY_SIZE);

        meshx_comp_send_uvp(0,
                            MESHX_MGMT_VIRTUAL_ADDR,
                            MESHX_FUNC_ID_SYS_PAIRING,
                            dist_buf,
                            sizeof(dist_buf),
                            false);

        s_kr_ctx.burst_count++;
        MESHX_LOGI(MODULE_ID_COMMON, "UVP KR: Sent burst %u/3 for epoch %lu",
                   s_kr_ctx.burst_count, (unsigned long)s_kr_ctx.target_epoch);

        if (s_kr_ctx.burst_count < 3)
        {
            /* Next burst with jitter delay */
            uint32_t next_delay_ms = MESHX_KR_BURST_JITTER_BASE_MS + (rand() % MESHX_KR_BURST_JITTER_RANGE_MS);
            meshx_os_timer_set_period(s_kr_burst_timer, next_delay_ms);
            meshx_os_timer_start(s_kr_burst_timer);
        }
    }
}

/**
 * @brief Key Rotation 4-minute dual-key overlap window expiry callback.
 *
 * Automatically triggers Phase 3 (Revoke Old Key) broadcast across the mesh network
 * once the dual-key coexistence window expires, notifying the host of rotation completion.
 *
 * @param[in] p_timer Pointer to the OS timer structure.
 */
static void mxcp_kr_overlap_timer_cb(const meshx_os_timer_t *p_timer)
{
    MESHX_UNUSED(p_timer);
    MESHX_LOGI(MODULE_ID_COMMON, "UVP KR: 4-minute overlap expired, advancing to Phase 3 (Revoke Old)");

    /* Phase 3 Revocation broadcast to virtual address */
    uint8_t revoke_buf[1 + sizeof(uint32_t)];
    revoke_buf[0] = MESHX_UVP_OP_KR_REVOKE_OLD;
    memcpy(&revoke_buf[1], &s_kr_ctx.target_epoch, sizeof(uint32_t));

    meshx_comp_send_uvp(0,
                        MESHX_MGMT_VIRTUAL_ADDR,
                        MESHX_FUNC_ID_SYS_PAIRING,
                        revoke_buf,
                        sizeof(revoke_buf),
                        false);

    /* Update host */
    mxcp_evt_uvp_key_rot_phase_t rsp;
    memset(&rsp, 0, sizeof(rsp));
    rsp.current_phase  = MXCP_KR_PHASE_REVOKE;
    rsp.rotation_epoch = s_kr_ctx.target_epoch;
    rsp.status_code    = 0;
    mxcp_send_event(MXCP_EVT_UVP_KEY_ROTATION_PHASE, (const uint8_t *)&rsp, sizeof(rsp));

    s_kr_ctx.active_phase = MXCP_KR_PHASE_IDLE;
}

/**
 * @brief Provisioner Redundancy Master heartbeat timer callback.
 *
 * Periodically broadcasts 5-second heartbeats containing synchronization epoch
 * and unicast address to keep standby provisioners informed and prevent failover.
 *
 * @param[in] p_timer Pointer to the OS timer structure.
 */
static void mxcp_redundancy_hb_timer_cb(const meshx_os_timer_t *p_timer)
{
    MESHX_UNUSED(p_timer);
    if (s_redundancy_role != MESHX_PROV_ROLE_MASTER) return;

    /* Master broadcasts 5-second periodic redundancy heartbeat */
    uint8_t hb_buf[1 + sizeof(uint32_t) + sizeof(uint16_t)];
    hb_buf[0] = MESHX_UVP_OP_REDUNDANCY_SYNC;
    memcpy(&hb_buf[1], &s_redundancy_sync_epoch, sizeof(uint32_t));
    uint16_t my_addr = meshx_get_node_addr();
    memcpy(&hb_buf[1 + sizeof(uint32_t)], &my_addr, sizeof(uint16_t));

    uint16_t dst = (s_redundancy_peer_addr != 0) ? s_redundancy_peer_addr : MESHX_MGMT_VIRTUAL_ADDR;
    meshx_comp_send_uvp(0,
                        dst,
                        MESHX_FUNC_ID_SYS_PAIRING,
                        hb_buf,
                        sizeof(hb_buf),
                        false);
    s_redundancy_sync_epoch++;
}

/**
 * @brief Handle MXCP_CMD_UVP_RPR_SCAN_START.
 *
 * Encapsulates MESHX_UVP_OP_RPR_SCAN_REQ frame with scan duration and UUID filter,
 * transmitting it over UVP to the target remote provisioning proxy relay node.
 *
 * @param[in] payload Pointer to mxcp_cmd_uvp_rpr_scan_t.
 * @param[in] len     Must be >= sizeof(mxcp_cmd_uvp_rpr_scan_t).
 */
static void mxcp_cmd_fn_uvp_rpr_scan_start(const uint8_t *payload, uint8_t len)
{
    if (payload == NULL || len < sizeof(mxcp_cmd_uvp_rpr_scan_t)) return;
    const mxcp_cmd_uvp_rpr_scan_t *cmd = (const mxcp_cmd_uvp_rpr_scan_t *)payload;
    MESHX_LOGI(MODULE_ID_COMMON, "MXCP: RPR Scan Start commanded for proxy 0x%04X (duration=%u s)",
               cmd->proxy_node_addr, cmd->duration_sec);

    /* Encapsulate MESHX_UVP_OP_RPR_SCAN_REQ and transmit over UVP to proxy relay */
    uint8_t req_buf[1 + sizeof(uint8_t) + MXCP_UUID_SIZE];
    req_buf[0] = MESHX_UVP_OP_RPR_SCAN_REQ;
    req_buf[1] = cmd->duration_sec;
    memcpy(&req_buf[2], cmd->uuid_filter, MXCP_UUID_SIZE);

    uint16_t dst = (cmd->proxy_node_addr != 0 && cmd->proxy_node_addr != 0xFFFF)
                   ? cmd->proxy_node_addr
                   : MESHX_MGMT_VIRTUAL_ADDR;

    meshx_comp_send_uvp(0,
                        dst,
                        MESHX_FUNC_ID_SYS_PAIRING,
                        req_buf,
                        sizeof(req_buf),
                        false);
}

/**
 * @brief Handle MXCP_CMD_UVP_RPR_TUNNEL_SEND.
 *
 * Encapsulates outbound provisioning PDU into MESHX_UVP_OP_RPR_TUNNEL_DOWN frame
 * and transmits it to the remote proxy server node for device provisioning.
 *
 * @param[in] payload Pointer to mxcp_cmd_uvp_rpr_tunnel_t.
 * @param[in] len     Must be >= 18 bytes (header + target_uuid + pdu_len).
 */
static void mxcp_cmd_fn_uvp_rpr_tunnel_send(const uint8_t *payload, uint8_t len)
{
    if (payload == NULL || len < 18) return;
    const mxcp_cmd_uvp_rpr_tunnel_t *cmd = (const mxcp_cmd_uvp_rpr_tunnel_t *)payload;
    MESHX_LOGI(MODULE_ID_COMMON, "MXCP: RPR Tunnel PDU Send to proxy 0x%04X (pdu_len=%u)",
               cmd->proxy_node_addr, cmd->pdu_len);

    /* Encapsulate MESHX_UVP_OP_RPR_TUNNEL_DOWN [msg_type | target_uuid (16B) | pdu_len (1B) | pdu...] */
    uint8_t down_buf[1 + MXCP_UUID_SIZE + 1 + MXCP_RPR_PDU_MAX_SIZE];
    down_buf[0] = MESHX_UVP_OP_RPR_TUNNEL_DOWN;
    memcpy(&down_buf[1], cmd->target_uuid, MXCP_UUID_SIZE);
    down_buf[1 + MXCP_UUID_SIZE] = cmd->pdu_len;
    uint8_t copy_len = (cmd->pdu_len > MXCP_RPR_PDU_MAX_SIZE) ? MXCP_RPR_PDU_MAX_SIZE : cmd->pdu_len;
    memcpy(&down_buf[2 + MXCP_UUID_SIZE], cmd->pdu_data, copy_len);

    meshx_comp_send_uvp(0,
                        cmd->proxy_node_addr,
                        MESHX_FUNC_ID_SYS_PAIRING,
                        down_buf,
                        2 + MXCP_UUID_SIZE + copy_len,
                        false);
}

/**
 * @brief Handle MXCP_CMD_UVP_KEY_ROTATE_TRIGGER.
 *
 * Orchestrates multi-phase network key rotation (Phase 1: Distribution with jitter bursts,
 * Phase 2: Switchover, Phase 3: Revocation) and arms overlap guard timers.
 *
 * @param[in] payload Pointer to mxcp_cmd_uvp_key_rotate_t.
 * @param[in] len     Must be >= 3 bytes (phase + epoch/key fields).
 */
static void mxcp_cmd_fn_uvp_key_rotate_trigger(const uint8_t *payload, uint8_t len)
{
    if (payload == NULL || len < 3) return;
    const mxcp_cmd_uvp_key_rotate_t *cmd = (const mxcp_cmd_uvp_key_rotate_t *)payload;

    uint32_t current_epoch = meshx_nvs_get_kr_epoch();
    mxcp_evt_uvp_key_rot_phase_t rsp;
    memset(&rsp, 0, sizeof(rsp));
    rsp.current_phase  = cmd->phase;
    rsp.status_code    = 0; /* Success */

    if (cmd->phase == MXCP_KR_PHASE_DISTRIBUTE)
    {
        /*
         * Phase 1: Distribution
         * 1. Monotonically increment and atomically commit epoch to NVS before transmission
         */
        uint32_t next_epoch = current_epoch + 1;
        meshx_nvs_set_kr_epoch(next_epoch);
        rsp.rotation_epoch = next_epoch;

        s_kr_ctx.active_phase = MXCP_KR_PHASE_DISTRIBUTE;
        s_kr_ctx.target_epoch = next_epoch;
        s_kr_ctx.burst_count  = 1;
        memcpy(s_kr_ctx.new_key, cmd->new_key, MXCP_NETKEY_SIZE);

        /* 2. Broadcast Burst 1 to MESHX_MGMT_VIRTUAL_ADDR */
        uint8_t dist_buf[1 + sizeof(uint32_t) + MXCP_NETKEY_SIZE];
        dist_buf[0] = MESHX_UVP_OP_KR_DISTRIBUTE;
        memcpy(&dist_buf[1], &next_epoch, sizeof(uint32_t));
        memcpy(&dist_buf[1 + sizeof(uint32_t)], cmd->new_key, MXCP_NETKEY_SIZE);

        meshx_comp_send_uvp(0,
                            MESHX_MGMT_VIRTUAL_ADDR,
                            MESHX_FUNC_ID_SYS_PAIRING,
                            dist_buf,
                            sizeof(dist_buf),
                            false);

        /* 3. Arm randomized jitter timer for Burst 2 */
        if (!s_kr_burst_timer)
        {
            meshx_os_timer_create("kr_burst", MESHX_KR_BURST_INITIAL_DELAY_MS, false, mxcp_kr_burst_timer_cb, &s_kr_burst_timer);
        }
        if (s_kr_burst_timer)
        {
            uint32_t delay_ms = MESHX_KR_BURST_JITTER_BASE_MS + (rand() % MESHX_KR_BURST_JITTER_RANGE_MS);
            meshx_os_timer_set_period(s_kr_burst_timer, delay_ms);
            meshx_os_timer_start(s_kr_burst_timer);
        }

        /* 4. Arm dual-key overlap window timer */
        if (!s_kr_overlap_timer)
        {
            meshx_os_timer_create("kr_overlap", MESHX_KR_OVERLAP_WINDOW_MS, false, mxcp_kr_overlap_timer_cb, &s_kr_overlap_timer);
        }
        if (s_kr_overlap_timer)
        {
            meshx_os_timer_stop(s_kr_overlap_timer);
            meshx_os_timer_start(s_kr_overlap_timer);
        }

        MESHX_LOGI(MODULE_ID_COMMON, "UVP KR: Phase 1 initiated for epoch %lu, overlap armed (4 min)",
                   (unsigned long)next_epoch);
    }
    else if (cmd->phase == MXCP_KR_PHASE_SWITCHOVER)
    {
        /*
         * Phase 2: Switchover broadcast
         */
        rsp.rotation_epoch = current_epoch;
        uint8_t sw_buf[1 + sizeof(uint32_t)];
        sw_buf[0] = MESHX_UVP_OP_KR_SWITCHOVER;
        memcpy(&sw_buf[1], &current_epoch, sizeof(uint32_t));

        meshx_comp_send_uvp(0,
                            MESHX_MGMT_VIRTUAL_ADDR,
                            MESHX_FUNC_ID_SYS_PAIRING,
                            sw_buf,
                            sizeof(sw_buf),
                            false);
        MESHX_LOGI(MODULE_ID_COMMON, "UVP KR: Phase 2 Switchover broadcasted for epoch %lu",
                   (unsigned long)current_epoch);
    }
    else if (cmd->phase == MXCP_KR_PHASE_REVOKE)
    {
        /*
         * Phase 3: Revoke Old Key broadcast
         */
        rsp.rotation_epoch = current_epoch;
        uint8_t rev_buf[1 + sizeof(uint32_t)];
        rev_buf[0] = MESHX_UVP_OP_KR_REVOKE_OLD;
        memcpy(&rev_buf[1], &current_epoch, sizeof(uint32_t));

        meshx_comp_send_uvp(0,
                            MESHX_MGMT_VIRTUAL_ADDR,
                            MESHX_FUNC_ID_SYS_PAIRING,
                            rev_buf,
                            sizeof(rev_buf),
                            false);
        MESHX_LOGI(MODULE_ID_COMMON, "UVP KR: Phase 3 Revocation broadcasted for epoch %lu",
                   (unsigned long)current_epoch);
    }

    mxcp_send_event(MXCP_EVT_UVP_KEY_ROTATION_PHASE, (const uint8_t *)&rsp, sizeof(rsp));
}

/**
 * @brief Handle MXCP_CMD_UVP_REDUNDANCY_SET_ROLE.
 *
 * Configures provisioner redundancy role (Standalone, Master, Standby) and address pool,
 * arming or disarming the 5-second periodic heartbeat broadcast.
 *
 * @param[in] payload Pointer to mxcp_cmd_uvp_redundancy_role_t.
 * @param[in] len     Must be >= sizeof(mxcp_cmd_uvp_redundancy_role_t).
 */
static void mxcp_cmd_fn_uvp_redundancy_set_role(const uint8_t *payload, uint8_t len)
{
    if (payload == NULL || len < sizeof(mxcp_cmd_uvp_redundancy_role_t)) return;
    const mxcp_cmd_uvp_redundancy_role_t *cmd = (const mxcp_cmd_uvp_redundancy_role_t *)payload;

    s_redundancy_role = (meshx_prov_role_t)cmd->role;
    s_redundancy_peer_addr = cmd->peer_addr;

    if (cmd->pool_start != 0 && cmd->pool_end != 0)
    {
        s_redundancy_pool_start = cmd->pool_start;
        s_redundancy_pool_end = cmd->pool_end;
    }
    else
    {
        meshx_prov_get_address_pool(s_redundancy_role, &s_redundancy_pool_start, &s_redundancy_pool_end);
    }

    /* If Master, initialize 5-second heartbeat broadcast timer */
    if (s_redundancy_role == MESHX_PROV_ROLE_MASTER)
    {
        if (!s_redundancy_hb_timer)
        {
            meshx_os_timer_create("redun_hb", MESHX_REDUNDANCY_HB_INTERVAL_MS, true, mxcp_redundancy_hb_timer_cb, &s_redundancy_hb_timer);
        }
        if (s_redundancy_hb_timer)
        {
            meshx_os_timer_start(s_redundancy_hb_timer);
        }
    }
    else if (s_redundancy_hb_timer)
    {
        meshx_os_timer_stop(s_redundancy_hb_timer);
    }

    mxcp_evt_uvp_redundancy_status_t rsp;
    memset(&rsp, 0, sizeof(rsp));
    rsp.current_role   = s_redundancy_role;
    rsp.peer_alive     = 1;
    rsp.next_free_addr = s_redundancy_pool_start;
    rsp.sync_epoch     = s_redundancy_sync_epoch;

    mxcp_send_event(MXCP_EVT_UVP_REDUNDANCY_STATUS, (const uint8_t *)&rsp, sizeof(rsp));
}

/**
 * @brief Handle MXCP_CMD_UVP_FAST_PROV_START.
 *
 * Initiates fast bulk provisioning by dispatching MESHX_UVP_OP_FAST_PROV_BATCH_DESC
 * with target count and contiguous address block to the fast provisioning proxy.
 *
 * @param[in] payload Pointer to mxcp_cmd_uvp_fast_prov_start_t.
 * @param[in] len     Must be >= sizeof(mxcp_cmd_uvp_fast_prov_start_t).
 */
static void mxcp_cmd_fn_uvp_fast_prov_start(const uint8_t *payload, uint8_t len)
{
    if (payload == NULL || len < sizeof(mxcp_cmd_uvp_fast_prov_start_t)) return;
    const mxcp_cmd_uvp_fast_prov_start_t *cmd = (const mxcp_cmd_uvp_fast_prov_start_t *)payload;

    /*
     * Delegate contiguous address block reservation to proxy via MESHX_UVP_OP_FAST_PROV_BATCH_DESC
     */
    uint8_t desc_buf[1 + sizeof(uint16_t) + sizeof(uint16_t) + MXCP_UUID_SIZE];
    desc_buf[0] = MESHX_UVP_OP_FAST_PROV_BATCH_DESC;
    memcpy(&desc_buf[1], &cmd->base_addr, sizeof(uint16_t));
    memcpy(&desc_buf[1 + sizeof(uint16_t)], &cmd->target_count, sizeof(uint16_t));
    memcpy(&desc_buf[1 + sizeof(uint16_t) * 2], cmd->uuid_mask, MXCP_UUID_SIZE);

    meshx_comp_send_uvp(0,
                        MESHX_MGMT_VIRTUAL_ADDR,
                        MESHX_FUNC_ID_SYS_PAIRING,
                        desc_buf,
                        sizeof(desc_buf),
                        false);

    mxcp_evt_uvp_fast_prov_progress_t rsp;
    memset(&rsp, 0, sizeof(rsp));
    rsp.total_target     = cmd->target_count;
    rsp.onboarded_count  = 0;
    rsp.last_addr        = cmd->base_addr;
    rsp.progress_percent = 0;

    mxcp_send_event(MXCP_EVT_UVP_FAST_PROV_PROGRESS, (const uint8_t *)&rsp, sizeof(rsp));
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
    buf = NULL;
    if (MESHX_MALLOC(&buf, buf_size) == MESHX_SUCCESS && buf)
    {
        memcpy(buf, &tx_hdr, sizeof(tx_hdr));
        if (tx_hdr.msg_len > 0)
        {
            memcpy(buf + sizeof(tx_hdr), &payload[sizeof(hdr)], tx_hdr.msg_len);
        }
        mxcp_send_event(MXCP_EVT_EL_DATA_TX_NOTIFY, buf, (uint8_t)buf_size);
        MESHX_FREE(&buf);
    }
}

#if CONFIG_MESHX_ENABLE_GPIO_TEST_API
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
#endif

