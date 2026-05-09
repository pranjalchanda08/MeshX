/**
 * @file meshx_sensor_element.cpp
 * @brief Implementation of the MeshX Sensor Server element.
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#include <variants/meshx_sensor_element.hpp>
#include <meshx_element_factory.hpp>
#include <meshx_element_registry.hpp>
#include <sensor_model/meshx_model_sensor.hpp>
#include <cstring>

#if CONFIG_ENABLE_SENSOR_SERVER

#define SENSOR_SRV_TO_BLE_EVT_MASK   CONTROL_TASK_MSG_EVT_TO_BLE_SENSOR_SRV

/**
 * @brief Constructs a new meshXSensorElement instance.
 */
meshXSensorElement::meshXSensorElement(uint16_t element_id)
    : meshXElementServer(element_id)
{
    memset(&element_ctx, 0, sizeof(element_ctx));
    this->register_element_ctx(&element_ctx, sizeof(element_ctx));
    this->set_element_variant(MESHX_ELEMENT_TYPE_SENSOR_SERVER);

    /* Subscribe to BLE events for this element type */
    control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_TO_BLE,
        SENSOR_SRV_TO_BLE_EVT_MASK,
        (control_task_msg_handle_t)&meshXSensorElement::s_to_ble_cb);
}

/**
 * @brief Lists and initializes SIG models for Sensor Server Element
 */
uint8_t meshXSensorElement::list_sig_models()
{
    auto sensor_srv = std::make_unique<meshXSensorServerModel>(
        this, &element_ctx.sensor_srv_state,
        (uint16_t)static_cast<int>(
            meshXSensorElementComposition::MESHX_SENSOR_ELEMENT_COMP_SENSOR_SERVER));
    this->get_sig_models().push_back(std::move(sensor_srv));

    return (uint8_t)this->get_sig_models().size();
}

/**
 * @brief Lists Vendor models for Sensor Server Element (none required)
 */
uint8_t meshXSensorElement::list_ven_models()
{
    return 0;
}

/**
 * @brief Notify element of state change from child model.
 * @details Overrides base class stub. Updates element_ctx, saves to NVS,
 *          and dispatches app notification via meshx_send_msg_to_app().
 */
meshx_err_t meshXSensorElement::element_state_change_notify(meshx_ptr_t param, size_t param_size)
{
    if (!param) return MESHX_INVALID_ARG;

    auto *msg = static_cast<meshx_sensor_srv_el_msg_t *>(param);
    uint16_t el_id  = msg->header.model.el_id;
    uint16_t mid    = msg->header.model.model_id;
    uint16_t func_id = 0;
    meshx_api_sensor_server_evt_t app_evt = {};

    if (mid == MESHX_MODEL_ID_SENSOR_SRV)
    {
        /* Update local context */
        element_ctx.sensor_srv_state = msg->state.sensor_status;

        /* Task B — NVS persistence */
        meshx_nvs_element_ctx_set(el_id, get_element_variant(), &element_ctx, sizeof(element_ctx));

        /* Prepare App notification */
        app_evt.state_change.sensor_status.property_id = msg->state.sensor_status.property_id;
        app_evt.state_change.sensor_status.data_len    = msg->state.sensor_status.data_len;
        memcpy(app_evt.state_change.sensor_status.data, msg->state.sensor_status.data,
               std::min((uint16_t)sizeof(app_evt.state_change.sensor_status.data), msg->state.sensor_status.data_len));

        func_id = MESHX_ELEMENT_FUNC_ID_SENSOR_SERVER_DATA;
    }
    else
    {
        return MESHX_INVALID_ARG;
    }

    return meshx_send_msg_to_app(
        el_id,
        MESHX_ELEMENT_TYPE_SENSOR_SERVER,
        func_id,
        sizeof(app_evt),
        &app_evt);
}

/* Task E — TO_BLE server handler: receives app command, sends Sensor Status via model */
meshx_err_t meshXSensorElement::s_to_ble_cb(
    const dev_struct_t      *pdev,
    control_task_msg_evt_t   evt,
    const void              *params)
{
    if (!pdev || !params) return MESHX_INVALID_ARG;
    if (evt != SENSOR_SRV_TO_BLE_EVT_MASK) return MESHX_SUCCESS;

    auto* app_msg = static_cast<const meshx_app_element_msg_header_t*>(params);
    auto* app_payload = reinterpret_cast<const meshx_data_payload_t*>(app_msg + 1);

    /* Look up the instance from the registry using the element_id */
    auto* el = meshXElementRegistry::get_instance().find_and_cast<meshXSensorElement>(
        app_msg->element_id, MESHX_ELEMENT_TYPE_SENSOR_SERVER);

    if (!el) return MESHX_NOT_FOUND;

    if (app_msg->func_id == MESHX_ELEMENT_FUNC_ID_SENSOR_SERVER_DATA)
    {
        auto &models = el->get_sig_models();
        if (models.empty()) return MESHX_INVALID_STATE;

        auto *sensor_srv = static_cast<meshXSensorServerModel *>(models[0].get());

        meshx_model_t model_ref = {
            .el_id    = app_msg->element_id,
            .model_id = MESHX_MODEL_ID_SENSOR_SRV,
            .pub_addr = el->element_ctx.pub_addr,
            .p_model  = nullptr
        };

        meshx_ctx_t ctx = {
            .app_idx  = el->element_ctx.app_id,
            .net_idx  = pdev->meshx_store.net_key_id,
            .opcode   = MESHX_MODEL_OP_SENSOR_STATUS,
            .src_addr = 0,
            .dst_addr = el->element_ctx.pub_addr,
            .p_ctx    = nullptr
        };

        meshx_sensor_server_send_params_t send_params = {
            .model = &model_ref,
            .ctx   = &ctx,
            .state = {
                .sensor_status = {
                    .property_id = app_payload->sensor_server_evt.state_change.sensor_status.property_id,
                    .data_len    = app_payload->sensor_server_evt.state_change.sensor_status.data_len,
                    .data        = {0}
                }
            }
        };
        memcpy(send_params.state.sensor_status.data, app_payload->sensor_server_evt.state_change.sensor_status.data,
               std::min((uint16_t)sizeof(send_params.state.sensor_status.data), app_payload->sensor_server_evt.state_change.sensor_status.data_len));

        return sensor_srv->model_send(&send_params);
    }

    return MESHX_SUCCESS;
}

void meshXSensorElement::sync(control_task_msg_evt_t evt)
{
    if (evt == CONTROL_TASK_MSG_EVT_SYSTEM_STACK_READY)
    {
        if (!meshx_prov_srv_is_provisioned()) return;
        if (element_ctx.pub_addr == MESHX_ADDR_UNASSIGNED) return;

        auto &models = this->get_sig_models();
        auto *sensor = static_cast<meshXSensorServerModel *>(models[0].get());

        sensor->request_status();
    }
}

void meshXSensorElement::handle_config(control_task_msg_evt_t evt, const meshx_config_srv_cb_param_t *params)
{
    MESHX_LOGD(MODULE_ID_ELEMENT_SENSOR_SERVER, "Sensor Server [%d] Config Evt: %d", get_element_idx(), evt);
}

#endif /* CONFIG_ENABLE_SENSOR_SERVER */
