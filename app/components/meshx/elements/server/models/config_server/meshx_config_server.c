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

#define CONFIG_SRV_INIT_MAGIC   0x2307

static uint16_t config_srv_init_flag = 0;

/**
 * @brief Initializes the Configuration Server.
 *
 * Registers the BLE Mesh Configuration Server callback function and prepares the server for use.
 *
 * @return MESHX_SUCCESS on success, an error code otherwise.
 */
meshx_err_t meshx_init_config_server()
{
    if(config_srv_init_flag == CONFIG_SRV_INIT_MAGIC)
        return MESHX_INVALID_ARG;

    config_srv_init_flag = CONFIG_SRV_INIT_MAGIC;

    return meshx_plat_config_srv_init();
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
meshx_err_t meshx_config_server_cb_reg(config_srv_cb_t cb, uint32_t config_evt_bmap)
{
    if (cb == NULL || config_evt_bmap == 0)
    {
        return MESHX_INVALID_ARG; // Invalid arguments
    }

    return control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_CONFIG,
        config_evt_bmap,
        (control_task_msg_handle_t)cb);

}

/**
 * @brief Retrieves the instance of the MeshX configuration server.
 *
 * This function provides access to the configuration server instance
 * used in the MeshX framework. The configuration server is responsible
 * for managing and storing configuration settings for the mesh network.
 *
 * @param[out] p_conf_srv Pointer to a variable where the configuration
 *                        server instance will be stored. The pointer
 *                        must be of type `void**`.
 *
 * @return
 * - `MESHX_SUCCESS` on successful retrieval of the configuration server instance.
 * - An appropriate error code of type `meshx_err_t` on failure.
 */
meshx_err_t meshx_get_config_srv_instance(void** p_conf_srv)
{
    return meshx_plat_get_config_srv_instance(p_conf_srv);
}

/**
 * @brief Retrieves the configuration server model for the MeshX framework.
 *
 * This function provides access to the configuration server model used in the
 * MeshX implementation. The retrieved model can be used for configuring and
 * managing the mesh network.
 *
 * @param[out] p_model Pointer to a variable where the address of the
 *                     configuration server model will be stored. The caller
 *                     must ensure that the pointer is valid.
 *
 * @return
 * - `MESHX_SUCCESS` on success.
 * - An appropriate error code of type `meshx_err_t` on failure.
 */
meshx_err_t meshx_get_config_srv_model(void* p_model)
{
    return meshx_plat_get_config_srv_model(p_model);
}
