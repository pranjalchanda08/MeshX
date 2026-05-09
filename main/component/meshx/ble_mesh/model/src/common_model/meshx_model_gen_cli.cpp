/**
 * @file meshx_model_gen_cli.cpp
 * @brief Implementation of Generic Client Model shim for C++ migration.
 */

#include "meshx_c_header.h"

extern "C" meshx_err_t meshx_gen_cli_send_msg(meshx_gen_client_send_params_t *params)
{
    if (!params) return MESHX_INVALID_ARG;

    bool is_get = false;
    /* Determine if opcode is GET (simplified check, usually based on opcode bitmask) */
    if ((params->opcode & 0xFF) == 0x8201) { // Example: GEN_ONOFF_GET
        is_get = true;
    }
    // Note: In real implementation, the platform layer should handle opcode properties.
    // For now, we delegate to platform layer.

    return meshx_plat_gen_cli_send_msg(
        params->model,
        params->state,
        params->opcode,
        params->addr,
        params->net_idx,
        params->app_idx,
        is_get
    );
}

extern "C" meshx_err_t meshx_gen_client_from_ble_reg_cb(uint32_t model_id, meshx_gen_client_cb_t cb)
{
    return control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_TO_APP,
        CONTROL_TASK_MSG_EVT_DATA, // Generic data event for client responses
        (control_task_msg_handle_t)cb
    );
}

extern "C" meshx_err_t meshx_gen_client_init(void)
{
    return meshx_plat_gen_cli_init();
}
