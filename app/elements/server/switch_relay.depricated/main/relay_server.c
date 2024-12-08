/* main.c - Application main entry point */

/*
 * SPDX-FileCopyrightText: 2017 Intel Corporation
 * SPDX-FileContributor: 2018-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "board.h"
#include "app_common.h"
#include "relay_server_model.h"

#define RELAY_OFF 0
#define RELAY_ON !RELAY_OFF

#define TAG "REL_SRV"

#if CONFIG_RELAY_SERVER_COUNT > 0
RELAY_SRV_MODEL(0, GPIO_NUM_8);
#endif /* CONFIG_RELAY_SERVER_COUNT */
#if CONFIG_RELAY_SERVER_COUNT > 1
RELAY_SRV_MODEL(1, GPIO_NUM_9);
#endif /* CONFIG_RELAY_SERVER_COUNT */
#if CONFIG_RELAY_SERVER_COUNT > 2
RELAY_SRV_MODEL(2, GPIO_NUM_10);
#endif /* CONFIG_RELAY_SERVER_COUNT */
#if CONFIG_RELAY_SERVER_COUNT > 3
RELAY_SRV_MODEL(3, GPIO_NUM_11);
#endif /* CONFIG_RELAY_SERVER_COUNT */
#if CONFIG_RELAY_SERVER_COUNT > 4
RELAY_SRV_MODEL(4, GPIO_NUM_12);
#endif /* CONFIG_RELAY_SERVER_COUNT */
#if CONFIG_RELAY_SERVER_COUNT > 5
RELAY_SRV_MODEL(5, GPIO_NUM_13);
#endif /* CONFIG_RELAY_SERVER_COUNT */
#if CONFIG_RELAY_SERVER_COUNT > 6
RELAY_SRV_MODEL(6, GPIO_NUM_14);
#endif /* CONFIG_RELAY_SERVER_COUNT */
#if CONFIG_RELAY_SERVER_COUNT > 7
RELAY_SRV_MODEL(7, GPIO_NUM_15);
#endif /* CONFIG_RELAY_SERVER_COUNT */

gpio_handle_t *element_gpio_list[] =
    {
#if CONFIG_RELAY_SERVER_COUNT > 0
        &RELAY_SRV_GPIO_HANDLE_0,
#endif
#if CONFIG_RELAY_SERVER_COUNT > 1
        &RELAY_SRV_GPIO_HANDLE_1,
#endif
#if CONFIG_RELAY_SERVER_COUNT > 2
        &RELAY_SRV_GPIO_HANDLE_2,
#endif
#if CONFIG_RELAY_SERVER_COUNT > 3
        &RELAY_SRV_GPIO_HANDLE_3,
#endif
#if CONFIG_RELAY_SERVER_COUNT > 4
        &RELAY_SRV_GPIO_HANDLE_4,
#endif
#if CONFIG_RELAY_SERVER_COUNT > 5
        &RELAY_SRV_GPIO_HANDLE_5,
#endif
#if CONFIG_RELAY_SERVER_COUNT > 6
        &RELAY_SRV_GPIO_HANDLE_6,
#endif
#if CONFIG_RELAY_SERVER_COUNT > 7
        &RELAY_SRV_GPIO_HANDLE_7,
#endif
};

uint16_t element_gpio_list_len = ARRAY_SIZE(element_gpio_list);

static bool app_will_hw_state_change(esp_ble_mesh_generic_server_cb_param_t *param)
{
    if (ESP_BLE_MESH_ADDR_IS_UNICAST(param->ctx.recv_dst) 
    || (ESP_BLE_MESH_ADDR_IS_GROUP(param->ctx.recv_dst) 
        && (esp_ble_mesh_is_model_subscribed_to_group(param->model, param->ctx.recv_dst))) 
    || (param->ctx.recv_dst == ESP_BLE_MESH_ADDR_ALL_NODES))
    {
        return true;
    }
    return false;
}

static void app_handle_gen_onoff_msg(esp_ble_mesh_generic_server_cb_param_t *param)
{
    esp_ble_mesh_gen_onoff_srv_t *srv = (esp_ble_mesh_gen_onoff_srv_t *)param->model->user_data;

    uint16_t el_idx = param->model->element->element_addr - esp_ble_mesh_get_primary_element_address();

    switch (param->ctx.recv_op) {
    case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET:
        esp_ble_mesh_server_model_send_msg(param->model, &param->ctx,
            ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS, sizeof(srv->state.onoff), &srv->state.onoff);
        break;
    case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET:
    case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK:
        if (param->value.set.onoff.op_en == false) {
            /* If no optional bit set */
            srv->state.onoff = param->value.set.onoff.onoff;
        } else {
            /* If optional bits set */
            /* TODO: Delay and state transition */
            srv->state.onoff = param->value.set.onoff.onoff;
        }
        if (param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET) {
            esp_ble_mesh_server_model_send_msg(param->model, &param->ctx,
                ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS, sizeof(srv->state.onoff), &srv->state.onoff);
        }
        esp_ble_mesh_model_publish(param->model, ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS,
            sizeof(srv->state.onoff), &srv->state.onoff, ROLE_NODE);
        if (app_will_hw_state_change(param))
                hw_state_set(el_idx, param->value.state_change.onoff_set.onoff);
        break;
    default:
        break;
    }
}

static void app_ble_mesh_generic_server_cb(esp_ble_mesh_generic_server_cb_event_t event,
                                               esp_ble_mesh_generic_server_cb_param_t *param)
{
    esp_ble_mesh_gen_onoff_srv_t *srv;
    ESP_LOGI(TAG, "event 0x%02x, opcode 0x%04" PRIx32 ", src 0x%04x, dst 0x%04x",
             event, param->ctx.recv_op, param->ctx.addr, param->ctx.recv_dst);
    
    switch (event)
    {
    case ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT");
        if (param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET ||
            param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK)
        {
            ESP_LOGI(TAG, "onoff 0x%02x", param->value.state_change.onoff_set.onoff);
            app_handle_gen_onoff_msg(param);
        }
        break;
    case ESP_BLE_MESH_GENERIC_SERVER_RECV_GET_MSG_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_GENERIC_SERVER_RECV_GET_MSG_EVT");
        if (param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET)
        {
            srv = (esp_ble_mesh_gen_onoff_srv_t *)param->model->user_data;
            ESP_LOGI(TAG, "onoff 0x%02x", srv->state.onoff);
            app_handle_gen_onoff_msg(param);
        }
        break;
    case ESP_BLE_MESH_GENERIC_SERVER_RECV_SET_MSG_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_GENERIC_SERVER_RECV_SET_MSG_EVT");
        if (param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET ||
            param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK)
        {
            ESP_LOGI(TAG, "onoff 0x%02x, tid 0x%02x", param->value.set.onoff.onoff, param->value.set.onoff.tid);
            if (param->value.set.onoff.op_en)
            {
                ESP_LOGI(TAG, "trans_time 0x%02x, delay 0x%02x",
                         param->value.set.onoff.trans_time, param->value.set.onoff.delay);
            }
            app_handle_gen_onoff_msg(param);
        }
        break;
    default:
        ESP_LOGE(TAG, "Unknown Generic Server event 0x%02x", event);
        break;
    }
}

esp_err_t prod_gen_srv_init()
{
    return esp_ble_mesh_register_generic_server_callback(app_ble_mesh_generic_server_cb);
}
