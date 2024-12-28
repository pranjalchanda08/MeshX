/**
 * @file prod_gen_server.c
 * @brief Implementation of the BLE Mesh Generic Server for the product.
 *
 * This file contains the implementation of the BLE Mesh Generic Server
 * for handling various server events and registering callbacks.
 *
 * @auther Pranjal Chanda
 */

#include "prod_gen_server.h"

#define TAG __func__

#define PROD_SERVER_INIT_MAGIC_NO   0x1121

static const char* server_state_str[] =
{
    [ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT] = "SRV_STATE_CH",
    [ESP_BLE_MESH_GENERIC_SERVER_RECV_GET_MSG_EVT] = "SRV_RECV_GET",
    [ESP_BLE_MESH_GENERIC_SERVER_RECV_SET_MSG_EVT] = "SRV_RECV_SET"
};

static uint16_t prod_server_init = 0;
static struct prod_server_cb_reg_head prod_server_cb_reg_list = SLIST_HEAD_INITIALIZER(prod_server_cb_reg_list);
static SemaphoreHandle_t prod_server_mutex;

static void prod_ble_mesh_generic_server_cb(esp_ble_mesh_generic_server_cb_event_t event,
                                           esp_ble_mesh_generic_server_cb_param_t *param)
{
    ESP_LOGI(TAG, "%s, op|src|dst:%04" PRIx32 "|%04x|%04x",
             server_state_str[event], param->ctx.recv_op, param->ctx.addr, param->ctx.recv_dst);

    if(event != ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT)
        return;

    xSemaphoreTake(prod_server_mutex, portMAX_DELAY);
    struct prod_server_cb_reg *item;
    SLIST_FOREACH(item, &prod_server_cb_reg_list, next) {
        if ((item->model_id == param->model->model_id
        || item->model_id == param->model->vnd.model_id)
        && item->cb)
        {
            /* Dispatch callback to generic model type */
            item->cb(param);
            break;
        }
    }
    xSemaphoreGive(prod_server_mutex);
}

esp_err_t prod_gen_srv_reg_cb(uint32_t model_id, prod_server_cb cb)
{
    xSemaphoreTake(prod_server_mutex, portMAX_DELAY);
    struct prod_server_cb_reg *item;
    SLIST_FOREACH(item, &prod_server_cb_reg_list, next) {
        if (item->model_id == model_id) {
            /* If already registered over-write */
            item->cb = cb;
            xSemaphoreGive(prod_server_mutex);
            return ESP_OK;
        }
    }

    item = malloc(sizeof(struct prod_server_cb_reg));
    if (item == NULL) {
        xSemaphoreGive(prod_server_mutex);
        return ESP_ERR_NO_MEM;
    }

    item->model_id = model_id;
    item->cb = cb;
    SLIST_INSERT_HEAD(&prod_server_cb_reg_list, item, next);
    xSemaphoreGive(prod_server_mutex);

    return ESP_OK;
}

esp_err_t prod_gen_srv_dereg_cb(uint32_t model_id)
{
    xSemaphoreTake(prod_server_mutex, portMAX_DELAY);
    struct prod_server_cb_reg *item;
    struct prod_server_cb_reg *tmp;
    SLIST_FOREACH_SAFE(item, &prod_server_cb_reg_list, next, tmp) {
        if (item->model_id == model_id) {
            SLIST_REMOVE(&prod_server_cb_reg_list, item, prod_server_cb_reg, next);
            free(item);
            xSemaphoreGive(prod_server_mutex);
            return ESP_OK;
        }
    }
    xSemaphoreGive(prod_server_mutex);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t prod_gen_srv_init(void)
{
    if(prod_server_init == PROD_SERVER_INIT_MAGIC_NO)
        return ESP_OK;
    prod_server_init = PROD_SERVER_INIT_MAGIC_NO;
    prod_server_mutex = xSemaphoreCreateMutex();
    if (prod_server_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    return esp_ble_mesh_register_generic_server_callback(prod_ble_mesh_generic_server_cb);
}
