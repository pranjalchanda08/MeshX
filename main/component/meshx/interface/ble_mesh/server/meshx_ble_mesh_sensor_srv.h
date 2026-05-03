/**
 * Copyright (c) 2024 - 2025 MeshX
 *
 * @file meshx_ble_mesh_sensor_srv.h
 * @brief Header file for the MeshX BLE Mesh Sensor Server module.
 * @note This is the public interface for the Sensor Server module.
 * @author Pranjal Chanda
 * @date 2026
 * @copyright Copyright 2026 - 2025 MeshX
 */

#ifndef __MESHX_BLE_MESH_SENSOR_SRV_H__
#define __MESHX_BLE_MESH_SENSOR_SRV_H__

#include "../meshx_ble_mesh_cmn.h"
#include "meshx_control_task.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MESHX_BLE_MESH_SENSOR_DATA_MAX_LEN 128

typedef struct
{
    uint16_t property_id;
    uint16_t data_len;
    uint8_t data[MESHX_BLE_MESH_SENSOR_DATA_MAX_LEN];
} meshx_sensor_srv_status_t;

/** Parameter of Sensor Status state change event */
typedef struct
{
    meshx_sensor_srv_status_t sensor_status;
} meshx_sensor_server_state_change_t;

/**
 * @brief Sensor Server Model callback parameters
 */
typedef struct {
    meshx_model_t model;
    meshx_ctx_t ctx;
    union {
        meshx_sensor_server_state_change_t state;
        meshx_sensor_srv_status_t sensor_status;
    };
} meshx_sensor_server_cb_param_t;

/**
 * @brief Initialize Sensor Server module.
 */
meshx_err_t meshx_plat_sensor_srv_init(void);

/**
 * @brief Create a Sensor Server instance.
 */
meshx_err_t meshx_plat_sensor_srv_create(meshx_ptr_t p_model, meshx_ptr_t *p_pub, meshx_ptr_t *p_sensor_srv);

/**
 * @brief Delete a Sensor Server instance.
 */
meshx_err_t meshx_plat_sensor_srv_delete(meshx_ptr_t *p_pub);

/**
 * @brief Send a status message from the Sensor Server.
 */
meshx_err_t meshx_plat_sensor_srv_send_status(const meshx_model_t *p_model,
                                              const meshx_ctx_t *p_ctx,
                                              const meshx_sensor_server_state_change_t *state_change);

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_BLE_MESH_SENSOR_SRV_H__ */
