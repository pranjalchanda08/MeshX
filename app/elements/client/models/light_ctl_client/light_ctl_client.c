/**
 * @file light_ctl_client.c
 * @brief Implementation of the Light CTL Client model for ESP32 BLE Mesh.
 *
 * This file contains the implementation of the Light CTL Client model, including
 * initialization, callback registration, and event handling.
 *
 * @auther Pranjal Chanda
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

SLIST_HEAD(light_ctl_cli_cb_reg_head, light_ctl_cli_cb_reg);
static struct light_ctl_cli_cb_reg_head light_ctl_cli_cb_reg_table = SLIST_HEAD_INITIALIZER(light_ctl_cli_cb_reg_table);
static SemaphoreHandle_t light_ctl_cli_cb_reg_mutex;

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
    if (xSemaphoreTake(light_ctl_cli_cb_reg_mutex, portMAX_DELAY) == pdTRUE)
    {
        light_ctl_cli_cb_reg_t *ptr;
        SLIST_FOREACH(ptr, &light_ctl_cli_cb_reg_table, next)
        {
            if ((evt & ptr->evt_bmap) && ptr->cb != NULL)
            {
                ptr->cb(param, evt); // Call the registered callback
            }
        }
        xSemaphoreGive(light_ctl_cli_cb_reg_mutex);
    }
    else
    {
        ESP_LOGW(TAG, "Failed to take mutex for callback dispatch");
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

    struct light_ctl_cli_cb_reg *new_entry = (struct light_ctl_cli_cb_reg *)malloc(sizeof(struct light_ctl_cli_cb_reg));
    if (new_entry == NULL)
        return ESP_ERR_NO_MEM; // Memory allocation failed

    new_entry->cb = cb;
    new_entry->evt_bmap = config_evt_bmap;

    if (xSemaphoreTake(light_ctl_cli_cb_reg_mutex, portMAX_DELAY) == pdTRUE)
    {
        SLIST_INSERT_HEAD(&light_ctl_cli_cb_reg_table, new_entry, next);
        xSemaphoreGive(light_ctl_cli_cb_reg_mutex);
    }

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
    if (light_ctl_client_init_flag == LIGHT_CTL_CLIENT_INIT_MAGIC)
        return ESP_OK;

    light_ctl_cli_cb_reg_mutex = xSemaphoreCreateMutex();
    if (light_ctl_cli_cb_reg_mutex == NULL)
        return ESP_ERR_NO_MEM; // Failed to create mutex

    esp_err_t err = esp_ble_mesh_register_light_client_callback((esp_ble_mesh_light_client_cb_t)&app_ble_mesh_light_client_cb);
    if (err == ESP_OK)
    {
        light_ctl_client_init_flag = LIGHT_CTL_CLIENT_INIT_MAGIC;
    }
    return err;
}

/**
 * @brief Send a Light CTL message.
 *
 * This function sends a Light CTL message with the specified parameters.
 *
 * @param[in] model Pointer to the BLE Mesh model.
 * @param[in] opcode Opcode of the message to be sent.
 * @param[in] addr Destination address of the message.
 * @param[in] net_idx Network index to be used.
 * @param[in] app_idx Application index to be used.
 * @param[in] lightness Lightness value to be sent.
 * @param[in] temperature Temperature value to be sent.
 * @param[in] delta_uv Delta UV value to be sent.
 * @param[in] tid Transaction ID.
 *
 * @return ESP_OK on success or an error code on failure.
 */
esp_err_t prod_light_ctl_send_msg(esp_ble_mesh_model_t *model,
                                  uint16_t opcode,
                                  uint16_t addr,
                                  uint16_t net_idx,
                                  uint16_t app_idx,
                                  uint16_t lightness,
                                  uint16_t temperature,
                                  uint16_t delta_uv,
                                  uint8_t tid)
{
    esp_err_t err = ESP_OK;
    esp_ble_mesh_client_common_param_t common = {0};
    esp_ble_mesh_light_client_set_state_t set = {0};

    common.model = model;
    common.opcode = opcode;
    common.ctx.addr = addr;
    common.ctx.net_idx = net_idx;
    common.ctx.app_idx = app_idx;
    common.msg_timeout = 0; /* 0 indicates that timeout value from menuconfig will be used */
    common.ctx.send_ttl = 3;
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 2, 0)
    common.msg_role = ROLE_NODE;
#endif

    if(opcode != ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_GET)
    {
        set.ctl_set.tid = tid;
        set.ctl_set.op_en = false;
        set.ctl_set.ctl_delta_uv = delta_uv;
        set.ctl_set.ctl_lightness = lightness;
        set.ctl_set.ctl_temperature = temperature;

        err = esp_ble_mesh_light_client_set_state(&common, &set);
        if (err)
        {
            ESP_LOGE(TAG, "Light CTL Client Send Message failed: (%d)", err);
        }
    }
    else{
        /* TODO: Issue: #14 */
        err = ESP_ERR_NOT_SUPPORTED;
    }
    return err;
}

/**
 * @brief Send a Light CTL Temperature message.
 *
 * This function sends a Light CTL Temperature message to a specified address.
 *
 * @param[in] model       Pointer to the BLE Mesh model.
 * @param[in] opcode      Opcode of the message.
 * @param[in] addr        Destination address of the message.
 * @param[in] net_idx     Network index to be used.
 * @param[in] app_idx     Application index to be used.
 * @param[in] temperature Light CTL Temperature value.
 * @param[in] delta_uv    Light CTL Delta UV value.
 * @param[in] tid         Transaction ID.
 *
 * @return
 *     - ESP_OK on success
 *     - Appropriate error code on failure
 */
esp_err_t prod_light_ctl_temperature_send_msg(esp_ble_mesh_model_t *model,
                                  uint16_t opcode,
                                  uint16_t addr,
                                  uint16_t net_idx,
                                  uint16_t app_idx,
                                  uint16_t temperature,
                                  uint16_t delta_uv,
                                  uint8_t tid)
{
    esp_err_t err = ESP_OK;
    esp_ble_mesh_client_common_param_t common = {0};
    esp_ble_mesh_light_client_set_state_t set = {0};

    common.model = model;
    common.opcode = opcode;
    common.ctx.addr = addr;
    common.ctx.net_idx = net_idx;
    common.ctx.app_idx = app_idx;
    common.msg_timeout = 0; /* 0 indicates that timeout value from menuconfig will be used */
    common.ctx.send_ttl = 3;
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 2, 0)
    common.msg_role = ROLE_NODE;
#endif

    if(opcode != ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_GET)
    {
        set.ctl_temperature_set.tid = tid;
        set.ctl_temperature_set.op_en = false;
        set.ctl_temperature_set.ctl_delta_uv = delta_uv;
        set.ctl_temperature_set.ctl_temperature = temperature;

        err = esp_ble_mesh_light_client_set_state(&common, &set);
        if (err)
        {
            ESP_LOGE(TAG, "Light CTL Client Send Message failed: (%d)", err);
        }
    }
    else{
        /* TODO: Issue: #14 */
        err = ESP_ERR_NOT_SUPPORTED;
    }
    return err;
}
