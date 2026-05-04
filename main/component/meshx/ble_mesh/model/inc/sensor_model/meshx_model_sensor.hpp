/**
 * @file meshx_model_sensor.hpp
 *
 * @brief Header file for MeshX Sensor Models
 * This file contains the declarations and definitions for the MeshX Sensor Models,
 * including the Sensor Server and Client models.
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#ifndef _MESHX_MODEL_SENSOR_HPP_
#define _MESHX_MODEL_SENSOR_HPP_

#include <meshx_model_class.hpp>
#include <meshx_base_model_sensor.hpp>

#define MESHX_SENSOR_SERVER_MODEL_TEMPLATE_PROTO
#define MESHX_SENSOR_SERVER_MODEL_TEMPLATE_PARAMS

#if CONFIG_ENABLE_SENSOR_SERVER

/**
 * @brief Structure to hold the Sensor Server to element message.
 */
struct meshx_sensor_srv_el_msg
{
    meshx_srv_model_send_param_header_t header; /**< Server model send param header */
    meshx_sensor_server_state_change_t  state;  /**< The state of the message. */
};

using meshx_sensor_srv_el_msg_t = struct meshx_sensor_srv_el_msg;

/**
 * @class meshXSensorServerModel
 * @brief A template class for creating Sensor Server models.
 */
MESHX_SENSOR_SERVER_MODEL_TEMPLATE_PROTO
class meshXSensorServerModel MESHX_SENSOR_SERVER_MODEL_TEMPLATE_PARAMS
    : public meshXServerModel<meshXBaseSensorServerModel, meshx_sensor_server_send_params_t>
{
private:
    /* New or updated model state from BLE layer */
    meshx_sensor_server_state_change_t model_state;

    meshx_err_t plat_model_create   (MESHX_MODEL* p_plat_model_ptr = nullptr) override;
    meshx_err_t plat_model_delete   (void) override;
    meshx_err_t element_state_change_handle (void) override;
    meshx_err_t prepare_element_msg (meshx_ptr_t *msg_ptr, size_t *msg_size) override;

public:
    meshx_err_t model_send          (meshx_sensor_server_send_params_t *params) override;
    meshx_err_t model_from_ble_cb   (dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;

    meshXSensorServerModel(
        meshXElementIF *parent_element = nullptr,
        meshx_ptr_t     parent_element_state = nullptr,
        uint16_t        model_func_id = 0
    );
    ~meshXSensorServerModel() override = default;

    meshx_sensor_server_state_change_t* get_state_ptr() { return &model_state; }

private:
    meshx_sensor_srv_el_msg_t element_msg;
};

#endif /* CONFIG_ENABLE_SENSOR_SERVER */

#endif /* _MESHX_MODEL_SENSOR_HPP_ */
