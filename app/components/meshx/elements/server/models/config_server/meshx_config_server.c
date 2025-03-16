/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_config_server.c
 * @brief Implementation of the Configuration Server for ESP BLE Mesh.
 *
 * This file contains the initialization and event handling logic for the BLE Mesh
 * Configuration Server, including the management of callback registrations and event dispatching.
 *
 * @author Pranjal Chanda
 */

#include "meshx_config_server.h"

#define TAG __func__

/**
 * @brief Structure for storing the configuration server callback registration.
 */
typedef struct config_server_cb_reg
{
    config_srv_cb cb;                       /**< Registered callback function */
    uint32_t evt_bmap;                      /**< Bitmap of events the callback is registered for */
    SLIST_ENTRY(config_server_cb_reg) next; /**< Pointer to the next callback registration */
} config_server_cb_reg_t;

SLIST_HEAD(config_server_cb_reg_head, config_server_cb_reg);
static struct config_server_cb_reg_head config_server_cb_reg_table = SLIST_HEAD_INITIALIZER(config_server_cb_reg_table);

/**
 * @brief Mapping of BLE Mesh model operation codes to configuration events.
 */
typedef struct config_server_model_evt_map
{
    uint16_t model_op_code;
    const char *op_str;
    config_evt_t config_evt;
} config_server_model_evt_map_t;

static const config_server_model_evt_map_t config_server_model_evt_map_table[] = {
    {ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD, "OP_APP_KEY_ADD", CONFIG_EVT_MODEL_APP_KEY_ADD},
    {ESP_BLE_MESH_MODEL_OP_NET_KEY_ADD, "OP_NET_KEY_ADD", CONFIG_EVT_MODEL_APP_KEY_ADD},
    {ESP_BLE_MESH_MODEL_OP_MODEL_SUB_ADD, "OP_MODEL_SUB_ADD", CONFIG_EVT_MODEL_SUB_ADD},
    {ESP_BLE_MESH_MODEL_OP_MODEL_PUB_SET, "OP_MODEL_PUB_SET", CONFIG_EVT_MODEL_PUB_ADD},
    {ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND, "OP_MODEL_APP_BIND", CONFIG_EVT_MODEL_APP_KEY_BIND},
    {ESP_BLE_MESH_MODEL_OP_NET_KEY_DELETE, "OP_NET_KEY_DELETE", CONFIG_EVT_MODEL_NET_KEY_DEL},
    {ESP_BLE_MESH_MODEL_OP_APP_KEY_DELETE, "OP_APP_KEY_DELETE", CONFIG_EVT_MODEL_APP_KEY_DEL},
    {ESP_BLE_MESH_MODEL_OP_MODEL_SUB_DELETE, "OP_MODEL_SUB_DELETE", CONFIG_EVT_MODEL_SUB_DEL},
    {ESP_BLE_MESH_MODEL_OP_MODEL_APP_UNBIND, "OP_MODEL_APP_UNBIND", CONFIG_EVT_MODEL_APP_KEY_UNBIND},
};

/* Mutex for protecting the callback registration table */
static SemaphoreHandle_t config_server_mutex;

/* Global variable for Configuration Server parameters */
MESHX_CFG_SRV g_meshx_config_server = {
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

/**
 * @brief Dispatches configuration events to registered callbacks.
 *
 * Iterates through the linked list of registered callbacks and invokes
 * the appropriate callback for a given event.
 *
 * @param[in] param Pointer to the BLE Mesh Configuration Server callback parameters.
 * @param[in] evt The event to dispatch.
 */
static void meshx_config_server_cb_dispatch(const esp_ble_mesh_cfg_server_cb_param_t *param, config_evt_t evt)
{
    if (SLIST_EMPTY(&config_server_cb_reg_table))
    {
        ESP_LOGW(TAG, "No config server callback registered for event: %p", (void *)evt);
        return;
    }

    config_server_cb_reg_t *ptr;

    SLIST_FOREACH(ptr, &config_server_cb_reg_table, next)
    {
        if ((evt & ptr->evt_bmap) && ptr->cb != NULL)
        {
            ptr->cb(param, evt); // Call the registered callback
        }
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
static void meshx_ble_mesh_config_server_cb(esp_ble_mesh_cfg_server_cb_event_t event,
                                           const esp_ble_mesh_cfg_server_cb_param_t *param)
{
    if (event == ESP_BLE_MESH_CFG_SERVER_STATE_CHANGE_EVT)
    {
        for (size_t map_id = 0; map_id < ARRAY_SIZE(config_server_model_evt_map_table); map_id++)
        {
            if (config_server_model_evt_map_table[map_id].model_op_code == param->ctx.recv_op)
            {
                ESP_LOGI(TAG, "%s", config_server_model_evt_map_table[map_id].op_str);
                meshx_config_server_cb_dispatch(param, config_server_model_evt_map_table[map_id].config_evt);
                break;
            }
        }
    }
}

/**
 * @brief Initializes the Configuration Server.
 *
 * Registers the BLE Mesh Configuration Server callback function and prepares the server for use.
 *
 * @return MESHX_SUCCESS on success, an error code otherwise.
 */
meshx_err_t meshx_init_config_server()
{
    config_server_mutex = xSemaphoreCreateMutex();
    if (config_server_mutex == NULL)
    {
        return MESHX_NO_MEM; // Mutex creation failed
    }

    esp_ble_mesh_register_config_server_callback((esp_ble_mesh_cfg_server_cb_t)&meshx_ble_mesh_config_server_cb);
    return MESHX_SUCCESS;
}

/**
 * @brief Registers a configuration server callback for specific events.
 *
 * Adds a new callback registration to the linked list for dispatching events.
 *
 * @param[in] cb Callback function to register.
 * @param[in] config_evt_bmap Bitmap of events the callback is interested in.
 *
 * @return MESHX_SUCCESS on success, an error code otherwise.
 */
meshx_err_t meshx_config_server_cb_reg(config_srv_cb cb, uint32_t config_evt_bmap)
{
    if (cb == NULL || config_evt_bmap == 0)
    {
        return MESHX_INVALID_ARG; // Invalid arguments
    }

    if (xSemaphoreTake(config_server_mutex, portMAX_DELAY) == pdTRUE)
    {
        config_server_cb_reg_t *new_node = (config_server_cb_reg_t *)MESHX_MALLOC(sizeof(config_server_cb_reg_t));
        if (new_node == NULL)
        {
            xSemaphoreGive(config_server_mutex);
            return MESHX_NO_MEM; // Memory allocation failed
        }

        new_node->cb = cb;
        new_node->evt_bmap = config_evt_bmap;
        SLIST_INSERT_HEAD(&config_server_cb_reg_table, new_node, next);

        xSemaphoreGive(config_server_mutex);
    }

    return MESHX_SUCCESS;
}
