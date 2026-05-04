/**
 * @file meshx_model_sensor.cpp
 * @brief Implementation of MeshX Sensor Model classes.
 *
 * @author Pranjal Chanda
 * @date 2026
 * @copyright Copyright 2026 - 2025 MeshX
 */

#include "sensor_model/meshx_model_sensor.hpp"
#include "interface/ble_mesh/server/meshx_ble_mesh_sensor_srv.h"

#if CONFIG_ENABLE_SENSOR_SERVER

/**
 * @brief Create platform-specific sensor model instance.CONFIG_ENABLE_SENSOR_SERVER
 */
MESHX_SENSOR_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXSensorServerModel MESHX_SENSOR_SERVER_MODEL_TEMPLATE_PARAMS
    :: plat_model_create(MESHX_MODEL* p_plat_model_ptr)
{
    meshx_ptr_t p_pub = nullptr;
    meshx_ptr_t p_srv = nullptr;

    if (p_plat_model_ptr) {
        this->set_plat_model(p_plat_model_ptr);
    }

    meshx_err_t err = meshx_plat_sensor_srv_create((meshx_model_t*)this->get_plat_model(), &p_pub, &p_srv);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to create Sensor Server Model");
        return err;
    }

    this->set_pub_struct(p_pub);
    return MESHX_SUCCESS;
}

/**
 * @brief Delete platform-specific sensor model instance.
 */
MESHX_SENSOR_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXSensorServerModel MESHX_SENSOR_SERVER_MODEL_TEMPLATE_PARAMS
    :: plat_model_delete(void)
{
    meshx_ptr_t p_pub = this->get_pub_struct();

    meshx_err_t err = meshx_plat_sensor_srv_delete(&p_pub);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to delete Sensor Server Model");
    }
    else
    {
        this->set_pub_struct(nullptr);
    }

    return err;
}

/**
 * @brief Send a packet to the MeshX stack based on the given parameters.
 */
MESHX_SENSOR_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXSensorServerModel MESHX_SENSOR_SERVER_MODEL_TEMPLATE_PARAMS
    :: model_send(meshx_sensor_server_send_params_t *params)
{
    if (!params || !params->model || !params->ctx)
    {
        return MESHX_INVALID_ARG;
    }
    params->ctx->opcode = MESHX_MODEL_OP_SENSOR_STATUS;

    return this->get_base_model()->plat_send_msg(params);
}

/**
 * @brief Callback for the Sensor Server model.
 */
MESHX_SENSOR_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXSensorServerModel MESHX_SENSOR_SERVER_MODEL_TEMPLATE_PARAMS
    :: model_from_ble_cb(dev_struct_t *p_dev, control_task_msg_evt_t evt, meshx_ptr_t p_params)
{
    if(!p_params || !p_dev)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid parameters");
        return MESHX_INVALID_ARG;
    }
    if (evt != CONTROL_TASK_MSG_EVT_TO_BLE_SENSOR_SRV &&
        evt != MESHX_MODEL_ID_SENSOR_SRV)
    {
        /* Callback triggered not for this model */
        return MESHX_SUCCESS;
    }

    auto *param = static_cast<meshx_sensor_server_cb_param_t *>(p_params);

    MESHX_LOGD(MODULE_ID_MODEL_SERVER, "op|src|dst:%04" PRIx32 "|%04x|%04x",
               param->ctx.opcode, param->ctx.src_addr, param->ctx.dst_addr);

    /* Update element message */
    element_msg = {
        .header = {
            .model = param->model,
            .element_state_change = MESHX_SUCCESS,
        },
        .state = param->state
    };

    if (this->get_parent_element())
    {
        return this->get_parent_element()->on_model_cb(&element_msg, sizeof(element_msg));
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Prepare element message for sensor server.
 */
MESHX_SENSOR_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXSensorServerModel MESHX_SENSOR_SERVER_MODEL_TEMPLATE_PARAMS
    :: prepare_element_msg(meshx_ptr_t *msg_ptr, size_t *msg_size)
{
    if (!msg_ptr || !msg_size) return MESHX_INVALID_ARG;

    *msg_ptr = &element_msg;
    *msg_size = sizeof(element_msg);

    return MESHX_SUCCESS;
}

/**
 * @brief Handle state change request from element.
 */
MESHX_SENSOR_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXSensorServerModel MESHX_SENSOR_SERVER_MODEL_TEMPLATE_PARAMS
    :: element_state_change_handle(void)
{
    // For sensor server, state change from element might mean "new data available to publish"
    return MESHX_SUCCESS;
}

/**
 * @brief Constructor for Sensor Server Model.
 */
MESHX_SENSOR_SERVER_MODEL_TEMPLATE_PROTO
meshXSensorServerModel MESHX_SENSOR_SERVER_MODEL_TEMPLATE_PARAMS
    ::meshXSensorServerModel(
        meshXElementIF *parent_element,
        meshx_ptr_t     parent_element_state,
        uint16_t        model_func_id
    )
    : meshXServerModel(nullptr, MESHX_MODEL_ID_SENSOR_SRV, parent_element, parent_element_state, model_func_id)
{
}

#endif /* CONFIG_ENABLE_SENSOR_SERVER */
