/**
 * @file config_server.c
 * @brief Implementation of the Configuration Server for ESP BLE Mesh.
 *
 * This file contains the initialization and event handling logic for the BLE Mesh
 * Configuration Server, including the management of callback registrations and event dispatching.
 *
 * @author [Pranjal Chanda]
 */

#include "config_server.h"

#define TAG __func__

/* Structure for storing callback registrations */
typedef struct config_server_cb_reg
{
    config_srv_cb cb;                     /**< Registered callback function */
    uint32_t evt_bmap;                    /**< Bitmap of events the callback is registered for */
    struct config_server_cb_reg *next;    /**< Pointer to the next callback registration */
} config_server_cb_reg_t;

/* Global variable for Configuration Server parameters */
esp_ble_mesh_cfg_srv_t g_prod_config_server = {
    /* 3 transmissions with 20ms interval */
    .net_transmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .relay = ESP_BLE_MESH_RELAY_ENABLED,
    .relay_retransmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .beacon = ESP_BLE_MESH_BEACON_ENABLED,
#if defined(CONFIG_BLE_MESH_GATT_PROXY_SERVER)
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_ENABLED,
#else
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_NOT_SUPPORTED,
#endif
#if defined(CONFIG_BLE_MESH_FRIEND)
    .friend_state = ESP_BLE_MESH_FRIEND_ENABLED,
#else
    .friend_state = ESP_BLE_MESH_FRIEND_NOT_SUPPORTED,
#endif
    .default_ttl = 7,
};

/* Linked list head for callback registration table */
static config_server_cb_reg_t *config_server_cb_reg_table = NULL;

/**
 * @brief Dispatches configuration events to registered callbacks.
 *
 * Iterates through the linked list of registered callbacks and invokes
 * the appropriate callback for a given event.
 *
 * @param[in] param Pointer to the BLE Mesh Configuration Server callback parameters.
 * @param[in] evt The event to dispatch.
 */
static void prod_config_server_cb_dispatch(const esp_ble_mesh_cfg_server_cb_param_t *param, config_evt_t evt)
{
    if (config_server_cb_reg_table == NULL)
    {
        ESP_LOGW(TAG, "No config server callback registered for event: %p", (void*)evt);
        return;
    }

    config_server_cb_reg_t *ptr = config_server_cb_reg_table;

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
 * @brief BLE Mesh Configuration Server callback function.
 *
 * Handles state change events and dispatches them to the appropriate callbacks
 * registered with the configuration server.
 *
 * @param[in] event Configuration server event type.
 * @param[in] param Pointer to the BLE Mesh Configuration Server callback parameters.
 */
static void prod_ble_mesh_config_server_cb(esp_ble_mesh_cfg_server_cb_event_t event,
                                           const esp_ble_mesh_cfg_server_cb_param_t *param)
{
    if (event == ESP_BLE_MESH_CFG_SERVER_STATE_CHANGE_EVT)
    {
        switch (param->ctx.recv_op)
        {
        case ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD:
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD");
            ESP_LOGD(TAG, "net_idx 0x%04x, app_idx 0x%04x",
                     param->value.state_change.appkey_add.net_idx,
                     param->value.state_change.appkey_add.app_idx);
            ESP_LOG_BUFFER_HEX("AppKey", param->value.state_change.appkey_add.app_key, 16);
            prod_config_server_cb_dispatch(param, CONFIG_EVT_MODEL_APP_KEY_ADD);
            break;
        case ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND:
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND");
            ESP_LOGD(TAG, "elem_addr 0x%04x, app_idx 0x%04x, cid 0x%04x, mod_id 0x%04x",
                     param->value.state_change.mod_app_bind.element_addr,
                     param->value.state_change.mod_app_bind.app_idx,
                     param->value.state_change.mod_app_bind.company_id,
                     param->value.state_change.mod_app_bind.model_id);
            prod_config_server_cb_dispatch(param, CONFIG_EVT_APP_BIND);
            break;
        case ESP_BLE_MESH_MODEL_OP_MODEL_SUB_ADD:
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_MODEL_SUB_ADD");
            ESP_LOGD(TAG, "elem_addr 0x%04x, sub_addr 0x%04x, cid 0x%04x, mod_id 0x%04x",
                     param->value.state_change.mod_sub_add.element_addr,
                     param->value.state_change.mod_sub_add.sub_addr,
                     param->value.state_change.mod_sub_add.company_id,
                     param->value.state_change.mod_sub_add.model_id);
            prod_config_server_cb_dispatch(param, CONFIG_EVT_MODEL_SUB_ADD);
            break;
        case ESP_BLE_MESH_MODEL_OP_MODEL_PUB_SET:
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_MODEL_PUB_SET");
            ESP_LOGD(TAG, "elem_addr 0x%04x, pub_addr 0x%04x, cid 0x%04x, mod_id 0x%04x",
                     param->value.state_change.mod_pub_set.element_addr,
                     param->value.state_change.mod_pub_set.pub_addr,
                     param->value.state_change.mod_pub_set.company_id,
                     param->value.state_change.mod_pub_set.model_id);
            prod_config_server_cb_dispatch(param, CONFIG_EVT_MODEL_PUB_ADD);
            break;
        case ESP_BLE_MESH_MODEL_OP_MODEL_SUB_DELETE:
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_MODEL_PUB_SET");
            prod_config_server_cb_dispatch(param, CONFIG_EVT_MODEL_SUB_DEL);
            break;
        case ESP_BLE_MESH_MODEL_OP_MODEL_APP_UNBIND:
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_MODEL_APP_UNBIND");
            prod_config_server_cb_dispatch(param, CONFIG_EVT_MODEL_SUB_DEL);
            break;
        default:
            ESP_LOGW(TAG, "Unhandled config op-code: %p", (void *)param->ctx.recv_op);
            break;
        }
    }
}

/**
 * @brief Initializes the Configuration Server.
 *
 * Registers the BLE Mesh Configuration Server callback function and prepares the server for use.
 *
 * @return ESP_OK on success, an error code otherwise.
 */
esp_err_t prod_init_config_server()
{
    esp_ble_mesh_register_config_server_callback((esp_ble_mesh_cfg_server_cb_t)&prod_ble_mesh_config_server_cb);
    return ESP_OK;
}

/**
 * @brief Registers a configuration server callback for specific events.
 *
 * Adds a new callback registration to the linked list for dispatching events.
 *
 * @param[in] cb Callback function to register.
 * @param[in] config_evt_bmap Bitmap of events the callback is interested in.
 *
 * @return ESP_OK on success, an error code otherwise.
 */
esp_err_t prod_config_server_cb_reg(config_srv_cb cb, uint32_t config_evt_bmap)
{
    if (cb == NULL || config_evt_bmap == 0)
    {
        return ESP_ERR_INVALID_ARG; // Invalid arguments
    }

    config_server_cb_reg_t **ptr = &config_server_cb_reg_table;

    while (*ptr != NULL)
    {
        ptr = &(*ptr)->next;
    }

    *ptr = (config_server_cb_reg_t *)malloc(sizeof(config_server_cb_reg_t));
    if (*ptr == NULL)
    {
        return ESP_ERR_NO_MEM; // Memory allocation failed
    }

    (*ptr)->cb = cb;
    (*ptr)->evt_bmap = config_evt_bmap;
    (*ptr)->next = NULL;

    return ESP_OK;
}
