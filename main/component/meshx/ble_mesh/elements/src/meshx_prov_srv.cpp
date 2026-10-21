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

extern "C" int bt_mesh_node_local_app_key_add(uint16_t net_idx, uint16_t app_idx, const uint8_t app_key[16]);

#include "meshx_element_registry.hpp"
#include "meshx_model_class.hpp"

/*
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

/*
 * @brief Structure to map provisioning callback events to control task events.
 */
typedef struct prov_cb_evt_ctrl_task_evt_table
{
#if CONFIG_MESHX_DEFAULT_LOG_LEVEL < MESHX_LOG_INFO
    const char *evt_str;                            /*< String representation of the provisioning event. */
#endif
    control_task_msg_evt_provision_t ctrl_task_evt; /*< Corresponding control task event. */
} prov_cb_evt_ctrl_task_evt_table_t;

/*
 * @brief Compile-time static const table mapping provisioning events to control task events.
 * Stored in Flash read-only cache (drom) to save DRAM.
 */
static const prov_cb_evt_ctrl_task_evt_table_t prov_cb_evt_ctrl_task_evt_table[MESHX_PROV_EVT_MAX] = {
#if CONFIG_MESHX_DEFAULT_LOG_LEVEL < MESHX_LOG_INFO
    /* [0] MESHX_PROV_REGISTER_COMP_EVT */        {"MESHX_PROV_REGISTER_COMP_EVT",        CONTROL_TASK_MSG_EVT_PROV_REG_COMP},
    /* [1] MESHX_NODE_SET_UPROV_NAME_COMP_EVT */  {"MESHX_NODE_SET_UPROV_NAME_COMP_EVT",  CONTROL_TASK_MSG_EVT_SET_NAME_COMP},
    /* [2] MESHX_NODE_PROV_RESET_EVT */           {"MESHX_NODE_PROV_RESET_EVT",           CONTROL_TASK_MSG_EVT_NODE_RESET},
    /* [3] MESHX_NODE_PROV_COMPLETE_EVT */        {"MESHX_NODE_PROV_COMPLETE_EVT",        CONTROL_TASK_MSG_EVT_PROVISION_STOP},
    /* [4] MESHX_NODE_PROV_LINK_OPEN_EVT */       {"MESHX_NODE_PROV_LINK_OPEN_EVT",       CONTROL_TASK_MSG_EVT_IDENTIFY_START},
    /* [5] MESHX_NODE_PROV_LINK_CLOSE_EVT */      {"MESHX_NODE_PROV_LINK_CLOSE_EVT",      CONTROL_TASK_MSG_EVT_IDENTIFY_STOP},
    /* [6] MESHX_NODE_PROV_ENABLE_COMP_EVT */     {"MESHX_NODE_PROV_ENABLE_COMP_EVT",     CONTROL_TASK_MSG_EVT_EN_NODE_PROV},
    /* [7] MESHX_PROXY_SERVER_CONNECTED_EVT */    {"MESHX_PROXY_SERVER_CONNECTED_EVT",    CONTROL_TASK_MSG_EVT_PROXY_CONNECT},
    /* [8] MESHX_PROXY_SERVER_DISCONNECTED_EVT */ {"MESHX_PROXY_SERVER_DISCONNECTED_EVT", CONTROL_TASK_MSG_EVT_PROXY_DISCONN},
#else
    /* [0] MESHX_PROV_REGISTER_COMP_EVT */        {CONTROL_TASK_MSG_EVT_PROV_REG_COMP},
    /* [1] MESHX_NODE_SET_UPROV_NAME_COMP_EVT */  {CONTROL_TASK_MSG_EVT_SET_NAME_COMP},
    /* [2] MESHX_NODE_PROV_RESET_EVT */           {CONTROL_TASK_MSG_EVT_NODE_RESET},
    /* [3] MESHX_NODE_PROV_COMPLETE_EVT */        {CONTROL_TASK_MSG_EVT_PROVISION_STOP},
    /* [4] MESHX_NODE_PROV_LINK_OPEN_EVT */       {CONTROL_TASK_MSG_EVT_IDENTIFY_START},
    /* [5] MESHX_NODE_PROV_LINK_CLOSE_EVT */      {CONTROL_TASK_MSG_EVT_IDENTIFY_STOP},
    /* [6] MESHX_NODE_PROV_ENABLE_COMP_EVT */     {CONTROL_TASK_MSG_EVT_EN_NODE_PROV},
    /* [7] MESHX_PROXY_SERVER_CONNECTED_EVT */    {CONTROL_TASK_MSG_EVT_PROXY_CONNECT},
    /* [8] MESHX_PROXY_SERVER_DISCONNECTED_EVT */ {CONTROL_TASK_MSG_EVT_PROXY_DISCONN},
#endif
};

static meshx_os_timer_t *g_boot_timer;
static meshx_os_timer_t *g_oob_watchdog_timer = NULL;

/*
 * @brief 30-second OOB Authentication Watchdog callback (Mitigation #1).
 */
static void meshx_oob_watchdog_timeout_cb(const meshx_os_timer_t *p_timer)
{
    MESHX_UNUSED(p_timer);
    MESHX_LOGW(MODULE_ID_COMMON, "Prov Srv: 30s OOB Input Watchdog timed out! Aborting provisioning session.");

    /* Disable node provisioning to free radio/stack */
    meshx_prov_bearer_t bearers = (meshx_prov_bearer_t)(MESHX_PROV_ADV | MESHX_PROV_GATT);
    meshx_plat_prov_disable(bearers);

    mxcp_evt_prov_failed_t fail_evt;
    fail_evt.reason = 0x05; /* REASON_TIMEOUT */
    mxcp_send_event(MXCP_EVT_PROV_FAILED, (const uint8_t *)&fail_evt, sizeof(fail_evt));

    mxcp_evt_prov_state_ind_t ind;
    memset(&ind, 0, sizeof(ind));
    ind.unicast_addr = meshx_get_node_addr();
    ind.net_idx      = meshx_get_net_key_id();
    ind.state        = (ind.unicast_addr != MESHX_ADDR_UNASSIGNED) ? 3 : 0;
    mxcp_send_event(MXCP_EVT_PROV_STATE_IND, (const uint8_t *)&ind, sizeof(ind));
}

static void meshx_arm_oob_watchdog(void)
{
    if (g_oob_watchdog_timer == NULL) {
        meshx_os_timer_create("oob_wdog", MESHX_PROV_OOB_WATCHDOG_TIMEOUT_MS, false, meshx_oob_watchdog_timeout_cb, &g_oob_watchdog_timer);
    }
    if (g_oob_watchdog_timer != NULL) {
        meshx_os_timer_start(g_oob_watchdog_timer);
    }
}

static void meshx_disarm_oob_watchdog(void)
{
    if (g_oob_watchdog_timer != NULL) {
        meshx_os_timer_stop(g_oob_watchdog_timer);
    }
}

/*
 * @brief Control task handler for the provisioning server.
 *
 * Translates provisioning server events into control task messages.
 *
 * @param[in] pdev       Pointer to the device structure.
 * @param[in] evt        Control task message event ID.
 * @param[in] params     Pointer to provisioning callback parameters.
 * @param[in] params_len Length of parameter buffer.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
static meshx_err_t meshx_prov_srv_control_task_handler(
    dev_struct_t *pdev,
    control_task_msg_evt_t evt,
    void *params,
    uint16_t params_len)
{
    meshx_prov_srv_param_t *p_params = (meshx_prov_srv_param_t *)params;
    control_task_msg_evt_provision_t prov_evt = (control_task_msg_evt_provision_t)0;

    if (!pdev || !p_params)
    {
        return MESHX_INVALID_ARG;
    }
    MESHX_UNUSED(params_len);
    if(evt != CONTROL_TASK_MSG_EVT_PROVISION_ALL)
    {
        return MESHX_INVALID_ARG;
    }

    if (p_params->prov_evt < MESHX_PROV_EVT_MAX && prov_cb_evt_ctrl_task_evt_table[p_params->prov_evt].ctrl_task_evt != 0)
    {
#if CONFIG_MESHX_DEFAULT_LOG_LEVEL < MESHX_LOG_INFO
        MESHX_LOGD(MODULE_ID_MODEL_SERVER, "Provisioning event mapped: %s",
                    prov_cb_evt_ctrl_task_evt_table[p_params->prov_evt].evt_str);
#endif
        prov_evt = prov_cb_evt_ctrl_task_evt_table[p_params->prov_evt].ctrl_task_evt;
    }
    else
    {
        MESHX_LOGD(MODULE_ID_MODEL_SERVER, "Unhandled event: %d", p_params->prov_evt);
    }
    if (p_params->prov_evt == MESHX_NODE_PROV_COMPLETE_EVT)
    {
        MESHX_LOGI(MODULE_ID_MODEL_SERVER, "net_idx: 0x%04x, addr: 0x%04x", p_params->param.node_prov_complete.net_idx, p_params->param.node_prov_complete.addr);
        MESHX_LOGD(MODULE_ID_MODEL_SERVER, "flags: 0x%02x, iv_index: 0x%08" PRIx32, p_params->param.node_prov_complete.flags, p_params->param.node_prov_complete.iv_index);
    }
    if (prov_evt == 0)
    {
        return MESHX_SUCCESS;
    }

    return control_task_msg_publish(
        CONTROL_TASK_MSG_CODE_PROVISION,
        (control_task_msg_evt_t)prov_evt,
        &p_params->param,
        sizeof(meshx_prov_cb_param_t));
}

/*
 * @brief Handle node reset event.
 *
 * Clears persistent element contexts, erases dynamic composition schema from NVS,
 * notifies the application, and initiates platform reset.
 *
 * @param[in] pdev Pointer to device structure.
 */
static void meshx_handle_node_reset(dev_struct_t *pdev)
{
    size_t i = 0;
    meshx_err_t err = MESHX_SUCCESS;

    if (!pdev)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Invalid device structure");
        return;
    }

    /* Mandatory Pre-Reset Lifecycle Teardown:
     * Dismantle active peer links and broadcast node reset notification before erasing keys. */
    meshx_unpair_pre_reset_teardown();

    for (i = 1; i < pdev->element_idx; ++i)
    {
        err = meshx_nvs_element_ctx_remove(static_cast<uint16_t>(i), MESHX_ELEMENT_TYPE_ALL);
        if (err != MESHX_SUCCESS)
        {
            MESHX_LOGE(MODULE_ID_COMMON, "Failed to erase element context (%zu): (%d)", i, err);
        }
    }

    /* Erase dynamic composition schema from NVS */
    meshx_nvs_comp_schema_erase();

    /* Notify application about node reset */
    meshx_send_ctrl_msg_to_app(MESHX_CTRL_EVT_NODE_RESET, 0, NULL);

    /* Reset the MCU */
    meshx_platform_reset();
}

/*
 * @brief Handles provisioning control task events.
 */
static meshx_err_t meshx_prov_control_task_handler(dev_struct_t *pdev, control_task_msg_evt_t evt, void *params, uint16_t params_len)
{
    const meshx_prov_cb_param_t *param = (meshx_prov_cb_param_t*) params;
    MESHX_UNUSED(params_len);

    switch (evt)
    {
        case CONTROL_TASK_MSG_EVT_PROVISION_STOP:
        {
            meshx_disarm_oob_watchdog();
            pdev->meshx_store.net_key_id = param->node_prov_complete.net_idx;
            pdev->meshx_store.node_addr  = param->node_prov_complete.addr;
            meshx_nvs_set(MESHX_NVS_STORE, &pdev->meshx_store, sizeof(pdev->meshx_store), MESHX_NVS_AUTO_COMMIT);
            meshx_nvs_commit(); // Force immediate commit for critical provisioning parameters

            /* Emit MXCP_EVT_PROV_COMP to Host MCU */
            mxcp_evt_prov_comp_t comp_evt;
            memset(&comp_evt, 0, sizeof(comp_evt));
            comp_evt.net_idx = param->node_prov_complete.net_idx;
            comp_evt.addr    = param->node_prov_complete.addr;
            mxcp_send_event(MXCP_EVT_PROV_COMP, (const uint8_t *)&comp_evt, sizeof(comp_evt));
            break;
        }
        case CONTROL_TASK_MSG_EVT_IDENTIFY_START:
        {
            MESHX_LOGI(MODULE_ID_COMMON, "Identify Start / Link Open");
            meshx_arm_oob_watchdog();

            /* Emit MXCP_EVT_PROV_LINK_OPEN to Host MCU */
            mxcp_evt_prov_link_open_t open_evt;
            memset(&open_evt, 0, sizeof(open_evt));
            open_evt.bearer = (param != NULL) ? param->node_prov_link_open.bearer : 0x01;
            mxcp_send_event(MXCP_EVT_PROV_LINK_OPEN, (const uint8_t *)&open_evt, sizeof(open_evt));
            break;
        }
        case CONTROL_TASK_MSG_EVT_IDENTIFY_STOP:
        {
            MESHX_LOGI(MODULE_ID_COMMON, "Identify Stop / Link Close");
            meshx_disarm_oob_watchdog();

            /* Emit MXCP_EVT_PROV_LINK_CLOSE to Host MCU */
            mxcp_evt_prov_link_close_t close_evt;
            memset(&close_evt, 0, sizeof(close_evt));
            close_evt.bearer = (param != NULL) ? param->node_prov_link_close.bearer : 0x01;
            close_evt.reason = (param != NULL) ? param->node_prov_link_close.reason : 0x00;
            mxcp_send_event(MXCP_EVT_PROV_LINK_CLOSE, (const uint8_t *)&close_evt, sizeof(close_evt));
            break;
        }
        case CONTROL_TASK_MSG_EVT_NODE_RESET:
            meshx_disarm_oob_watchdog();
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

/*
 * @brief Register callback for provisioning control task messages.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
extern "C" meshx_err_t meshx_prov_srv_meshx_reg_cb(void)
{
    return control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_PROVISION,
        CONTROL_TASK_PROV_EVT_MASK,
        (control_task_msg_handle_t)&meshx_prov_control_task_handler);
}

/*
 * @brief Callback invoked when the fresh boot timer expires.
 *
 * @param[in] p_timer Pointer to the OS timer structure.
 */
static void meshx_init_freshboot_timer_trigger_cb(const meshx_os_timer_t* p_timer)
{
    meshx_err_t err = MESHX_SUCCESS;

    MESHX_UNUSED(p_timer);
    MESHX_LOGD(MODULE_ID_COMMON, "Fresh Boot Timer Expired");

    err = control_task_msg_publish(
        CONTROL_TASK_MSG_CODE_PROVISION,
        CONTROL_TASK_MSG_EVT_SYSTEM_FRESH_BOOT,
        NULL,
        0
    );
    if (err)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to publish fresh boot event: (%d)", err);
    }
}

/*
 * @brief Initialize and start the fresh boot timer for provisioned devices.
 *
 * @param[in] p_dev      Pointer to device structure.
 * @param[in] timeout_ms Timer timeout in milliseconds.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
static meshx_err_t meshx_init_freshboot_timer(dev_struct_t *p_dev, uint16_t timeout_ms)
{
    meshx_err_t err = MESHX_SUCCESS;

    if (p_dev->meshx_store.node_addr == MESHX_ADDR_UNASSIGNED)
    {
        MESHX_LOGI(MODULE_ID_COMMON, "Device not provisioned, not starting boot timer");
        return MESHX_SUCCESS;
    }

    err = meshx_os_timer_create("boot_timer",
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

/*
 * @brief Register callback for platform provisioning events.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
extern "C" meshx_err_t meshx_prov_srv_reg_from_ble_cb(void)
{
    return control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_PROV_PLAT,
        CONTROL_TASK_MSG_EVT_PROVISION_ALL,
        (control_task_msg_handle_t)&meshx_prov_srv_control_task_handler
    );
}

/*
 * @brief Initialize provisioning service with device configuration.
 *
 * @param[in] p_dev    Pointer to device structure.
 * @param[in] prov_cfg Pointer to provisioning parameters.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
extern "C" meshx_err_t meshx_init_prov(dev_struct_t *p_dev, const meshx_prov_params_t *prov_cfg)
{
    meshx_err_t err = MESHX_SUCCESS;

    static const meshx_uuid_addr_t empty_uuid = {0};
    if (!prov_cfg || memcmp(prov_cfg->uuid, empty_uuid, sizeof(meshx_uuid_addr_t)) == 0)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid server configuration");
        return MESHX_INVALID_ARG;
    }

    err = meshx_prov_srv_reg_from_ble_cb();
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

/*
 * @brief Register client element callback for provisioning events.
 *
 * @param[in] cb Callback function to register.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
extern "C" meshx_err_t meshx_prov_srv_reg_el_client_cb(prov_srv_cb_t cb)
{
    return control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_PROVISION,
        MESHX_PROV_SRV_CLIENT_EVENT_BMAP,
        (control_task_msg_handle_t)cb
    );
}

/*
 * @brief Register server element callback for provisioning events.
 *
 * @param[in] cb Callback function to register.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
extern "C" meshx_err_t meshx_prov_srv_reg_el_server_cb(prov_srv_cb_t cb)
{
    return control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_PROVISION,
        MESHX_PROV_SRV_SERVER_EVENT_BMAP,
        (control_task_msg_handle_t)cb
    );
}

/*
 * @brief Publish a platform provisioning event to control task.
 *
 * @param[in] param Pointer to provisioning parameter structure.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
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

/*
 * @brief Check if the device is currently provisioned.
 *
 * @return true if provisioned, false otherwise.
 */
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
