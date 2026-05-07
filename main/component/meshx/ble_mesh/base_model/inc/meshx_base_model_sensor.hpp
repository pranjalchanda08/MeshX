/**
 * @file meshx_base_model_sensor.hpp
 * @brief Header file for MeshX Sensor Server model declarations.
 */

#ifndef _MESHX_BASE_MODEL_SENSOR_H_
#define _MESHX_BASE_MODEL_SENSOR_H_

#include <meshx_base_model_class.hpp>

#define MESHX_BASE_SENSOR_SERVER_TEMPLATE_PROTO
#define MESHX_BASE_SENSOR_SERVER_TEMPLATE_PARAMS

#if CONFIG_ENABLE_SENSOR_SERVER

/* Restore params for sensor server state restore */
using meshx_sensor_server_restore_params_t = struct meshx_sensor_server_restore_params
{
    meshx_model_t *p_model;                     /**< Pointer to the server model. */
    meshx_sensor_server_state_change_t state_change; /**< State change information. */
};

/* Send params for sensor server */
using meshx_sensor_server_send_params_t = struct meshx_sensor_server_send_params
{
    meshx_model_t *model;
    meshx_ctx_t *ctx;
    meshx_sensor_server_state_change_t state;
};


MESHX_BASE_SENSOR_SERVER_TEMPLATE_PROTO
class meshXBaseSensorServerModel : public meshXBaseServerModel<meshXBaseSensorServerModel, meshx_sensor_server_send_params_t, meshx_sensor_server_restore_params_t, meshx_sensor_server_cb_param_t>
{
public:
    meshx_err_t plat_model_init(void) override;
    meshx_err_t validate_server_status_opcode(uint16_t opcode) override;
public:
    meshx_err_t server_state_restore(meshx_sensor_server_restore_params_t* param) override;
    meshx_err_t plat_send_msg(meshx_sensor_server_send_params_t *params) override;
    meshXBaseSensorServerModel(uint32_t model_id, meshx_ptr_t p_plat_model, const control_msg_cb& from_ble_cb);

    meshXBaseSensorServerModel() = delete;
    ~meshXBaseSensorServerModel() override = default;
};
#endif /* CONFIG_ENABLE_SENSOR_SERVER */

#endif /* _MESHX_BASE_MODEL_SENSOR_H_ */
