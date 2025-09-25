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

#if CONFIG_ENABLE_CONFIG_SERVER
static uint16_t config_srv_init_flag = 0;

typedef struct config_server_model_evt_map
{
    uint16_t model_op_code;
    const char *op_str;
    config_evt_t config_evt;
} config_server_model_evt_map_t;

static const config_server_model_evt_map_t config_server_model_evt_map_table[] = {
    {MESHX_MODEL_OP_APP_KEY_ADD, "OP_APP_KEY_ADD", CONTROL_TASK_MSG_EVT_APP_KEY_ADD},
    {MESHX_MODEL_OP_NET_KEY_ADD, "OP_NET_KEY_ADD", CONTROL_TASK_MSG_EVT_NET_KEY_ADD},
    {MESHX_MODEL_OP_MODEL_SUB_ADD, "OP_MODEL_SUB_ADD", CONTROL_TASK_MSG_EVT_SUB_ADD},
    {MESHX_MODEL_OP_MODEL_PUB_SET, "OP_MODEL_PUB_SET", CONTROL_TASK_MSG_EVT_PUB_ADD},
    {MESHX_MODEL_OP_MODEL_APP_BIND, "OP_MODEL_APP_BIND", CONTROL_TASK_MSG_EVT_APP_KEY_BIND},
    {MESHX_MODEL_OP_NET_KEY_DELETE, "OP_NET_KEY_DELETE", CONTROL_TASK_MSG_EVT_NET_KEY_DEL},
    {MESHX_MODEL_OP_APP_KEY_DELETE, "OP_APP_KEY_DELETE", CONTROL_TASK_MSG_EVT_APP_KEY_DEL},
    {MESHX_MODEL_OP_MODEL_SUB_DELETE, "OP_MODEL_SUB_DELETE", CONTROL_TASK_MSG_EVT_SUB_DEL},
    {MESHX_MODEL_OP_MODEL_APP_UNBIND, "OP_MODEL_APP_UNBIND", CONTROL_TASK_MSG_EVT_APP_KEY_UNBIND},
};
uint16_t config_srv_evt_map_count = sizeof(config_server_model_evt_map_table) / sizeof(config_server_model_evt_map_t);

/**
 * @brief Handles the configuration server events from the control task.
 *
 * This function processes the events received from the control task and publishes them
 * to the registered callbacks for the configuration server.
 *
 * @param[in] pdev Pointer to the device structure.
 * @param[in] evt Event type from the control task.
 * @param[in] params Pointer to the parameters associated with the event.
 *
 * @return MESHX_SUCCESS on success, an error code otherwise.
 */
static meshx_err_t meshx_config_server_control_task_handler(
    const dev_struct_t *pdev,
    control_task_msg_evt_t evt,
    const void *params
)
{
    if(!pdev || !params)
    {
        return MESHX_INVALID_ARG;
    }
    const meshx_config_srv_cb_param_t *pub_param = (const meshx_config_srv_cb_param_t *)params;

    if(evt != MESHX_MODEL_ID_CONFIG_SRV)
    {
        return MESHX_INVALID_ARG;
    }
    /* Map event to config event */
    config_evt_t config_evt = CONTROL_TASK_MSG_EVT_CONFIG_ALL;
    for (uint16_t i = 0; i < config_srv_evt_map_count; i++)
    {
        if (config_server_model_evt_map_table[i].model_op_code == pub_param->ctx.opcode)
        {
            config_evt = config_server_model_evt_map_table[i].config_evt;
            break;
        }
    }
    if (config_evt == CONTROL_TASK_MSG_EVT_CONFIG_ALL)
    {
        return MESHX_INVALID_ARG;
    }
    /* Publish the event */
    return control_task_msg_publish(
        CONTROL_TASK_MSG_CODE_CONFIG,
        config_evt,
        pub_param,
        sizeof(meshx_config_srv_cb_param_t));
}

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

    control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_FRM_BLE,
        MESHX_MODEL_ID_CONFIG_SRV,
        (control_task_msg_handle_t)meshx_config_server_control_task_handler);

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

#endif /* CONFIG_ENABLE_CONFIG_SERVER */
