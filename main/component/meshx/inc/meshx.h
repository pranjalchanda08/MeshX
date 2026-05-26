/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file    meshx.h
 * @brief   This file contains the headers for meshx.c
 *
 * @author Pranjal Chanda
 */

#ifndef __MESHX_H__
#define __MESHX_H__

#include <meshx_common.h>
#include <meshx_nvs.h>
#include <meshx_api.h>
#include <meshx_err.h>
#include <meshx_os_timer.h>
#include <meshx_prov_srv.h>
#include <meshx_config_server.h>
#include "meshx_serial.h"
#include <meshx_txcm.h>
#include <interface/meshx_platform.h>

#include <meshx_builder_api.h>

#if CONFIG_ENABLE_UNIT_TEST
#include <unit_test.h>
#endif /* CONFIG_ENABLE_UNIT_TEST */

/**
 * @brief Print and return error message if an error occurs.
 *
 * This macro prints an error message and returns the error code if an error occurs.
 *
 * @param _e_str Error string to print
 * @param _err   Error code to return
 * @return       Error code
 */
#define MESHX_ERR_PRINT_RET(_e_str, _err)                         \
    if (_err != MESHX_SUCCESS)                                    \
    {                                                             \
        MESHX_LOGE(MODULE_ID_COMMON, _e_str " (err 0x%x)", _err); \
        return _err;                                              \
    }

typedef struct meshx_config
{
    uint32_t meshx_nvs_save_period;     /**< NVS save period */
    meshx_api_data_cb_t app_element_cb; /**< Application element callback */
    meshx_api_ctrl_cb_t app_ctrl_cb;    /**< Application control callback */
    unsigned meshx_log_level;           /**< MeshX log level */
}meshx_config_t;

/**
 * @brief MeshX initialisation function
 *
 * This function initialises the MeshX stack with the given configuration.
 *
 * @param[in] config Pointer to the configuration structure
 *
 * @return MESHX_SUCCESS, Success
 */
meshx_err_t meshx_init(meshx_config_t const *config);

#endif /* __MESHX_H__ */
