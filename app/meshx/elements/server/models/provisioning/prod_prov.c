/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file prod_prov.c
 * @brief This file contains the implementation of the provisioning process for the BLE mesh node.
 *
 * The provisioning process is responsible for setting up the BLE mesh node, including
 * initializing necessary components and handling communication with other nodes.
 * 
 */

#include "prod_prov.h"
#include "string.h"
#include "control_task.h"

#define TAG __func__

/**
 * @brief Structure to map provisioning callback events to control task events.
 *
 * This structure contains a string representation of the provisioning event
 * and the corresponding control task event.
 */
typedef struct prov_cb_evt_ctrl_task_evt_table
{
    const char * evt_str;                            /**< String representation of the provisioning event. */
    control_task_msg_evt_provision_t ctrl_task_evt; /**< Corresponding control task event. */
} prov_cb_evt_ctrl_task_evt_table_t;

/**
 * @brief Table mapping provisioning events to control task events.
 *
 * This table is indexed by provisioning event types and contains the string
 * representation of the event and the corresponding control task event.
 */
static prov_cb_evt_ctrl_task_evt_table_t prov_cb_evt_ctrl_task_evt_table[ESP_BLE_MESH_PROV_EVT_MAX] = {
    [ESP_BLE_MESH_NODE_PROV_RESET_EVT]              = {"ESP_BLE_MESH_NODE_PROV_RESET_EVT",              CONTROL_TASK_MSG_EVT_NODE_RESET},
    [ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT]           = {"ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT",           CONTROL_TASK_MSG_EVT_PROVISION_STOP},
    [ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT]          = {"ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT",          CONTROL_TASK_MSG_EVT_IDENTIFY_START},
    [ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT]         = {"ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT",         CONTROL_TASK_MSG_EVT_IDENTIFY_STOP},
    [ESP_BLE_MESH_PROXY_SERVER_CONNECTED_EVT]       = {"ESP_BLE_MESH_PROXY_SERVER_CONNECTED_EVT",       CONTROL_TASK_MSG_EVT_PROXY_CONNECT},
    [ESP_BLE_MESH_PROXY_SERVER_DISCONNECTED_EVT]    = {"ESP_BLE_MESH_PROXY_SERVER_DISCONNECTED_EVT",    CONTROL_TASK_MSG_EVT_PROXY_DISCONN},
};

/**
 * @brief Provisioning parameters.
 *
 * This structure holds the provisioning parameters used in the provisioning process.
 */
static prov_params_t prov_params;

/**
 * @brief Global provisioning structure.
 *
 * This structure holds the global provisioning configuration.
 */
esp_ble_mesh_prov_t g_prod_prov;

/**
 * @brief Send a control message to the control task.
 *
 * This function sends a control message to the control task with the given event type.
 *
 * @param[in] param Pointer to the provisioning callback parameters.
 * @param[in] evt The event type to send to the control task.
 *
 * @return ESP_OK on success, or an error code on failure.
 */

static esp_err_t send_control_msg(const esp_ble_mesh_prov_cb_param_t *param, control_task_msg_evt_provision_t evt)
{
    return control_task_publish(CONTROL_TASK_MSG_CODE_PROVISION, evt, param, sizeof(esp_ble_mesh_prov_cb_param_t));
}

/**
 * @brief Callback function for BLE Mesh provisioning events.
 *
 * This function is called whenever a BLE Mesh provisioning event occurs.
 *
 * @param event The provisioning event type.
 * @param param Pointer to the provisioning event parameters.
 */
static void app_ble_mesh_provisioning_cb(esp_ble_mesh_prov_cb_event_t event,
                                         const esp_ble_mesh_prov_cb_param_t *param)
{
    ESP_LOGD(TAG, "Event 0x%02x", event);


    if(prov_cb_evt_ctrl_task_evt_table[event].ctrl_task_evt != 0)
    {
        /* Entry available in prov_cb_evt_ctrl_task_evt_table */
        ESP_LOGD(TAG, "%s", prov_cb_evt_ctrl_task_evt_table[event].evt_str);
        if(send_control_msg(param,  prov_cb_evt_ctrl_task_evt_table[event].ctrl_task_evt) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to send control message");
        }
    }
    else
    {
        ESP_LOGW(TAG, "Unhandled event: %d", event);
    }
    if (event == ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT)
    {
        ESP_LOGI(TAG, "net_idx: 0x%04x, addr: " LOG_ANSI_COLOR_REGULAR(LOG_ANSI_COLOR_CYAN) "0x%04x" LOG_ANSI_COLOR_RESET, param->node_prov_complete.net_idx, param->node_prov_complete.addr);
        ESP_LOGI(TAG, "flags: 0x%02x, iv_index: 0x%08" PRIx32, param->node_prov_complete.flags, param->node_prov_complete.iv_index);
    }
}

/**
 * @brief Initialize provisioning parameters.
 *
 * This function initializes the provisioning parameters by copying the UUID from the provided
 * server configuration and registering the provisioning callback.
 *
 * @param[in] svr_cfg Pointer to the provisioning parameters structure containing the UUID.
 *
 * @return
 *    - ESP_OK: Success
 *    - ESP_FAIL: Failed to register provisioning callback
 */
esp_err_t prod_init_prov(const prov_params_t * svr_cfg)
{
    if (!svr_cfg)
    {
        ESP_LOGE(TAG, "Invalid server configuration");
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(&prov_params.uuid, svr_cfg->uuid, sizeof(prov_params.uuid));

    g_prod_prov.uuid = prov_params.uuid;
    return esp_ble_mesh_register_prov_callback((esp_ble_mesh_prov_cb_t)app_ble_mesh_provisioning_cb);
}
