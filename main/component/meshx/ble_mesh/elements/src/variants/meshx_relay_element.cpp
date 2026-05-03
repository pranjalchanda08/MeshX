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
#include <new>
#include <meshx_nvs.h>
#include <meshx_api.h>
#include <interface/ble_mesh/meshx_ble_mesh_cmn.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "meshx_gen_client.h"
#if CONFIG_ENABLE_CONFIG_SERVER
#include "meshx_config_server.h"
#define CONFIG_SERVER_CB_MASK   \
    CONTROL_TASK_MSG_EVT_PUB_ADD  \
  | CONTROL_TASK_MSG_EVT_PUB_DEL  \
  | CONTROL_TASK_MSG_EVT_APP_KEY_BIND
#endif /* CONFIG_ENABLE_CONFIG_SERVER */
#if CONFIG_ENABLE_PROVISIONING
#include "meshx_prov_srv.h"
#endif /* CONFIG_ENABLE_PROVISIONING */
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

#define MESHX_GEN_ON_OFF_CLI_MSG_ACK    1
#define MESHX_GEN_ON_OFF_CLI_MSG_NO_ACK 0

#define RELAY_SRV_TO_BLE_EVT_MASK   CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF_SRV
#define RELAY_CLI_TO_BLE_EVT_MASK   CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF


/************************************************************************************
 * meshXRelayServerElement
 ************************************************************************************/
#if CONFIG_RELAY_SERVER_COUNT > 0

std::once_flag meshXRelayServerElement::s_callbacks_registered;

/**
 * @brief Register class-level callbacks (called exactly once via once_flag).
 */
void meshXRelayServerElement::register_class_callbacks()
{
    meshx_err_t err;

#if CONFIG_ENABLE_CONFIG_SERVER
    err = meshx_config_server_cb_reg(
        (config_srv_cb_t)&meshXRelayServerElement::s_config_srv_cb,
        CONFIG_SERVER_CB_MASK);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER,
                   "Relay Srv: config server cb reg failed: %d", err);
    }
#endif

#if CONFIG_ENABLE_PROVISIONING
    err = meshx_prov_srv_reg_el_server_cb(
        (prov_srv_cb_t)&meshXRelayServerElement::s_prov_cb);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER,
                   "Relay Srv: prov cb reg failed: %d", err);
    }
#endif

    err = control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_TO_BLE,
        RELAY_SRV_TO_BLE_EVT_MASK,
        (control_task_msg_handle_t)&meshXRelayServerElement::s_to_ble_cb);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER,
                   "Relay Srv: to_ble cb reg failed: %d", err);
    }
}

/**
 * @brief Constructs a new meshXRelayServerElement instance.
 */
MESHX_RELAY_SERVER_ELEMENT_TEMPLATE_PROTO
meshXRelayServerElement::meshXRelayServerElement (uint16_t element_idx)
    : meshXElementServer(element_idx)
{
    this->register_element_ctx(&element_ctx, sizeof(meshx_relay_srv_el_ctx_t));
    this->set_element_variant(MESHX_ELEMENT_TYPE_RELAY_SERVER);
    std::call_once(s_callbacks_registered, register_class_callbacks);
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

/* Task A+B — element_state_change_notify: NVS save + app notify */
MESHX_RELAY_SERVER_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXRelayServerElement::element_state_change_notify(meshx_ptr_t param, size_t param_size)
{
    if (!param)
        return MESHX_INVALID_ARG;

    auto *msg = static_cast<meshx_on_off_srv_el_msg_t *>(param);
    uint16_t element_id = msg->header.model.el_id;

    /* Update local context */
    element_ctx.gen_on_off_state.on_off = msg->state.on_off;

    /* Task B — NVS persistence */
    meshx_err_t err = meshx_nvs_element_ctx_set(
        element_id, &element_ctx, sizeof(element_ctx));
    if (err)
    {
        MESHX_LOGW(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER,
                   "Relay Srv [%d]: NVS save failed: %d", element_id, err);
    }

    /* Task B — App notification */
    meshx_api_relay_server_evt_t app_evt = {
        .on_off = element_ctx.gen_on_off_state.on_off
    };
    return meshx_send_msg_to_app(
        element_id,
        MESHX_ELEMENT_TYPE_RELAY_SERVER,
        MESHX_ELEMENT_FUNC_ID_RELAY_SERVER_ONN_OFF,
        sizeof(app_evt),
        &app_evt);
}

/* Task C — Config server callback (appkey bind + publication address) */
#if CONFIG_ENABLE_CONFIG_SERVER
MESHX_RELAY_SERVER_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXRelayServerElement::s_config_srv_cb(
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

    auto *el = meshXElementRegistry::get_instance().find_and_cast<meshXRelayServerElement>(
        element_id, MESHX_ELEMENT_TYPE_RELAY_SERVER);
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
                                   sizeof(meshx_relay_srv_el_ctx_t));
    }
    return MESHX_SUCCESS;
}
#endif /* CONFIG_ENABLE_CONFIG_SERVER */

/* Task D — Provisioning server callback (re-publish state after provisioning) */
#if CONFIG_ENABLE_PROVISIONING
MESHX_RELAY_SERVER_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXRelayServerElement::s_prov_cb(
        const dev_struct_t      *pdev,
        control_task_msg_evt_t   evt,
        const void              *params)
{
    MESHX_UNUSED(params);
    if (!pdev) return MESHX_INVALID_ARG;

    switch (evt)
    {
    case CONTROL_TASK_MSG_EVT_EN_NODE_PROV:
    {
        auto elements = meshXElementRegistry::get_instance().get_all_elements();
        for (auto const& [abs_id, base_el] : elements)
        {
            if (!base_el || base_el->get_element_variant() != MESHX_ELEMENT_TYPE_RELAY_SERVER) continue;
            auto *el = static_cast<meshXRelayServerElement *>(base_el);
            if (!el || el->element_ctx.pub_addr == MESHX_ADDR_UNASSIGNED) continue;

            auto &models = el->get_sig_models();
            auto *onoff  = static_cast<meshXGenericOnOffServerModel *>(models[0].get());

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
        }
        break;
    }
    default:
        MESHX_LOGW(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER,
                   "Relay Srv: unhandled prov evt: %p", (void *)evt);
        break;
    }
    return MESHX_SUCCESS;
}
#endif /* CONFIG_ENABLE_PROVISIONING */

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

std::once_flag meshXRelayClientElement::s_callbacks_registered;

/**
 * @brief Register class-level callbacks (called exactly once via once_flag).
 */
void meshXRelayClientElement::register_class_callbacks()
{
    meshx_err_t err;

#if CONFIG_ENABLE_CONFIG_SERVER
    err = meshx_config_server_cb_reg(
        (config_srv_cb_t)&meshXRelayClientElement::s_config_srv_cb,
        CONFIG_SERVER_CB_MASK);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_CLIENT,
                   "Relay Cli: config server cb reg failed: %d", err);
    }
#endif

#if CONFIG_ENABLE_PROVISIONING
    err = meshx_prov_srv_reg_el_client_cb(
        (prov_srv_cb_t)&meshXRelayClientElement::s_prov_cb);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_CLIENT,
                   "Relay Cli: prov cb reg failed: %d", err);
    }
#endif

    err = control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_TO_BLE,
        RELAY_CLI_TO_BLE_EVT_MASK,
        (control_task_msg_handle_t)&meshXRelayClientElement::s_to_ble_cb);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_CLIENT,
                   "Relay Cli: to_ble cb reg failed: %d", err);
    }
}

/**
 * @brief Constructs a new meshXRelayClientElement instance.
 */
MESHX_RELAY_CLIENT_ELEMENT_TEMPLATE_PROTO
meshXRelayClientElement::meshXRelayClientElement (uint16_t element_idx)
    : meshXElementClient(element_idx)
{
    this->register_element_ctx(&element_ctx, sizeof(meshx_relay_cli_el_ctx_t));
    this->set_element_variant(MESHX_ELEMENT_TYPE_RELAY_CLIENT);
    std::call_once(s_callbacks_registered, register_class_callbacks);
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

/* Task A+B — element_state_change_notify: app notify (NVS not required for client state) */
MESHX_RELAY_CLIENT_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXRelayClientElement::element_state_change_notify(meshx_ptr_t param, size_t param_size)
{
    if (!param)
        return MESHX_INVALID_ARG;

    auto *msg       = static_cast<meshx_on_off_cli_el_msg_t *>(param);
    uint16_t element_id = msg->header.model.el_id;

    /* Update local context */
    element_ctx.gen_on_off_state.on_off = msg->state.on_off;

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

/* Task C — Config server callback */
#if CONFIG_ENABLE_CONFIG_SERVER
MESHX_RELAY_CLIENT_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXRelayClientElement::s_config_srv_cb(
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

    auto *el = meshXElementRegistry::get_instance().find_and_cast<meshXRelayClientElement>(
        element_id, MESHX_ELEMENT_TYPE_RELAY_CLIENT);
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
                                   sizeof(meshx_relay_cli_el_ctx_t));
    }
    return MESHX_SUCCESS;
}
#endif /* CONFIG_ENABLE_CONFIG_SERVER */

/* Task D — Provisioning client callback (send GET to refresh state on fresh boot) */
#if CONFIG_ENABLE_PROVISIONING
MESHX_RELAY_CLIENT_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXRelayClientElement::s_prov_cb(
        const dev_struct_t      *pdev,
        control_task_msg_evt_t   evt,
        const void              *params)
{
    MESHX_UNUSED(params);
    if (!pdev) return MESHX_INVALID_ARG;

    switch (evt)
    {
    case CONTROL_TASK_MSG_EVT_SYSTEM_FRESH_BOOT:
    {
        auto elements = meshXElementRegistry::get_instance().get_all_elements();
        for (auto const& [abs_id, base_el] : elements)
        {
            if (!base_el || base_el->get_element_variant() != MESHX_ELEMENT_TYPE_RELAY_CLIENT) continue;
            auto *el = static_cast<meshXRelayClientElement *>(base_el);
            if (!el) continue;

            meshx_gen_on_off_cli_msg_t msg = {
                .element_id = abs_id,
                .ack        = MESHX_GEN_ON_OFF_CLI_MSG_ACK,
                .set_get    = MESHX_GEN_ON_OFF_CLI_MSG_GET
            };
            control_task_msg_publish(CONTROL_TASK_MSG_CODE_TO_BLE,
                CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF, &msg, sizeof(msg));
        }
    }
    break;
    default:
        MESHX_LOGW(MODULE_ID_ELEMENT_SWITCH_RELAY_CLIENT,
                   "Relay Cli: unhandled prov evt: %p", (void *)evt);
        break;
    }
    return MESHX_SUCCESS;
}
#endif /* CONFIG_ENABLE_PROVISIONING */

/* Task E — TO_BLE client handler: receives app command, sends OnOff via model */
MESHX_RELAY_CLIENT_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXRelayClientElement::s_to_ble_cb(
        const dev_struct_t      *pdev,
        control_task_msg_evt_t   evt,
        const void              *params)
{
    if (!pdev || !params) return MESHX_INVALID_ARG;

    if (evt == CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF)
    {
        const auto *msg = static_cast<const meshx_gen_on_off_cli_msg_t *>(params);
        auto *el = meshXElementRegistry::get_instance().find_and_cast<meshXRelayClientElement>(
            msg->element_id, MESHX_ELEMENT_TYPE_RELAY_CLIENT);
        if (!el) return MESHX_SUCCESS;

        if (el->element_ctx.pub_addr == MESHX_ADDR_UNASSIGNED)
        {
            MESHX_LOGW(MODULE_ID_ELEMENT_SWITCH_RELAY_CLIENT,
                       "Relay Cli [%d]: no pub addr", msg->element_id);
            return MESHX_INVALID_STATE;
        }

        /* Route through the model's model_send to leverage the C++ model stack */
        auto &models = el->get_sig_models();
        if (models.empty()) return MESHX_INVALID_STATE;

        auto *onoff_model = static_cast<meshXGenericOnOffClientModel *>(models[0].get());

        uint16_t opcode = (msg->set_get == MESHX_GEN_ON_OFF_CLI_MSG_GET)
            ? MESHX_MODEL_OP_GEN_ONOFF_GET
            : (msg->ack ? MESHX_MODEL_OP_GEN_ONOFF_SET : MESHX_MODEL_OP_GEN_ONOFF_SET_UNACK);

        meshx_model_t model_ref = {
            .el_id    = msg->element_id,
            .model_id = 0,
            .pub_addr = el->element_ctx.pub_addr,
            .p_model  = nullptr
        };
        meshx_ctx_t ctx = {
            .app_idx  = el->element_ctx.app_id,
            .net_idx  = pdev->meshx_store.net_key_id,
            .opcode   = opcode,
            .src_addr = 0,
            .dst_addr = el->element_ctx.pub_addr,
            .p_ctx    = nullptr
        };
        meshx_gen_onoff_send_params_t send_params = {
            .model = &model_ref,
            .ctx   = &ctx,
            .state = { .on_off = el->element_ctx.gen_on_off_state.on_off },
            .tid   = 0
        };
        return onoff_model->model_send(&send_params);
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
#endif /* CONFIG_RELAY_CLIENT_COUNT > 0 */

#ifdef __cplusplus
} /* extern "C" */
#endif
