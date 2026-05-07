/**
 * @file meshx_base_model_common.cpp
 * @brief Implementation of the MeshX Config Server base class.
 *
 * Provides platform initialization, opcode validation and basic send/restore
 * stubs for the Configuration Server model. The implementation mirrors the
 * style used in other base model implementations (Generic/Light).
 *
 * Note: The platform currently exposes init/get instance helpers for the
 * configuration server but does not expose a specific "send status" or
 * "restore" helper. As such, `plat_send_msg` and `server_state_restore`
 * return `MESHX_NOT_SUPPORTED` until a platform API is added.
 */

#include "meshx_base_model_common.hpp"

#if CONFIG_ENABLE_CONFIG_SERVER

// MESHX_CONFIG_SERVER_INIT_MAGIC_NO replaced by std::once_flag

MESHX_BASE_CONFIG_SERVER_TEMPLATE_PROTO
meshXBaseConfigServerModel MESHX_BASE_CONFIG_SERVER_TEMPLATE_PARAMS
    ::meshXBaseConfigServerModel(uint32_t model_id, meshx_ptr_t p_plat_model, const control_msg_cb &from_ble_cb)
    : meshXBaseServerModel(model_id, p_plat_model, from_ble_cb)
{
    set_status(meshXBaseConfigServerModel::plat_model_init());
    if (get_status() != MESHX_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "plat_model_init failed");
        return;
    }
}

MESHX_BASE_CONFIG_SERVER_TEMPLATE_PROTO
meshx_err_t meshXBaseConfigServerModel MESHX_BASE_CONFIG_SERVER_TEMPLATE_PARAMS
    ::plat_model_init(void)
{
    meshx_err_t err = MESHX_SUCCESS;
    std::call_once(plat_server_init_flag, [&]() {
        err = meshx_plat_config_srv_init();
    });
    return err;
}

MESHX_BASE_CONFIG_SERVER_TEMPLATE_PROTO
meshx_err_t meshXBaseConfigServerModel MESHX_BASE_CONFIG_SERVER_TEMPLATE_PARAMS
    ::validate_server_status_opcode(uint16_t opcode)
{
    switch (opcode)
    {
        case MESHX_MODEL_OP_BEACON_STATUS:
        case MESHX_MODEL_OP_COMPOSITION_DATA_STATUS:
        case MESHX_MODEL_OP_DEFAULT_TTL_STATUS:
        case MESHX_MODEL_OP_GATT_PROXY_STATUS:
        case MESHX_MODEL_OP_RELAY_STATUS:
        case MESHX_MODEL_OP_MODEL_PUB_STATUS:
        case MESHX_MODEL_OP_MODEL_SUB_STATUS:
        case MESHX_MODEL_OP_SIG_MODEL_SUB_LIST:
        case MESHX_MODEL_OP_VENDOR_MODEL_SUB_LIST:
        case MESHX_MODEL_OP_NET_KEY_STATUS:
        case MESHX_MODEL_OP_NET_KEY_LIST:
        case MESHX_MODEL_OP_APP_KEY_STATUS:
        case MESHX_MODEL_OP_APP_KEY_LIST:
        case MESHX_MODEL_OP_NODE_IDENTITY_STATUS:
        case MESHX_MODEL_OP_MODEL_APP_STATUS:
        case MESHX_MODEL_OP_SIG_MODEL_APP_LIST:
        case MESHX_MODEL_OP_VENDOR_MODEL_APP_LIST:
        case MESHX_MODEL_OP_NODE_RESET_STATUS:
        case MESHX_MODEL_OP_FRIEND_STATUS:
        case MESHX_MODEL_OP_KEY_REFRESH_PHASE_STATUS:
        case MESHX_MODEL_OP_HEARTBEAT_PUB_STATUS:
        case MESHX_MODEL_OP_HEARTBEAT_SUB_STATUS:
        case MESHX_MODEL_OP_LPN_POLLTIMEOUT_STATUS:
        case MESHX_MODEL_OP_NETWORK_TRANSMIT_STATUS:
            return MESHX_SUCCESS;
        default:
            return MESHX_FAIL;
    }
}

MESHX_BASE_CONFIG_SERVER_TEMPLATE_PROTO
meshx_err_t meshXBaseConfigServerModel MESHX_BASE_CONFIG_SERVER_TEMPLATE_PARAMS
    ::plat_send_msg(meshx_config_server_send_params_t *params)
{
    if (!params || !params->p_model || !params->p_ctx || params->p_ctx->dst_addr == MESHX_ADDR_UNASSIGNED)
    {
        return MESHX_INVALID_ARG;
    }

    if (validate_server_status_opcode((uint16_t)params->p_ctx->opcode) != MESHX_SUCCESS)
    {
        return MESHX_INVALID_ARG;
    }

    /* Platform currently doesn't provide a config-server specific "send status" helper.
     * Returning NOT_SUPPORTED so callers can handle or platform wrapper can be added.
     */
    MESHX_LOGW(MODULE_ID_MODEL_SERVER, "Config server plat_send_msg not supported on this platform");
    return MESHX_NOT_SUPPORTED;
}

MESHX_BASE_CONFIG_SERVER_TEMPLATE_PROTO
meshx_err_t meshXBaseConfigServerModel MESHX_BASE_CONFIG_SERVER_TEMPLATE_PARAMS
    ::server_state_restore(meshx_config_server_restore_params_t* param)
{
    (void)param;
    /* No platform restore API exposed for config server currently. */
    return MESHX_SUCCESS;
}

#endif /* CONFIG_ENABLE_CONFIG_SERVER */
