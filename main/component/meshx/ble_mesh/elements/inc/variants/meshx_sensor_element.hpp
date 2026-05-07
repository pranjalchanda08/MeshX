/**
 * @file meshx_sensor_element.hpp
 * @brief Header file for the MeshX Sensor Server element.
 */

#ifndef _MESHX_SENSOR_ELEMENT_HPP_
#define _MESHX_SENSOR_ELEMENT_HPP_

#include <meshx_element_class.hpp>
#include <mutex>

#if CONFIG_ENABLE_SENSOR_SERVER

enum class meshXSensorElementComposition : uint8_t
{
    MESHX_SENSOR_ELEMENT_COMP_SENSOR_SERVER = 0,
    MESHX_SENSOR_ELEMENT_COMP_MAX,
};

struct meshx_sensor_srv_el_ctx_t
{
    uint8_t                     app_id;             /**< Application key ID for publication */
    uint16_t                    pub_addr;           /**< Publication address */
    meshx_sensor_srv_status_t   sensor_srv_state;   /**< Current sensor server state */
};

class meshXSensorElement : public meshXElementServer
{
private:
    meshx_sensor_srv_el_ctx_t element_ctx;

    uint8_t list_sig_models (void) override;
    uint8_t list_ven_models (void) override;

    /**
     * @brief Notify element of state change from child model.
     */
    meshx_err_t element_state_change_notify(meshx_ptr_t param, size_t param_size) override;

    /**
     * @brief Synchronize element state (Status broadcast)
     */
    void sync(control_task_msg_evt_t evt) override;

    /**
     * @brief Handle configuration server events
     */
    void handle_config(control_task_msg_evt_t evt, const meshx_config_srv_cb_param_t *params) override;

    static meshx_err_t s_to_ble_cb(
        const dev_struct_t      *pdev,
        control_task_msg_evt_t   evt,
        const void              *params);

public:
    /**
     * @brief Constructs a new meshXSensorElement instance.
     */
    explicit meshXSensorElement(uint16_t element_id);
    ~meshXSensorElement() override = default;

    meshXSensorElement (void) = delete;
};

#endif /* CONFIG_ENABLE_SENSOR_SERVER */

#endif /* _MESHX_SENSOR_ELEMENT_HPP_ */
