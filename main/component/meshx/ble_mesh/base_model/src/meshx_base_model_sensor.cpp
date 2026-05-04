/**
 * @file meshx_base_model_sensor.cpp
 * @brief Implementation of the MeshX Sensor Server base model.
 */

#include "meshx_base_model_sensor.hpp"

#if CONFIG_ENABLE_SENSOR_SERVER

/**
 * @brief Initialization function for the Sensor Server model.
 */
meshx_err_t meshXBaseSensorServerModel::plat_model_init(void)
{
    return MESHX_SUCCESS;
}

/**
 * @brief Validate a sensor server status opcode.
 */
meshx_err_t meshXBaseSensorServerModel::validate_server_status_opcode(uint16_t opcode)
{
    switch(opcode)
    {
        case MESHX_MODEL_OP_SENSOR_STATUS:
        case MESHX_MODEL_OP_SENSOR_COLUMN_STATUS:
        case MESHX_MODEL_OP_SENSOR_SERIES_STATUS:
            return MESHX_SUCCESS;
        default:
            return MESHX_FAIL;
    }
}

/**
 * @brief Sends a status message for the Sensor Server model.
 */
meshx_err_t meshXBaseSensorServerModel::plat_send_msg(meshx_sensor_server_send_params_t *params)
{
    if (!params || !params->model || !params->ctx)
    {
        return MESHX_INVALID_ARG;
    }

    return meshx_plat_sensor_srv_send_status(
        (meshx_model_t*)params->model,
        (meshx_ctx_t*)params->ctx,
        &params->state
    );
}

/**
 * @brief Restores the state of the Sensor Server model.
 */
meshx_err_t meshXBaseSensorServerModel::server_state_restore(meshx_sensor_server_restore_params_t *param)
{
    return MESHX_SUCCESS;
}

meshXBaseSensorServerModel::meshXBaseSensorServerModel(uint32_t model_id, const control_msg_cb& from_ble_cb)
    : meshXBaseServerModel<meshXBaseSensorServerModel, meshx_sensor_server_send_params_t, meshx_sensor_server_restore_params_t>(model_id, from_ble_cb)
{
    set_status(meshXBaseSensorServerModel::plat_model_init());
    if (get_status() != MESHX_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "plat_model_init failed");
        return;
    }
}

#endif /* CONFIG_ENABLE_SENSOR_SERVER */
