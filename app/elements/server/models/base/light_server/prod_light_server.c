/**
 * @file prod_light_server.c
 * @brief Implementation of the BLE Mesh Lighting Server for the ESP32.
 *
 * This file contains the implementation of the BLE Mesh Lighting Server,
 * including initialization, event handling, and callback registration.
 *
 * @author [Pranjal Chanda]
 */

#include "prod_light_server.h"
#include "prod_gen_server.h"

#define TAG __func__

#define PROD_SERVER_INIT_MAGIC_NO   0x2483

static const char* server_state_str[] =
{
    [ESP_BLE_MESH_LIGHTING_SERVER_STATE_CHANGE_EVT] = "STATE_CHANGE_EVT",
    [ESP_BLE_MESH_LIGHTING_SERVER_RECV_GET_MSG_EVT] = "RECV_GET_MSG_EVT",
    [ESP_BLE_MESH_LIGHTING_SERVER_RECV_SET_MSG_EVT] = "RECV_SET_MSG_EVT",
    [ESP_BLE_MESH_LIGHTING_SERVER_RECV_STATUS_MSG_EVT] = "RECV_STATUS_MSG_EVT",
};

static uint16_t prod_lighting_server_init = 0;
static struct prod_lighting_server_cb_list prod_lighting_server_cb_reg_table = SLIST_HEAD_INITIALIZER(prod_lighting_server_cb_reg_table);
static SemaphoreHandle_t prod_lighting_server_mutex;

/**
 * @brief Callback function for BLE Mesh Lightness Server events.
 *
 * This function is called whenever a BLE Mesh Lightness Server event occurs.
 *
 * @param[in] event The event type for the BLE Mesh Lightness Server.
 * @param[in] param Parameters associated with the event.
 */
static void prod_ble_lightness_server_cb(esp_ble_mesh_lighting_server_cb_event_t event,
                                         esp_ble_mesh_lighting_server_cb_param_t *param)
{
    ESP_LOGD(TAG, "event 0x%02x, opcode 0x%04" PRIx32 ", src 0x%04x, dst 0x%04x",
             event, param->ctx.recv_op, param->ctx.addr, param->ctx.recv_dst);

    ESP_LOGI(TAG, "%s", server_state_str[event]);

    if (xSemaphoreTake(prod_lighting_server_mutex, portMAX_DELAY) == pdTRUE) {
        prod_lighting_server_cb_reg_t *entry;
        SLIST_FOREACH(entry, &prod_lighting_server_cb_reg_table, entries) {
            if ((entry->model_id == param->model->model_id || entry->model_id == param->model->vnd.model_id) && entry->cb) {
                /* Dispatch callback to lighting model type */
                entry->cb(param);
            }
        }
        xSemaphoreGive(prod_lighting_server_mutex);
    }
}

/**
 * @brief Register a callback function for the lighting server model.
 *
 * This function registers a callback function that will be called when
 * certain events occur in the lighting server model.
 *
 * @param[in] model_id  The ID of the lighting server model.
 * @param[in] cb        The callback function to register.
 *
 * @return
 *    - ESP_OK: Success
 *    - ESP_ERR_INVALID_ARG: Invalid argument
 *    - ESP_FAIL: Other failures
 */
esp_err_t prod_lighting_reg_cb(uint32_t model_id, prod_lighting_server_cb cb)
{
    if (xSemaphoreTake(prod_lighting_server_mutex, portMAX_DELAY) == pdTRUE) {
        prod_lighting_server_cb_reg_t *entry;
        SLIST_FOREACH(entry, &prod_lighting_server_cb_reg_table, entries) {
            if (entry->model_id == model_id) {
                /* If already registered, overwrite */
                entry->cb = cb;
                xSemaphoreGive(prod_lighting_server_mutex);
                return ESP_OK;
            }
        }

        entry = malloc(sizeof(prod_lighting_server_cb_reg_t));
        if (!entry) {
            ESP_LOGE(TAG, "No Memory left for prod_lighting_server_cb_reg_table");
            xSemaphoreGive(prod_lighting_server_mutex);
            return ESP_ERR_NO_MEM;
        }

        entry->model_id = model_id;
        entry->cb = cb;
        SLIST_INSERT_HEAD(&prod_lighting_server_cb_reg_table, entry, entries);
        xSemaphoreGive(prod_lighting_server_mutex);
    }

    return ESP_OK;
}

/**
 * @brief Initialize the production lighting server.
 *
 * This function sets up the necessary configurations and initializes the
 * production lighting server for the BLE mesh node.
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_FAIL: Failed to initialize the lighting server
 */
esp_err_t prod_lighting_srv_init(void)
{
    if (prod_lighting_server_init == PROD_SERVER_INIT_MAGIC_NO)
        return ESP_OK;

    prod_lighting_server_mutex = xSemaphoreCreateMutex();
    if (prod_lighting_server_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_FAIL;
    }

    prod_lighting_server_init = PROD_SERVER_INIT_MAGIC_NO;
    return esp_ble_mesh_register_lighting_server_callback(prod_ble_lightness_server_cb);
}
