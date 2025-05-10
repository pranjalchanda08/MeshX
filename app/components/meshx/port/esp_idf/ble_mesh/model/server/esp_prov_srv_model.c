/**
 * Copyright (c) 2024 - 2025 MeshX
 *
 * @file esp_prov_srv_model.c
 * @brief Implementation of BLE Mesh provisioning server model for ESP32.
 *        This file contains the provisioning callback handling, control task
 *        message mapping, and provisioning parameter initialization.
 *        It facilitates the provisioning process and event handling for BLE Mesh.
 *
 * @author Pranjal Chanda
 *
 */

#include "interface/ble_mesh/meshx_ble_mesh_prov_srv.h"

/**
 * @brief Structure to map provisioning callback events to control task events.
 *
 * This structure contains a string representation of the provisioning event
 * and the corresponding control task event.
 */
typedef struct prov_cb_evt_ctrl_task_evt_table
{
    const char *evt_str;                            /**< String representation of the provisioning event. */
    control_task_msg_evt_provision_t ctrl_task_evt; /**< Corresponding control task event. */
} prov_cb_evt_ctrl_task_evt_table_t;

/**
 * @brief Table mapping provisioning events to control task events.
 *
 * This table is indexed by provisioning event types and contains the string
 * representation of the event and the corresponding control task event.
 */
static prov_cb_evt_ctrl_task_evt_table_t prov_cb_evt_ctrl_task_evt_table[ESP_BLE_MESH_PROV_EVT_MAX] = {
    [ESP_BLE_MESH_NODE_PROV_RESET_EVT] = {"ESP_BLE_MESH_NODE_PROV_RESET_EVT", CONTROL_TASK_MSG_EVT_NODE_RESET},
    [ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT] = {"ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT", CONTROL_TASK_MSG_EVT_PROVISION_STOP},
    [ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT] = {"ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT", CONTROL_TASK_MSG_EVT_IDENTIFY_START},
    [ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT] = {"ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT", CONTROL_TASK_MSG_EVT_IDENTIFY_STOP},
    [ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT] = {"ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT", CONTROL_TASK_MSG_EVT_EN_NODE_PROV},
    [ESP_BLE_MESH_PROXY_SERVER_CONNECTED_EVT] = {"ESP_BLE_MESH_PROXY_SERVER_CONNECTED_EVT", CONTROL_TASK_MSG_EVT_PROXY_CONNECT},
    [ESP_BLE_MESH_PROXY_SERVER_DISCONNECTED_EVT] = {"ESP_BLE_MESH_PROXY_SERVER_DISCONNECTED_EVT", CONTROL_TASK_MSG_EVT_PROXY_DISCONN},
};

/**
 * @brief Provisioning parameters.
 *
 * This structure holds the provisioning parameters used in the provisioning process.
 */
static meshx_prov_params_t prov_params;

/**
 * @brief Global provisioning structure.
 *
 * This structure holds the global provisioning configuration.
 */
static MESHX_PROV g_meshx_prov;

/**
 * @brief Send a control message to the control task.
 *
 * This function sends a control message to the control task with the given event type.
 *
 * @param[in] param Pointer to the provisioning callback parameters.
 * @param[in] evt The event type to send to the control task.
 *
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */

static meshx_err_t send_control_msg(const esp_ble_mesh_prov_cb_param_t *param, control_task_msg_evt_provision_t evt)
{
    return control_task_msg_publish(CONTROL_TASK_MSG_CODE_PROVISION, evt, param, sizeof(esp_ble_mesh_prov_cb_param_t));
}

/**
 * @brief Callback function for BLE Mesh provisioning events.
 *
 * This function is called whenever a BLE Mesh provisioning event occurs.
 *
 * @param event The provisioning event type.
 * @param param Pointer to the provisioning event parameters.
 */
static void meshx_provisioning_cb(esp_ble_mesh_prov_cb_event_t event,
                                  const esp_ble_mesh_prov_cb_param_t *param)
{
    MESHX_LOGD(MODULE_ID_MODEL_SERVER, "Event 0x%02x", event);

    if (prov_cb_evt_ctrl_task_evt_table[event].ctrl_task_evt != 0)
    {
        /* Entry available in prov_cb_evt_ctrl_task_evt_table */
        MESHX_LOGD(MODULE_ID_MODEL_SERVER, "%s", prov_cb_evt_ctrl_task_evt_table[event].evt_str);
        if (send_control_msg(param, prov_cb_evt_ctrl_task_evt_table[event].ctrl_task_evt) != MESHX_SUCCESS)
        {
            ESP_LOGE(TAG, "Failed to send control message");
        }
    }
    else
    {
        MESHX_LOGD(MODULE_ID_MODEL_SERVER, "Unhandled event: %d", event);
    }
    if (event == ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT)
    {
        MESHX_LOGI(MODULE_ID_MODEL_SERVER, "net_idx: 0x%04x, addr: " LOG_ANSI_COLOR_REGULAR(LOG_ANSI_COLOR_CYAN) "0x%04x" LOG_ANSI_COLOR_RESET, param->node_prov_complete.net_idx, param->node_prov_complete.addr);
        MESHX_LOGI(MODULE_ID_MODEL_SERVER, "flags: 0x%02x, iv_index: 0x%08" PRIx32, param->node_prov_complete.flags, param->node_prov_complete.iv_index);
    }
}

/**
 * @brief Initialize provisioning parameters.
 *
 * This function initializes the provisioning parameters by copying the UUID from the provided
 * server configuration and registering the provisioning callback.
 *
 * @param uuid Pointer to the UUID of the device.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_FAIL: Failed to register provisioning callback
 */
meshx_err_t meshx_plat_init_prov(const uint8_t *uuid)
{
    if (!uuid)
    {
        ESP_LOGE(TAG, "Invalid server configuration");
        return MESHX_INVALID_ARG;
    }
    memcpy(&prov_params.uuid, uuid, sizeof(prov_params.uuid));

    g_meshx_prov.uuid = prov_params.uuid;

    return esp_ble_mesh_register_prov_callback((esp_ble_mesh_prov_cb_t)meshx_provisioning_cb);
}

/**
 * @brief Get the provisioning parameters.
 *
 * This function returns a pointer to the global provisioning parameters.
 *
 * @return Pointer to the global provisioning parameters.
 */
MESHX_PROV *meshx_plat_get_prov(void)
{
    return &g_meshx_prov;
}
