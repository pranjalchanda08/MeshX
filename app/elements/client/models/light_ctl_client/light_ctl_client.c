/**
 * @file light_ctl_client.c
 * @brief Implementation of the Light CTL Client model for ESP32 BLE Mesh.
 *
 * This file contains the implementation of the Light CTL Client model, including
 * initialization, callback registration, and event handling.
 *
 * @date
 */

#include "light_ctl_client.h"

#define TAG __func__

#define LIGHT_CTL_CLIENT_INIT_MAGIC 0x8932

static uint16_t light_ctl_client_init_flag = 0;

static const char *client_state_str[] = {
    [ESP_BLE_MESH_LIGHT_CLIENT_GET_STATE_EVT] = "GET_STATE_EVT",
    [ESP_BLE_MESH_LIGHT_CLIENT_SET_STATE_EVT] = "SET_STATE_EVT",
    [ESP_BLE_MESH_LIGHT_CLIENT_PUBLISH_EVT] = "PUBLISH_EVT",
    [ESP_BLE_MESH_LIGHT_CLIENT_TIMEOUT_EVT] = "TIMEOUT_EVT",
};

static light_ctl_cli_cb_reg_t *light_ctl_cli_cb_reg_table;

/**
 * @brief Dispatch the registered callbacks for the Light CTL Client events.
 *
 * This function iterates through the registered callback table and calls the
 * appropriate callback functions based on the event bitmap.
 *
 * @param[in] param Pointer to the BLE Mesh Light Client callback parameters.
 * @param[in] evt The Light CTL Client event.
 */
static void light_ctl_cli_reg_cb_dispatch(const esp_ble_mesh_light_client_cb_param_t *param, light_ctl_cli_evt_t evt)
{
    if (light_ctl_cli_cb_reg_table == NULL)
    {
        ESP_LOGW(TAG, "No ctl client callback registered for event: %p", (void *)evt);
        return;
    }

    light_ctl_cli_cb_reg_t *ptr = light_ctl_cli_cb_reg_table;

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
 * @brief BLE Mesh Light Client callback function.
 *
 * This function is called by the BLE Mesh stack when a Light Client event occurs.
 * It logs the event details and dispatches the registered callbacks.
 *
 * @param[in] event The Light Client event.
 * @param[in] param Pointer to the BLE Mesh Light Client callback parameters.
 */
void app_ble_mesh_light_client_cb(esp_ble_mesh_light_client_cb_event_t event,
                                  const esp_ble_mesh_light_client_cb_param_t *param)
{
    ESP_LOGI(TAG, "event 0x%02x, opcode 0x%04" PRIx32 ", src 0x%04x, dst 0x%04x",
             event, param->params->ctx.recv_op, param->params->ctx.addr, param->params->ctx.recv_dst);
    ESP_LOGI(TAG, "%s", client_state_str[event]);
    light_ctl_cli_reg_cb_dispatch(param, (light_ctl_cli_evt_t)BIT(event));
}

/**
 * @brief Register a callback for Light CTL Client events.
 *
 * This function registers a callback function for the specified Light CTL Client
 * events. The callback will be called when the corresponding events occur.
 *
 * @param[in] cb The callback function to register.
 * @param[in] config_evt_bmap The event bitmap specifying the events to register for.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t prod_light_ctl_cli_reg_cb(light_cli_cb cb, uint32_t config_evt_bmap)
{
    if (cb == NULL || config_evt_bmap == 0)
        return ESP_ERR_INVALID_ARG; // Invalid arguments

    light_ctl_cli_cb_reg_t **ptr = &light_ctl_cli_cb_reg_table;

    while (*ptr != NULL)
        ptr = &(*ptr)->next;

    *ptr = (light_ctl_cli_cb_reg_t *)malloc(sizeof(light_ctl_cli_cb_reg_t));
    if (*ptr == NULL)
        return ESP_ERR_NO_MEM; // Memory allocation failed

    (*ptr)->cb = cb;
    (*ptr)->evt_bmap = config_evt_bmap;
    (*ptr)->next = NULL;

    return ESP_OK;
}

/**
 * @brief Initialize the Light CTL Client model.
 *
 * This function initializes the Light CTL Client model by registering the
 * Light Client callback with the BLE Mesh stack.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t prod_light_ctl_client_init()
{
    return (light_ctl_client_init_flag == LIGHT_CTL_CLIENT_INIT_MAGIC) ? ESP_OK
                                                                       : esp_ble_mesh_register_light_client_callback((esp_ble_mesh_light_client_cb_t)&app_ble_mesh_light_client_cb);
}
