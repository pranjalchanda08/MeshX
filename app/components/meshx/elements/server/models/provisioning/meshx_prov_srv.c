/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_prov_srv.c
 * @brief This file contains the implementation of the provisioning process for the BLE mesh node.
 *
 * The provisioning process is responsible for setting up the BLE mesh node, including
 * initializing necessary components and handling communication with other nodes.
 *
 * @author Pranjal Chanda
 */

#include "string.h"
#include "meshx_nvs.h"
#include "meshx_os_timer.h"
#include "meshx_control_task.h"
#include "meshx_prov_srv.h"

#if CONFIG_ENABLE_PROVISIONING

/**
 * @brief Mask for control task provisioning events.
 *
 * This macro defines a mask that combines multiple control task message events
 * related to provisioning.
 */
#define CONTROL_TASK_PROV_EVT_MASK CONTROL_TASK_MSG_EVT_IDENTIFY_START \
                                 | CONTROL_TASK_MSG_EVT_PROVISION_STOP \
                                 | CONTROL_TASK_MSG_EVT_IDENTIFY_STOP \
                                 | CONTROL_TASK_MSG_EVT_NODE_RESET

#define MESHX_PROV_SRV_CLIENT_EVENT_BMAP    CONTROL_TASK_MSG_EVT_SYSTEM_FRESH_BOOT
#define MESHX_PROV_SRV_SERVER_EVENT_BMAP    CONTROL_TASK_MSG_EVT_EN_NODE_PROV

/**
 * @brief Structure to map provisioning callback events to control task events.
 *
 * This structure contains a string representation of the provisioning event
 * and the corresponding control task event.
 */
typedef struct prov_cb_evt_ctrl_task_evt_table
{
#if CONFIG_MESHX_DEFAULT_LOG_LEVEL < MESHX_LOG_INFO
    const char *evt_str;                            /**< String representation of the provisioning event. */
#endif
    control_task_msg_evt_provision_t ctrl_task_evt; /**< Corresponding control task event. */
} prov_cb_evt_ctrl_task_evt_table_t;

#if CONFIG_MESHX_DEFAULT_LOG_LEVEL < MESHX_LOG_INFO
/**
 * @brief Table mapping provisioning events to control task events.
 *
 * This table is indexed by provisioning event types and contains the string
 * representation of the event and the corresponding control task event.
 */
static prov_cb_evt_ctrl_task_evt_table_t prov_cb_evt_ctrl_task_evt_table[MESHX_PROV_EVT_MAX] = {
    [MESHX_NODE_PROV_RESET_EVT]             = {"MESHX_NODE_PROV_RESET_EVT",             CONTROL_TASK_MSG_EVT_NODE_RESET},
    [MESHX_NODE_PROV_COMPLETE_EVT]          = {"MESHX_NODE_PROV_COMPLETE_EVT",          CONTROL_TASK_MSG_EVT_PROVISION_STOP},
    [MESHX_NODE_PROV_LINK_OPEN_EVT]         = {"MESHX_NODE_PROV_LINK_OPEN_EVT",         CONTROL_TASK_MSG_EVT_IDENTIFY_START},
    [MESHX_NODE_PROV_LINK_CLOSE_EVT]        = {"MESHX_NODE_PROV_LINK_CLOSE_EVT",        CONTROL_TASK_MSG_EVT_IDENTIFY_STOP},
    [MESHX_NODE_PROV_ENABLE_COMP_EVT]       = {"MESHX_NODE_PROV_ENABLE_COMP_EVT",       CONTROL_TASK_MSG_EVT_EN_NODE_PROV},
    [MESHX_PROXY_SERVER_CONNECTED_EVT]      = {"MESHX_PROXY_SERVER_CONNECTED_EVT",      CONTROL_TASK_MSG_EVT_PROXY_CONNECT},
    [MESHX_PROXY_SERVER_DISCONNECTED_EVT]   = {"MESHX_PROXY_SERVER_DISCONNECTED_EVT",   CONTROL_TASK_MSG_EVT_PROXY_DISCONN},
};

#else
/**
 * @brief Table mapping provisioning events to control task events.
 *
 * This table is indexed by provisioning event types and contains the string
 * representation of the event and the corresponding control task event.
 */
static prov_cb_evt_ctrl_task_evt_table_t prov_cb_evt_ctrl_task_evt_table[MESHX_PROV_EVT_MAX] = {
    [MESHX_NODE_PROV_RESET_EVT]             = {CONTROL_TASK_MSG_EVT_NODE_RESET},
    [MESHX_NODE_PROV_COMPLETE_EVT]          = {CONTROL_TASK_MSG_EVT_PROVISION_STOP},
    [MESHX_NODE_PROV_LINK_OPEN_EVT]         = {CONTROL_TASK_MSG_EVT_IDENTIFY_START},
    [MESHX_NODE_PROV_LINK_CLOSE_EVT]        = {CONTROL_TASK_MSG_EVT_IDENTIFY_STOP},
    [MESHX_NODE_PROV_ENABLE_COMP_EVT]       = {CONTROL_TASK_MSG_EVT_EN_NODE_PROV},
    [MESHX_PROXY_SERVER_CONNECTED_EVT]      = {CONTROL_TASK_MSG_EVT_PROXY_CONNECT},
    [MESHX_PROXY_SERVER_DISCONNECTED_EVT]   = {CONTROL_TASK_MSG_EVT_PROXY_DISCONN},
};
#endif

static meshx_os_timer_t *g_boot_timer;

/**
 * @brief Control task handler for the provisioning server.
 *
 * @param[in] pdev Pointer to the device structure.
 * @param[in] evt Event type.
 * @param[in] params Pointer to the provisioning server parameters.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
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
    prov_evt_t prov_evt = CONTROL_TASK_MSG_EVT_PROVISION_ALL;
    if(evt != CONTROL_TASK_MSG_EVT_PROVISION_ALL)
    {
        return MESHX_INVALID_ARG;
    }

    if (prov_cb_evt_ctrl_task_evt_table[params->prov_evt].ctrl_task_evt != 0)
    {
        MESHX_LOGD(MODULE_ID_MODEL_SERVER, "Provisioning event mapped: %s",
                    prov_cb_evt_ctrl_task_evt_table[params->prov_evt].evt_str);
        prov_evt = prov_cb_evt_ctrl_task_evt_table[params->prov_evt].ctrl_task_evt;
    }
    else
    {
        MESHX_LOGD(MODULE_ID_MODEL_SERVER, "Unhandled event: %d", event);
    }
    if (params->prov_evt == MESHX_NODE_PROV_COMPLETE_EVT)
    {
        MESHX_LOGI(MODULE_ID_MODEL_SERVER, "net_idx: 0x%04x, addr: 0x%04x", params->param.node_prov_complete.net_idx, params->param.node_prov_complete.addr);
        MESHX_LOGI(MODULE_ID_MODEL_SERVER, "flags: 0x%02x, iv_index: 0x%08" PRIx32, params->param.node_prov_complete.flags, params->param.node_prov_complete.iv_index);
    }
    if (prov_evt == CONTROL_TASK_MSG_EVT_PROVISION_ALL)
    {
        return MESHX_INVALID_ARG;
    }

    /* Publish the event */
    return control_task_msg_publish(
        CONTROL_TASK_MSG_CODE_PROVISION,
        prov_evt,
        &params->param,
        sizeof(meshx_prov_cb_param_t));
    return MESHX_SUCCESS;
}

/**
 * @brief Handle node reset event.
 *
 * This function handles the node reset event by removing all element contexts
 * from NVS and resetting the MCU.
 *
 * @param[in] pdev Pointer to the device structure.
 *
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
        meshx_err_t err = meshx_nvs_element_ctx_remove(i);
        if(err != MESHX_SUCCESS)
        {
            MESHX_LOGE(MODULE_ID_COMMON, "Failed to erase element context (%d): (%d)", i, err);
        }
    }
    /* Reset the MCU */
    meshx_platform_reset();
}

/**
 * @brief Handles provisioning control task events.
 *
 * This function processes various provisioning-related events and updates
 * the device structure accordingly. It handles events such as provisioning
 * completion and identification start.
 *
 * @param[in] pdev Pointer to the device structure.
 * @param[in] evt The control task message event type.
 * @param[in] params Pointer to the event parameters.
 *
 * @return MESHX_SUCCESS on success, or an error code on failure.
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
        default:
            break;
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Register the BLE Mesh provisioning callback for MeshX Layer Internal.
 *
 * This function registers the BLE Mesh provisioning callback to handle
 * provisioning events.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_FAIL: Failed to register provisioning callback
 */
static meshx_err_t meshx_prov_srv_meshx_reg_cb(void)
{
    return control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_PROVISION,
        CONTROL_TASK_PROV_EVT_MASK,
        &meshx_prov_control_task_handler);
}

/**
 * @brief Callback function for the boot timer.
 *
 * This function is called when the boot timer expires.
 *
 * @param[in] p_timer Pointer to the timer structure.
 */
static void meshx_init_freshboot_timer_trigger_cb(const meshx_os_timer_t* p_timer)
{
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

/**
 * @brief Initializes the fresh boot timer.
 *
 * @param[in] p_dev Pointer to the device structure.
 * @param[in] timeout_ms Timeout duration in milliseconds.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
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

/**
 * @brief Register the BLE Mesh provisioning callback.
 *
 * This function registers the BLE Mesh provisioning callback to handle
 * provisioning events.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_FAIL: Failed to register provisioning callback
 */
static meshx_err_t meshx_prov_srv_reg_from_ble_cb(void)
{
    return control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_FRM_BLE,
        CONTROL_TASK_MSG_EVT_PROVISION_ALL,
        (prov_srv_cb_t)&meshx_prov_srv_control_task_handler
    );
}

/**
 * @brief Initialize provisioning parameters.
 *
 * This function initializes the provisioning parameters by copying the UUID from the provided
 * server configuration and registering the provisioning callback.
 *
 * @param[in] p_dev Pointer to the device structure.
 * @param[in] prov_cfg Pointer to the provisioning parameters structure containing the UUID.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_FAIL: Failed to register provisioning callback
 */
meshx_err_t meshx_init_prov(dev_struct_t *p_dev, const meshx_prov_params_t *prov_cfg)
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

/**
 * @brief Register a callback function for provisioning events.
 *
 * This function registers a callback function that will be called when
 * certain provisioning events occur.
 *
 * @param[in] cb The callback function to register.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_FAIL: Failed to register the callback
 */
meshx_err_t meshx_prov_srv_reg_el_client_cb(prov_srv_cb_t cb)
{
    return control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_PROVISION,
        MESHX_PROV_SRV_CLIENT_EVENT_BMAP,
        (prov_srv_cb_t)cb
    );
}

/**
 * @brief Register a callback function for provisioning events.
 *
 * This function registers a callback function that will be called when
 * certain provisioning events occur.
 *
 * @param[in] cb The callback function to register.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_FAIL: Failed to register the callback
 */
meshx_err_t meshx_prov_srv_reg_el_server_cb(prov_srv_cb_t cb)
{
    return control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_PROVISION,
        MESHX_PROV_SRV_SERVER_EVENT_BMAP,
        (prov_srv_cb_t)cb
    );
}

/**
 * @brief Notify the model event to the application.
 *
 * @note This API is not to be defined in Platform Port layer and shall be called by the
 *       respective platform event handler to notify the MeshX of a model event.
 *
 * This function notifies the application of a model event by invoking the registered
 * provisioning callback with the provided event parameters.
 *
 * @param[in] param Pointer to the event parameters structure.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_FAIL: Failed to notify model event
 */
meshx_err_t meshx_prov_srv_notify_plat_event(meshx_prov_srv_param_t *param)
{
    if (!param)
    {
        return MESHX_INVALID_ARG;
    }

    return control_task_msg_publish(
        CONTROL_TASK_MSG_CODE_FRM_BLE,
        CONTROL_TASK_MSG_EVT_PROVISION_ALL,
        param,
        sizeof(meshx_prov_srv_param_t)
    );
}

#endif /* CONFIG_ENABLE_PROVISIONING */
