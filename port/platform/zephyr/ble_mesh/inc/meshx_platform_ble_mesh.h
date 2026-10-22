/**
 * Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_platform_ble_mesh.h
 * @brief Header file for MeshX BLE Mesh platform abstraction layer for Zephyr RTOS.
 *        This file provides type definitions and macros to map MeshX
 *        BLE Mesh components to Zephyr Bluetooth Mesh APIs (subsys/bluetooth/mesh).
 *
 * @author Pranjal Chanda
 */

#ifndef __MESHX_PLATFORM_BLE_MESH_ZEPHYR_H__
#define __MESHX_PLATFORM_BLE_MESH_ZEPHYR_H__

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <meshx_err.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/mesh.h>

/**
 * MESHX_COMMONS mapped to Zephyr Bluetooth Mesh types
 */
#define MESHX_COMPOSITION       struct bt_mesh_comp
#define MESHX_ELEMENT           struct bt_mesh_elem
#define MESHX_MODEL             struct bt_mesh_model
#define MESHX_MODEL_PUB         struct bt_mesh_model_pub

#define MESHX_SET_ELEM_LOCATION(e, val)       memcpy((void *)&(e)->loc, &(val), sizeof((e)->loc))
#define MESHX_SET_ELEM_SIG_COUNT(e, val)      memcpy((void *)&(e)->model_count, &(val), sizeof((e)->model_count))
#define MESHX_SET_ELEM_VND_COUNT(e, val)      memcpy((void *)&(e)->vnd_model_count, &(val), sizeof((e)->vnd_model_count))
#define MESHX_SET_ELEM_SIG_MODELS(e, val)     memcpy((void *)&(e)->models, &(val), sizeof((e)->models))
#define MESHX_SET_ELEM_VND_MODELS(e, val)     memcpy((void *)&(e)->vnd_models, &(val), sizeof((e)->vnd_models))

#define MESHX_SET_COMP_ELEM_COUNT(comp, val)  ((comp)->elem_count = (val))
#define MESHX_SET_COMP_ELEMS(comp, val)       ((comp)->elem = (val))
#define MESHX_MODEL_GET_SIG_ID(m)             ((m)->id)
#define MESHX_MODEL_GET_VND_ID(m)             ((m)->vnd.id)
#define MESHX_MODEL_GET_VND_CID(m)            ((m)->vnd.company)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize runtime context, keys, and subscription groups for a baked Zephyr model.
 * @param p_model Pointer to struct bt_mesh_model.
 * @return meshx_err_t MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_plat_init_model_keys_groups(void *p_model);

/**
 * @brief Initialize runtime context for a baked Zephyr element.
 * @param p_elem Pointer to struct bt_mesh_elem.
 * @return meshx_err_t MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_plat_init_element(void *p_elem);

#ifdef __cplusplus
}
#endif

#define MESHX_INIT_MODEL_KEYS_GROUPS(p_baked) meshx_plat_init_model_keys_groups((void *)(p_baked))
#define MESHX_INIT_ELEMENT(e)                 meshx_plat_init_element((void *)(e))

#define MESHX_SET_MODEL_VND_ID(m, vnd_id_32) \
    do { \
        uint32_t _v = (vnd_id_32); \
        uint16_t _cid = (uint16_t)(_v & 0xFFFF); \
        uint16_t _mid = (uint16_t)((_v >> 16) & 0xFFFF); \
        memcpy((void *)&(m)->vnd.company, &_cid, sizeof(uint16_t)); \
        memcpy((void *)&(m)->vnd.id, &_mid, sizeof(uint16_t)); \
    } while(0)

/**
 * MESHX_SERVER_MODELS
 */
#define MESHX_PROV              struct bt_mesh_prov
#define MESHX_CFG_SRV           struct bt_mesh_cfg_srv

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Platform callback types for Zephyr models
 */
typedef int (*meshx_zephyr_model_op_handler_t)(const struct bt_mesh_model *model,
                                               struct bt_mesh_msg_ctx *ctx,
                                               struct net_buf_simple *buf);

/**
 * @brief Initialize the vendor server model platform for Zephyr.
 * @return meshx_err_t MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_plat_ven_srv_init(void);

/**
 * @brief Bind the platform-specific UVP opcodes to a Zephyr model instance.
 * @param p_model Pointer to the struct bt_mesh_model.
 * @return meshx_err_t MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_plat_bind_uvp_opcodes(void *p_model);

/**
 * @brief Add a group subscription address to a platform model.
 *
 * @param[in] p_model   Pointer to the platform model instance.
 * @param[in] sub_addr  Group address to subscribe to (0xC000 - 0xC3FF).
 * @return meshx_err_t MESHX_SUCCESS on success, MESHX_ERR_NO_MEM if full, or error code.
 */
meshx_err_t meshx_plat_model_sub_add(void *p_model, uint16_t sub_addr);

/**
 * @brief Delete a group subscription address from a platform model.
 *
 * @param[in] p_model   Pointer to the platform model instance.
 * @param[in] sub_addr  Group address to remove.
 * @return meshx_err_t MESHX_SUCCESS on success, MESHX_ERR_NOT_FOUND if not present, or error code.
 */
meshx_err_t meshx_plat_model_sub_del(void *p_model, uint16_t sub_addr);

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_PLATFORM_BLE_MESH_ZEPHYR_H__ */
