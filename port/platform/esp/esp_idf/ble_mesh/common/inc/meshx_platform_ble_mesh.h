/**
 * Copyright (c) 2024 - 2025 MeshX
 *
 * @file meshx_platform_ble_mesh.h
 * @brief Header file for MeshX BLE Mesh platform abstraction layer.
 *        This file provides type definitions and macros to map MeshX
 *        BLE Mesh components to ESP-IDF BLE Mesh APIs.
 *
 * @author Pranjal Chanda
 *
 */

#ifndef __MESHX_BLE_MESH__
#define __MESHX_BLE_MESH__

#include "stdio.h"
#include "stdint.h"

#include "sdkconfig.h"
#include <meshx_err.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_local_data_operation_api.h"
#include "esp_ble_mesh_provisioning_api.h"
#include "esp_ble_mesh_config_model_api.h"
#pragma GCC diagnostic pop


/**
 * MESHX_COMMONS
 */
#define MESHX_COMPOSITION       esp_ble_mesh_comp_t
#define MESHX_ELEMENT           esp_ble_mesh_elem_t
#define MESHX_MODEL             esp_ble_mesh_model_t
#define MESHX_CLIENT            esp_ble_mesh_client_t
#define MESHX_MODEL_PUB         esp_ble_mesh_model_pub_t

#define MESHX_SET_ELEM_LOCATION(e, val)       memcpy((void *)&(e)->location, &(val), sizeof((e)->location))
#define MESHX_SET_ELEM_SIG_COUNT(e, val)      memcpy((void *)&(e)->sig_model_count, &(val), sizeof((e)->sig_model_count))
#define MESHX_SET_ELEM_VND_COUNT(e, val)      memcpy((void *)&(e)->vnd_model_count, &(val), sizeof((e)->vnd_model_count))
#define MESHX_SET_ELEM_SIG_MODELS(e, val)     memcpy((void *)&(e)->sig_models, &(val), sizeof((e)->sig_models))
#define MESHX_SET_ELEM_VND_MODELS(e, val)     memcpy((void *)&(e)->vnd_models, &(val), sizeof((e)->vnd_models))

#define MESHX_SET_COMP_ELEM_COUNT(comp, val)  ((comp)->element_count = (val))
#define MESHX_SET_COMP_ELEMS(comp, val)       ((comp)->elements = (val))
#define MESHX_MODEL_GET_SIG_ID(m)             ((m)->model_id)
#define MESHX_MODEL_GET_VND_ID(m)             ((m)->vnd.model_id)
#define MESHX_MODEL_GET_VND_CID(m)            ((m)->vnd.company_id)
#define MESHX_INIT_ELEMENT(e)                 do {} while(0)

#define MESHX_INIT_MODEL_KEYS_GROUPS(p_baked) \
    do { \
        for (size_t k = 0; k < sizeof((p_baked)->keys) / sizeof((p_baked)->keys[0]); k++) { \
            (p_baked)->keys[k] = MESHX_KEY_UNUSED; \
        } \
        for (size_t g = 0; g < sizeof((p_baked)->groups) / sizeof((p_baked)->groups[0]); g++) { \
            (p_baked)->groups[g] = MESHX_ADDR_UNASSIGNED; \
        } \
    } while(0)

#define MESHX_SET_MODEL_VND_ID(m, vnd_id_32) \
    do { \
        uint32_t _v = (vnd_id_32); \
        memcpy((void *)&(m)->model_id, &_v, sizeof(uint32_t)); \
    } while(0)

/**
 * MESHX_SERVER_MODELS
 */
#define MESHX_PROV              esp_ble_mesh_prov_t
#define MESHX_PROV_CB           esp_ble_mesh_prov_cb_t
#define MESHX_PROV_CB_EVT       esp_ble_mesh_prov_cb_event_t
#define MESHX_PROV_CB_PARAM     esp_ble_mesh_prov_cb_param_t
#define MESHX_CFG_SRV           esp_ble_mesh_cfg_srv_t

#define MESHX_VND_SRV_CB        esp_ble_mesh_model_cb_t
#define MESHX_VND_SRV_CB_EVT    esp_ble_mesh_model_cb_event_t
#define MESHX_VND_SRV_CB_PARAM  esp_ble_mesh_model_cb_param_t

/**
 * @brief Initialize the vendor server model platform.
 */
meshx_err_t meshx_plat_ven_srv_init(void);

/**
 * @brief Bind the platform-specific UVP opcodes to a model instance.
 * @param p_model Pointer to the platform model structure.
 * @return MESHX_SUCCESS on success, or an error code.
 */
meshx_err_t meshx_plat_bind_uvp_opcodes(void *p_model);

/**
 * @brief Add a group subscription address to a platform model.
 *
 * @param[in] p_model   Pointer to the platform model instance.
 * @param[in] sub_addr  Group address to subscribe to (0xC000 - 0xC3FF).
 *
 * @return MESHX_SUCCESS on success, MESHX_ERR_NO_MEM if table full, or error code.
 */
meshx_err_t meshx_plat_model_sub_add(void *p_model, uint16_t sub_addr);

/**
 * @brief Delete a group subscription address from a platform model.
 *
 * @param[in] p_model   Pointer to the platform model instance.
 * @param[in] sub_addr  Group address to remove.
 *
 * @return MESHX_SUCCESS on success, MESHX_ERR_NOT_FOUND if not present, or error code.
 */
meshx_err_t meshx_plat_model_sub_del(void *p_model, uint16_t sub_addr);

#endif /* __MESHX_BLE_MESH__ */

