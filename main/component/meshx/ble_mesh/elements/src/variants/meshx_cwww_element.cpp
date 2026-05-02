/**
 * @file meshx_cwww_element.cpp
 * @brief Implementation of MeshX CWWW Element (Tasks A-E).
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#include <cstring>
#include <meshx_api.h>
#include <meshx_nvs.h>
#include <meshx_element_factory.hpp>
#include <meshx_element_registry.hpp>
#include <variants/meshx_cwww_element.hpp>
#include <light_model/meshx_model_ctl.hpp>
#include <generic_model/meshx_model_onoff.hpp>
#include <interface/ble_mesh/meshx_ble_mesh_cmn.h>

#ifdef __cplusplus
extern "C" {
#endif
#if CONFIG_ENABLE_CONFIG_SERVER
#include "meshx_config_server.h"
#define CONFIG_SERVER_CB_MASK   \
    CONTROL_TASK_MSG_EVT_PUB_ADD  \
  | CONTROL_TASK_MSG_EVT_PUB_DEL  \
  | CONTROL_TASK_MSG_EVT_APP_KEY_BIND
#endif
#if CONFIG_ENABLE_PROVISIONING
#include "meshx_prov_srv.h"
#endif
#ifdef __cplusplus
}
#endif

typedef enum {
    MESHX_GEN_ON_OFF_CLI_MSG_SET = 0,
    MESHX_GEN_ON_OFF_CLI_MSG_GET
} meshx_gen_on_off_cli_msg_type_t;

typedef struct meshx_gen_on_off_cli_msg {
    uint16_t element_id;
    bool ack;
    meshx_gen_on_off_cli_msg_type_t set_get;
} meshx_gen_on_off_cli_msg_t;

typedef struct meshx_light_ctl_cli_msg {
    uint16_t element_id;
    bool ack;
    uint8_t set_get;
    uint8_t tid;
} meshx_light_ctl_cli_msg_t;

#define MESHX_GEN_ON_OFF_CLI_MSG_ACK    1
#define MESHX_GEN_ON_OFF_CLI_MSG_NO_ACK 0


/************************************************************************************
 * meshXCWWWServerElement
 ************************************************************************************/
#if CONFIG_LIGHT_CWWW_SRV_COUNT > 0

std::once_flag meshXCWWWServerElement::s_callbacks_registered;

void meshXCWWWServerElement::register_class_callbacks()
{
    meshx_err_t err;
#if CONFIG_ENABLE_CONFIG_SERVER
    err = meshx_config_server_cb_reg(
        (config_srv_cb_t)&meshXCWWWServerElement::s_config_srv_cb,
        CONFIG_SERVER_CB_MASK);
    if (err)
        MESHX_LOGE(MODULE_ID_ELEMENT_LIGHT_CWWW_SERVER, "CWWW Srv: cfg cb reg failed: %d", err);
#endif
#if CONFIG_ENABLE_PROVISIONING
    err = meshx_prov_srv_reg_el_server_cb(
        (prov_srv_cb_t)&meshXCWWWServerElement::s_prov_cb);
    if (err)
        MESHX_LOGE(MODULE_ID_ELEMENT_LIGHT_CWWW_SERVER, "CWWW Srv: prov cb reg failed: %d", err);
#endif
    err = control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_TO_BLE,
        (CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF_SRV | CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL_SRV),
        (control_task_msg_handle_t)&meshXCWWWServerElement::s_to_ble_cb);
    if (err)
        MESHX_LOGE(MODULE_ID_ELEMENT_LIGHT_CWWW_SERVER, "CWWW Srv: to_ble cb reg failed: %d", err);
}

MESHX_CWWW_SERVER_ELEMENT_TEMPLATE_PROTO
meshXCWWWServerElement MESHX_CWWW_SERVER_ELEMENT_TEMPLATE_PARAMS
    ::meshXCWWWServerElement(uint16_t element_idx)
    : meshXElementServer(element_idx)
{
    this->register_element_ctx(&element_ctx, sizeof(meshx_cwww_srv_el_ctx_t));
    this->set_element_variant(MESHX_ELEMENT_TYPE_LIGHT_CWWW_SERVER);
    std::call_once(s_callbacks_registered, register_class_callbacks);
}

MESHX_CWWW_SERVER_ELEMENT_TEMPLATE_PROTO
uint8_t meshXCWWWServerElement MESHX_CWWW_SERVER_ELEMENT_TEMPLATE_PARAMS
    ::list_sig_models()
{
    auto onoff_model = std::make_unique<meshXGenericOnOffServerModel>(
        this, &element_ctx.gen_on_off_state,
        (uint16_t)static_cast<int>(
            meshxCWWWServerElementComposition::MESHX_CWWW_SERVER_ELEMENT_COMP_GENERIC_ONOFF_SERVER));
    this->get_sig_models().push_back(std::move(onoff_model));


    auto ctl_model = std::make_unique<meshXLightCTLServerModel>(
        this, &element_ctx.light_ctl_state,
        (uint16_t)static_cast<int>(
            meshxCWWWServerElementComposition::MESHX_CWWW_SERVER_ELEMENT_COMP_LIGHT_CTL_SERVER));
    this->get_sig_models().push_back(std::move(ctl_model));

    return (uint8_t)this->get_sig_models().size();
}

/**
 * @brief Lists Vendor models for CWWW Server Element (none required)
 */
MESHX_CWWW_SERVER_ELEMENT_TEMPLATE_PROTO
uint8_t meshXCWWWServerElement MESHX_CWWW_SERVER_ELEMENT_TEMPLATE_PARAMS
    ::list_ven_models()
{
    return 0; /* No vendor models for CWWW server */
}

/* Task A+B: state notify — discriminate by model_id, update ctx, NVS, app notify */
MESHX_CWWW_SERVER_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXCWWWServerElement MESHX_CWWW_SERVER_ELEMENT_TEMPLATE_PARAMS
    ::element_state_change_notify(meshx_ptr_t param, size_t param_size)
{
    if (!param) return MESHX_INVALID_ARG;

    auto *header    = static_cast<meshx_srv_model_send_param_header_t *>(param);
    uint16_t el_id  = header->model.el_id;
    uint16_t mid    = header->model.model_id;
    uint16_t func_id = 0;
    meshx_api_light_cwww_server_evt_t app_evt = {};

    if (mid == MESHX_MODEL_ID_GEN_ONOFF_SRV)
    {
        auto *msg = static_cast<meshx_on_off_srv_el_msg_t *>(param);
        if (element_ctx.gen_on_off_state.on_off == msg->state.on_off)
            return MESHX_SUCCESS;
        element_ctx.gen_on_off_state.on_off = msg->state.on_off;
        app_evt.state_change.on_off.state   = element_ctx.gen_on_off_state.on_off;
        func_id = MESHX_ELEMENT_FUNC_ID_LIGHT_CWWW_SERVER_ONN_OFF;
    }
    else if (mid == MESHX_MODEL_ID_LIGHT_CTL_SRV)
    {
        auto *msg = static_cast<meshx_light_ctl_srv_el_msg_t *>(param);
        if (memcmp(&element_ctx.light_ctl_state, &msg->state,
                   sizeof(element_ctx.light_ctl_state)) == 0)
            return MESHX_SUCCESS;
        element_ctx.light_ctl_state = msg->state;
        app_evt.state_change.ctl = {
            .lightness      = msg->state.lightness,
            .temperature    = msg->state.temperature,
            .delta_uv       = (uint16_t)msg->state.delta_uv,
            .temp_range_min = msg->state.temp_range_min,
            .temp_range_max = msg->state.temp_range_max,
        };
        func_id = MESHX_ELEMENT_FUNC_ID_LIGHT_CWWW_SERVER_CTL;
    }
    else
    {
        return MESHX_INVALID_ARG;
    }

    meshx_err_t err = meshx_nvs_element_ctx_set(el_id, &element_ctx, sizeof(element_ctx));
    if (err)
        MESHX_LOGW(MODULE_ID_ELEMENT_LIGHT_CWWW_SERVER, "CWWW Srv [%d]: NVS save failed: %d", el_id, err);

    return meshx_send_msg_to_app(el_id, MESHX_ELEMENT_TYPE_LIGHT_CWWW_SERVER,
                                 func_id, sizeof(app_evt), &app_evt);
}

/* Task C: config server callback */
#if CONFIG_ENABLE_CONFIG_SERVER
MESHX_CWWW_SERVER_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXCWWWServerElement MESHX_CWWW_SERVER_ELEMENT_TEMPLATE_PARAMS
    ::s_config_srv_cb(
        const dev_struct_t              *pdev,
        control_task_msg_evt_t           evt,
        const meshx_config_srv_cb_param_t *params)
{
    MESHX_UNUSED(pdev);
    if (!params) return MESHX_INVALID_ARG;

    uint16_t element_id = 0;
    bool     save   = false;

    switch (evt)
    {
    case CONTROL_TASK_MSG_EVT_APP_KEY_BIND:
        element_id = params->state_change.mod_app_bind.element_addr;
        break;
    case CONTROL_TASK_MSG_EVT_PUB_ADD:
    case CONTROL_TASK_MSG_EVT_PUB_DEL:
        element_id = params->state_change.mod_pub_set.element_addr;
        break;
    default: break;
    }

    auto *el = meshXElementRegistry::get_instance().find_and_cast<meshXCWWWServerElement>(
        element_id, MESHX_ELEMENT_TYPE_LIGHT_CWWW_SERVER);
    if (!el) return MESHX_SUCCESS;

    if (evt == CONTROL_TASK_MSG_EVT_APP_KEY_BIND)
    {
        el->element_ctx.app_id = params->state_change.mod_app_bind.app_idx;
        save = true;
    }
    else if (evt == CONTROL_TASK_MSG_EVT_PUB_ADD || evt == CONTROL_TASK_MSG_EVT_PUB_DEL)
    {
        el->element_ctx.pub_addr =
            (evt == CONTROL_TASK_MSG_EVT_PUB_ADD)
            ? params->state_change.mod_pub_set.pub_addr : MESHX_ADDR_UNASSIGNED;
        el->element_ctx.app_id = params->state_change.mod_pub_set.app_idx;
        save = true;
    }

    if (save)
    {
        meshx_nvs_element_ctx_set(element_id, &el->element_ctx,
                                   sizeof(meshx_cwww_srv_el_ctx_t));
    }
    return MESHX_SUCCESS;
}
#endif

/* Task D: provisioning callback — re-publish OnOff + CTL after provisioning */
#if CONFIG_ENABLE_PROVISIONING
MESHX_CWWW_SERVER_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXCWWWServerElement MESHX_CWWW_SERVER_ELEMENT_TEMPLATE_PARAMS
    ::s_prov_cb(const dev_struct_t *pdev, control_task_msg_evt_t evt, const void *params)
{
    MESHX_UNUSED(params);
    if (!pdev) return MESHX_INVALID_ARG;

    if (evt == CONTROL_TASK_MSG_EVT_EN_NODE_PROV)
    {
        auto elements = meshXElementRegistry::get_instance().get_all_elements();
        for (auto const& [abs_id, base_el] : elements)
        {
            if (!base_el || base_el->get_element_variant() != MESHX_ELEMENT_TYPE_LIGHT_CWWW_SERVER) continue;
            auto *el = static_cast<meshXCWWWServerElement *>(base_el);
            if (el->element_ctx.pub_addr == MESHX_ADDR_UNASSIGNED) continue;

            auto &models = el->get_sig_models();
            auto *onoff  = static_cast<meshXGenericOnOffServerModel *>(models[0].get());
            auto *ctl    = static_cast<meshXLightCTLServerModel *>(models[1].get());

            meshx_model_t model_ref = { .el_id    = abs_id,
                                        .model_id = 0,
                                        .pub_addr = el->element_ctx.pub_addr,
                                        .p_model  = nullptr };
            meshx_ctx_t ctx = { .app_idx  = el->element_ctx.app_id,
                                .net_idx  = pdev->meshx_store.net_key_id,
                                .opcode   = MESHX_MODEL_OP_GEN_ONOFF_STATUS,
                                .src_addr = 0,
                                .dst_addr = el->element_ctx.pub_addr,
                                .p_ctx    = nullptr };
            meshx_gen_onoff_send_params_t sp = {
                .model = &model_ref, .ctx = &ctx,
                .state = { .on_off = el->element_ctx.gen_on_off_state.on_off },
                .tid = 0 };
            onoff->model_send(&sp);

            meshx_ctx_t ctl_ctx = { .app_idx  = el->element_ctx.app_id,
                                    .net_idx  = pdev->meshx_store.net_key_id,
                                    .opcode   = MESHX_MODEL_OP_LIGHT_CTL_STATUS,
                                    .src_addr = 0,
                                    .dst_addr = el->element_ctx.pub_addr,
                                    .p_ctx    = nullptr };
            meshx_light_ctl_send_params_t cp = {
                .model = &model_ref, .ctx = &ctl_ctx,
                .tid = 0,
                .state = { .lightness   = el->element_ctx.light_ctl_state.lightness,
                           .temperature = el->element_ctx.light_ctl_state.temperature,
                           .delta_uv    = el->element_ctx.light_ctl_state.delta_uv,
                           .temp_range_min = 0,
                           .temp_range_max = 0 } };
            ctl->model_send(&cp);
        }
    }
    return MESHX_SUCCESS;
}
#endif

/* Task E: TO_BLE server — no-op; app-driven sends go via client elements */
MESHX_CWWW_SERVER_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXCWWWServerElement MESHX_CWWW_SERVER_ELEMENT_TEMPLATE_PARAMS
    ::s_to_ble_cb(const dev_struct_t *pdev, control_task_msg_evt_t evt, const void *params)
{
    MESHX_UNUSED(pdev); MESHX_UNUSED(evt); MESHX_UNUSED(params);
    return MESHX_SUCCESS;
}

#endif /* CONFIG_LIGHT_CWWW_SRV_COUNT */

/************************************************************************************
 * meshXCWWWClientElement
 ************************************************************************************/
#if CONFIG_LIGHT_CWWW_CLIENT_COUNT > 0

std::once_flag meshXCWWWClientElement::s_callbacks_registered;

void meshXCWWWClientElement::register_class_callbacks()
{
    meshx_err_t err;
#if CONFIG_ENABLE_CONFIG_SERVER
    err = meshx_config_server_cb_reg(
        (config_srv_cb_t)&meshXCWWWClientElement::s_config_srv_cb,
        CONFIG_SERVER_CB_MASK);
    if (err)
        MESHX_LOGE(MODULE_ID_ELEMENT_LIGHT_CWWW_CLIENT, "CWWW Cli: cfg cb reg failed: %d", err);
#endif
#if CONFIG_ENABLE_PROVISIONING
    err = meshx_prov_srv_reg_el_client_cb(
        (prov_srv_cb_t)&meshXCWWWClientElement::s_prov_cb);
    if (err)
        MESHX_LOGE(MODULE_ID_ELEMENT_LIGHT_CWWW_CLIENT, "CWWW Cli: prov cb reg failed: %d", err);
#endif
    err = control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_TO_BLE,
        CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF | CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL,
        (control_task_msg_handle_t)&meshXCWWWClientElement::s_to_ble_cb);
    if (err)
        MESHX_LOGE(MODULE_ID_ELEMENT_LIGHT_CWWW_CLIENT, "CWWW Cli: to_ble cb reg failed: %d", err);
}

MESHX_CWWW_CLIENT_ELEMENT_TEMPLATE_PROTO
meshXCWWWClientElement MESHX_CWWW_CLIENT_ELEMENT_TEMPLATE_PARAMS
    ::meshXCWWWClientElement(uint16_t element_idx)
    : meshXElementClient(element_idx)
{
    this->register_element_ctx(&element_ctx, sizeof(meshx_cwww_cli_el_ctx_t));
    this->set_element_variant(MESHX_ELEMENT_TYPE_LIGHT_CWWW_CLIENT);
    std::call_once(s_callbacks_registered, register_class_callbacks);
}

MESHX_CWWW_CLIENT_ELEMENT_TEMPLATE_PROTO
uint8_t meshXCWWWClientElement MESHX_CWWW_CLIENT_ELEMENT_TEMPLATE_PARAMS
    ::list_sig_models()
{
    auto onoff_model = std::make_unique<meshXGenericOnOffClientModel>(
        this, &element_ctx.gen_on_off_state,
        (uint16_t)static_cast<int>(
            meshxCWWWClientElementComposition::MESHX_CWWW_CLIENT_ELEMENT_COMP_GENERIC_ONOFF_CLIENT));
    this->get_sig_models().push_back(std::move(onoff_model));


    auto ctl_model = std::make_unique<meshXLightCTLClientModel>(
        this, &element_ctx.light_ctl_state,
        (uint16_t)static_cast<int>(
            meshxCWWWClientElementComposition::MESHX_CWWW_CLIENT_ELEMENT_COMP_LIGHT_CTL_CLIENT));
    this->get_sig_models().push_back(std::move(ctl_model));

    return (uint8_t)this->get_sig_models().size();
}

/**
 * @brief Lists Vendor models for CWWW Client Element (none required)
 */
MESHX_CWWW_CLIENT_ELEMENT_TEMPLATE_PROTO
uint8_t meshXCWWWClientElement MESHX_CWWW_CLIENT_ELEMENT_TEMPLATE_PARAMS
    ::list_ven_models()
{
    return 0; /* No vendor models for CWWW client */
}

/* Task A+B: state notify — discriminate by model_id, notify app */
MESHX_CWWW_CLIENT_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXCWWWClientElement MESHX_CWWW_CLIENT_ELEMENT_TEMPLATE_PARAMS
    ::element_state_change_notify(meshx_ptr_t param, size_t param_size)
{
    if (!param) return MESHX_INVALID_ARG;

    auto *header    = static_cast<meshx_cli_model_send_param_header_t *>(param);
    uint16_t el_id  = header->model.el_id;
    uint16_t mid    = header->model.model_id;
    uint16_t func_id = 0;
    meshx_api_light_cwww_client_evt_t app_evt = {};
    app_evt.err_code = header->err_code;

    if (mid == MESHX_MODEL_ID_GEN_ONOFF_CLI)
    {
        auto *msg = static_cast<meshx_on_off_cli_el_msg_t *>(param);
        element_ctx.gen_on_off_state.on_off  = msg->state.on_off;
        app_evt.state_change.on_off.state    = msg->state.on_off;
        func_id = MESHX_ELEMENT_FUNC_ID_LIGHT_CWWW_CLIENT_ONN_OFF;
    }
    else if (mid == MESHX_MODEL_ID_LIGHT_CTL_CLI)
    {
        auto *msg = static_cast<meshx_light_ctl_cli_el_msg_t *>(param);
        element_ctx.light_ctl_state = msg->state;
        app_evt.state_change.ctl = {
            .lightness      = msg->state.lightness,
            .temperature    = msg->state.temperature,
            .delta_uv       = (uint16_t)msg->state.delta_uv,
            .temp_range_min = msg->state.temp_range_min,
            .temp_range_max = msg->state.temp_range_max,
        };
        func_id = MESHX_ELEMENT_FUNC_ID_LIGHT_CWWW_CLIENT_CTL;
    }
    else
    {
        return MESHX_INVALID_ARG;
    }

    return meshx_send_msg_to_app(el_id, MESHX_ELEMENT_TYPE_LIGHT_CWWW_CLIENT,
                                 func_id, sizeof(app_evt), &app_evt);
}

/* Task C: config server callback */
#if CONFIG_ENABLE_CONFIG_SERVER
MESHX_CWWW_CLIENT_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXCWWWClientElement MESHX_CWWW_CLIENT_ELEMENT_TEMPLATE_PARAMS
    ::s_config_srv_cb(
        const dev_struct_t              *pdev,
        control_task_msg_evt_t           evt,
        const meshx_config_srv_cb_param_t *params)
{
    MESHX_UNUSED(pdev);
    if (!params) return MESHX_INVALID_ARG;

    uint16_t element_id = 0;
    bool     save   = false;

    switch (evt)
    {
    case CONTROL_TASK_MSG_EVT_APP_KEY_BIND:
        element_id = params->state_change.mod_app_bind.element_addr;
        break;
    case CONTROL_TASK_MSG_EVT_PUB_ADD:
    case CONTROL_TASK_MSG_EVT_PUB_DEL:
        element_id = params->state_change.mod_pub_set.element_addr;
        break;
    default: break;
    }

    auto *el = meshXElementRegistry::get_instance().find_and_cast<meshXCWWWClientElement>(
        element_id, MESHX_ELEMENT_TYPE_LIGHT_CWWW_CLIENT);
    if (!el) return MESHX_SUCCESS;

    if (evt == CONTROL_TASK_MSG_EVT_APP_KEY_BIND)
    {
        el->element_ctx.app_id = params->state_change.mod_app_bind.app_idx;
        save = true;
    }
    else if (evt == CONTROL_TASK_MSG_EVT_PUB_ADD || evt == CONTROL_TASK_MSG_EVT_PUB_DEL)
    {
        el->element_ctx.pub_addr =
            (evt == CONTROL_TASK_MSG_EVT_PUB_ADD)
            ? params->state_change.mod_pub_set.pub_addr : MESHX_ADDR_UNASSIGNED;
        el->element_ctx.app_id = params->state_change.mod_pub_set.app_idx;
        save = true;
    }

    if (save)
    {
        meshx_nvs_element_ctx_set(element_id, &el->element_ctx,
                                   sizeof(meshx_cwww_cli_el_ctx_t));
    }
    return MESHX_SUCCESS;
}
#endif

/* Task D: provisioning client callback — send GET to refresh server state */
#if CONFIG_ENABLE_PROVISIONING
MESHX_CWWW_CLIENT_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXCWWWClientElement MESHX_CWWW_CLIENT_ELEMENT_TEMPLATE_PARAMS
    ::s_prov_cb(const dev_struct_t *pdev, control_task_msg_evt_t evt, const void *params)
{
    MESHX_UNUSED(params);
    if (!pdev) return MESHX_INVALID_ARG;

    if (evt == CONTROL_TASK_MSG_EVT_EN_NODE_PROV)
    {
        auto elements = meshXElementRegistry::get_instance().get_all_elements();
        for (auto const& [abs_id, base_el] : elements)
        {
            if (!base_el || base_el->get_element_variant() != MESHX_ELEMENT_TYPE_LIGHT_CWWW_CLIENT) continue;
            auto *el = static_cast<meshXCWWWClientElement *>(base_el);
            if (!el || el->element_ctx.pub_addr == MESHX_ADDR_UNASSIGNED) continue;

            auto &models = el->get_sig_models();
            auto *onoff  = static_cast<meshXGenericOnOffClientModel *>(models[0].get());
            auto *ctl    = static_cast<meshXLightCTLClientModel *>(models[1].get());

            meshx_model_t model_ref = { .el_id    = abs_id,
                                        .model_id = 0,
                                        .pub_addr = el->element_ctx.pub_addr,
                                        .p_model  = nullptr };
            meshx_ctx_t ctx = { .app_idx  = el->element_ctx.app_id,
                                .net_idx  = pdev->meshx_store.net_key_id,
                                .opcode   = MESHX_MODEL_OP_GEN_ONOFF_GET,
                                .src_addr = 0,
                                .dst_addr = el->element_ctx.pub_addr,
                                .p_ctx    = nullptr };
            meshx_gen_onoff_send_params_t sp = {
                .model = &model_ref, .ctx = &ctx,
                .state = { .on_off = el->element_ctx.gen_on_off_state.on_off },
                .tid = 0 };
            onoff->model_send(&sp);

            meshx_ctx_t ctl_ctx = { .app_idx  = el->element_ctx.app_id,
                                    .net_idx  = pdev->meshx_store.net_key_id,
                                    .opcode   = MESHX_MODEL_OP_LIGHT_CTL_GET,
                                    .src_addr = 0,
                                    .dst_addr = el->element_ctx.pub_addr,
                                    .p_ctx    = nullptr };
            meshx_light_ctl_send_params_t cp = {
                .model = &model_ref, .ctx = &ctl_ctx,
                .tid = 0,
                .state = { .lightness   = el->element_ctx.light_ctl_state.lightness,
                           .temperature = el->element_ctx.light_ctl_state.temperature,
                           .delta_uv    = el->element_ctx.light_ctl_state.delta_uv,
                           .temp_range_min = 0,
                           .temp_range_max = 0 } };
            ctl->model_send(&cp);
        }
    }
    return MESHX_SUCCESS;
}
#endif

/* Task E: TO_BLE client handler */
MESHX_CWWW_CLIENT_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXCWWWClientElement MESHX_CWWW_CLIENT_ELEMENT_TEMPLATE_PARAMS
    ::s_to_ble_cb(const dev_struct_t *pdev, control_task_msg_evt_t evt, const void *params)
{
    if (!pdev || !params) return MESHX_INVALID_ARG;

    if (evt == CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF)
    {
        const auto *msg = static_cast<const meshx_gen_on_off_cli_msg_t *>(params);
        auto *el = meshXElementRegistry::get_instance().find_and_cast<meshXCWWWClientElement>(
            msg->element_id, MESHX_ELEMENT_TYPE_LIGHT_CWWW_CLIENT);
        if (!el) return MESHX_SUCCESS;

        if (el->element_ctx.pub_addr == MESHX_ADDR_UNASSIGNED)
            return MESHX_INVALID_STATE;

        auto &models = el->get_sig_models();
        auto *onoff  = static_cast<meshXGenericOnOffClientModel *>(models[0].get());

        uint16_t opcode = (msg->set_get == MESHX_GEN_ON_OFF_CLI_MSG_GET)
            ? MESHX_MODEL_OP_GEN_ONOFF_GET
            : (msg->ack ? MESHX_MODEL_OP_GEN_ONOFF_SET : MESHX_MODEL_OP_GEN_ONOFF_SET_UNACK);

        meshx_model_t model_ref = { .el_id    = msg->element_id,
                                    .model_id = 0,
                                    .pub_addr = el->element_ctx.pub_addr,
                                    .p_model  = nullptr };
        meshx_ctx_t ctx = { .app_idx  = el->element_ctx.app_id,
                            .net_idx  = pdev->meshx_store.net_key_id,
                            .opcode   = opcode,
                            .src_addr = 0,
                            .dst_addr = el->element_ctx.pub_addr,
                            .p_ctx    = nullptr };
        meshx_gen_onoff_send_params_t sp = {
            .model = &model_ref, .ctx = &ctx,
            .state = { .on_off = el->element_ctx.gen_on_off_state.on_off },
            .tid = 0 };
        return onoff->model_send(&sp);
    }
    else if (evt == CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL)
    {
        const auto *msg = static_cast<const meshx_light_ctl_cli_msg_t *>(params);
        auto *el = meshXElementRegistry::get_instance().find_and_cast<meshXCWWWClientElement>(
            msg->element_id, MESHX_ELEMENT_TYPE_LIGHT_CWWW_CLIENT);
        if (!el) return MESHX_SUCCESS;

        if (el->element_ctx.pub_addr == MESHX_ADDR_UNASSIGNED)
            return MESHX_INVALID_STATE;

        auto &models = el->get_sig_models();
        auto *ctl    = static_cast<meshXLightCTLClientModel *>(models[1].get());

        meshx_model_t model_ref = { .el_id    = msg->element_id,
                                    .model_id = 0,
                                    .pub_addr = el->element_ctx.pub_addr,
                                    .p_model  = nullptr };
        meshx_ctx_t ctx = { .app_idx  = el->element_ctx.app_id,
                            .net_idx  = pdev->meshx_store.net_key_id,
                            .opcode   = MESHX_MODEL_OP_LIGHT_CTL_SET,
                            .src_addr = 0,
                            .dst_addr = el->element_ctx.pub_addr,
                            .p_ctx    = nullptr };
        meshx_light_ctl_send_params_t sp = {
            .model = &model_ref, .ctx = &ctx,
            .tid = 0,
            .state = { .lightness   = el->element_ctx.light_ctl_state.lightness,
                       .temperature = el->element_ctx.light_ctl_state.temperature,
                       .delta_uv    = el->element_ctx.light_ctl_state.delta_uv,
                       .temp_range_min = 0,
                       .temp_range_max = 0 } };
        return ctl->model_send(&sp);
    }
    return MESHX_SUCCESS;
}

#endif /* CONFIG_LIGHT_CWWW_CLIENT_COUNT */

/************************************************************************************
 * C-linkage factory functions — used by composition.c dispatch table
 * These replace the legacy meshx_create_cwww_elements / create_cwww_client_elements
 * from the elements_c/ path.
 ************************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_LIGHT_CWWW_SRV_COUNT > 0
/**
 * @brief C++ factory for CWWW Server elements.
 *
 * Constructs CONFIG_LIGHT_CWWW_SRV_COUNT meshXCWWWServerElement objects and
 * registers each with the platform BLE Mesh composition.
 *
 * @param pdev         Pointer to the device structure.
 * @param element_cnt  Number of light CWWW server elements to create.
 *                     Capped by CONFIG_LIGHT_CWWW_SRV_COUNT.
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t create_cwww_server_elements_cpp(dev_struct_t *pdev, uint16_t element_cnt)
{
    return meshx_element_factory_helper<meshXCWWWServerElement, meshx_cwww_srv_el_ctx_t>(
        pdev,
        element_cnt,
        CONFIG_LIGHT_CWWW_SRV_COUNT,
        MODULE_ID_ELEMENT_LIGHT_CWWW_SERVER,
        "CWWW Srv"
    );
}
#endif /* CONFIG_LIGHT_CWWW_SRV_COUNT > 0 */

#if CONFIG_LIGHT_CWWW_CLIENT_COUNT > 0
/**
 * @brief C++ factory for CWWW Client elements.
 *
 * Constructs CONFIG_LIGHT_CWWW_CLIENT_COUNT meshXCWWWClientElement objects and
 * registers each with the platform BLE Mesh composition.
 *
 * @param pdev         Pointer to the device structure.
 * @param element_cnt  Number of light CWWW client elements to create.
 *                     Capped by CONFIG_LIGHT_CWWW_CLI_COUNT.
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t create_cwww_client_elements_cpp(dev_struct_t *pdev, uint16_t element_cnt)
{
    return meshx_element_factory_helper<meshXCWWWClientElement, meshx_cwww_cli_el_ctx_t>(
        pdev,
        element_cnt,
        CONFIG_LIGHT_CWWW_CLIENT_COUNT,
        MODULE_ID_ELEMENT_LIGHT_CWWW_CLIENT,
        "CWWW Cli"
    );
}
#endif /* CONFIG_LIGHT_CWWW_CLIENT_COUNT > 0 */

#ifdef __cplusplus
} /* extern "C" */
#endif
