/**
 * @file meshx_model_config.cpp
 * @brief Implementation of Config Server Model.
 */

#include <common_model/meshx_model_config.hpp>

#if CONFIG_ENABLE_CONFIG_SERVER

/**
 * @brief Creates and initializes a config server model instance.
 *
 * This function handles the platform-specific model creation process for config server models.
 * It initializes config server-specific features and cannot be overridden by derived classes.
 *
 * @return meshx_err_t Returns an error code indicating the result of the operation.
 *         - MESHX_SUCCESS on successful model creation and initialization
 *         - MESHX_ERR_NO_MEM if memory allocation fails
 *         - Other error codes for platform-specific failures
 */
MESHX_CONFIG_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXConfigModel MESHX_CONFIG_SERVER_MODEL_TEMPLATE_PARAMS::plat_model_create(MESHX_MODEL* p_plat_model_ptr)
{
    meshx_err_t err = MESHX_SUCCESS;
    
    if (p_plat_model_ptr) {
        this->set_plat_model(p_plat_model_ptr);
    }

    MESHX_MODEL* p_use = (MESHX_MODEL*)this->get_plat_model();
    if (!p_use)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "No platform model pointer available");
        return MESHX_INVALID_STATE;
    }

    // For config server, we use the platform's get_config_srv_model function
    // to get the model instance since there's no specific create function
    err = meshx_plat_get_config_srv_model(p_use);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to get Config Server Model");
        return err;
    }

    // Config server doesn't use pub/gen structures like other models
    // Set them to nullptr to indicate they're not used
    this->set_pub_struct(nullptr);
    this->set_gen_struct(nullptr);

    return MESHX_SUCCESS;
}

/**
 * @brief Deletes the Config Server model and its associated resources.
 *
 * This function frees the memory allocated for the Config Server
 * and sets the pointer to NULL. For config server, this is mainly
 * a cleanup operation since the model is managed by the platform.
 *
 * @return
 *     - MESHX_SUCCESS: Model deleted successfully.
 *     - MESHX_FAIL: Failed to delete the model.
 */
MESHX_CONFIG_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXConfigModel MESHX_CONFIG_SERVER_MODEL_TEMPLATE_PARAMS::plat_model_delete(void)
{
    // Config server is managed by the platform, so we don't need to
    // explicitly delete it. Just clean up our references.
    this->set_pub_struct(nullptr);
    this->set_gen_struct(nullptr);

    return MESHX_SUCCESS;
}

/**
 * @brief Send a packet to the MeshX stack based on the given parameters
 *
 * This function takes a pointer to a meshx_config_send_params_t structure as input
 * and sends the corresponding packet to the MeshX stack for processing.
 * For config server, this is typically used for sending status responses.
 *
 * @param[in] params Pointer to the meshx_config_send_params_t structure containing the parameters
 * @return MESHX_SUCCESS on success, or an error code on failure
 */
MESHX_CONFIG_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXConfigModel MESHX_CONFIG_SERVER_MODEL_TEMPLATE_PARAMS::model_send(meshx_config_send_params_t *params)
{
    if (!params || !params->model || !params->ctx)
    {
        return MESHX_INVALID_ARG;
    }

    // For config server, we use the base model's send functionality
    // The config server send params structure is minimal (just a stub)
    meshx_config_server_send_params_t send_params = {
        .p_model   = params->model,
        .p_ctx     = params->ctx,
        .p_data    = &params->stub,  // Use stub field as data
        .data_len  = sizeof(params->stub)
    };

    return this->get_base_model()->plat_send_msg(&send_params);
}

/**
 * @brief This function is the callback for the Config Server model.
 *
 * This function is triggered whenever a Config Server message is received from the MeshX stack.
 * It handles the Config Server state change notifications from the MeshX stack and publishes
 * the state change event to the element layer.
 *
 * @param[in] p_dev       A pointer to the device structure.
 * @param[in] model_id    The unique identifier of the BLE mesh model.
 * @param[in] params      A pointer to the callback parameter structure containing the details of the
 *                        received message.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_INVALID_ARG: Invalid argument
 *    - MESHX_FAIL: Other failures
 */
#include <meshx_element_registry.hpp>

#include <variants/meshx_relay_element.hpp>

MESHX_CONFIG_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXConfigModel MESHX_CONFIG_SERVER_MODEL_TEMPLATE_PARAMS::model_from_ble_cb(
    dev_struct_t *p_dev,
    evt_model_id_t model_id,
    meshx_ptr_t params)
{
    if(!params || !p_dev)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid parameters");
        return MESHX_INVALID_ARG;
    }
    if(model_id != MESHX_MODEL_ID_CONFIG_SRV)
    {
        /* Callback triggered not for this model */
        return MESHX_SUCCESS;
    }

    auto const *param = static_cast<meshx_config_srv_cb_param_t *>(params);

    MESHX_LOGI(MODULE_ID_MODEL_SERVER, "Config Event: op=0x%04x, src=0x%04x, dst=0x%04x",
               param->ctx.opcode, param->ctx.src_addr, param->ctx.dst_addr);

    /* Sync publication address and app key index to the targeted element's context */
    uint16_t target_addr = 0;
    uint16_t new_pub_addr = MESHX_ADDR_UNASSIGNED;
    uint16_t new_app_idx = 0;
    bool sync_required = false;

    if (param->ctx.opcode == MESHX_MODEL_OP_MODEL_PUB_SET) 
    {
        target_addr  = param->state_change.mod_pub_set.element_addr;
        new_pub_addr = param->state_change.mod_pub_set.pub_addr;
        new_app_idx  = param->state_change.mod_pub_set.app_idx;
        sync_required = true;
        MESHX_LOGI(MODULE_ID_MODEL_SERVER, "  Syncing PubAddr: el_addr=0x%04x -> pub=0x%04x, app_idx=%d", 
                   target_addr, new_pub_addr, new_app_idx);
    }
    else if (param->ctx.opcode == MESHX_MODEL_OP_MODEL_APP_BIND)
    {
        target_addr  = param->state_change.mod_app_bind.element_addr;
        new_app_idx  = param->state_change.mod_app_bind.app_idx;
        sync_required = true;
        MESHX_LOGI(MODULE_ID_MODEL_SERVER, "  Syncing AppBind: el_addr=0x%04x -> app_idx=%d", 
                   target_addr, new_app_idx);
    }
    else if (param->ctx.opcode == MESHX_MODEL_OP_MODEL_APP_UNBIND)
    {
        target_addr  = param->state_change.mod_app_unbind.element_addr;
        new_app_idx  = param->state_change.mod_app_unbind.app_idx;
        sync_required = true;
        MESHX_LOGI(MODULE_ID_MODEL_SERVER, "  Syncing AppUnbind: el_addr=0x%04x -> app_idx=%d", 
                   target_addr, new_app_idx);
    }

    if (sync_required)
    {
        uint16_t el_id = target_addr - p_dev->meshx_store.node_addr;
        auto *el = meshXElementRegistry::get_instance().find_element(el_id);
        if (el && el->get_element_ctx())
        {
            meshx_element_type_t variant = el->get_element_variant();
            if (variant == MESHX_ELEMENT_TYPE_RELAY_CLIENT || variant == MESHX_ELEMENT_TYPE_RELAY_SERVER) {
                // Relay Client and Server contexts share the same header structure for pub/app fields
                auto *ctx = static_cast<meshx_relay_cli_el_ctx_t*>(el->get_element_ctx());
                if (param->ctx.opcode == MESHX_MODEL_OP_MODEL_PUB_SET) {
                    ctx->pub_addr = new_pub_addr;
                }
                ctx->app_id = new_app_idx;
                
                meshx_nvs_element_ctx_set(el_id, variant, ctx, el->get_element_ctx_size());
                meshx_nvs_commit(); // Force immediate commit for critical config changes
                MESHX_LOGI(MODULE_ID_MODEL_SERVER, "  Element [%d] context updated and committed to NVS (variant: %d)", el_id, variant);
            }
        }
    }

    // Store the message in member variable for later use by prepare_element_msg
    element_msg = {
        .header = {
            .model                  = param->model,
            .element_state_change   = MESHX_SUCCESS,
        },
        .state = { .state_change = param->state_change }
    };

    /* Map opcode to legacy config event for publishing */
    config_evt_t config_evt = CONTROL_TASK_MSG_EVT_CONFIG_ALL;
    switch (param->ctx.opcode)
    {
        case MESHX_MODEL_OP_APP_KEY_ADD:    config_evt = CONTROL_TASK_MSG_EVT_APP_KEY_ADD; break;
        case MESHX_MODEL_OP_NET_KEY_ADD:    config_evt = CONTROL_TASK_MSG_EVT_NET_KEY_ADD; break;
        case MESHX_MODEL_OP_MODEL_SUB_ADD:  config_evt = CONTROL_TASK_MSG_EVT_SUB_ADD; break;
        case MESHX_MODEL_OP_MODEL_PUB_SET:  config_evt = CONTROL_TASK_MSG_EVT_PUB_ADD; break;
        case MESHX_MODEL_OP_MODEL_APP_BIND: config_evt = CONTROL_TASK_MSG_EVT_APP_KEY_BIND; break;
        case MESHX_MODEL_OP_NET_KEY_DELETE: config_evt = CONTROL_TASK_MSG_EVT_NET_KEY_DEL; break;
        case MESHX_MODEL_OP_APP_KEY_DELETE: config_evt = CONTROL_TASK_MSG_EVT_APP_KEY_DEL; break;
        case MESHX_MODEL_OP_MODEL_SUB_DELETE: config_evt = CONTROL_TASK_MSG_EVT_SUB_DEL; break;
        case MESHX_MODEL_OP_MODEL_APP_UNBIND: config_evt = CONTROL_TASK_MSG_EVT_APP_KEY_UNBIND; break;
        default: break;
    }

    if (config_evt != CONTROL_TASK_MSG_EVT_CONFIG_ALL)
    {
        control_task_msg_publish(
            CONTROL_TASK_MSG_CODE_CONFIG,
            config_evt,
            param,
            sizeof(meshx_config_srv_cb_param_t));
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Prepare message for element notification
 * @details Returns pointer to the stored element message
 *
 * @param[out] msg_ptr   Pointer to message structure (output parameter)
 * @param[out] msg_size  Size of the message structure (output parameter)
 * @return MESHX_SUCCESS if message prepared successfully, error code otherwise
 */
MESHX_CONFIG_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXConfigModel MESHX_CONFIG_SERVER_MODEL_TEMPLATE_PARAMS::prepare_element_msg(
    meshx_ptr_t *msg_ptr,
    size_t *msg_size)
{
    if (!msg_ptr || !msg_size)
    {
        return MESHX_INVALID_ARG;
    }

    *msg_ptr  = &element_msg;
    *msg_size = sizeof(element_msg);

    return MESHX_SUCCESS;
}

/**
 * @brief Constructor for meshXConfigModel class
 *
 * @param[in]   parent_element  Pointer to parent element
 */
meshXConfigModel::meshXConfigModel(
        meshXElementIF *parent_element,
        meshx_ptr_t     parent_element_state,
        uint16_t        model_func_id
    )
    : meshXServerModel(nullptr, MESHX_MODEL_ID_CONFIG_SRV, parent_element, parent_element_state, model_func_id)
{
}

#endif /* CONFIG_ENABLE_CONFIG_SERVER */

/**
 * Legacy support for Config Server initialization and callback registration.
 * These are provided to support the transition away from elements_c.
 */
extern "C" meshx_err_t meshx_init_config_server(void)
{
#if CONFIG_ENABLE_CONFIG_SERVER
    /* In the new architecture, the config server is initialized as part of the root element models.
     * We just need to ensure the platform layer is initialized.
     */
    return meshx_plat_config_srv_init();
#else
    return MESHX_NOT_SUPPORTED;
#endif
}

extern "C" meshx_err_t meshx_config_server_cb_reg(config_srv_cb_t cb, uint32_t config_evt_bmap)
{
    if (cb == NULL || config_evt_bmap == 0)
    {
        return MESHX_INVALID_ARG;
    }

    return control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_CONFIG,
        config_evt_bmap,
        (control_task_msg_handle_t)cb);
}

extern "C" meshx_err_t meshx_get_config_srv_model(meshx_ptr_t p_model)
{
    return meshx_plat_get_config_srv_model(p_model);
}
