/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_provisioning_server.c
 * @brief This file contains the implementation of the provisioning process for the BLE mesh node.
 *
 * The provisioning process is responsible for setting up the BLE mesh node, including
 * initializing necessary components and handling communication with other nodes.
 *
 * @author Pranjal Chanda
 */

#include "string.h"
#include "meshx_control_task.h"
#include "meshx_provisioning_server.h"

/**
 * @brief Initialize provisioning parameters.
 *
 * This function initializes the provisioning parameters by copying the UUID from the provided
 * server configuration and registering the provisioning callback.
 *
 * @param[in] prov_cfg Pointer to the provisioning parameters structure containing the UUID.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_FAIL: Failed to register provisioning callback
 */
meshx_err_t meshx_init_prov(const meshx_prov_params_t *prov_cfg)
{
    if (!prov_cfg || memcmp(prov_cfg->uuid, MESHX_UUID_EMPTY, sizeof(meshx_uuid_addr_t)) == 0)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid server configuration");
        return MESHX_INVALID_ARG;
    }
    return meshx_plat_init_prov(prov_cfg->uuid);
}

/**
 * @brief Get the provisioning parameters.
 *
 * This function returns a pointer to the global provisioning parameters.
 *
 * @return Pointer to the global provisioning parameters.
 */
MESHX_PROV *meshx_get_prov(void)
{
    return meshx_plat_get_prov();
}
