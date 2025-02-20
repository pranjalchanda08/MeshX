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

#if CONFIG_ENABLE_UNIT_TEST
#include <unit_test.h>
#endif /* CONFIG_ENABLE_UNIT_TEST */

#if CONFIG_ENABLE_PROVISIONING
#include <prod_prov.h>
#endif /* CONFIG_ENABLE_PROVISIONING */

#if CONFIG_ENABLE_CONFIG_SERVER
#include <config_server.h>
#endif /* CONFIG_ENABLE_CONFIG_SERVER */

#if CONFIG_GEN_ONOFF_SERVER_COUNT
#include <prod_onoff_server.h>
#endif /* CONFIG_GEN_ONOFF_SERVER_COUNT */

#if CONFIG_GEN_ONOFF_CLIENT_COUNT
#include <prod_onoff_client.h>
#endif /* CONFIG_GEN_ONOFF_CLIENT_COUNT */

#if CONFIG_ENABLE_LIGHT_CTL_SERVER
#include <prod_light_ctl_srv.h>
#endif /* CONFIG_ENABLE_LIGHT_CTL_SERVER */

#define ESP_ERR_PRINT_RET(_e_str, _err)            \
    if (_err != ESP_OK)                            \
    {                                              \
        ESP_LOGE(TAG, _e_str " (err 0x%x)", _err); \
        return _err;                               \
    }

#define CID_ESP CONFIG_CID_ID

typedef enum meshx_element_type
{
    MESHX_ELEMENT_TYPE_RELAY_SERVER,
    MESHX_ELEMENT_TYPE_RELAY_CLIENT,
    MESHX_ELEMENT_TYPE_LIGHT_CWWW_SERVER,
    MESHX_ELEMENT_TYPE_LIGHT_CWWW_CLIENT,
    MESHX_ELEMENT_TYPE_MAX
}meshx_element_type_t;

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
