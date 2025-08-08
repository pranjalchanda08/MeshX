/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file composition.c
 * @brief BLE Mesh Composition Initialization and Element Creation
 *
 * This file contains functions and definitions for initializing BLE Mesh composition data
 * and creating BLE Mesh elements for various configurations such as relay servers, relay clients,
 * and CWWW (Cool White and Warm White) servers.
 *
 * The file includes necessary headers and conditionally includes headers based on configuration
 * macros. It defines macros for error handling and contains static variables for provisioning
 * parameters and Light CTL state.
 *
 */

#include "app_common.h"
#include "meshx.h"
#include "interface/ble_mesh/server/meshx_ble_mesh_prov_srv.h"

#if CONFIG_RELAY_SERVER_COUNT
#include "meshx_relay_server_element.h"
#endif /* CONFIG_RELAY_SERVER_COUNT */

#if CONFIG_RELAY_CLIENT_COUNT
#include "meshx_relay_client_element.h"
#endif /* CONFIG_RELAY_CLIENT_COUNT */

#if CONFIG_LIGHT_CWWW_SRV_COUNT
#include "meshx_cwww_server_element.h"
#endif /* CONFIG_LIGHT_CWWW_SRV_COUNT */

#if CONFIG_LIGHT_CWWW_CLIENT_COUNT
#include "meshx_light_cwww_client_element.h"
#endif /* CONFIG_LIGHT_CWWW_CLIENT_COUNT */

/**
 * @brief Mask for control task provisioning events.
 *
 * This macro defines a mask that combines multiple control task message events
 * related to provisioning.
 */
#define CONTROL_TASK_PROV_EVT_MASK CONTROL_TASK_MSG_EVT_IDENTIFY_START \
                                 | CONTROL_TASK_MSG_EVT_PROVISION_STOP \
                                 | CONTROL_TASK_MSG_EVT_IDENTIFY_STOP \
                                 | CONTROL_TASK_MSG_EVT_NODE_RESET

#if CONFIG_SECTION_ENABLE_ELEMENT_TABLE
#define MESHX_ELEMENT_COMP_TABLE_START  _element_table_start
#define MESHX_ELEMENT_COMP_TABLE_STOP   _element_table_end

extern element_comp_table_t MESHX_ELEMENT_COMP_TABLE_START;
extern element_comp_table_t MESHX_ELEMENT_COMP_TABLE_STOP;

#else

element_comp_fn_t element_comp_fn [MESHX_ELEMENT_TYPE_MAX] = {
#if CONFIG_RELAY_SERVER_COUNT
    [MESHX_ELEMENT_TYPE_RELAY_SERVER]       = &meshx_create_relay_elements,
#endif /* CONFIG_RELAY_SERVER_COUNT */
#if CONFIG_RELAY_CLIENT_COUNT
    [MESHX_ELEMENT_TYPE_RELAY_CLIENT]       = &create_relay_client_elements,
#endif /* CONFIG_RELAY_CLIENT_COUNT */
#if CONFIG_LIGHT_CWWW_SRV_COUNT
    [MESHX_ELEMENT_TYPE_LIGHT_CWWW_SERVER]  = &meshx_create_cwww_elements,
#endif /* CONFIG_LIGHT_CWWW_CLIENT_COUNT */
#if CONFIG_LIGHT_CWWW_CLIENT_COUNT
    [MESHX_ELEMENT_TYPE_LIGHT_CWWW_CLIENT]  = &create_cwww_client_elements,
#endif /* CONFIG_LIGHT_CWWW_SRV_COUNT */
};

#endif /* CONFIG_SECTION_ENABLE_ELEMENT_TABLE */


#if 0
#if CONFIG_ENABLE_LIGHT_CTL_SERVER
/** Light CTL state. */
MESHX_LIGHT_CTL_STATE ctl_state;
ESP_BLE_MESH_MODEL_PUB_DEFINE(ctl_setup_pub, 16, ROLE_NODE);

/** Light CTL setup server model. */
static esp_ble_mesh_light_ctl_setup_srv_t ctl_setup_server = {
    .rsp_ctrl = {
        .get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
        .set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
    },
    .state = &ctl_state,
};
#endif

/** Root models for BLE Mesh elements. */
static MESHX_MODEL meshx_sig_root_model_arr[] = {
#if CONFIG_ENABLE_CONFIG_SERVER
    ESP_BLE_MESH_MODEL_CFG_SRV(&MESHX_CONFIG_SERVER_INSTANCE),
#endif /* CONFIG_ENABLE_CONFIG_SERVER */
#if CONFIG_ENABLE_LIGHT_CTL_SERVER
    ESP_BLE_MESH_MODEL_LIGHT_CTL_SETUP_SRV(&ctl_setup_pub, &ctl_setup_server),
#endif
};
#else

/** Root models for BLE Mesh elements. */
static MESHX_MODEL *meshx_sig_root_model_arr = NULL;
static MESHX_MODEL *meshx_ven_root_model_arr = NULL;

typedef meshx_err_t (*root_model_getfn_t)(void* p_model);

static root_model_getfn_t meshx_sig_root_model_getfn[] = {
#if CONFIG_ENABLE_CONFIG_SERVER
    meshx_get_config_srv_model,
#endif /* CONFIG_ENABLE_CONFIG_SERVER */
};
static root_model_getfn_t meshx_ven_root_model_getfn[] = {};

static uint16_t meshx_sig_root_model_arr_len = MESHX_ARRAY_SIZE(meshx_sig_root_model_getfn);
static uint16_t meshx_ven_root_model_arr_len = MESHX_ARRAY_SIZE(meshx_ven_root_model_getfn);

#endif /* #if 0 */
/**
 * @brief Handles provisioning control task events.
 *
 * This function processes various provisioning-related events and updates
 * the device structure accordingly. It handles events such as provisioning
 * completion and identification start.
 *
 * @param[in] pdev Pointer to the device structure.
 * @param[in] evt The control task message event type.
 * @param[in] params Pointer to the event parameters.
 *
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
static meshx_err_t meshx_prov_control_task_handler(dev_struct_t *pdev, control_task_msg_evt_t evt, void *params)
{
    const meshx_prov_cb_param_t *param = (meshx_prov_cb_param_t*) params;

    switch (evt)
    {
        case CONTROL_TASK_MSG_EVT_PROVISION_STOP:
            pdev->meshx_store.net_key_id = param->node_prov_complete.net_idx;
            pdev->meshx_store.node_addr  = param->node_prov_complete.addr;
            meshx_nvs_set(MESHX_NVS_STORE, &pdev->meshx_store, sizeof(pdev->meshx_store), MESHX_NVS_AUTO_COMMIT);
            break;
        case CONTROL_TASK_MSG_EVT_IDENTIFY_START:
            MESHX_LOGI(MODULE_ID_COMMON, "Identify Start");
            break;
        default:
            break;
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Returns the root models for BLE Mesh elements.
 *
 * @return Pointer to the root models.
 */
MESHX_MODEL * get_root_sig_models(void)
{
    static MESHX_MODEL temp_model;
    if(meshx_sig_root_model_arr == NULL && meshx_sig_root_model_arr_len)
    {
        meshx_sig_root_model_arr = (MESHX_MODEL *) MESHX_MALLOC(sizeof(MESHX_MODEL) * meshx_sig_root_model_arr_len);
        if(meshx_sig_root_model_arr == NULL)
        {
            MESHX_LOGE(MODULE_ID_COMMON, "Failed to allocate memory for root models");
            return NULL;
        }
        memset(meshx_sig_root_model_arr, 0, sizeof(MESHX_MODEL) * meshx_sig_root_model_arr_len);

        for(uint16_t i = 0; i < meshx_sig_root_model_arr_len; i++)
        {
            if(meshx_sig_root_model_getfn[i] == NULL)
            {
                continue;
            }
            meshx_sig_root_model_getfn[i]((void**)&temp_model);
            memcpy(&meshx_sig_root_model_arr[i], &temp_model, sizeof(MESHX_MODEL));
        }
    }
    return meshx_sig_root_model_arr;
}

/**
 * @brief Returns the root models for BLE Mesh elements.
 *
 * @return Pointer to the root models.
 */
MESHX_MODEL * get_root_ven_models(void)
{
    static MESHX_MODEL temp_model;
    if(meshx_ven_root_model_arr == NULL && meshx_ven_root_model_arr_len)
    {
        meshx_ven_root_model_arr = (MESHX_MODEL *) MESHX_MALLOC(sizeof(MESHX_MODEL) * meshx_ven_root_model_arr_len);
        if(meshx_ven_root_model_arr == NULL)
        {
            MESHX_LOGE(MODULE_ID_COMMON, "Failed to allocate memory for root models");
            return NULL;
        }
        memset(meshx_ven_root_model_arr, 0, sizeof(MESHX_MODEL) * meshx_ven_root_model_arr_len);

        for(uint16_t i = 0; i < meshx_ven_root_model_arr_len; i++)
        {
            if(meshx_ven_root_model_getfn[i] == NULL)
            {
                continue;
            }
            meshx_ven_root_model_getfn[i]((void**)&temp_model);
            memcpy(&meshx_ven_root_model_arr[i], &temp_model, sizeof(MESHX_MODEL));
        }
    }
    return meshx_ven_root_model_arr;
}
/**
 * @brief Returns the count of the root models.
 *
 * @return Size of the root models.
 */
size_t get_root_sig_models_count(void)
{
    return meshx_sig_root_model_arr_len;
}

/**
 * @brief Returns the count of the vendor root models.
 *
 * @return Size of the vendor root models.
 */
size_t get_root_ven_models_count(void)
{
    return meshx_ven_root_model_arr_len;
}

/**
 * @brief Creates the BLE Mesh element composition.
 *
 * Initializes the BLE Mesh elements for different configurations like relay servers,
 * relay clients, and CWWW (Cool White and Warm White) servers.
 *
 * @param[in] p_dev Pointer to the device structure containing element information.
 * @param[in] config Pointer to the meshx configuration.
 *
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
meshx_err_t meshx_create_element_composition(dev_struct_t *p_dev, meshx_config_t const *config)
{
#if CONFIG_MAX_ELEMENT_COUNT > 0
    meshx_err_t err;
#if CONFIG_SECTION_ENABLE_ELEMENT_TABLE
    element_comp_table_t *element_comp_table;
#endif /* CONFIG_SECTION_ENABLE_ELEMENT_TABLE */
    if(!p_dev || !config || !config->element_comp_arr_len || !config->element_comp_arr)
        return MESHX_INVALID_ARG;

    err = control_task_msg_subscribe(
            CONTROL_TASK_MSG_CODE_PROVISION,
            CONTROL_TASK_PROV_EVT_MASK,
            &meshx_prov_control_task_handler);
    MESHX_ERR_PRINT_RET("Failed to register control task callback", err);

    err = meshx_init_config_server();
    MESHX_ERR_PRINT_RET("Failed to initialize config server", err);
    for(uint16_t element_id = 0; element_id < config->element_comp_arr_len; element_id++)
    {
#if !CONFIG_SECTION_ENABLE_ELEMENT_TABLE
        if(config->element_comp_arr[element_id].element_cnt != 0
        && element_comp_fn[config->element_comp_arr[element_id].type] != NULL)
        {
            err = element_comp_fn[config->element_comp_arr[element_id].type](p_dev, config->element_comp_arr[element_id].element_cnt);
            if(err)
            {
                MESHX_LOGE(MODULE_ID_COMMON, "Element composition failed: (%d)", err);
                return err;
            }
        }
#else
        element_comp_table = &MESHX_ELEMENT_COMP_TABLE_START;
        while(element_comp_table < &MESHX_ELEMENT_COMP_TABLE_STOP)
        {
            if(element_comp_table->idx == element_id && config->element_comp_arr[element_id].element_cnt != 0)
            {
                err = element_comp_table->element_comp_fn(p_dev, config->element_comp_arr[element_id].element_cnt);
                if(err)
                {
                    MESHX_LOGE(MODULE_ID_COMMON, "Element composition failed: (%d)", err);
                    return err;
                }
            }
            element_comp_table++;
        }
#endif /* CONFIG_SECTION_ENABLE_ELEMENT_TABLE */
    }
#endif /* CONFIG_MAX_ELEMENT_COUNT */
    return err;
}
