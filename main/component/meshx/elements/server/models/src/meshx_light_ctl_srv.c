/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_light_ctl_srv.c
 * @brief Implementation of the Light CTL Server model for BLE Mesh Node.
 *
 * This file contains the implementation of the Light CTL (Color Temperature Light) Server model
 * for the BLE Mesh Node. The Light CTL Server model is responsible for controlling the color
 * temperature and lightness of a light.
 *
 * @author Pranjal Chanda
 */

#include <meshx_light_ctl_srv.h>

#define MESHX_SERVER_INIT_MAGIC_NO 0x2483

#if CONFIG_ENABLE_LIGHT_CTL_SERVER

/**
 * @brief Structure representing the MeshX Light CTL Setup Server.
 *
 * This structure holds the initialization state, model information, and a pointer
 * to the server model for the Light CTL (Color Temperature Lightness) Setup Server.
 */
typedef struct meshx_light_ctl_setup_server
{
    uint16_t ctl_setup_server_init;
    MESHX_MODEL ctl_setup_srv_model;
    meshx_ctl_setup_server_model_t *meshx_srv_model;
}meshx_light_ctl_setup_server_t;

/**
 * @brief Structure to maintain the initialization and creation state of the MeshX Light CTL Server.
 *
 * This static structure holds flags indicating whether the CTL Setup Server and CTL Lighting Server
 * have been initialized, as well as whether the CTL Lighting Server has been created.
 */
static struct
{
    uint16_t ctl_lighting_server_init;
    uint16_t ctl_lighting_server_created;
    meshx_light_ctl_setup_server_t ctl_setup_server;
}g_meshx_light_ctl_srv = {
    .ctl_setup_server = {
        .ctl_setup_server_init = 0,
        .meshx_srv_model = NULL,
    },
    .ctl_lighting_server_init = 0,
    .ctl_lighting_server_created = 0,
};

/**
 * @brief Perform hardware change for the light control server model.
 *
 * This function is responsible for performing the necessary hardware changes
 * based on the parameters provided by the BLE Mesh lighting server callback.
 *
 * @param param Pointer to the BLE Mesh lighting server callback parameters.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_FAIL: Failure
 */
static meshx_err_t meshx_state_change_notify(meshx_lighting_server_cb_param_t *param)
{
    meshx_light_ctl_srv_t ctl_srv_params = {
        .model = param->model};

    if (MESHX_ADDR_IS_UNICAST(param->ctx.dst_addr) || (MESHX_ADDR_BROADCAST(param->ctx.dst_addr)) || (MESHX_ADDR_IS_GROUP(param->ctx.dst_addr) && (MESHX_SUCCESS == meshx_is_group_subscribed(param->model.p_model, param->ctx.dst_addr))))
    {
        switch (param->ctx.opcode)
        {
        case MESHX_MODEL_OP_LIGHT_CTL_SET:
        case MESHX_MODEL_OP_LIGHT_CTL_SET_UNACK:
            ctl_srv_params.state.delta_uv = param->state_change.ctl_set.delta_uv;
            ctl_srv_params.state.lightness = param->state_change.ctl_set.lightness;
            ctl_srv_params.state.temperature = param->state_change.ctl_set.temperature;
            break;
        case MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_GET:
        case MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_SET:
            ctl_srv_params.state.delta_uv = param->state_change.ctl_temp_set.delta_uv;
            ctl_srv_params.state.temperature = param->state_change.ctl_temp_set.temperature;
            break;
        default:
            return MESHX_NOT_SUPPORTED;
        }
        /* Send msg for hw manipulation */
        MESHX_LOGD(MODULE_ID_MODEL_SERVER, "HW change requested, Element_id: 0x%x",
                   param->model.el_id);

        meshx_err_t err = control_task_msg_publish(
            CONTROL_TASK_MSG_CODE_EL_STATE_CH,
            CONTROL_TASK_MSG_EVT_EL_STATE_CH_SET_CTL,
            &ctl_srv_params,
            sizeof(meshx_light_ctl_srv_t));
        return err;
    }
    return MESHX_NOT_SUPPORTED;
}

/**
 * @brief Handle Light CTL messages for the lighting server model.
 *
 * This function processes incoming Light CTL messages and performs the necessary
 * actions based on the message parameters.
 *
 * @param pdev Pointer to the device structure.
 * @param evt Event type of the control task message.
 * @param param Pointer to the BLE Mesh lighting server callback parameter structure.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_INVALID_ARG: Invalid argument
 *    - MESHX_FAIL: Other failures
 */
static meshx_err_t meshx_handle_light_ctl_msg(const dev_struct_t *pdev,
                                              const control_task_msg_evt_t evt,
                                              meshx_lighting_server_cb_param_t *param)
{
    if (!pdev || (evt != MESHX_MODEL_ID_LIGHT_CTL_SRV && evt != MESHX_MODEL_ID_LIGHT_CTL_SETUP_SRV))
        return MESHX_INVALID_ARG;
    meshx_err_t err = MESHX_SUCCESS;

    bool send_reply_to_src = false;
    bool state_change_notify = false;
    uint32_t op_code = param->ctx.opcode;
    uint32_t status_op = 0;
    switch (op_code)
    {
        case MESHX_MODEL_OP_LIGHT_CTL_STATUS:
        case MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_STATUS:
        case MESHX_MODEL_OP_LIGHT_CTL_DEFAULT_STATUS:
        case MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_STATUS:
            /* Ignore, as these are status messages */
            break;
        /*!< Light CTL Message Opcode */
        case MESHX_MODEL_OP_LIGHT_CTL_GET:
        case MESHX_MODEL_OP_LIGHT_CTL_SET:
        case MESHX_MODEL_OP_LIGHT_CTL_SET_UNACK:
            status_op = MESHX_MODEL_OP_LIGHT_CTL_STATUS;
            if (op_code != MESHX_MODEL_OP_LIGHT_CTL_GET)
                state_change_notify = true;

            if (op_code != MESHX_MODEL_OP_LIGHT_CTL_SET_UNACK)
                send_reply_to_src = true;

            break;
        case MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_GET:
        case MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_SET:
        case MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_SET_UNACK:
            status_op = MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_STATUS;
            if (op_code != MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_GET)
                state_change_notify = true;

            if (op_code != MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_SET_UNACK)
                send_reply_to_src = true;
            break;
        /*!< Light CTL Setup Message Opcode */
        case MESHX_MODEL_OP_LIGHT_CTL_DEFAULT_SET:
        case MESHX_MODEL_OP_LIGHT_CTL_DEFAULT_GET:
        case MESHX_MODEL_OP_LIGHT_CTL_DEFAULT_SET_UNACK:
            status_op = MESHX_MODEL_OP_LIGHT_CTL_DEFAULT_STATUS;
            if (op_code != MESHX_MODEL_OP_LIGHT_CTL_DEFAULT_GET)
                MESHX_DO_NOTHING;

            if (op_code != MESHX_MODEL_OP_LIGHT_CTL_DEFAULT_SET_UNACK)
                send_reply_to_src = true;

            break;
        case MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_SET:
        case MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_GET:
        case MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_SET_UNACK:
            status_op = MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_STATUS;
            if (op_code != MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_GET)
                MESHX_DO_NOTHING;

            if (op_code != MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_SET_UNACK)
                send_reply_to_src = true;

            break;
        default:
            MESHX_LOGW(MODULE_ID_MODEL_SERVER, "CTL Unhandled Event %p", (void *)param->ctx.opcode);
            break;
    }
    if (state_change_notify)
    {
        err = meshx_state_change_notify(param);
        if (err)
            return err;
    }
    if (send_reply_to_src
        /* This is meant to notify the respective publish client */
        || param->ctx.src_addr != param->model.pub_addr)
    {
        /* Here the message was received from unregistered source and mention the state to the respective client */
        MESHX_LOGD(MODULE_ID_MODEL_SERVER, "PUB: src|pub %x|%x", param->ctx.src_addr, param->model.pub_addr);
        param->ctx.opcode = (uint16_t)status_op;
        param->ctx.dst_addr = param->model.pub_addr;

        err = meshx_gen_light_srv_send_msg_to_ble(
            CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL_SRV,
            param
        );
    }

    return err;
}

/**
 * @brief Creates and initializes the Light CTL (Color Temperature Lightness) Setup Server.
 *
 * This function sets up the Light CTL Setup Server model, allocating necessary resources
 * and initializing internal state. It should be called during the provisioning or initialization
 * phase of the mesh node.
 *
 * @return meshx_err_t Returns MESHX_OK on success, or an appropriate error code on failure.
 */
static meshx_err_t meshx_light_ctl_setup_server_create(void)
{
    meshx_err_t err = MESHX_SUCCESS;
    if  (   g_meshx_light_ctl_srv.ctl_setup_server.meshx_srv_model != NULL
        ||  g_meshx_light_ctl_srv.ctl_setup_server.ctl_setup_server_init == MESHX_SERVER_INIT_MAGIC_NO)
        return MESHX_SUCCESS;

    g_meshx_light_ctl_srv.ctl_setup_server.meshx_srv_model =
        (meshx_ctl_setup_server_model_t *)MESHX_CALOC(1, sizeof(meshx_ctl_setup_server_model_t));
    if (NULL == g_meshx_light_ctl_srv.ctl_setup_server.meshx_srv_model)
    {
        return MESHX_NO_MEM;
    }

    err = meshx_plat_light_ctl_setup_srv_create(
        &g_meshx_light_ctl_srv.ctl_setup_server.ctl_setup_srv_model,
        &g_meshx_light_ctl_srv.ctl_setup_server.meshx_srv_model->meshx_server_pub,
        &g_meshx_light_ctl_srv.ctl_setup_server.meshx_srv_model->meshx_server_gen_srv);
    if(err != MESHX_SUCCESS)
    {
        /* Tops down deinit for CTL setup server model */
        meshx_light_ctl_setup_server_delete();
        return err;
    }
    else
    {
        g_meshx_light_ctl_srv.ctl_lighting_server_init++;
    }

    return err;
}

/**
 * @brief Deletes or deinitializes the Light CTL (Color Temperature Lightness) Setup Server instance.
 *
 * This function is responsible for cleaning up resources and performing any necessary teardown
 * for the Light CTL Setup Server in the MeshX framework.
 *
 * @return meshx_err_t Returns an error code indicating the result of the delete operation.
 */
static meshx_err_t meshx_light_ctl_setup_server_delete(void)
{
    if (g_meshx_light_ctl_srv.ctl_setup_server.meshx_srv_model != NULL)
    {
        MESHX_FREE(g_meshx_light_ctl_srv.ctl_setup_server.meshx_srv_model);
        g_meshx_light_ctl_srv.ctl_setup_server.meshx_srv_model = NULL;

        meshx_plat_light_srv_delete(
            &g_meshx_light_ctl_srv.ctl_setup_server.ctl_setup_srv_model,
            &g_meshx_light_ctl_srv.ctl_setup_server.meshx_srv_model->meshx_server_gen_srv);

        g_meshx_light_ctl_srv.ctl_setup_server.ctl_setup_srv_model = NULL;
    }

    g_meshx_light_ctl_srv.ctl_setup_server.ctl_setup_server_init = 0;
    return MESHX_SUCCESS;
}

/**
 * @brief Retrieves the CTL (Color Temperature Lightness) Setup Server model instance.
 *
 * This function is used to obtain a reference or handle to the CTL Setup Server model,
 * which is responsible for managing color temperature and lightness control in a mesh network.
 *
 * @param[in] p_model Pointer to the model structure or context.
 *
 * @return meshx_err_t Returns an error code indicating the result of the operation.
 *                     Typically, MESHX_OK on success or an appropriate error code on failure.
 */
meshx_err_t meshx_get_ctl_setup_srv_model(meshx_ptr_t p_model)
{
    if (!p_model)
        return MESHX_INVALID_ARG;

    if (g_meshx_light_ctl_srv.ctl_setup_server.meshx_srv_model == NULL)
        return MESHX_FAIL;

    memcpy(p_model, &g_meshx_light_ctl_srv.ctl_setup_server.ctl_setup_srv_model, sizeof(MESHX_MODEL));
    return MESHX_SUCCESS;
}
/**
 * @brief Initialize the Light CTL Server model.
 *
 * This function initializes the Light CTL (Color Temperature Light) Server model
 * for the BLE Mesh Node. It sets up the necessary configurations and state for
 * the Light CTL Server to operate correctly.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_FAIL: Failure
 */
meshx_err_t meshx_light_ctl_server_init(void)
{
    meshx_err_t err = MESHX_SUCCESS;

    if (g_meshx_light_ctl_srv.ctl_setup_server.ctl_setup_server_init == MESHX_SERVER_INIT_MAGIC_NO)
        return MESHX_SUCCESS;

    g_meshx_light_ctl_srv.ctl_setup_server.ctl_setup_server_init = MESHX_SERVER_INIT_MAGIC_NO;

    err = meshx_lighting_srv_init();
    if (err)
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to initialize prod server");

    err = meshx_lighting_reg_cb(
        MESHX_MODEL_ID_LIGHT_CTL_SRV,
        (meshx_lighting_server_cb)&meshx_handle_light_ctl_msg
    );
    if (err)
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to initialize meshx_gen_srv_reg_cb (Err: %d)", err);

    err = meshx_lighting_reg_cb(
        MESHX_MODEL_ID_LIGHT_CTL_SETUP_SRV,
        (meshx_lighting_server_cb)&meshx_handle_light_ctl_msg
    );
    if (err)
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to initialize meshx_gen_srv_reg_cb (Err: %d)", err);

    return err;
}

/**
 * @brief Create and initialize a new CTL server model instance.
 *
 * This function allocates memory for a new CTL server model and initializes
 * it using the platform-specific creation function. It ensures that the model
 * is properly set up for handling Generic OnOff messages in a BLE Mesh network.
 *
 * @param[in,out] p_model Pointer to a pointer where the newly created CTL server model
 *                instance will be stored.
 * @param[in,out] p_sig_model Pointer to a pointer where the offset of the model will be stored.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully created and initialized the model.
 *     - MESHX_INVALID_ARG: The provided pointer is NULL.
 *     - MESHX_NO_MEM: Memory allocation failed.
 */
meshx_err_t meshx_light_ctl_server_create(meshx_ctl_server_model_t **p_model, void *p_sig_model)
{
    meshx_err_t err = MESHX_SUCCESS;
    if (!p_model || !p_sig_model)
    {
        return MESHX_INVALID_ARG;
    }

    *p_model = (meshx_ctl_server_model_t *)MESHX_CALOC(1, sizeof(meshx_ctl_server_model_t));
    if (!*p_model)
    {
        return MESHX_NO_MEM;
    }

    err = meshx_plat_light_ctl_srv_create(
        p_sig_model,
        &((*p_model)->meshx_server_pub),
        &((*p_model)->meshx_server_gen_srv));
    if(err != MESHX_SUCCESS)
    {
        /* Tops down deinit for CTL server model */
        meshx_light_ctl_server_delete(p_model);
        return err;
    }

    if(     g_meshx_light_ctl_srv.ctl_lighting_server_created == 0
        &&  g_meshx_light_ctl_srv.ctl_setup_server.ctl_setup_server_init == 0)
    {
        MESHX_LOGI(MODULE_ID_MODEL_SERVER, "Creating CTL setup server model");
        /* Create CTL setup server */
        err = meshx_light_ctl_setup_server_create();
        if(err != MESHX_SUCCESS)
        {
            /* Tops down deinit for CTL server model */
            meshx_light_ctl_setup_server_delete();
            /* The code execution below is only possible if this is the very first model being created */
            meshx_light_ctl_server_delete(p_model);
            return err;
        }
        g_meshx_light_ctl_srv.ctl_setup_server.ctl_setup_server_init = MESHX_SERVER_INIT_MAGIC_NO;
    }
    else
    {
        g_meshx_light_ctl_srv.ctl_lighting_server_created++;
    }

    (*p_model)->meshx_server_sig_model = p_sig_model;

    return err;
}

/**
 * @brief Delete the CTL server model instance.
 *
 * This function deletes an instance of the CTL server model, freeing
 * associated resources and setting the model pointer to NULL.
 *
 * @param[in,out] p_model Double pointer to the CTL server model instance to be deleted.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully deleted the model.
 *     - MESHX_INVALID_ARG: Invalid argument provided.
 */
meshx_err_t meshx_light_ctl_server_delete(meshx_ctl_server_model_t **p_model)
{
    if (p_model == NULL || *p_model == NULL)
    {
        return MESHX_INVALID_ARG;
    }

    meshx_plat_light_srv_delete(
        &((*p_model)->meshx_server_pub),
        &((*p_model)->meshx_server_gen_srv));

    MESHX_FREE(*p_model);
    *p_model = NULL;
    if (g_meshx_light_ctl_srv.ctl_lighting_server_created == 0)
    {
        MESHX_LOGW(MODULE_ID_MODEL_SERVER, "Deleting CTL setup server model");
        /* Delete CTL setup server */
    }
    else
    {
        g_meshx_light_ctl_srv.ctl_lighting_server_created--;
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Restore the CTL state for the generic server model.
 *
 * This function restores the CTL state of the specified server model
 * using the provided state value. It checks for a valid model pointer
 * before proceeding with the restoration.
 *
 * @param p_model Pointer to the CTL server model structure.
 * @param ctl_state The CTL state to be restored.
 *
 * @return
 *     - MESHX_INVALID_STATE: If the model pointer is NULL.
 *     - Result of the platform-specific restoration function.
 */
meshx_err_t meshx_light_ctl_srv_state_restore(meshx_ctl_server_model_t *p_model, meshx_light_ctl_srv_state_t ctl_state)
{
    if (!p_model)
        return MESHX_INVALID_STATE;

    return meshx_plat_light_ctl_srv_restore(p_model->meshx_server_sig_model,
                                            ctl_state.delta_uv,
                                            ctl_state.lightness,
                                            ctl_state.temperature,
                                            ctl_state.temperature_range_max,
                                            ctl_state.temperature_range_min);
}
/**
 * @brief Send the Light CTL status message.
 *
 * This function sends the Light CTL status message to the specified context.
 *
 * @param[in] p_model       Pointer to the MeshX model structure.
 * @param[in] ctx           Context structure containing the necessary parameters for sending the message.
 * @param[in] delta_uv      The Delta UV value to be included in the status message.
 * @param[in] lightness     The Lightness value to be included in the status message.
 * @param[in] temperature   The Temperature value to be included in the status message.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully sent the status message.
 *     - Appropriate error code on failure.
 */
meshx_err_t meshx_light_ctl_srv_status_send( meshx_model_t *p_model,
                                             meshx_ctx_t *ctx,
                                             int16_t  delta_uv,
                                             uint16_t lightness,
                                             uint16_t temperature)
{
    if (!p_model)
    {
        return MESHX_INVALID_ARG;
    }

    meshx_lighting_server_state_change_t state_change = {
        .ctl_set = {
            .delta_uv = delta_uv,
            .lightness = lightness,
            .temperature = temperature
        }
    };

    meshx_err_t err = meshx_gen_light_srv_status_send(
        p_model,
        ctx,
        &state_change
    );

    if (err != MESHX_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to send Light CTL status: %d", err);
    }

    return err;
}

/**
 * @brief Create a Light CTL Server send message packet.
 *
 * This function creates a Light CTL Server send message packet with the provided parameters.
 *
 * @param[in]  p_model        Pointer to the MeshX model structure.
 * @param[in]  element_id     Element ID associated with the model.
 * @param[in]  net_idx        Network Index for the message.
 * @param[in]  app_idx        Application Index for the message.
 * @param[in]  pub_addr       Publication address for the message.
 * @param[in]  ctl_state      Current state of the Light CTL Server.
 * @param[out] light_srv_send Pointer to the structure where the created message packet will be stored.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully created the message packet.
 *     - MESHX_INVALID_ARG: Invalid argument provided.
 */
meshx_err_t meshx_light_ctl_srv_send_pack_create(
    meshx_ptr_t p_model,
    uint16_t element_id,
    uint16_t net_idx,
    uint16_t app_idx,
    uint16_t pub_addr,
    meshx_light_ctl_srv_state_t ctl_state,
    meshx_lighting_server_cb_param_t *light_srv_send)
{
    if (!p_model || !light_srv_send)
    {
        return MESHX_INVALID_ARG;
    }

    memset(light_srv_send, 0, sizeof(meshx_lighting_server_cb_param_t));
    light_srv_send->ctx.net_idx = net_idx;
    light_srv_send->ctx.app_idx = app_idx;
    light_srv_send->ctx.src_addr = element_id;
    light_srv_send->ctx.dst_addr = pub_addr;
    light_srv_send->ctx.opcode = MESHX_MODEL_OP_LIGHT_CTL_STATUS;
    light_srv_send->ctx.p_ctx = NULL;

    light_srv_send->model.el_id = element_id;
    light_srv_send->model.p_model = p_model;

    light_srv_send->state_change.ctl_set.delta_uv = ctl_state.delta_uv;
    light_srv_send->state_change.ctl_set.lightness = ctl_state.lightness;
    light_srv_send->state_change.ctl_set.temperature = ctl_state.temperature;

    return MESHX_SUCCESS;
}

#endif /* CONFIG_ENABLE_LIGHT_CTL_SERVER */
