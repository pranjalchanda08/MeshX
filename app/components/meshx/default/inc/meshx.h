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

#include <app_common.h>
#include <meshx_nvs.h>
#include <meshx_api.h>
#include <meshx_err.h>
#include <meshx_os_timer.h>
#include <meshx_elements.h>
#include <interface/meshx_platform.h>

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

#define CID_ESP CONFIG_CID_ID

/**
 * @struct element_comp
 * @brief Structure for element composition.
 */
typedef struct element_comp
{
    meshx_element_type_t type;  /**< Element type */
    uint16_t element_cnt;       /**< Number of elements */
}element_comp_t;

/**
 * @struct meshx_config
 * @brief Structure for MeshX configuration.
 */
typedef struct meshx_config
{
    uint16_t cid;                       /**< Company ID */
    uint16_t pid;                       /**< Product ID */
    char *product_name;                 /**< Product name */
    uint32_t meshx_nvs_save_period;     /**< NVS save period */
    uint16_t element_comp_arr_len;      /**< Length of the element composition array */
    element_comp_t *element_comp_arr;   /**< Element composition array */
    meshx_app_data_cb_t app_element_cb; /**< Application element callback */
    meshx_app_ctrl_cb_t app_ctrl_cb;    /**< Application control callback */
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
