/**
 * Copyright (c) 2024 - 2025 MeshX
 *
 * @file esp_platform_ble_mesh.c
 * @brief This file contains platform-specific implementations for BLE Mesh functionality
 *        on the ESP32 using the MeshX framework. It provides APIs for managing BLE Mesh
 *        models, compositions, provisioning, and initialization.
 *
 *        The functions in this file handle tasks such as checking group subscriptions,
 *        creating and deleting model publications, managing model IDs, initializing
 *        compositions, and setting up BLE Mesh provisioning and node configurations.
 *
 * @author Pranjal Chanda
 *
 */
#include "string.h"
#include "esp_bt_device.h"
#include "ble_mesh_plat_init.h"
#include "interface/meshx_platform.h"
#include "interface/logging/meshx_log.h"
#include "interface/ble_mesh/meshx_ble_mesh_cmn.h"
#include "interface/ble_mesh/server/meshx_ble_mesh_prov_srv.h"

meshx_err_t meshx_is_group_subscribed(meshx_model_t *p_model, uint16_t addr)
{
    const uint16_t * res = esp_ble_mesh_is_model_subscribed_to_group(p_model->p_model, addr);
    if(res != NULL)
        return MESHX_SUCCESS;

    return MESHX_FAIL;
}
meshx_err_t meshx_plat_create_model_pub(void ** p_pub, uint16_t nmax)
{
    if(!p_pub)
        return MESHX_INVALID_ARG;

    *p_pub   = (MESHX_MODEL_PUB *) MESHX_CALOC(nmax, sizeof(MESHX_MODEL_PUB));
    if(!*p_pub)
        return MESHX_NO_MEM;

    return MESHX_SUCCESS;
}

meshx_err_t meshx_plat_del_model_pub(void ** p_pub)
{
    if (!p_pub)
        return MESHX_INVALID_ARG;

    if (*p_pub) {
        MESHX_FREE(*p_pub);
        *p_pub = NULL;
    }

    return MESHX_SUCCESS;
}

meshx_err_t meshx_plat_client_create(meshx_ptr_t p_model, meshx_ptr_t* p_pub, meshx_ptr_t* p_cli, uint16_t model_id)
{
    if (!p_model || !p_pub || !p_cli)
    {
        return MESHX_INVALID_ARG; // Invalid arguments
    }
    meshx_err_t err = MESHX_SUCCESS;

    /* SIG On OFF Init */
    memcpy((meshx_ptr_t)&(((MESHX_MODEL *)p_model)->model_id), &model_id, sizeof(model_id));

    // Create the publication context for the model
    err = meshx_plat_create_model_pub(p_pub, 1);
    if (err)
    {
        return meshx_plat_del_model_pub(p_pub); // Clean up on error
    }

    // Allocate memory for the generic client model
    *p_cli = (MESHX_CLI *)MESHX_CALOC(1, sizeof(MESHX_CLI));
    if (!*p_cli)
    {
        return MESHX_NO_MEM; // Memory allocation failed
    }

    // Initialize the generic client model
    ((MESHX_MODEL *)p_model)->user_data = *p_cli;

    meshx_ptr_t*temp = (meshx_ptr_t*)&((MESHX_MODEL *)p_model)->pub;

    *temp = *p_pub;

    return MESHX_SUCCESS; // Successfully created the model and publication context
}

meshx_err_t meshx_plat_client_delete(meshx_ptr_t p_model, meshx_ptr_t* p_pub, meshx_ptr_t* p_cli)
{
    if (!p_model || !p_pub || !p_cli)
    {
        return MESHX_INVALID_ARG; // Invalid arguments
    }

    meshx_plat_del_model_pub(p_pub);

    MESHX_FREE(*p_cli);
    *p_cli = NULL;

    return MESHX_SUCCESS;
}

meshx_err_t meshx_get_model_id(meshx_ptr_t p_model, uint16_t *model_id)
{
    if(!p_model)
        return MESHX_INVALID_ARG;

    MESHX_MODEL * model = (MESHX_MODEL *)p_model;
    *model_id = model->model_id;

    return MESHX_SUCCESS;
}

meshx_err_t meshx_create_plat_composition(meshx_ptr_t* p_comp)
{
    if(!p_comp)
        return MESHX_INVALID_ARG;

    *p_comp = (MESHX_COMPOSITION *) MESHX_MALLOC (sizeof(MESHX_COMPOSITION));
    if(!*p_comp)
        return MESHX_NO_MEM;

    return MESHX_SUCCESS;
}

meshx_err_t meshx_plat_add_element_to_composition(
    uint16_t index,
    meshx_ptr_t p_element_list,
    meshx_ptr_t p_sig_models,
    meshx_ptr_t p_ven_models,
    uint8_t sig_cnt,
    uint8_t ven_cnt
) {
    if (!p_element_list) {
        return MESHX_INVALID_ARG;
    }

    MESHX_ELEMENT* element = (MESHX_ELEMENT*)(p_element_list) + index;
    element->sig_models = p_sig_models;
    element->vnd_models = p_ven_models;

    memcpy((meshx_ptr_t)&(element->sig_model_count), &sig_cnt, sizeof(uint8_t));
    memcpy((meshx_ptr_t)&(element->vnd_model_count), &ven_cnt, sizeof(uint8_t));

    return MESHX_SUCCESS;
}

meshx_err_t meshx_plat_composition_init(
    meshx_ptr_t p_composition,
    meshx_ptr_t p_elements,
    uint16_t cid,
    uint16_t pid,
    uint16_t element_idx
)
{
    if(!p_composition)
        return MESHX_INVALID_ARG;

    MESHX_COMPOSITION * composition = (MESHX_COMPOSITION *) p_composition;
    composition->cid = cid;
    composition->pid = pid;
    composition->element_count = element_idx;
    composition->elements = p_elements;

    return MESHX_SUCCESS;
}

meshx_err_t meshx_get_base_element_id(uint16_t *base_el_id)
{
    if (!base_el_id)
    {
        return MESHX_INVALID_ARG;
    }

    *base_el_id = esp_ble_mesh_get_primary_element_address();

    return MESHX_SUCCESS;
}

meshx_err_t meshx_platform_bt_init(meshx_uuid_addr_t uuid)
{
    esp_err_t err = ESP_OK;
    /* Initialize Bluetooth */
    err = bluetooth_init();
    if(err)
    {
        return MESHX_ERR_PLAT;
    }

    if(uuid == NULL)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Invalid configuration for Bluetooth initialization");
        return MESHX_INVALID_ARG;
    }
    /* Set the device UUID */
    if(memcmp(uuid, MESHX_UUID_EMPTY, sizeof(meshx_uuid_addr_t)) == 0)
    {
        const uint8_t *mac_addr = esp_bt_dev_get_address();
        if (mac_addr == NULL) {
            MESHX_LOGE(MODULE_ID_COMMON, "Failed to get device address");
            return MESHX_ERR_PLAT;
        }
        memcpy(uuid + 2, mac_addr, MESHX_BD_ADDR_LEN);
    }
    return MESHX_SUCCESS;
}

meshx_err_t meshx_plat_ble_mesh_init(const meshx_prov_params_t *prov_cfg, meshx_ptr_t comp)
{
    if(comp == NULL || prov_cfg == NULL)
        return MESHX_INVALID_ARG;

    meshx_err_t err = MESHX_SUCCESS;

    /* Initialize BLE Mesh Provisioner */
    MESHX_PROV *p_prov = NULL;

    p_prov = (MESHX_PROV *)meshx_plat_get_prov();
    if(p_prov == NULL)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to get provisioning instance");
        return MESHX_ERR_PLAT;
    }
    err = esp_ble_mesh_init(p_prov, comp);
    if(err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to initialize mesh stack");
        return err;
    }
    err = esp_ble_mesh_set_unprovisioned_device_name((char*)prov_cfg->node_name);
    if(err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to set device name");
        return err;
    }
    err = esp_ble_mesh_node_prov_enable((esp_ble_mesh_prov_bearer_t)(ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT));
    if(err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to enable mesh node");
        return err;
    }
    MESHX_LOGD(MODULE_ID_MODEL_SERVER, "BLE Mesh Node initialized");
    return MESHX_SUCCESS;
}
