/**
 * Copyright (c) 2024 - 2025 MeshX
 *
 * @file meshx_ble_mesh_config_srv.h
 * @brief Header file for MeshX BLE Mesh Configuration Server.
 *
 * This file contains the definitions, data structures, and function declarations
 * for the MeshX BLE Mesh Configuration Server. It provides the necessary APIs
 * and context for managing the configuration server model in a BLE Mesh network.
 *
 * @author Pranjal Chanda
 */

#ifndef __MESHX_BLE_MESH_CONFIG_SRV_H__
#define __MESHX_BLE_MESH_CONFIG_SRV_H__

#include "../meshx_ble_mesh_cmn.h"
#include "meshx_control_task.h"

typedef control_task_msg_handle_t config_srv_cb_t;
typedef control_task_msg_evt_config_t config_evt_t;

/**
 * @brief Configuration Server model related context.
 */

/** Parameters of Config Model Publication Set */
typedef struct {
    uint16_t element_addr;      /*!< Element Address */
    uint16_t pub_addr;          /*!< Publish Address */
    uint16_t app_idx;           /*!< AppKey Index */
    bool     cred_flag;         /*!< Friendship Credential Flag */
    uint8_t  pub_ttl;           /*!< Publish TTL */
    uint8_t  pub_period;        /*!< Publish Period */
    uint8_t  pub_retransmit;    /*!< Publish Retransmit */
    uint16_t company_id;        /*!< Company ID */
    uint16_t model_id;          /*!< Model ID */
} meshx_state_change_cfg_mod_pub_set_t;

/** Parameters of Config Model Publication Virtual Address Set */
typedef struct {
    uint16_t element_addr;      /*!< Element Address */
    uint8_t  label_uuid[16];    /*!< Label UUID */
    uint16_t app_idx;           /*!< AppKey Index */
    bool     cred_flag;         /*!< Friendship Credential Flag */
    uint8_t  pub_ttl;           /*!< Publish TTL */
    uint8_t  pub_period;        /*!< Publish Period */
    uint8_t  pub_retransmit;    /*!< Publish Retransmit */
    uint16_t company_id;        /*!< Company ID */
    uint16_t model_id;          /*!< Model ID */
} meshx_state_change_cfg_mod_pub_va_set_t;

/** Parameters of Config Model Subscription Add */
typedef struct {
    uint16_t element_addr;      /*!< Element Address */
    uint16_t sub_addr;          /*!< Subscription Address */
    uint16_t company_id;        /*!< Company ID */
    uint16_t model_id;          /*!< Model ID */
} meshx_state_change_cfg_model_sub_add_t;

/** Parameters of Config Model Subscription Delete */
typedef struct {
    uint16_t element_addr;      /*!< Element Address */
    uint16_t sub_addr;          /*!< Subscription Address */
    uint16_t company_id;        /*!< Company ID */
    uint16_t model_id;          /*!< Model ID */
} meshx_state_change_cfg_model_sub_delete_t;

/** Parameters of Config NetKey Add */
typedef struct {
    uint16_t net_idx;           /*!< NetKey Index */
    uint8_t  net_key[16];       /*!< NetKey */
} meshx_state_change_cfg_netkey_add_t;

/** Parameters of Config NetKey Update */
typedef struct {
    uint16_t net_idx;           /*!< NetKey Index */
    uint8_t  net_key[16];       /*!< NetKey */
} meshx_state_change_cfg_netkey_update_t;

/** Parameter of Config NetKey Delete */
typedef struct {
    uint16_t net_idx;           /*!< NetKey Index */
} meshx_state_change_cfg_netkey_delete_t;

/** Parameters of Config AppKey Add */
typedef struct {
    uint16_t net_idx;           /*!< NetKey Index */
    uint16_t app_idx;           /*!< AppKey Index */
    uint8_t  app_key[16];       /*!< AppKey */
} meshx_state_change_cfg_appkey_add_t;

/** Parameters of Config AppKey Update */
typedef struct {
    uint16_t net_idx;           /*!< NetKey Index */
    uint16_t app_idx;           /*!< AppKey Index */
    uint8_t  app_key[16];       /*!< AppKey */
} meshx_state_change_cfg_appkey_update_t;

/** Parameters of Config AppKey Delete */
typedef struct {
    uint16_t net_idx;           /*!< NetKey Index */
    uint16_t app_idx;           /*!< AppKey Index */
} meshx_state_change_cfg_appkey_delete_t;

/** Parameters of Config Model App Bind */
typedef struct {
    uint16_t element_addr;      /*!< Element Address */
    uint16_t app_idx;           /*!< AppKey Index */
    uint16_t company_id;        /*!< Company ID */
    uint16_t model_id;          /*!< Model ID */
} meshx_state_change_cfg_model_app_bind_t;

/** Parameters of Config Model App Unbind */
typedef struct {
    uint16_t element_addr;      /*!< Element Address */
    uint16_t app_idx;           /*!< AppKey Index */
    uint16_t company_id;        /*!< Company ID */
    uint16_t model_id;          /*!< Model ID */
} meshx_state_change_cfg_model_app_unbind_t;

/** Parameters of Config Key Refresh Phase Set */
typedef struct {
    uint16_t net_idx;           /*!< NetKey Index */
    uint8_t  kr_phase;          /*!< New Key Refresh Phase Transition */
} meshx_state_change_cfg_kr_phase_set_t;

/**
 * @brief Configuration Server model state change value union
 */
typedef union {
    /**
     * The recv_op in ctx can be used to decide which state is changed.
     */
    meshx_state_change_cfg_mod_pub_set_t         mod_pub_set;        /*!< Config Model Publication Set */
    meshx_state_change_cfg_mod_pub_va_set_t      mod_pub_va_set;     /*!< Config Model Publication Virtual Address Set */
    meshx_state_change_cfg_model_sub_add_t       mod_sub_add;        /*!< Config Model Subscription Add */
    meshx_state_change_cfg_model_sub_delete_t    mod_sub_delete;     /*!< Config Model Subscription Delete */
    meshx_state_change_cfg_netkey_add_t          netkey_add;         /*!< Config NetKey Add */
    meshx_state_change_cfg_netkey_update_t       netkey_update;      /*!< Config NetKey Update */
    meshx_state_change_cfg_netkey_delete_t       netkey_delete;      /*!< Config NetKey Delete */
    meshx_state_change_cfg_appkey_add_t          appkey_add;         /*!< Config AppKey Add */
    meshx_state_change_cfg_appkey_update_t       appkey_update;      /*!< Config AppKey Update */
    meshx_state_change_cfg_appkey_delete_t       appkey_delete;      /*!< Config AppKey Delete */
    meshx_state_change_cfg_model_app_bind_t      mod_app_bind;       /*!< Config Model App Bind */
    meshx_state_change_cfg_model_app_unbind_t    mod_app_unbind;     /*!< Config Model App Unbind */
    meshx_state_change_cfg_kr_phase_set_t        kr_phase_set;       /*!< Config Key Refresh Phase Set */
} meshx_cfg_srv_state_change_t;

typedef struct meshx_config_srv_cb_param
{
    meshx_ctx_t ctx;
    meshx_model_t model;
    meshx_cfg_srv_state_change_t state_change;
} meshx_config_srv_cb_param_t;

/**
 * @brief Initializes the MeshX platform configuration server.
 *
 * This function sets up the necessary resources and configurations
 * for the MeshX BLE Mesh configuration server. It should be called
 * during the initialization phase of the application to ensure
 * proper operation of the MeshX BLE Mesh stack.
 *
 * @return
 *     - MESHX_SUCCESS on successful initialization.
 *     - Appropriate error code from meshx_err_t on failure.
 */
meshx_err_t meshx_plat_config_srv_init(void);

/**
 * @brief Retrieve the instance of the BLE Mesh configuration server.
 *
 * This function provides access to the BLE Mesh configuration server instance.
 *
 * @param[out] p_conf_srv Pointer to a variable where the configuration server instance will be stored.
 *                        The caller must ensure that the pointer is valid and non-NULL.
 *
 * @return
 *     - MESHX_SUCCESS on success.
 *     - Appropriate error code from `meshx_err_t` on failure.
 */
meshx_err_t meshx_plat_get_config_srv_instance(meshx_ptr_t* p_conf_srv);

/**
 * @brief Retrieves the configuration server model for the BLE Mesh.
 *
 * This function provides access to the configuration server model used in the BLE Mesh
 * implementation. The retrieved model can be used for configuring and managing the mesh network.
 *
 * @param[out] p_model Pointer to a variable where the address of the configuration server model
 *                      will be stored. The caller must ensure that the pointer is valid.
 *
 * @return
 *     - MESHX_SUCCESS on success.
 *     - Appropriate error code from `meshx_err_t` on failure.
 */
meshx_err_t meshx_plat_get_config_srv_model(meshx_ptr_t p_model);

#endif /* __MESHX_BLE_MESH_CONFIG_SRV_H__ */
