#pragma once

#include "app_common.h"

#define RELAY_SERVER_ELEMENT_NOS_DEF 1

#ifndef CONFIG_RELAY_SERVER_COUNT
#define CONFIG_RELAY_SERVER_COUNT RELAY_SERVER_ELEMENT_NOS_DEF
#endif

#define RELAY_SRV_ELEMENT_INSTANCE(_n) relay_server_model_##_n
#define RELAY_SRV_MODEL_INSTANCE(_n) onoff_server_relay_server_model_##_n
#define RELAY_SRV_MODEL(_n, _pin)                                                                                      \
    ESP_BLE_MESH_MODEL_PUB_DEFINE(onoff_pub_relay_server_model_##_n, sizeof(esp_ble_mesh_gen_onoff_set_t), ROLE_NODE); \
    esp_ble_mesh_gen_onoff_srv_t onoff_server_relay_server_model_##_n = {                                              \
        .rsp_ctrl = {                                                                                                  \
            .get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,                                                              \
            .set_auto_rsp = ESP_BLE_MESH_SERVER_RSP_BY_APP,                                                            \
        },                                                                                                             \
    };                                                                                                                 \
    esp_ble_mesh_model_t relay_server_model_##_n[1] = {                                                                \
        ESP_BLE_MESH_MODEL_GEN_ONOFF_SRV(&onoff_pub_relay_server_model_##_n, &onoff_server_relay_server_model_##_n),   \
    };                                                                                                                 \
    static gpio_handle_t RELAY_SRV_GPIO_HANDLE_##_n = {.pin = _pin, .dir = GPIO_MODE_OUTPUT};

esp_err_t prod_gen_srv_init();

#if CONFIG_RELAY_SERVER_COUNT > 0
extern esp_ble_mesh_model_t RELAY_SRV_ELEMENT_INSTANCE(0)[1];
#endif /* CONFIG_RELAY_SERVER_COUNT */
#if CONFIG_RELAY_SERVER_COUNT > 1
extern esp_ble_mesh_model_t RELAY_SRV_ELEMENT_INSTANCE(1)[1];
#endif /* CONFIG_RELAY_SERVER_COUNT */
#if CONFIG_RELAY_SERVER_COUNT > 2
extern esp_ble_mesh_model_t RELAY_SRV_ELEMENT_INSTANCE(2)[1];
#endif /* CONFIG_RELAY_SERVER_COUNT */
#if CONFIG_RELAY_SERVER_COUNT > 3
extern esp_ble_mesh_model_t RELAY_SRV_ELEMENT_INSTANCE(3)[1];
#endif /* CONFIG_RELAY_SERVER_COUNT */
#if CONFIG_RELAY_SERVER_COUNT > 4
extern esp_ble_mesh_model_t RELAY_SRV_ELEMENT_INSTANCE(4)[1];
#endif /* CONFIG_RELAY_SERVER_COUNT */
#if CONFIG_RELAY_SERVER_COUNT > 5
extern esp_ble_mesh_model_t RELAY_SRV_ELEMENT_INSTANCE(5)[1];
#endif /* CONFIG_RELAY_SERVER_COUNT */
#if CONFIG_RELAY_SERVER_COUNT > 6
extern esp_ble_mesh_model_t RELAY_SRV_ELEMENT_INSTANCE(6)[1];
#endif /* CONFIG_RELAY_SERVER_COUNT */
#if CONFIG_RELAY_SERVER_COUNT > 7
extern esp_ble_mesh_model_t RELAY_SRV_ELEMENT_INSTANCE(7)[1];
#endif /* CONFIG_RELAY_SERVER_COUNT */
