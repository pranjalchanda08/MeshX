/**
 * @file meshx_cwww_element.cpp
 * @brief Implementation of MeshX CWWW Element (Tasks A-E).
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#include <cstring>
#include <meshx_element_factory.hpp>
#include <meshx_element_registry.hpp>
#include <variants/meshx_cwww_element.hpp>
#include <light_model/meshx_model_ctl.hpp>
#include <generic_model/meshx_model_onoff.hpp>
#include <variants/meshx_relay_msg_defs.h>

#if CONFIG_ENABLE_UNIT_TEST
extern "C" {
    meshx_err_t cwww_cli_ut_handler(int cmd_id, int argc, char **argv);
    meshx_err_t cwww_srv_ut_handler(int cmd_id, int argc, char **argv);
}
#endif

#ifdef __cplusplus
extern "C" {
#endif
#if CONFIG_ENABLE_CONFIG_SERVER
#define CONFIG_SERVER_CB_MASK   \
    CONTROL_TASK_MSG_EVT_PUB_ADD  \
  | CONTROL_TASK_MSG_EVT_PUB_DEL  \
  | CONTROL_TASK_MSG_EVT_APP_KEY_BIND
#endif
#if CONFIG_ENABLE_PROVISIONING
#endif
#ifdef __cplusplus
}
#endif

#define CWWW_CLI_TO_BLE_EVT_MASK   (CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF | CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL)
#define CWWW_SRV_TO_BLE_EVT_MASK   (CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF_SRV | CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL_SRV)


typedef struct meshx_light_ctl_cli_msg {
    uint16_t element_id;
    bool ack;
    uint8_t set_get;
    uint16_t lightness;
    uint16_t temperature;
    int16_t delta_uv;
    uint8_t tid;
} meshx_light_ctl_cli_msg_t;

#define MESHX_GEN_ON_OFF_CLI_MSG_ACK    1
#define MESHX_GEN_ON_OFF_CLI_MSG_NO_ACK 0


/************************************************************************************
 * meshXCWWWServerElement
 ************************************************************************************/
#if CONFIG_LIGHT_CWWW_SRV_COUNT > 0


MESHX_CWWW_SERVER_ELEMENT_TEMPLATE_PROTO
meshXCWWWServerElement::meshXCWWWServerElement(uint16_t element_idx)
    : meshXElementServer(element_idx)
{
    memset(&element_ctx, 0, sizeof(meshx_cwww_srv_el_ctx_t));
    element_ctx.pub_addr = MESHX_ADDR_UNASSIGNED;

    /* Restore from NVS if available */
    meshx_nvs_element_ctx_get(element_idx, MESHX_ELEMENT_TYPE_LIGHT_CWWW_SERVER, &element_ctx, sizeof(meshx_cwww_srv_el_ctx_t));

    this->register_element_ctx(&element_ctx, sizeof(meshx_cwww_srv_el_ctx_t));
    this->set_element_variant(MESHX_ELEMENT_TYPE_LIGHT_CWWW_SERVER);

    /* Subscribe to BLE events for this element type */
    control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_TO_BLE,
        CWWW_SRV_TO_BLE_EVT_MASK,
        (control_task_msg_handle_t)&meshXCWWWServerElement::s_to_ble_cb);

#if CONFIG_ENABLE_UNIT_TEST
    register_unit_test(MODULE_ID_ELEMENT_LIGHT_CWWW_SERVER, cwww_srv_ut_handler);
#endif
}

MESHX_CWWW_SERVER_ELEMENT_TEMPLATE_PROTO
uint8_t meshXCWWWServerElement::list_sig_models()
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
uint8_t meshXCWWWServerElement::list_ven_models()
{
    return 0; /* No vendor models for CWWW server */
}

MESHX_CWWW_SERVER_ELEMENT_TEMPLATE_PROTO
const char* meshXCWWWServerElement::get_element_name(void) const
{
    return "CWWW Server";
}

/* Task A+B: state notify — discriminate by model_id, update ctx, NVS, app notify */
MESHX_CWWW_SERVER_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXCWWWServerElement::element_state_change_notify(meshx_ptr_t param, size_t param_size)
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
        /* State is already updated by model layer via pointer, but we sync here for clarity */
        element_ctx.gen_on_off_state = msg->state;
        app_evt.state_change.on_off.state   = element_ctx.gen_on_off_state.on_off;
        func_id = MESHX_ELEMENT_FUNC_ID_LIGHT_CWWW_SERVER_ONN_OFF;
    }
    else if (mid == MESHX_MODEL_ID_LIGHT_CTL_SRV)
    {
        auto *msg = static_cast<meshx_light_ctl_srv_el_msg_t *>(param);
        element_ctx.light_ctl_state = msg->state;
        app_evt.state_change.ctl = {
            .lightness      = msg->state.lightness,
            .temperature    = msg->state.temperature,
            .delta_uv       = (uint16_t)msg->state.delta_uv,
            .temp_range_min = msg->state.temp_range_min,
            .temp_range_max = msg->state.temp_range_max
        };
        func_id = MESHX_ELEMENT_FUNC_ID_LIGHT_CWWW_SERVER_CTL;
    }
    else
    {
        return MESHX_INVALID_ARG;
    }

    meshx_err_t err = meshx_nvs_element_ctx_set(el_id, get_element_variant(), &element_ctx, sizeof(element_ctx));
    if (err)
        MESHX_LOGW(MODULE_ID_ELEMENT_LIGHT_CWWW_SERVER, "CWWW Srv [%d]: NVS save failed: %d", el_id, err);

    return meshx_send_msg_to_app(el_id, MESHX_ELEMENT_TYPE_LIGHT_CWWW_SERVER,
                                 func_id, sizeof(app_evt), &app_evt);
}

void meshXCWWWServerElement::sync(control_task_msg_evt_t evt)
{
    MESHX_LOGD(MODULE_ID_ELEMENT_LIGHT_CWWW_SERVER, "CWWW Srv [%d] sync event: %d", get_element_idx(), evt);
    if (evt == CONTROL_TASK_MSG_EVT_SYSTEM_STACK_READY)
    {
        if (!meshx_prov_srv_is_provisioned()) return;
        if (element_ctx.pub_addr == MESHX_ADDR_UNASSIGNED) return;

        auto &models = this->get_sig_models();

#if CONFIG_ENABLE_GEN_ONOFF_SERVER
        auto *onoff  = static_cast<meshXGenericOnOffServerModel *>(models[0].get());
        onoff->request_status();
#endif

#if CONFIG_ENABLE_LIGHT_CTL_SERVER
        auto *ctl = static_cast<meshXLightCTLServerModel *>(models[1].get());
        ctl->request_status();
#endif
    }
}

void meshXCWWWServerElement::handle_config(control_task_msg_evt_t evt, const meshx_config_srv_cb_param_t *params)
{
    MESHX_LOGD(MODULE_ID_ELEMENT_LIGHT_CWWW_SERVER, "CWWW Server [%d] Config Evt: %d", get_element_idx(), evt);
}

/* Task E: TO_BLE server — no-op; app-driven sends go via client elements */
MESHX_CWWW_SERVER_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXCWWWServerElement::s_to_ble_cb(const dev_struct_t *pdev, control_task_msg_evt_t evt, const void *params)
{
    MESHX_UNUSED(pdev); MESHX_UNUSED(evt); MESHX_UNUSED(params);
    return MESHX_SUCCESS;
}

#endif /* CONFIG_LIGHT_CWWW_SRV_COUNT */

/************************************************************************************
 * meshXCWWWClientElement
 ************************************************************************************/
#if CONFIG_LIGHT_CWWW_CLIENT_COUNT > 0


MESHX_CWWW_CLIENT_ELEMENT_TEMPLATE_PROTO
meshXCWWWClientElement::meshXCWWWClientElement(uint16_t element_idx)
    : meshXElementClient(element_idx)
{
    memset(&element_ctx, 0, sizeof(meshx_cwww_cli_el_ctx_t));
    element_ctx.pub_addr = MESHX_ADDR_UNASSIGNED;

    /* Restore from NVS if available */
    meshx_nvs_element_ctx_get(element_idx, MESHX_ELEMENT_TYPE_LIGHT_CWWW_CLIENT, &element_ctx, sizeof(meshx_cwww_cli_el_ctx_t));

    this->register_element_ctx(&element_ctx, sizeof(meshx_cwww_cli_el_ctx_t));
    this->set_element_variant(MESHX_ELEMENT_TYPE_LIGHT_CWWW_CLIENT);

    /* Subscribe to BLE events for this element type */
    control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_TO_BLE,
        CWWW_CLI_TO_BLE_EVT_MASK,
        (control_task_msg_handle_t)&meshXCWWWClientElement::s_to_ble_cb);

#if CONFIG_ENABLE_UNIT_TEST
    register_unit_test(MODULE_ID_ELEMENT_LIGHT_CWWW_CLIENT, cwww_cli_ut_handler);
#endif
}

MESHX_CWWW_CLIENT_ELEMENT_TEMPLATE_PROTO
uint8_t meshXCWWWClientElement::list_sig_models()
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
uint8_t meshXCWWWClientElement::list_ven_models()
{
    return 0; /* No vendor models for CWWW client */
}

MESHX_CWWW_CLIENT_ELEMENT_TEMPLATE_PROTO
const char* meshXCWWWClientElement::get_element_name(void) const
{
    return "CWWW Client";
}

/* Task A+B: state notify — discriminate by model_id, notify app */
MESHX_CWWW_CLIENT_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXCWWWClientElement::element_state_change_notify(meshx_ptr_t param, size_t param_size)
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

void meshXCWWWClientElement::sync(control_task_msg_evt_t evt)
{
    if (evt == CONTROL_TASK_MSG_EVT_SYSTEM_FRESH_BOOT)
    {
        if (!meshx_prov_srv_is_provisioned()) return;
        if (element_ctx.pub_addr == MESHX_ADDR_UNASSIGNED) return;

        auto &models = this->get_sig_models();
        auto *onoff  = static_cast<meshXGenericOnOffClientModel *>(models[0].get());
        auto *ctl    = static_cast<meshXLightCTLClientModel *>(models[1].get());

#if CONFIG_ENABLE_GEN_ONOFF_CLIENT
        meshx_gen_on_off_cli_msg_t msg = {
            .ack        = 1,
            .set_get    = MESHX_GEN_ON_OFF_CLI_MSG_GET,
            .on_off     = 0,
            .reserved   = 0,
            .element_id = this->get_element_idx()
        };
        onoff->request_onoff(&msg);
#endif

#if CONFIG_ENABLE_LIGHT_CTL_CLIENT
        meshx_model_t model_ref = { .el_id    = this->get_element_idx(),
                                    .model_id = (uint16_t)ctl->get_model_id(),
                                    .pub_addr = element_ctx.pub_addr,
                                    .p_model  = (MESHX_MODEL*)ctl->get_plat_model() };
        meshx_ctx_t ctl_ctx = { .app_idx  = element_ctx.app_id,
                                .net_idx  = meshx_get_net_key_id(),
                                .opcode   = MESHX_MODEL_OP_LIGHT_CTL_GET,
                                .src_addr = 0,
                                .dst_addr = element_ctx.pub_addr,
                                .p_ctx    = nullptr };
        meshx_light_ctl_send_params_t cp = {
            .model = &model_ref, .ctx = &ctl_ctx,
            .tid = 0,
            .state = { .lightness   = element_ctx.light_ctl_state.lightness,
                       .temperature = element_ctx.light_ctl_state.temperature,
                       .delta_uv    = element_ctx.light_ctl_state.delta_uv,
                       .temp_range_min = 0,
                       .temp_range_max = 0 } };
        ctl->model_send(&cp);
#endif
    }
}

void meshXCWWWClientElement::handle_config(control_task_msg_evt_t evt, const meshx_config_srv_cb_param_t *params)
{
    MESHX_LOGD(MODULE_ID_ELEMENT_LIGHT_CWWW_CLIENT, "CWWW Client [%d] Config Evt: %d", get_element_idx(), evt);
}

/* Task E: TO_BLE client handler */
MESHX_CWWW_CLIENT_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXCWWWClientElement::s_to_ble_cb(const dev_struct_t *pdev, control_task_msg_evt_t evt, const void *params)
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

        /* Route through the model's request_onoff to leverage the centralized logic */
        auto &models = el->get_sig_models();
#if CONFIG_ENABLE_GEN_ONOFF_CLIENT
        auto *onoff  = static_cast<meshXGenericOnOffClientModel *>(models[0].get());
        return onoff->request_onoff(msg);
#else
        return MESHX_SUCCESS;
#endif
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
#if CONFIG_ENABLE_LIGHT_CTL_CLIENT
        auto *ctl    = static_cast<meshXLightCTLClientModel *>(models[1].get());
        return ctl->request_ctl(msg->lightness, msg->temperature, msg->delta_uv, ++el->element_ctx.ctl_tid);
#else
        return MESHX_SUCCESS;
#endif
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

#if CONFIG_ENABLE_UNIT_TEST
meshx_err_t cwww_cli_ut_handler(int cmd_id, int argc, char **argv)
{
    if (argc < 1) return MESHX_INVALID_ARG;
    uint16_t el_id = UT_GET_ARG(0, uint16_t, argv);

    MESHX_LOGI(MODULE_ID_ELEMENT_LIGHT_CWWW_CLIENT, "cwww_cli_ut_handler: cmd_id=%d, argc=%d, el_id=%d", cmd_id, argc, el_id);

    auto *el = meshXElementRegistry::get_instance().find_and_cast<meshXCWWWClientElement>(
        el_id, MESHX_ELEMENT_TYPE_LIGHT_CWWW_CLIENT);
    if (!el) {
        MESHX_LOGE(MODULE_ID_ELEMENT_LIGHT_CWWW_CLIENT, "cwww_cli_ut_handler: Element %d not found!", el_id);
        return MESHX_NOT_FOUND;
    }

    auto &ctx = el->get_ctx();
    MESHX_LOGI(MODULE_ID_ELEMENT_LIGHT_CWWW_CLIENT, "CWWW Cli [%d]: pub_addr=0x%04x, app_id=%d", el_id, ctx.pub_addr, ctx.app_id);
    if (ctx.pub_addr == MESHX_ADDR_UNASSIGNED) {
        MESHX_LOGW(MODULE_ID_ELEMENT_LIGHT_CWWW_CLIENT, "CWWW Cli [%d]: no pub addr", el_id);
    }

    if (cmd_id == 0x01 || cmd_id == 0x02) { // ONOFF SET / SET UNACK
        meshx_gen_on_off_cli_msg_t msg = {};
        msg.ack        = (uint8_t)(cmd_id == 0x01 ? 1 : 0);
        msg.set_get    = MESHX_GEN_ON_OFF_CLI_MSG_SET;
        msg.on_off     = ctx.gen_on_off_state.next_on_off;
        msg.reserved   = 0;
        msg.element_id = el_id;

        MESHX_LOGI(MODULE_ID_ELEMENT_LIGHT_CWWW_CLIENT, "cwww_cli_ut_handler: Publishing ONOFF SET, on_off=%d, ack=%d, size=%zu",
                   msg.on_off, msg.ack, sizeof(msg));
        return control_task_msg_publish(CONTROL_TASK_MSG_CODE_TO_BLE, CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF, &msg, sizeof(msg));
    }
    else if (cmd_id == 0x04 || cmd_id == 0x05) { // CTL SET / SET UNACK
        if (argc < 4) return MESHX_INVALID_ARG;
        meshx_light_ctl_cli_msg_t msg = {};
        msg.ack        = (cmd_id == 0x04);
        msg.set_get    = MESHX_GEN_ON_OFF_CLI_MSG_SET;
        msg.element_id = el_id;

        msg.lightness   = UT_GET_ARG(1, uint16_t, argv);
        msg.temperature = UT_GET_ARG(2, uint16_t, argv);
        msg.delta_uv    = UT_GET_ARG(3, int16_t, argv);

        MESHX_LOGI(MODULE_ID_ELEMENT_LIGHT_CWWW_CLIENT, "cwww_cli_ut_handler: Publishing CTL SET, light=%d, temp=%d, delta=%d, ack=%d, size=%zu",
                   msg.lightness, msg.temperature, msg.delta_uv, msg.ack, sizeof(msg));
        return control_task_msg_publish(CONTROL_TASK_MSG_CODE_TO_BLE, CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL, &msg, sizeof(msg));
    }
    else if (cmd_id == 0x06 || cmd_id == 0x07) { // LIGHTNESS SET / UNACK
        if (argc < 2) return MESHX_INVALID_ARG;
        meshx_light_ctl_cli_msg_t msg = {};
        msg.ack        = (cmd_id == 0x06);
        msg.set_get    = MESHX_GEN_ON_OFF_CLI_MSG_SET;
        msg.element_id = el_id;
        msg.lightness   = UT_GET_ARG(1, uint16_t, argv);
        msg.temperature = ctx.light_ctl_state.temperature;
        msg.delta_uv    = ctx.light_ctl_state.delta_uv;

        MESHX_LOGI(MODULE_ID_ELEMENT_LIGHT_CWWW_CLIENT, "cwww_cli_ut_handler: Publishing Lightness SET, value=%d, ack=%d, size=%zu",
                   msg.lightness, msg.ack, sizeof(msg));
        return control_task_msg_publish(CONTROL_TASK_MSG_CODE_TO_BLE, CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL, &msg, sizeof(msg));
    }
    else if (cmd_id == 0x08 || cmd_id == 0x09) { // TEMPERATURE SET / UNACK
        if (argc < 2) return MESHX_INVALID_ARG;
        meshx_light_ctl_cli_msg_t msg = {};
        msg.ack        = (cmd_id == 0x08);
        msg.set_get    = MESHX_GEN_ON_OFF_CLI_MSG_SET;
        msg.element_id = el_id;
        msg.temperature = UT_GET_ARG(1, uint16_t, argv);
        msg.lightness   = ctx.light_ctl_state.lightness;
        msg.delta_uv    = ctx.light_ctl_state.delta_uv;

        MESHX_LOGI(MODULE_ID_ELEMENT_LIGHT_CWWW_CLIENT, "cwww_cli_ut_handler: Publishing Temp SET, value=%d, ack=%d, size=%zu",
                   msg.temperature, msg.ack, sizeof(msg));
        return control_task_msg_publish(CONTROL_TASK_MSG_CODE_TO_BLE, CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL, &msg, sizeof(msg));
    }
    else if (cmd_id == 0x0A || cmd_id == 0x0B) { // DELTA UV SET / UNACK
        if (argc < 2) return MESHX_INVALID_ARG;
        meshx_light_ctl_cli_msg_t msg = {};
        msg.ack        = (cmd_id == 0x0A);
        msg.set_get    = MESHX_GEN_ON_OFF_CLI_MSG_SET;
        msg.element_id = el_id;
        msg.delta_uv    = UT_GET_ARG(1, int16_t, argv);
        msg.lightness   = ctx.light_ctl_state.lightness;
        msg.temperature = ctx.light_ctl_state.temperature;

        MESHX_LOGI(MODULE_ID_ELEMENT_LIGHT_CWWW_CLIENT, "cwww_cli_ut_handler: Publishing DeltaUV SET, value=%d, ack=%d, size=%zu",
                   msg.delta_uv, msg.ack, sizeof(msg));
        return control_task_msg_publish(CONTROL_TASK_MSG_CODE_TO_BLE, CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL, &msg, sizeof(msg));
    }
    return MESHX_FAIL;
}

meshx_err_t cwww_srv_ut_handler(int cmd_id, int argc, char **argv)
{
    if (argc < 1) return MESHX_INVALID_ARG;
    uint16_t el_id = UT_GET_ARG(0, uint16_t, argv);

    MESHX_LOGI(MODULE_ID_ELEMENT_LIGHT_CWWW_SERVER, "cwww_srv_ut_handler: cmd_id=%d, argc=%d, el_id=%d", cmd_id, argc, el_id);

    auto *el = meshXElementRegistry::get_instance().find_and_cast<meshXCWWWServerElement>(
        el_id, MESHX_ELEMENT_TYPE_LIGHT_CWWW_SERVER);
    if (!el) {
        MESHX_LOGE(MODULE_ID_ELEMENT_LIGHT_CWWW_SERVER, "cwww_srv_ut_handler: Element %d not found!", el_id);
        return MESHX_NOT_FOUND;
    }

    if (cmd_id == 0x01) { // Check ONOFF state
        uint8_t expected = UT_GET_ARG(1, uint8_t, argv);
        uint8_t actual = el->get_onoff();
        MESHX_LOGI(MODULE_ID_ELEMENT_LIGHT_CWWW_SERVER, "CWWW Server [%d] ONOFF state: %d, expected: %d", el_id, actual, expected);
        return (actual == expected) ? MESHX_SUCCESS : MESHX_FAIL;
    }
    else if (cmd_id == 0x02) { // Check CTL state
        uint16_t exp_light = UT_GET_ARG(1, uint16_t, argv);
        uint16_t exp_temp  = UT_GET_ARG(2, uint16_t, argv);
        auto actual = el->get_ctl();
        MESHX_LOGI(MODULE_ID_ELEMENT_LIGHT_CWWW_SERVER, "CWWW Server [%d] CTL state: light %d, temp %d, expected: light %d, temp %d",
                   el_id, actual.lightness, actual.temperature, exp_light, exp_temp);
        return (actual.lightness == exp_light && actual.temperature == exp_temp) ? MESHX_SUCCESS : MESHX_FAIL;
    }
    return MESHX_FAIL;
}

#endif

#ifdef __cplusplus
}
#endif

