/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_prov_srv.cpp
 * @brief This file contains the implementation of the provisioning process for the BLE mesh node.
 *        Migrated to C++ to align with the modern MeshX element architecture.
 *
 * @author Pranjal Chanda
 */

#include <meshx_c_header.h>

#if CONFIG_ENABLE_PROVISIONING

/**
 * @brief Mask for control task provisioning events.
 */
#define CONTROL_TASK_PROV_EVT_MASK ( CONTROL_TASK_MSG_EVT_PROV_REG_COMP  |  \
                                     CONTROL_TASK_MSG_EVT_IDENTIFY_START |  \
                                     CONTROL_TASK_MSG_EVT_IDENTIFY_STOP  |  \
                                     CONTROL_TASK_MSG_EVT_PROVISION_STOP |  \
                                     CONTROL_TASK_MSG_EVT_NODE_RESET     |  \
                                     CONTROL_TASK_MSG_EVT_SET_NAME_COMP  |  \
                                     CONTROL_TASK_MSG_EVT_EN_NODE_PROV   |  \
                                     CONTROL_TASK_MSG_EVT_SYSTEM_STACK_READY | \
                                     CONTROL_TASK_MSG_EVT_PROXY_CONNECT  |  \
                                     CONTROL_TASK_MSG_EVT_PROXY_DISCONN )

#define MESHX_PROV_SRV_CLIENT_EVENT_BMAP    (CONTROL_TASK_MSG_EVT_SYSTEM_FRESH_BOOT)
#define MESHX_PROV_SRV_SERVER_EVENT_BMAP    (CONTROL_TASK_MSG_EVT_SYSTEM_STACK_READY)

/**
 * @brief Structure to map provisioning callback events to control task events.
 */
typedef struct prov_cb_evt_ctrl_task_evt_table
{
#if CONFIG_MESHX_DEFAULT_LOG_LEVEL < MESHX_LOG_INFO
    const char *evt_str;                            /**< String representation of the provisioning event. */
#endif
    control_task_msg_evt_provision_t ctrl_task_evt; /**< Corresponding control task event. */
} prov_cb_evt_ctrl_task_evt_table_t;

/**
 * @brief Table mapping provisioning events to control task events.
 * @note Fixed for C++17 compatibility (removed C99 designated initializers for large arrays if needed,
 *       but here we use explicit indices for clarity).
 */
static prov_cb_evt_ctrl_task_evt_table_t prov_cb_evt_ctrl_task_evt_table[MESHX_PROV_EVT_MAX] = {};

static void init_prov_cb_table(void)
{
    static bool initialized = false;
    if (initialized) return;

#if CONFIG_MESHX_DEFAULT_LOG_LEVEL < MESHX_LOG_INFO
    prov_cb_evt_ctrl_task_evt_table[MESHX_PROV_REGISTER_COMP_EVT]          = {"MESHX_PROV_REGISTER_COMP_EVT",          CONTROL_TASK_MSG_EVT_PROV_REG_COMP};
    prov_cb_evt_ctrl_task_evt_table[MESHX_NODE_SET_UPROV_NAME_COMP_EVT]    = {"MESHX_NODE_SET_UPROV_NAME_COMP_EVT",    CONTROL_TASK_MSG_EVT_SET_NAME_COMP};
    prov_cb_evt_ctrl_task_evt_table[MESHX_NODE_PROV_RESET_EVT]             = {"MESHX_NODE_PROV_RESET_EVT",             CONTROL_TASK_MSG_EVT_NODE_RESET};
    prov_cb_evt_ctrl_task_evt_table[MESHX_NODE_PROV_COMPLETE_EVT]          = {"MESHX_NODE_PROV_COMPLETE_EVT",          CONTROL_TASK_MSG_EVT_PROVISION_STOP};
    prov_cb_evt_ctrl_task_evt_table[MESHX_NODE_PROV_LINK_OPEN_EVT]         = {"MESHX_NODE_PROV_LINK_OPEN_EVT",         CONTROL_TASK_MSG_EVT_IDENTIFY_START};
    prov_cb_evt_ctrl_task_evt_table[MESHX_NODE_PROV_LINK_CLOSE_EVT]        = {"MESHX_NODE_PROV_LINK_CLOSE_EVT",        CONTROL_TASK_MSG_EVT_IDENTIFY_STOP};
    prov_cb_evt_ctrl_task_evt_table[MESHX_NODE_PROV_ENABLE_COMP_EVT]       = {"MESHX_NODE_PROV_ENABLE_COMP_EVT",       CONTROL_TASK_MSG_EVT_EN_NODE_PROV};
    prov_cb_evt_ctrl_task_evt_table[MESHX_PROXY_SERVER_CONNECTED_EVT]      = {"MESHX_PROXY_SERVER_CONNECTED_EVT",      CONTROL_TASK_MSG_EVT_PROXY_CONNECT};
    prov_cb_evt_ctrl_task_evt_table[MESHX_PROXY_SERVER_DISCONNECTED_EVT]   = {"MESHX_PROXY_SERVER_DISCONNECTED_EVT",   CONTROL_TASK_MSG_EVT_PROXY_DISCONN};
#else
    prov_cb_evt_ctrl_task_evt_table[MESHX_PROV_REGISTER_COMP_EVT].ctrl_task_evt          = CONTROL_TASK_MSG_EVT_PROV_REG_COMP;
    prov_cb_evt_ctrl_task_evt_table[MESHX_NODE_SET_UPROV_NAME_COMP_EVT].ctrl_task_evt    = CONTROL_TASK_MSG_EVT_SET_NAME_COMP;
    prov_cb_evt_ctrl_task_evt_table[MESHX_NODE_PROV_RESET_EVT].ctrl_task_evt             = CONTROL_TASK_MSG_EVT_NODE_RESET;
    prov_cb_evt_ctrl_task_evt_table[MESHX_NODE_PROV_COMPLETE_EVT].ctrl_task_evt          = CONTROL_TASK_MSG_EVT_PROVISION_STOP;
    prov_cb_evt_ctrl_task_evt_table[MESHX_NODE_PROV_LINK_OPEN_EVT].ctrl_task_evt         = CONTROL_TASK_MSG_EVT_IDENTIFY_START;
    prov_cb_evt_ctrl_task_evt_table[MESHX_NODE_PROV_LINK_CLOSE_EVT].ctrl_task_evt        = CONTROL_TASK_MSG_EVT_IDENTIFY_STOP;
    prov_cb_evt_ctrl_task_evt_table[MESHX_NODE_PROV_ENABLE_COMP_EVT].ctrl_task_evt       = CONTROL_TASK_MSG_EVT_EN_NODE_PROV;
    prov_cb_evt_ctrl_task_evt_table[MESHX_PROXY_SERVER_CONNECTED_EVT].ctrl_task_evt      = CONTROL_TASK_MSG_EVT_PROXY_CONNECT;
    prov_cb_evt_ctrl_task_evt_table[MESHX_PROXY_SERVER_DISCONNECTED_EVT].ctrl_task_evt   = CONTROL_TASK_MSG_EVT_PROXY_DISCONN;
#endif
    initialized = true;
}

static meshx_os_timer_t *g_boot_timer;

/**
 * @brief Control task handler for the provisioning server.
 */
static meshx_err_t meshx_prov_srv_control_task_handler(
    dev_struct_t *pdev,
    control_task_msg_evt_t evt,
    meshx_prov_srv_param_t *params)
{
    if (!pdev || !params)
    {
        return MESHX_INVALID_ARG;
    }
    init_prov_cb_table();
    control_task_msg_evt_provision_t prov_evt = (control_task_msg_evt_provision_t)0;
    if(evt != CONTROL_TASK_MSG_EVT_PROVISION_ALL)
    {
        return MESHX_INVALID_ARG;
    }

    if (params->prov_evt < MESHX_PROV_EVT_MAX && prov_cb_evt_ctrl_task_evt_table[params->prov_evt].ctrl_task_evt != 0)
    {
#if CONFIG_MESHX_DEFAULT_LOG_LEVEL < MESHX_LOG_INFO
        MESHX_LOGD(MODULE_ID_MODEL_SERVER, "Provisioning event mapped: %s",
                    prov_cb_evt_ctrl_task_evt_table[params->prov_evt].evt_str);
#endif
        prov_evt = prov_cb_evt_ctrl_task_evt_table[params->prov_evt].ctrl_task_evt;
    }
    else
    {
        MESHX_LOGD(MODULE_ID_MODEL_SERVER, "Unhandled event: %d", params->prov_evt);
    }
    if (params->prov_evt == MESHX_NODE_PROV_COMPLETE_EVT)
    {
        MESHX_LOGI(MODULE_ID_MODEL_SERVER, "net_idx: 0x%04x, addr: 0x%04x", params->param.node_prov_complete.net_idx, params->param.node_prov_complete.addr);
        MESHX_LOGI(MODULE_ID_MODEL_SERVER, "flags: 0x%02x, iv_index: 0x%08" PRIx32, params->param.node_prov_complete.flags, params->param.node_prov_complete.iv_index);
    }
    if (prov_evt == 0)
    {
        return MESHX_SUCCESS;
    }

    /* Publish the event */
    return control_task_msg_publish(
        CONTROL_TASK_MSG_CODE_PROVISION,
        (control_task_msg_evt_t)prov_evt,
        &params->param,
        sizeof(meshx_prov_cb_param_t));
}

/**
 * @brief Handle node reset event.
 */
static void meshx_handle_node_reset(dev_struct_t *pdev)
{
    if(!pdev)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Invalid device structure");
        return;
    }

    for(uint16_t i = 1; i < pdev->element_idx; i++)
    {
        meshx_err_t err = meshx_nvs_element_ctx_remove(i, MESHX_ELEMENT_TYPE_ALL);
        if(err != MESHX_SUCCESS)
        {
            MESHX_LOGE(MODULE_ID_COMMON, "Failed to erase element context (%d): (%d)", i, err);
        }
    }

    /* Notify application about node reset */
    meshx_send_ctrl_msg_to_app(MESHX_CTRL_EVT_NODE_RESET, 0, NULL);

    /* Reset the MCU */
    meshx_platform_reset();
}

/**
 * @brief Handles provisioning control task events.
 */
static meshx_err_t meshx_prov_control_task_handler(dev_struct_t *pdev, control_task_msg_evt_t evt, void *params)
{
    const meshx_prov_cb_param_t *param = (meshx_prov_cb_param_t*) params;

    switch (evt)
    {
        case CONTROL_TASK_MSG_EVT_PROVISION_STOP:
            pdev->meshx_store.net_key_id = param->node_prov_complete.net_idx;
            pdev->meshx_store.node_addr  = param->node_prov_complete.addr;
            meshx_nvs_set(MESHX_NVS_STORE, &pdev->meshx_store, sizeof(pdev->meshx_store), MESHX_NVS_AUTO_COMMIT);
            break;
        case CONTROL_TASK_MSG_EVT_IDENTIFY_START:
            MESHX_LOGI(MODULE_ID_COMMON, "Identify Start");
            break;
        case CONTROL_TASK_MSG_EVT_NODE_RESET:
            MESHX_LOGW(MODULE_ID_COMMON, "Node Reset Event");
            meshx_handle_node_reset(pdev);
            break;
        case CONTROL_TASK_MSG_EVT_SET_NAME_COMP:
        case CONTROL_TASK_MSG_EVT_EN_NODE_PROV:
        case CONTROL_TASK_MSG_EVT_PROV_REG_COMP:
            /* Benign events during boot, handled to silence warnings */
            break;
        default:
            break;
    }
    return MESHX_SUCCESS;
}

extern "C" meshx_err_t meshx_prov_srv_meshx_reg_cb(void)
{
    return control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_PROVISION,
        CONTROL_TASK_PROV_EVT_MASK,
        (control_task_msg_handle_t)&meshx_prov_control_task_handler);
}

static void meshx_init_freshboot_timer_trigger_cb(const meshx_os_timer_t* p_timer)
{
    MESHX_UNUSED(p_timer);
    MESHX_LOGI(MODULE_ID_COMMON, "Fresh Boot Timer Expired");

    meshx_err_t err = control_task_msg_publish(
        CONTROL_TASK_MSG_CODE_PROVISION,
        CONTROL_TASK_MSG_EVT_SYSTEM_FRESH_BOOT,
        NULL,
        0
    );
    if(err)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to publish fresh boot event: (%d)", err);
    }
}

static meshx_err_t meshx_init_freshboot_timer(dev_struct_t *p_dev, uint16_t timeout_ms)
{
    if(p_dev->meshx_store.node_addr == MESHX_ADDR_UNASSIGNED)
    {
        MESHX_LOGI(MODULE_ID_COMMON, "Device not provisioned, not starting boot timer");
        return MESHX_SUCCESS;
    }
    meshx_err_t err = meshx_os_timer_create("boot_timer",
        timeout_ms,
        false,
        meshx_init_freshboot_timer_trigger_cb,
        &g_boot_timer
    );
    if(err != MESHX_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to create boot timer: (%d)", err);
    }

    err = meshx_os_timer_start(g_boot_timer);
    if(err != MESHX_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to start boot timer: (%d)", err);
    }

    return err;
}

extern "C" meshx_err_t meshx_prov_srv_reg_from_ble_cb(void)
{
    init_prov_cb_table();
    return control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_PROV_PLAT,
        CONTROL_TASK_MSG_EVT_PROVISION_ALL,
        (control_task_msg_handle_t)&meshx_prov_srv_control_task_handler
    );
}

extern "C" meshx_err_t meshx_init_prov(dev_struct_t *p_dev, const meshx_prov_params_t *prov_cfg)
{
    if (!prov_cfg || memcmp(prov_cfg->uuid, MESHX_UUID_EMPTY, sizeof(meshx_uuid_addr_t)) == 0)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid server configuration");
        return MESHX_INVALID_ARG;
    }

    meshx_err_t err = meshx_prov_srv_reg_from_ble_cb();
    if (err != MESHX_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to register provisioning callback");
        return err;
    }

    err = meshx_prov_srv_meshx_reg_cb();
    if (err != MESHX_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to register provisioning callback");
        return err;
    }

    err = meshx_init_freshboot_timer(p_dev, prov_cfg->freshboot_timeout_ms);
    if (err != MESHX_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to initialize boot timer");
        return err;
    }

    return meshx_plat_init_prov(prov_cfg->uuid);
}

extern "C" meshx_err_t meshx_prov_srv_reg_el_client_cb(prov_srv_cb_t cb)
{
    return control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_PROVISION,
        MESHX_PROV_SRV_CLIENT_EVENT_BMAP,
        (control_task_msg_handle_t)cb
    );
}

extern "C" meshx_err_t meshx_prov_srv_reg_el_server_cb(prov_srv_cb_t cb)
{
    return control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_PROVISION,
        MESHX_PROV_SRV_SERVER_EVENT_BMAP,
        (control_task_msg_handle_t)cb
    );
}

extern "C" meshx_err_t meshx_prov_srv_notify_plat_event(meshx_prov_srv_param_t *param)
{
    if (!param)
    {
        return MESHX_INVALID_ARG;
    }

    return control_task_msg_publish(
        CONTROL_TASK_MSG_CODE_PROV_PLAT,
        CONTROL_TASK_MSG_EVT_PROVISION_ALL,
        param,
        sizeof(meshx_prov_srv_param_t)
    );
}

extern "C" bool meshx_prov_srv_is_provisioned(void)
{
    uint16_t primary_addr = 0;
    if (MESHX_SUCCESS == meshx_get_base_element_id(&primary_addr))
    {
        return (primary_addr != MESHX_ADDR_UNASSIGNED);
    }
    return false;
}

#endif /* CONFIG_ENABLE_PROVISIONING */
