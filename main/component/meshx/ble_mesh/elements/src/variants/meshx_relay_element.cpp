/**
 * @file meshx_relay_element.cpp
 * @brief Implementation of MeshX Relay Element.
 *        This file contains the implementation of the MeshX Relay Element class,
 *        which represents a relay element in the MeshX BLE mesh network.
 * Key Features:
 *  - Implements relay element functionality
 *  - Inherits from meshXElementServer / meshXElementClient
 *  - Automatically initializes required SIG models for relay elements
 *  - Maintains state context for NVS persistence (mirrors C el_ctx)
 *  - Config server callback for app-key bind and publication address
 *  - Provisioning event handling (server: re-publish; client: send GET)
 *  - TO_BLE client send path for app-driven OnOff commands
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#include <variants/meshx_relay_element.hpp>
#include "meshx_element_factory.hpp"
#include "meshx_element_registry.hpp"

#if CONFIG_ENABLE_UNIT_TEST
extern "C" {
    meshx_err_t relay_cli_ut_handler(int cmd_id, int argc, char **argv);
    meshx_err_t relay_srv_ut_handler(int cmd_id, int argc, char **argv);
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
#endif /* CONFIG_ENABLE_CONFIG_SERVER */
#if CONFIG_ENABLE_PROVISIONING
#endif /* CONFIG_ENABLE_PROVISIONING */
#ifdef __cplusplus
}
#endif


#define RELAY_SRV_TO_BLE_EVT_MASK   CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF_SRV
#define RELAY_CLI_TO_BLE_EVT_MASK   CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF


/************************************************************************************
 * meshXRelayServerElement
 ************************************************************************************/
#if CONFIG_RELAY_SERVER_COUNT > 0

/**
 * @brief Register class-level callbacks (called exactly once via once_flag).
 */

/**
 * @brief Constructs a new meshXRelayServerElement instance.
 */
MESHX_RELAY_SERVER_ELEMENT_TEMPLATE_PROTO
meshXRelayServerElement::meshXRelayServerElement (uint16_t element_idx)
    : meshXElementServer(element_idx)
{
    memset(&element_ctx, 0, sizeof(meshx_relay_srv_el_ctx_t));
    element_ctx.pub_addr = MESHX_ADDR_UNASSIGNED;
    element_ctx.gen_on_off_state.on_off = 0;
    element_ctx.gen_on_off_state.next_on_off = 1;

    /* Restore from NVS if available */
    meshx_nvs_element_ctx_get(element_idx, MESHX_ELEMENT_TYPE_RELAY_SERVER, &element_ctx, sizeof(meshx_relay_srv_el_ctx_t));

    this->register_element_ctx(&element_ctx, sizeof(meshx_relay_srv_el_ctx_t));
    this->set_element_variant(MESHX_ELEMENT_TYPE_RELAY_SERVER);

    control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_TO_BLE,
        RELAY_SRV_TO_BLE_EVT_MASK,
        (control_task_msg_handle_t)&meshXRelayServerElement::s_to_ble_cb);

#if CONFIG_ENABLE_UNIT_TEST
    register_unit_test(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, relay_srv_ut_handler);
#endif
}

/**
 * @brief Lists and initializes SIG models for Relay Server Element
 */
MESHX_RELAY_SERVER_ELEMENT_TEMPLATE_PROTO
uint8_t meshXRelayServerElement::list_sig_models()
{
    auto relay_model = std::make_unique<meshXGenericOnOffServerModel>(
        this,
        &element_ctx.gen_on_off_state.on_off,
        (uint16_t)static_cast<int>(
            meshxRelayServerElementComposition::MESHX_RELAY_SERVER_ELEMENT_COMP_GENERIC_ONOFF_SERVER)
    );
    this->get_sig_models().push_back(std::move(relay_model));
    return (uint8_t)this->get_sig_models().size();
}

/**
 * @brief Lists Vendor models for Relay Server Element (none required)
 */
MESHX_RELAY_SERVER_ELEMENT_TEMPLATE_PROTO
uint8_t meshXRelayServerElement::list_ven_models()
{
    return 0; /* No vendor models for relay server */
}

MESHX_RELAY_SERVER_ELEMENT_TEMPLATE_PROTO
const char* meshXRelayServerElement::get_element_name(void) const
{
    return "Relay Server";
}

/* Task A+B — element_state_change_notify: NVS save + app notify */
MESHX_RELAY_SERVER_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXRelayServerElement::element_state_change_notify(meshx_ptr_t param, size_t param_size)
{
    if (!param)
        return MESHX_INVALID_ARG;

    auto *msg = static_cast<meshx_on_off_srv_el_msg_t *>(param);
    uint16_t element_id = msg->header.model.el_id;

    /* Update local context */
    element_ctx.gen_on_off_state = msg->state;

    /* Task B — NVS persistence */
    meshx_err_t err = meshx_nvs_element_ctx_set(
        element_id, get_element_variant(), &element_ctx, sizeof(element_ctx));
    if (err)
    {
        MESHX_LOGW(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER,
                   "Relay Srv [%d]: NVS save failed: %d", element_id, err);
    }

    /* Task B — App notification */
    meshx_api_relay_server_evt_t app_evt = {
        .on_off = element_ctx.gen_on_off_state.on_off
    };
    MESHX_LOGD(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Relay state changed to: %d", app_evt.on_off);
    return meshx_send_msg_to_app(
        element_id,
        MESHX_ELEMENT_TYPE_RELAY_SERVER,
        MESHX_ELEMENT_FUNC_ID_RELAY_SERVER_ONN_OFF,
        sizeof(app_evt),
        &app_evt);
}

void meshXRelayServerElement::handle_config(control_task_msg_evt_t evt, const meshx_config_srv_cb_param_t *params)
{
    MESHX_LOGD(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Relay Server [%d] Config Evt: %d", get_element_idx(), evt);
}

void meshXRelayServerElement::sync(control_task_msg_evt_t evt)
{
    MESHX_LOGD(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Relay Server [%d] sync event: %d", get_element_idx(), evt);
    if (evt == CONTROL_TASK_MSG_EVT_SYSTEM_STACK_READY)
    {
        if (!meshx_prov_srv_is_provisioned()) return;
        if (element_ctx.pub_addr == MESHX_ADDR_UNASSIGNED) return;

        auto &models = this->get_sig_models();
        auto *onoff  = static_cast<meshXGenericOnOffServerModel *>(models[0].get());
        onoff->request_status();
    }
}

/* Task E — TO_BLE server handler (app triggers a publish of current state to BLE) */
MESHX_RELAY_SERVER_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXRelayServerElement::s_to_ble_cb(
        const dev_struct_t      *pdev,
        control_task_msg_evt_t   evt,
        const void              *params)
{
    /* Server-side TO_BLE: re-publish current state on demand.
       The payload is meshx_gen_srv_send_msg_t with the element_id. */
    MESHX_UNUSED(params);
    MESHX_UNUSED(pdev);
    MESHX_UNUSED(evt);
    /* No-op for server elements in relay; the app-driven send goes via client elements */
    return MESHX_SUCCESS;
}

#endif /* CONFIG_RELAY_SERVER_COUNT */

/************************************************************************************
 * meshXRelayClientElement
 ************************************************************************************/
#if CONFIG_RELAY_CLIENT_COUNT > 0

/**
 * @brief Register class-level callbacks (called exactly once via once_flag).
 */
void meshXRelayClientElement::sync(control_task_msg_evt_t evt)
{
    MESHX_LOGD(MODULE_ID_ELEMENT_SWITCH_RELAY_CLIENT, "Relay Client [%d] sync event: %d", get_element_idx(), evt);
    if (evt == CONTROL_TASK_MSG_EVT_SYSTEM_FRESH_BOOT)
    {
        if (!meshx_prov_srv_is_provisioned()) return;
        if (element_ctx.pub_addr == MESHX_ADDR_UNASSIGNED) return;

        auto &models = this->get_sig_models();
        auto *onoff  = static_cast<meshXGenericOnOffClientModel *>(models[0].get());
        meshx_gen_on_off_cli_msg_t msg = {
            .ack        = 1,
            .set_get    = MESHX_GEN_ON_OFF_CLI_MSG_GET,
            .on_off     = 0,
            .reserved   = 0,
            .element_id = this->get_element_idx()
        };
        onoff->request_onoff(&msg);
    }
}

void meshXRelayClientElement::handle_config(control_task_msg_evt_t evt, const meshx_config_srv_cb_param_t *params)
{
    MESHX_LOGD(MODULE_ID_ELEMENT_SWITCH_RELAY_CLIENT, "Relay Client [%d] Config Evt: %d", get_element_idx(), evt);
}

/**
 * @brief Constructs a new meshXRelayClientElement instance.
 */
MESHX_RELAY_CLIENT_ELEMENT_TEMPLATE_PROTO
meshXRelayClientElement::meshXRelayClientElement (uint16_t element_idx)
    : meshXElementClient(element_idx)
{
    memset(&element_ctx, 0, sizeof(meshx_relay_cli_el_ctx_t));
    element_ctx.pub_addr = MESHX_ADDR_UNASSIGNED;
    element_ctx.gen_on_off_state.on_off = 0;
    element_ctx.gen_on_off_state.next_on_off = 1;

    /* Restore from NVS if available */
    meshx_nvs_element_ctx_get(element_idx, MESHX_ELEMENT_TYPE_RELAY_CLIENT, &element_ctx, sizeof(meshx_relay_cli_el_ctx_t));

    this->register_element_ctx(&element_ctx, sizeof(meshx_relay_cli_el_ctx_t));
    this->set_element_variant(MESHX_ELEMENT_TYPE_RELAY_CLIENT);

    control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_TO_BLE,
        RELAY_CLI_TO_BLE_EVT_MASK,
        (control_task_msg_handle_t)&meshXRelayClientElement::s_to_ble_cb);

#if CONFIG_ENABLE_UNIT_TEST
    register_unit_test(MODULE_ID_ELEMENT_SWITCH_RELAY_CLIENT, relay_cli_ut_handler);
#endif
}

/**
 * @brief Lists and initializes SIG models for Relay Client Element
 */
MESHX_RELAY_CLIENT_ELEMENT_TEMPLATE_PROTO
uint8_t meshXRelayClientElement::list_sig_models()
{
    auto relay_model = std::make_unique<meshXGenericOnOffClientModel>(
        this,
        &element_ctx.gen_on_off_state.on_off,
        (uint16_t)static_cast<int>(
            meshxRelayClientElementComposition::MESHX_RELAY_CLIENT_ELEMENT_COMP_GENERIC_ONOFF_CLIENT)
    );
    this->get_sig_models().push_back(std::move(relay_model));
    return (uint8_t)this->get_sig_models().size();
}

/**
 * @brief Lists Vendor models for Relay Client Element (none required)
 */
MESHX_RELAY_CLIENT_ELEMENT_TEMPLATE_PROTO
uint8_t meshXRelayClientElement::list_ven_models()
{
    return 0; /* No vendor models for relay client */
}

MESHX_RELAY_CLIENT_ELEMENT_TEMPLATE_PROTO
const char* meshXRelayClientElement::get_element_name(void) const
{
    return "Relay Client";
}

/* Task A+B — element_state_change_notify: app notify (NVS not required for client state) */
MESHX_RELAY_CLIENT_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXRelayClientElement::element_state_change_notify(meshx_ptr_t param, size_t param_size)
{
    if (!param)
        return MESHX_INVALID_ARG;

    auto *msg       = static_cast<meshx_on_off_cli_el_msg_t *>(param);
    uint16_t element_id = msg->header.model.el_id;

    /* Update local context */
    element_ctx.gen_on_off_state = msg->state;

    /* App notification */
    meshx_api_relay_client_evt_t app_evt = {
        .err_code = msg->header.err_code,
        .on_off   = element_ctx.gen_on_off_state.on_off
    };
    return meshx_send_msg_to_app(
        element_id,
        MESHX_ELEMENT_TYPE_RELAY_CLIENT,
        MESHX_ELEMENT_FUNC_ID_RELAY_SERVER_ONN_OFF,
        sizeof(app_evt),
        &app_evt);
}

/* Task E — TO_BLE client handler: receives app command, sends OnOff via model */
MESHX_RELAY_CLIENT_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXRelayClientElement::s_to_ble_cb(
        const dev_struct_t      *pdev,
        control_task_msg_evt_t   evt,
        const void              *params)
{
    MESHX_LOGD(MODULE_ID_ELEMENT_SWITCH_RELAY_CLIENT, "Entering C++ Relay Client TO_BLE Callback: evt=0x%x", (unsigned int)evt);
    if (!pdev || !params) return MESHX_INVALID_ARG;

    if (evt == CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF)
    {
        const auto *msg = static_cast<const meshx_gen_on_off_cli_msg_t *>(params);
        MESHX_LOGD(MODULE_ID_ELEMENT_SWITCH_RELAY_CLIENT, "Entering C++ Relay Client TO_BLE Callback: target_el_id=%d", msg->element_id);

        auto *el = meshXElementRegistry::get_instance().find_and_cast<meshXRelayClientElement>(
            msg->element_id, MESHX_ELEMENT_TYPE_RELAY_CLIENT);
        if (!el) {
            // Check if the element exists at all but is of a different variant
            if (!meshXElementRegistry::get_instance().find_element(msg->element_id)) {
                MESHX_LOGW(MODULE_ID_ELEMENT_SWITCH_RELAY_CLIENT, "Relay Client [%d] not found in registry!", msg->element_id);
                auto all = meshXElementRegistry::get_instance().get_all_elements();
                for (auto const& [id, ptr] : all) {
                    MESHX_LOGI(MODULE_ID_ELEMENT_SWITCH_RELAY_CLIENT, "  Registry contains ID [%d], variant %d", id, ptr->get_element_variant());
                }
            }
            return MESHX_SUCCESS;
        }

        MESHX_LOGI(MODULE_ID_ELEMENT_SWITCH_RELAY_CLIENT, "Relay Cli [%d]: pub_addr=0x%04x, app_id=%d",
                   msg->element_id, el->element_ctx.pub_addr, el->element_ctx.app_id);

        if (el->element_ctx.pub_addr == MESHX_ADDR_UNASSIGNED)
        {
            MESHX_LOGW(MODULE_ID_ELEMENT_SWITCH_RELAY_CLIENT,
                       "Relay Cli [%d]: no pub addr", msg->element_id);
            return MESHX_INVALID_STATE;
        }

        /* Route through the model's request_onoff to leverage the centralized logic */
        auto &models = el->get_sig_models();
        if (models.empty()) return MESHX_INVALID_STATE;

        auto *onoff_model = static_cast<meshXGenericOnOffClientModel *>(models[0].get());
        return onoff_model->request_onoff(msg);
    }
    return MESHX_SUCCESS;
}

#endif /* CONFIG_RELAY_CLIENT_COUNT */

/************************************************************************************
 * C-linkage factory functions — used by composition.c dispatch table
 * These replace the legacy meshx_create_relay_elements / create_relay_client_elements
 * from elements_c/ path.
 ************************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_RELAY_SERVER_COUNT > 0
/**
 * @brief C++ factory for Relay Server elements.
 *
 * Constructs CONFIG_RELAY_SERVER_COUNT meshXRelayServerElement objects and
 * registers each with the platform BLE Mesh composition.
 *
 * @param pdev         Pointer to the device structure.
 * @param element_cnt  Number of relay server elements to create.
 *                     Capped by CONFIG_RELAY_SERVER_COUNT.
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t create_relay_server_elements_cpp(dev_struct_t *pdev, uint16_t element_cnt)
{
    return meshx_element_factory_helper<meshXRelayServerElement, meshx_relay_srv_el_ctx_t>(
        pdev,
        element_cnt,
        CONFIG_RELAY_SERVER_COUNT,
        MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER,
        "Relay Srv"
    );
}
#endif /* CONFIG_RELAY_SERVER_COUNT > 0 */

#if CONFIG_RELAY_CLIENT_COUNT > 0
/**
 * @brief C++ factory for Relay Client elements.
 *
 * Constructs CONFIG_RELAY_CLIENT_COUNT meshXRelayClientElement objects and
 * registers each with the platform BLE Mesh composition.
 *
 * @param pdev         Pointer to the device structure.
 * @param element_cnt  Number of relay client elements to create.
 *                     Capped by CONFIG_RELAY_CLIENT_COUNT.
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t create_relay_client_elements_cpp(dev_struct_t *pdev, uint16_t element_cnt)
{
    return meshx_element_factory_helper<meshXRelayClientElement, meshx_relay_cli_el_ctx_t>(
        pdev,
        element_cnt,
        CONFIG_RELAY_CLIENT_COUNT,
        MODULE_ID_ELEMENT_SWITCH_RELAY_CLIENT,
        "Relay Cli"
    );
}
REG_MESHX_ELEMENT_FN(relay_cli_el, MESHX_ELEMENT_TYPE_RELAY_CLIENT, create_relay_client_elements_cpp);
#endif /* CONFIG_RELAY_CLIENT_COUNT > 0 */

#if CONFIG_RELAY_SERVER_COUNT > 0
REG_MESHX_ELEMENT_FN(relay_srv_el, MESHX_ELEMENT_TYPE_RELAY_SERVER, create_relay_server_elements_cpp);
#endif

#if CONFIG_ENABLE_UNIT_TEST
meshx_err_t meshx_relay_el_set_state(uint16_t el_id, bool ack)
{
    auto *el = meshXElementRegistry::get_instance().find_and_cast<meshXRelayClientElement>(
        el_id, MESHX_ELEMENT_TYPE_RELAY_CLIENT);
    if (!el) return MESHX_NOT_FOUND;

    meshx_gen_on_off_cli_msg_t msg = {};
    msg.ack        = (uint8_t)(ack ? 1 : 0);
    msg.set_get    = MESHX_GEN_ON_OFF_CLI_MSG_SET;
    msg.reserved   = 0;
    msg.element_id = el_id;

    // We use the model's next_on_off state internally as the target for the SET operation.
    // This allows the UT command to set the target state in the context before triggering.
    msg.on_off     = el->get_ctx().gen_on_off_state.next_on_off;

    return control_task_msg_publish(
            CONTROL_TASK_MSG_CODE_TO_BLE,
            CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF,
            &msg,
            sizeof(msg));
}

meshx_err_t meshx_relay_el_get_state(uint16_t el_id)
{
    meshx_gen_on_off_cli_msg_t msg = {};
    msg.ack        = MESHX_GEN_ON_OFF_CLI_MSG_ACK;
    msg.set_get    = MESHX_GEN_ON_OFF_CLI_MSG_GET;
    msg.reserved   = 0;
    msg.element_id = el_id;

    return control_task_msg_publish(
            CONTROL_TASK_MSG_CODE_TO_BLE,
            CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF,
            &msg,
            sizeof(msg));
}

meshx_err_t relay_cli_ut_handler(int cmd_id, int argc, char **argv)
{
    MESHX_LOGI(MODULE_ID_ELEMENT_SWITCH_RELAY_CLIENT, "Entering C++ Relay Client UT Handler: cmd_id=%d, argc=%d", cmd_id, argc);
    if (argc < 1) return MESHX_INVALID_ARG;
    uint16_t el_id = UT_GET_ARG(0, uint16_t, argv);

    if (cmd_id == 0x01) { // SET
        return meshx_relay_el_set_state(el_id, true);
    } else if (cmd_id == 0x02) { // SET UNACK
        return meshx_relay_el_set_state(el_id, false);
    } else { // GET
        return meshx_relay_el_get_state(el_id);
    }
}

meshx_err_t relay_srv_ut_handler(int cmd_id, int argc, char **argv)
{
    MESHX_LOGI(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Entering C++ Relay Server UT Handler: cmd_id=%d, argc=%d", cmd_id, argc);
    // Server UT can be used to query local state
    if (argc < 1) return MESHX_INVALID_ARG;
    uint16_t el_id = UT_GET_ARG(0, uint16_t, argv);

    auto *el = meshXElementRegistry::get_instance().find_and_cast<meshXRelayServerElement>(
        el_id, MESHX_ELEMENT_TYPE_RELAY_SERVER);
    if (!el) return MESHX_NOT_FOUND;

    if (cmd_id == 0x01) {
        uint8_t expected = UT_GET_ARG(1, uint8_t, argv);
        uint8_t actual = el->get_state();
        MESHX_LOGI(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Relay Server [%d] state: %d, expected: %d", el_id, actual, expected);
        return (actual == expected) ? MESHX_SUCCESS : MESHX_FAIL;
    }

    MESHX_LOGI(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Relay Server [%d] state: %d", el_id, el->get_state());
    return MESHX_SUCCESS;
}

#endif

#ifdef __cplusplus
} /* extern "C" */
#endif
