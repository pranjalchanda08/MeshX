/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file    meshx.h
 * @brief   Headers for meshx.c
 *
 */
#ifndef __MESHX_H__
#define __MESHX_H__

#include <app_common.h>
#include <os_timer.h>
#include <meshx_nvs.h>
#include <meshx_api.h>

#if CONFIG_ENABLE_UNIT_TEST
#include <unit_test.h>
#endif /* CONFIG_ENABLE_UNIT_TEST */

#if CONFIG_ENABLE_PROVISIONING
#include <prod_prov.h>
#endif /* CONFIG_ENABLE_PROVISIONING */

#if CONFIG_ENABLE_CONFIG_SERVER
#include <config_server.h>
#endif /* CONFIG_ENABLE_CONFIG_SERVER */

#if CONFIG_RELAY_SERVER_COUNT
#include <relay_server_element.h>
#endif /* CONFIG_RELAY_SERVER_COUNT */

#if CONFIG_RELAY_CLIENT_COUNT
#include <relay_client_element.h>
#endif /* CONFIG_RELAY_CLIENT_COUNT */

#if CONFIG_LIGHT_CWWW_SRV_COUNT
#include <cwww_server_element.h>
#endif /* CONFIG_LIGHT_CWWW_SRV_COUNT */

#if CONFIG_LIGHT_CWWW_CLIENT_COUNT
#include <light_cwww_client.h>
#endif /* CONFIG_LIGHT_CWWW_CLIENT_COUNT */

#define ESP_ERR_PRINT_RET(_e_str, _err)            \
    if (_err != ESP_OK)                            \
    {                                              \
        ESP_LOGE(TAG, _e_str " (err 0x%x)", _err); \
        return _err;                               \
    }

#define CID_ESP CONFIG_CID_ID

typedef struct element_comp
{
    meshx_element_type_t type;
    uint16_t element_cnt;
}element_comp_t;

typedef struct meshx_config
{
    uint16_t cid;
    uint16_t pid;
    char *product_name;
    uint32_t meshx_nvs_save_period;
    uint16_t element_comp_arr_len;
    element_comp_t *element_comp_arr;
}meshx_config_t;

/**
 * @brief MeshX initialisation function
 *
 * This function initialises the MeshX stack with the given configuration.
 * @param config Pointer to the configuration structure
 *
 * @return ESP_OK, Success
 */

esp_err_t meshx_init(meshx_config_t const *config);

#endif /* __MESHX_H__ */
