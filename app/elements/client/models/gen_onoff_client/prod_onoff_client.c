/**
 * @file prod_onoff_client.c
 * @brief Implementation of the Generic OnOff Client Model for BLE Mesh.
 *
 * This file provides the implementation of the Generic OnOff Client Model
 * used in BLE Mesh networks. It includes functions to initialize the client,
 * register callbacks, and handle BLE Mesh events related to the OnOff Client.
 */

#include "prod_onoff_client.h"

#define TAG __func__

#define PROD_CLIENT_INIT_MAGIC 0x2378

static uint16_t prod_client_init_flag = 0;

/**
 * @brief Mapping of BLE Mesh client state events to string representations.
 */
static const char *client_state_str[] =
{
    [ESP_BLE_MESH_GENERIC_CLIENT_PUBLISH_EVT] = "PUBLISH_EVT",
    [ESP_BLE_MESH_GENERIC_CLIENT_TIMEOUT_EVT] = "TIMEOUT_EVT",
    [ESP_BLE_MESH_GENERIC_CLIENT_GET_STATE_EVT] = "GET_STATE_EVT",
    [ESP_BLE_MESH_GENERIC_CLIENT_SET_STATE_EVT] = "SET_STATE_EVT",
};

static prod_onoff_cli_cb_reg_t *prod_onoff_cli_cb_reg_table;

/**
 * @brief Dispatch registered callbacks for a given OnOff Client event.
 *
 * This function iterates through the callback registration table and invokes
 * the callbacks that match the provided event bitmap.
 *
 * @param[in] param Pointer to the BLE Mesh generic client callback parameter structure.
 * @param[in] evt   OnOff Client event type.
 */
static void prod_onoff_reg_cb_dispatch(const esp_ble_mesh_generic_client_cb_param_t *param, prod_onoff_cli_evt_t evt)
{
    if (prod_onoff_cli_cb_reg_table == NULL)
    {
        ESP_LOGW(TAG, "No onoff client callback registered for event: %p", (void *)evt);
        return;
    }

    prod_onoff_cli_cb_reg_t *ptr = prod_onoff_cli_cb_reg_table;

    while (ptr)
    {
        if ((evt & ptr->evt_bmap) && ptr->cb != NULL)
        {
            ptr->cb(param, evt); // Call the registered callback
        }
        ptr = ptr->next; // Move to the next registration
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
static void app_ble_mesh_generic_client_cb(esp_ble_mesh_generic_client_cb_event_t event,
                                           const esp_ble_mesh_generic_client_cb_param_t *param)
{
    ESP_LOGI(TAG, "%s, op|src|dst: %04" PRIx32 "|%04x|%04x",
            client_state_str[event], param->params->ctx.recv_op, param->params->ctx.addr, param->params->ctx.recv_dst);
    prod_onoff_reg_cb_dispatch(param, (prod_onoff_cli_evt_t) BIT(event));
}

/**
 * @brief Register a callback for OnOff Client events.
 *
 * This function allows users to register a callback for handling specific
 * OnOff Client events based on a provided event bitmap.
 *
 * @param[in] cb              Pointer to the callback function to register.
 * @param[in] config_evt_bmap Bitmap of events to register for.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t prod_onoff_reg_cb(prod_onoff_cli_cb cb, uint32_t config_evt_bmap)
{
    if (cb == NULL || config_evt_bmap == 0)
    {
        return ESP_ERR_INVALID_ARG; // Invalid arguments
    }

    prod_onoff_cli_cb_reg_t **ptr = &prod_onoff_cli_cb_reg_table;

    while (*ptr != NULL)
    {
        ptr = &(*ptr)->next;
    }

    *ptr = (prod_onoff_cli_cb_reg_t *)malloc(sizeof(prod_onoff_cli_cb_reg_t));
    if (*ptr == NULL)
    {
        return ESP_ERR_NO_MEM; // Memory allocation failed
    }

    (*ptr)->cb = cb;
    (*ptr)->evt_bmap = config_evt_bmap;
    (*ptr)->next = NULL;

    return ESP_OK;
}

/**
 * @brief Initialize the Generic OnOff Client.
 *
 * This function initializes the OnOff Client by registering the BLE Mesh
 * generic client callback.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t prod_onoff_client_init(void)
{
    return (prod_client_init_flag == PROD_CLIENT_INIT_MAGIC) ? ESP_OK :
        esp_ble_mesh_register_generic_client_callback((esp_ble_mesh_generic_client_cb_t)&app_ble_mesh_generic_client_cb);
}
