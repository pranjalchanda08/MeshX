/**
 * @file meshx_rgb_element.cpp
 * @brief Implementation of MeshX RGB (HSL) Element.
 */

#include <variants/meshx_rgb_element.hpp>
#include "meshx_element_factory.hpp"
#include "meshx_element_registry.hpp"
#include <light_model/meshx_model_hsl.hpp>
#include <light_model/meshx_model_ctl.hpp>

#ifdef __cplusplus
extern "C" {
#endif
#ifdef __cplusplus
}
#endif

/************************************************************************************
 * meshXRGBServerElement
 ************************************************************************************/

/************************************************************************************
 * meshXRGBServerElement
 ************************************************************************************/


meshXRGBServerElement::meshXRGBServerElement(uint16_t element_idx)
    : meshXElementServer(element_idx)
{
    set_element_variant(MESHX_ELEMENT_TYPE_LIGHT_HSL_SERVER);

    element_ctx.app_id = 0;
    element_ctx.pub_addr = MESHX_ADDR_UNASSIGNED;
    element_ctx.gen_on_off_state.on_off = 0;
    element_ctx.light_hsl_state.lightness = 0;
    element_ctx.light_hsl_state.hue = 0;
    element_ctx.light_hsl_state.saturation = 0;

    register_element_ctx(&element_ctx, sizeof(element_ctx));

    /* Add models:
       1. Generic OnOff Server (mapped to HSL OnOff)
       2. Light HSL Server
    */
#if CONFIG_ENABLE_GEN_ONOFF_SERVER
    auto onoff_srv = std::make_unique<meshXGenericOnOffServerModel>(
        this,
        &element_ctx.gen_on_off_state,
        (uint16_t)MESHX_ELEMENT_FUNC_ID_LIGHT_HSL_SERVER_ONN_OFF
    );
    this->get_sig_models().push_back(std::move(onoff_srv));
#endif
#if CONFIG_ENABLE_LIGHT_HSL_SERVER
    auto hsl_srv = std::make_unique<meshXLightHSLServerModel>(
        this,
        &element_ctx.light_hsl_state,
        (uint16_t)MESHX_ELEMENT_FUNC_ID_LIGHT_HSL_SERVER_HSL
    );
    this->get_sig_models().push_back(std::move(hsl_srv));
#endif

    set_no_of_sig_models(get_sig_model_count());
    set_no_of_ven_models(get_ven_model_count());

    sig_plat_model_array_allocate();
    ven_plat_model_array_allocate();
}

uint8_t meshXRGBServerElement::list_sig_models()
{
    return get_no_of_sig_models();
}

uint8_t meshXRGBServerElement::list_ven_models()
{
    return 0;
}

meshx_err_t meshXRGBServerElement::element_state_change_notify(meshx_ptr_t param, size_t param_size)
{
    MESHX_UNUSED(param_size);
    if (!param) return MESHX_INVALID_ARG;

    auto *header = static_cast<meshx_srv_model_send_param_header_t *>(param);
    uint16_t element_id = get_element_idx();
    uint32_t model_id = header->model.model_id;

    meshx_err_t err = MESHX_SUCCESS;

    if (model_id == 0x1000) /* Generic OnOff Server */
    {
#if CONFIG_ENABLE_GEN_ONOFF_SERVER
        auto *msg = static_cast<meshx_on_off_srv_el_msg_t *>(param);
        element_ctx.gen_on_off_state.on_off = msg->state.on_off;

        /* App notification */
        meshx_api_light_hsl_server_evt_t app_evt = {};
        app_evt.state_change.on_off.state = element_ctx.gen_on_off_state.on_off;

        err = meshx_send_msg_to_app(
            element_id,
            MESHX_ELEMENT_TYPE_LIGHT_HSL_SERVER,
            MESHX_ELEMENT_FUNC_ID_LIGHT_HSL_SERVER_ONN_OFF,
            sizeof(app_evt),
            &app_evt);
#endif
    }
    else if (model_id == 0x1307) /* Light HSL Server */
    {
        auto *msg = static_cast<meshx_light_hsl_srv_el_msg_t *>(param);
        element_ctx.light_hsl_state = msg->state;

        /* App notification */
        meshx_api_light_hsl_server_evt_t app_evt = {};
        app_evt.state_change.hsl.lightness = element_ctx.light_hsl_state.lightness;
        app_evt.state_change.hsl.hue = element_ctx.light_hsl_state.hue;
        app_evt.state_change.hsl.saturation = element_ctx.light_hsl_state.saturation;

        err = meshx_send_msg_to_app(
            element_id,
            MESHX_ELEMENT_TYPE_LIGHT_HSL_SERVER,
            MESHX_ELEMENT_FUNC_ID_LIGHT_HSL_SERVER_HSL,
            sizeof(app_evt),
            &app_evt);
    }

    /* NVS persistence */
    meshx_nvs_element_ctx_set(element_id, get_element_variant(), &element_ctx, sizeof(element_ctx));

    return err;
}

meshx_err_t meshXRGBServerElement::s_to_ble_cb(
    const dev_struct_t      *pdev,
    control_task_msg_evt_t   evt,
    const void              *params)
{
    MESHX_UNUSED(pdev);
    MESHX_UNUSED(evt);
    if (!params) return MESHX_INVALID_ARG;

    auto* app_msg = static_cast<const meshx_app_element_msg_header_t*>(params);
    auto* app_payload = reinterpret_cast<const meshx_data_payload_t*>(app_msg + 1);

    /* Look up the instance from the registry using the element_id */
    auto* el = meshXElementRegistry::get_instance().find_and_cast<meshXRGBServerElement>(
        app_msg->element_id, MESHX_ELEMENT_TYPE_LIGHT_HSL_SERVER);

    if (!el) return MESHX_NOT_FOUND;

    if (app_msg->func_id == MESHX_ELEMENT_FUNC_ID_LIGHT_HSL_SERVER_HSL)
    {
        el->element_ctx.light_hsl_state.hue = app_payload->light_hsl_server_evt.state_change.hsl.hue;
        el->element_ctx.light_hsl_state.saturation = app_payload->light_hsl_server_evt.state_change.hsl.saturation;
        el->element_ctx.light_hsl_state.lightness = app_payload->light_hsl_server_evt.state_change.hsl.lightness;

        /* Publish the new state */
        for (auto& m : el->get_sig_models())
        {
            if (m->get_model_id() == MESHX_MODEL_ID_LIGHT_HSL_SRV)
            {
#if CONFIG_ENABLE_LIGHT_HSL_SERVER
                auto* hsl_srv = static_cast<meshXLightHSLServerModel*>(m.get());
                meshx_model_t model_ref = {
                    .el_id = el->get_element_idx(),
                    .model_id = (uint16_t)hsl_srv->get_model_id(),
                    .pub_addr = el->element_ctx.pub_addr,
                    .p_model = hsl_srv->get_plat_model()
                };

                meshx_ctx_t ctx = {
                    .app_idx = el->element_ctx.app_id,
                    .net_idx = pdev->meshx_store.net_key_id,
                    .opcode = MESHX_MODEL_OP_LIGHT_HSL_STATUS,
                    .src_addr = 0,
                    .dst_addr = el->element_ctx.pub_addr,
                    .p_ctx = hsl_srv->get_pub_struct()
                };

                meshx_light_hsl_send_params_t send_params = {
                    .model = &model_ref,
                    .ctx = &ctx,
                    .tid = 0,
                    .state = el->element_ctx.light_hsl_state
                };
                hsl_srv->model_send(&send_params);
#endif
            }
        }

        /* Persist to NVS */
        meshx_nvs_element_ctx_set(el->get_element_idx(), MESHX_ELEMENT_TYPE_LIGHT_HSL_SERVER,
                                   &el->element_ctx, sizeof(el->element_ctx));
    }

    return MESHX_SUCCESS;
}
void meshXRGBServerElement::sync(control_task_msg_evt_t evt)
{
    MESHX_LOGD(MODULE_ID_ELEMENT_LIGHT_HSL_SERVER, "RGB Server [%d] sync event: %d", get_element_idx(), evt);
    if (evt == CONTROL_TASK_MSG_EVT_SYSTEM_STACK_READY)
    {
        if (!meshx_prov_srv_is_provisioned()) return;
        if (element_ctx.pub_addr == MESHX_ADDR_UNASSIGNED) return;

        for (auto& m : this->get_sig_models())
        {
#if CONFIG_ENABLE_LIGHT_HSL_SERVER
            if (m->get_model_id() == MESHX_MODEL_ID_LIGHT_HSL_SRV)
            {
                auto* hsl_srv = static_cast<meshXLightHSLServerModel*>(m.get());
                meshx_model_t model_ref = {
                    .el_id = this->get_element_idx(),
                    .model_id = (uint16_t)hsl_srv->get_model_id(),
                    .pub_addr = element_ctx.pub_addr,
                    .p_model = hsl_srv->get_plat_model()
                };

                meshx_ctx_t ctx = {
                    .app_idx = element_ctx.app_id,
                    .net_idx = meshx_get_net_key_id(),
                    .opcode = MESHX_MODEL_OP_LIGHT_HSL_STATUS,
                    .src_addr = 0,
                    .dst_addr = element_ctx.pub_addr,
                    .p_ctx = hsl_srv->get_pub_struct()
                };

                meshx_light_hsl_send_params_t send_params = {
                    .model = &model_ref,
                    .ctx = &ctx,
                    .tid = 0,
                    .state = element_ctx.light_hsl_state
                };
                hsl_srv->model_send(&send_params);
            }
#endif
#if CONFIG_ENABLE_GEN_ONOFF_SERVER
            if (m->get_model_id() == MESHX_MODEL_ID_GEN_ONOFF_SRV)
            {
                auto* onoff = static_cast<meshXGenericOnOffServerModel*>(m.get());
                meshx_model_t onoff_ref = {
                    .el_id = this->get_element_idx(),
                    .model_id = (uint16_t)onoff->get_model_id(),
                    .pub_addr = element_ctx.pub_addr,
                    .p_model = onoff->get_plat_model()
                };

                meshx_ctx_t ctx = {
                    .app_idx = element_ctx.app_id,
                    .net_idx = meshx_get_net_key_id(),
                    .opcode = MESHX_MODEL_OP_GEN_ONOFF_STATUS,
                    .src_addr = 0,
                    .dst_addr = element_ctx.pub_addr,
                    .p_ctx = nullptr
                };

                meshx_gen_onoff_send_params_t sp = {
                    .model = &onoff_ref,
                    .ctx = &ctx,
                    .state = { .on_off = element_ctx.gen_on_off_state.on_off },
                    .tid = 0
                };
                onoff->model_send(&sp);
            }
#endif
        }
    }
}

void meshXRGBServerElement::handle_config(control_task_msg_evt_t evt, const meshx_config_srv_cb_param_t *params)
{
    MESHX_LOGD(MODULE_ID_ELEMENT_LIGHT_HSL_SERVER, "RGB Server [%d] Config Evt: %d", get_element_idx(), evt);
}
