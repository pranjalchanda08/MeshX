/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_onoff_client.c
 * @brief Implementation of the Generic OnOff Client Model for BLE Mesh.
 *
 * This file provides the implementation of the Generic OnOff Client Model
 * used in BLE Mesh networks. It includes functions to initialize the client,
 * register callbacks, and handle BLE Mesh events related to the OnOff Client.
 */

#include "meshx_onoff_client.h"

#define TAG __func__

#define MESHX_CLIENT_INIT_MAGIC 0x2378

static uint16_t meshx_client_init_flag = 0;
static SemaphoreHandle_t meshx_onoff_cli_mutex;

/**
 * @brief Mapping of BLE Mesh client state events to string representations.
 */
static const char *client_state_str[] =
{
    [ESP_BLE_MESH_GENERIC_CLIENT_PUBLISH_EVT]   = "PUBLISH_EVT",
    [ESP_BLE_MESH_GENERIC_CLIENT_TIMEOUT_EVT]   = "TIMEOUT_EVT",
    [ESP_BLE_MESH_GENERIC_CLIENT_GET_STATE_EVT] = "GET_STATE_EVT",
    [ESP_BLE_MESH_GENERIC_CLIENT_SET_STATE_EVT] = "SET_STATE_EVT",
};

SLIST_HEAD(meshx_onoff_cli_cb_reg_head, meshx_onoff_cli_cb_reg);
static struct meshx_onoff_cli_cb_reg_head meshx_onoff_cli_cb_reg_table = SLIST_HEAD_INITIALIZER(meshx_onoff_cli_cb_reg_table);

/**
 * @brief Dispatch registered callbacks for a given OnOff Client event.
 *
 * This function iterates through the callback registration table and invokes
 * the callbacks that match the provided event bitmap.
 *
 * @param[in] param Pointer to the BLE Mesh generic client callback parameter structure.
 * @param[in] evt   OnOff Client event type.
 */
static void meshx_onoff_reg_cb_dispatch(const esp_ble_mesh_generic_client_cb_param_t *param, meshx_onoff_cli_evt_t evt)
{
    if (SLIST_EMPTY(&meshx_onoff_cli_cb_reg_table))
    {
        ESP_LOGW(TAG, "No onoff client callback registered for event: %p", (void *)evt);
        return;
    }

    if (xSemaphoreTake(meshx_onoff_cli_mutex, portMAX_DELAY) == pdTRUE)
    {
        meshx_onoff_cli_cb_reg_t *ptr;
        SLIST_FOREACH(ptr, &meshx_onoff_cli_cb_reg_table, entries)
        {
            if ((evt & ptr->evt_bmap) && ptr->cb != NULL)
            {
                ptr->cb(param, evt); // Call the registered callback
            }
        }
        xSemaphoreGive(meshx_onoff_cli_mutex);
    }
}

/**
 * @brief BLE Mesh Generic Client callback handler.
 *
 * This function processes generic client events and invokes the appropriate
 * registered callbacks.
 *
 * @param[in] event Event type received from the BLE Mesh stack.
 * @param[in] param Pointer to the BLE Mesh generic client callback parameter structure.
 */
static void meshx_generic_client_cb(esp_ble_mesh_generic_client_cb_event_t event,
                                           const esp_ble_mesh_generic_client_cb_param_t *param)
{
    ESP_LOGD(TAG, "%s, err|op|src|dst: %d|%04" PRIx32 "|%04x|%04x",
            client_state_str[event], param->error_code, param->params->ctx.recv_op, param->params->ctx.addr, param->params->ctx.recv_dst);
    if(param->error_code == MESHX_SUCCESS)
    {
        meshx_onoff_reg_cb_dispatch(param, (meshx_onoff_cli_evt_t) BIT(event));
    }
}

/**
 * @brief Register a callback for OnOff Client events.
 *
 * This function allows users to register a callback for handling specific
 * OnOff Client events based on a provided event bitmap.
 *
 * @param[in] cb              Pointer to the callback function to register.
 * @param[in] config_evt_bmap Bitmap of events to register for.
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
meshx_err_t meshx_onoff_reg_cb(meshx_onoff_cli_cb cb, uint32_t config_evt_bmap)
{
    if (cb == NULL || config_evt_bmap == 0)
    {
        return MESHX_INVALID_ARG; // Invalid arguments
    }

    meshx_onoff_cli_cb_reg_t *new_entry = (meshx_onoff_cli_cb_reg_t *) malloc(sizeof(meshx_onoff_cli_cb_reg_t));
    if (new_entry == NULL)
    {
        return MESHX_NO_MEM; // Memory allocation failed
    }

    new_entry->cb = cb;
    new_entry->evt_bmap = config_evt_bmap;

    if (xSemaphoreTake(meshx_onoff_cli_mutex, portMAX_DELAY) == pdTRUE)
    {
        SLIST_INSERT_HEAD(&meshx_onoff_cli_cb_reg_table, new_entry, entries);
        xSemaphoreGive(meshx_onoff_cli_mutex);
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Initialize the Generic OnOff Client.
 *
 * This function initializes the OnOff Client by registering the BLE Mesh
 * generic client callback.
 *
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
meshx_err_t meshx_onoff_client_init(void)
{
    if (meshx_client_init_flag == MESHX_CLIENT_INIT_MAGIC)
    {
        return MESHX_SUCCESS;
    }

    meshx_onoff_cli_mutex = xSemaphoreCreateMutex();
    if (meshx_onoff_cli_mutex == NULL)
    {
        return MESHX_NO_MEM; // Mutex creation failed
    }

    return esp_ble_mesh_register_generic_client_callback((esp_ble_mesh_generic_client_cb_t)&meshx_generic_client_cb);
}

/**
 * @brief Send a generic on/off client message.
 *
 * This function sends a generic on/off client message with the specified parameters.
 *
 * @param[in] model   Pointer to the BLE Mesh model structure.
 * @param[in] opcode  The operation code of the message.
 * @param[in] addr    The destination address to which the message is sent.
 * @param[in] net_idx The network index to be used for sending the message.
 * @param[in] app_idx The application index to be used for sending the message.
 * @param[in] state   The state value to be sent in the message.
 * @param[in] tid     The transaction ID to be used for the message.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_INVALID_ARG: Invalid argument
 *    - MESHX_NO_MEM: Out of memory
 *    - ESP_FAIL: Sending message failed
 */
meshx_err_t meshx_onoff_client_send_msg(
        esp_ble_mesh_model_t *model,
        uint16_t opcode,
        uint16_t addr,
        uint16_t net_idx,
        uint16_t app_idx,
        uint8_t state,
        uint8_t tid
)
{
    meshx_err_t err;
    esp_ble_mesh_client_common_param_t common = {0};
    esp_ble_mesh_generic_client_set_state_t set = {0};

    common.model        = model;
    common.opcode       = opcode;
    common.ctx.addr     = addr;
    common.ctx.net_idx  = net_idx;
    common.ctx.app_idx  = app_idx;
    common.msg_timeout  = 0; /* 0 indicates that timeout value from menuconfig will be used */
    common.ctx.send_ttl = 3;
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 2, 0)
    common.msg_role = ROLE_NODE;
#endif
    if (common.opcode != ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET)
    {
        set.onoff_set.tid   = tid;
        set.onoff_set.onoff = state;
        set.onoff_set.op_en = false;
        err = esp_ble_mesh_generic_client_set_state(&common, &set);
        if (err)
        {
            ESP_LOGE(TAG, "Send Generic OnOff failed");
            return err;
        }
    }
    else{
        err = esp_ble_mesh_client_model_send_msg(common.model, &common.ctx, common.opcode, 0, NULL, 0, true, ROLE_NODE);
        if (err)
        {
            ESP_LOGE(TAG, "Send Generic OnOff failed");
            return err;
        }
    }
    return err;
}
