/**
 * Copyright (c) 2024 - 2025 MeshX
 *
 * @file esp_light_srv_model.c
 * @brief Implementation of the BLE Mesh Light CTL Server Model for ESP32.
 *        This file contains the initialization, message handling, and state
 *        management for the Light CTL Server Model, including support for
 *        Lightness, Temperature, and Delta UV operations.
 *
 *        The implementation includes:
 *        - BLE Mesh message handling for Light CTL Server.
 *        - State management for Lightness, Temperature, and Delta UV.
 *        - Callback functions for BLE Mesh Lightness Server events.
 *        - Initialization and cleanup routines for the Light CTL Server.
 *
 * @author Pranjal Chanda
 */

#include "meshx_platform_ble_mesh.h"
#include "interface/ble_mesh/server/meshx_ble_mesh_light_srv.h"
#include "interface/ble_mesh/server/meshx_ble_mesh_gen_srv.h"

/**
 * @brief Light CTL status packet.
 */
typedef union meshx_plat_ctl_status
{
    struct
    {
        uint16_t lightness;   /**< Lightness level */
        uint16_t temperature; /**< Color temperature */
    } ctl_status;
    struct
    {
        uint16_t temperature; /**< Color temperature */
        uint16_t delta_uv;    /**< Delta UV value */
    } ctl_temp_status;
    struct
    {
        uint16_t lightness_def;   /**< Default lightness */
        uint16_t temperature_def; /**< Default temperature */
        uint16_t delta_uv_def;    /**< Default delta UV */
    } ctl_default;
    struct
    {
        uint8_t status_code; /**< Status code */
        uint16_t range_min;  /**< Minimum temperature range */
        uint16_t range_max;  /**< Maximum temperature range */
    } ctl_temp_range;
} meshx_plat_ctl_status_t;

/**
 * @brief Template for CTL Setup Srv SIG model initialization.
 */
static const MESHX_MODEL light_ctl_setup_sig_template = ESP_BLE_MESH_SIG_MODEL(MESHX_MODEL_ID_LIGHT_CTL_SETUP_SRV, NULL, NULL, NULL);

/**
 * @brief Template for CTL Srv SIG model initialization.
 */
static const MESHX_MODEL light_ctl_sig_template = ESP_BLE_MESH_SIG_MODEL(MESHX_MODEL_ID_LIGHT_CTL_SRV, NULL, NULL, NULL);

/**
 * @brief Template for Lightness Srv SIG model initialization.
 */
static const MESHX_MODEL light_lightness_sig_template = ESP_BLE_MESH_SIG_MODEL(MESHX_MODEL_ID_LIGHT_LIGHTNESS_SRV, NULL, NULL, NULL);

/**
 * @brief Template for HSL Srv SIG model initialization.
 */
static const MESHX_MODEL light_hsl_sig_template = ESP_BLE_MESH_SIG_MODEL(MESHX_MODEL_ID_LIGHT_HSL_SRV, NULL, NULL, NULL);

/**
 * @brief Template for xyL Srv SIG model initialization.
 */
static const MESHX_MODEL light_xyl_sig_template = ESP_BLE_MESH_SIG_MODEL(MESHX_MODEL_ID_LIGHT_XYL_SRV, NULL, NULL, NULL);

/**
 * @brief Template for LC Srv SIG model initialization.
 */
static const MESHX_MODEL light_lc_sig_template = ESP_BLE_MESH_SIG_MODEL(MESHX_MODEL_ID_LIGHT_LC_SRV, NULL, NULL, NULL);

/**
 * @brief Callback function for BLE Mesh Lightness Server events.
 *
 * This function is called whenever a BLE Mesh Lightness Server event occurs.
 *
 * @param[in] event The event type for the BLE Mesh Lightness Server.
 * @param[in] param Parameters associated with the event.
 */
static void meshx_ble_lightness_server_cb(MESHX_LIGHT_SRV_CB_EVT event,
                                          MESHX_LIGHT_SRV_CB_PARAM *param)
{
    MESHX_UNUSED(event);
    MESHX_LOGD(MODULE_ID_MODEL_SERVER, "evt|op|src|dst: %02x|%04x|%04x|%04x|%04x",
             event, (unsigned)param->ctx.recv_op, param->ctx.addr, param->ctx.recv_dst,
             param->model->model_id);

    if(event != ESP_BLE_MESH_LIGHTING_SERVER_STATE_CHANGE_EVT)
    {
        return;
    }
    MESHX_LIGHT_CTL_SRV *srv = (MESHX_LIGHT_CTL_SRV *)param->model->user_data;

    bool publish_flag = false;
    uint32_t op_code = param->ctx.recv_op;
    meshx_lighting_server_cb_param_t pub_param = {
        .ctx = {
            .net_idx    = param->ctx.net_idx,
            .app_idx    = param->ctx.app_idx,
            .dst_addr   = param->ctx.recv_dst,
            .src_addr   = param->ctx.addr,
            .opcode     = param->ctx.recv_op,
            .p_ctx      = &param->ctx
        },
        .model = {
            .el_id     = param->model->element_idx,
            .pub_addr  = param->model->pub->publish_addr,
            .model_id  = param->model->model_id,
            .p_model   = param->model
        }
    };

    switch (op_code)
    {
    /*!< Light CTL Message Opcode */
    case MESHX_MODEL_OP_LIGHT_CTL_GET:
    case MESHX_MODEL_OP_LIGHT_CTL_SET:
    case MESHX_MODEL_OP_LIGHT_CTL_SET_UNACK:
        if (op_code != MESHX_MODEL_OP_LIGHT_CTL_GET)
        {
            srv->state->temperature = param->value.state_change.ctl_set.temperature;
            srv->state->lightness = param->value.state_change.ctl_set.lightness;
            srv->state->delta_uv = param->value.state_change.ctl_set.delta_uv;
            MESHX_LOGD(MODULE_ID_MODEL_SERVER, "lightness|temp|del_uv:%d|%d|%d",
                     srv->state->lightness,
                     srv->state->temperature,
                     srv->state->delta_uv);

            pub_param.state_change.ctl_set.delta_uv = srv->state->delta_uv;
            pub_param.state_change.ctl_set.lightness = srv->state->lightness;
            pub_param.state_change.ctl_set.temperature = srv->state->temperature;

            publish_flag = true;
        }
        break;
    case MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_GET:
    case MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_SET:
    case MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_SET_UNACK:
        if (op_code != MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_GET)
        {
            srv->state->temperature = param->value.state_change.ctl_temp_set.temperature;
            srv->state->delta_uv = param->value.state_change.ctl_temp_set.delta_uv;
            MESHX_LOGI(MODULE_ID_MODEL_SERVER, "lightness|del_uv:%d|%d",
                     srv->state->temperature,
                     srv->state->delta_uv);

            pub_param.state_change.ctl_temp_set.delta_uv = srv->state->delta_uv;
            pub_param.state_change.ctl_temp_set.temperature = srv->state->temperature;

            publish_flag = true;
        }
        break;
    /*!< Light CTL Setup Message Opcode */
    case MESHX_MODEL_OP_LIGHT_CTL_DEFAULT_SET:
    case MESHX_MODEL_OP_LIGHT_CTL_DEFAULT_GET:
    case MESHX_MODEL_OP_LIGHT_CTL_DEFAULT_SET_UNACK:
        if (op_code != MESHX_MODEL_OP_LIGHT_CTL_DEFAULT_GET)
        {
            MESHX_LOGI(MODULE_ID_MODEL_SERVER, "lightness|temp|del_uv:%d|%d|%d",
                     param->value.state_change.ctl_default_set.lightness,
                     param->value.state_change.ctl_default_set.temperature,
                     param->value.state_change.ctl_default_set.delta_uv);
            srv->state->temperature_default = param->value.state_change.ctl_default_set.temperature;
            srv->state->lightness_default = param->value.state_change.ctl_default_set.lightness;
            srv->state->delta_uv_default = param->value.state_change.ctl_default_set.delta_uv;

            pub_param.state_change.ctl_default_set.delta_uv = srv->state->delta_uv_default;
            pub_param.state_change.ctl_default_set.lightness = srv->state->lightness_default;
            pub_param.state_change.ctl_default_set.temperature = srv->state->temperature_default;

            publish_flag = true;
        }
        break;
    case MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_SET:
    case MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_GET:
    case MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_SET_UNACK:
        if (op_code != MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_GET)
        {
            MESHX_LOGI(MODULE_ID_MODEL_SERVER, "temp min|max: %dK|%dK",
                     param->value.state_change.ctl_temp_range_set.range_min,
                     param->value.state_change.ctl_temp_range_set.range_max);
            srv->state->temperature_range_min = param->value.state_change.ctl_temp_range_set.range_min;
            srv->state->temperature_range_max = param->value.state_change.ctl_temp_range_set.range_max;

            pub_param.state_change.ctl_temp_range_set.range_max = srv->state->temperature_range_max;
            pub_param.state_change.ctl_temp_range_set.range_min = srv->state->temperature_range_min;

            publish_flag = true;
        }
        break;
    default:
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "CTL Unhandled Event %p", (meshx_ptr_t )param->ctx.recv_op);
        break;
    }
    if (publish_flag)
        control_task_msg_publish(CONTROL_TASK_MSG_CODE_FRM_BLE,
                                 pub_param.model.model_id,
                                 &pub_param,
                                 sizeof(meshx_lighting_server_cb_param_t));
}

/**
 * @brief Send a status message from the Light Server.
 * This function constructs and sends a status message containing the current state of the Light Server.
 * @param[in] p_model       Pointer to the Light Server model.
 * @param[in] p_ctx         Pointer to the context containing message information.
 * @param[in] state_change  The state change data to be sent in the status message.
 * @return
 *     - MESHX_SUCCESS: Successfully sent the status message.
 *     - MESHX_INVALID_ARG: Invalid argument provided.
 *     - MESHX_ERR_PLAT: Platform-specific error occurred.
 */
meshx_err_t meshx_plat_gen_light_srv_send_status(
    const meshx_model_t *p_model,
    const meshx_ctx_t *p_ctx,
    const meshx_lighting_server_state_change_t *state_change)
{
    if (!p_model || !p_ctx || !state_change)
    {
        return MESHX_INVALID_ARG;
    }
    meshx_err_t err = MESHX_SUCCESS;
    static esp_ble_mesh_msg_ctx_t ctx;
    const esp_ble_mesh_msg_ctx_t *pctx = (esp_ble_mesh_msg_ctx_t *)p_ctx->p_ctx;
    if(pctx != NULL)
    {
        memcpy(&ctx, pctx, sizeof(esp_ble_mesh_msg_ctx_t));
    }
    meshx_plat_ctl_status_t ctl_status_union;
    uint8_t ctl_status_pack_len = 0;

    switch (p_ctx->opcode)
    {
    case MESHX_MODEL_OP_LIGHT_CTL_STATUS:
        ctl_status_union.ctl_status.temperature = state_change->ctl_set.temperature;
        ctl_status_union.ctl_status.lightness = state_change->ctl_set.lightness;
        ctl_status_pack_len = sizeof(ctl_status_union.ctl_status);
        break;
    case MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_STATUS:
        ctl_status_union.ctl_temp_status.temperature = state_change->ctl_temp_set.temperature;
        ctl_status_union.ctl_temp_status.delta_uv = state_change->ctl_temp_set.delta_uv;
        ctl_status_pack_len = sizeof(ctl_status_union.ctl_temp_status);
        break;
    case MESHX_MODEL_OP_LIGHT_CTL_DEFAULT_STATUS:
        ctl_status_union.ctl_default.delta_uv_def = state_change->ctl_default_set.delta_uv;
        ctl_status_union.ctl_default.lightness_def = state_change->ctl_default_set.lightness;
        ctl_status_union.ctl_default.temperature_def = state_change->ctl_default_set.temperature;
        ctl_status_pack_len = sizeof(ctl_status_union.ctl_default);
        break;
    case MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_STATUS:
        ctl_status_union.ctl_temp_range.status_code = MESHX_SUCCESS;
        ctl_status_union.ctl_temp_range.range_min = state_change->ctl_temp_range_set.range_min;
        ctl_status_union.ctl_temp_range.range_max = state_change->ctl_temp_range_set.range_max;
        ctl_status_pack_len = sizeof(ctl_status_union.ctl_temp_range);
        break;
    default:
        err = MESHX_INVALID_ARG;
    }

    ctx.net_idx    =   p_ctx->net_idx;
    ctx.app_idx    =   p_ctx->app_idx;
    ctx.addr       =   p_ctx->dst_addr;
    ctx.send_ttl   =   ESP_BLE_MESH_TTL_DEFAULT;
    ctx.send_cred  =   0;
    ctx.send_tag   =   BIT1;

    esp_err_t esp_err = esp_ble_mesh_server_model_send_msg(p_model->p_model,
                                                       &ctx,
                                                       p_ctx->opcode,
                                                       ctl_status_pack_len,
                                                       (uint8_t *)&ctl_status_union);
    if (esp_err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Mesh Model msg send failed (err: 0x%x)", esp_err);
        return MESHX_ERR_PLAT;
    }
    MESHX_LOGD(MODULE_ID_MODEL_SERVER, "Mesh Model msg sent (opcode: 0x%04x, len: %d)", p_ctx->opcode, ctl_status_pack_len);

    return err;
}

/**
 * @brief Initialize the platform-specific Light Server.
 *
 * This function sets up the necessary resources for the Light Server.
 *
 * @return
 *      - MESHX_SUCCESS on success.
 *      - Appropriate error code on failure.
 */
meshx_err_t meshx_plat_light_srv_init(void)
{
    /* Register callback for enabling Sending of Model Msg From MeshX to BLE Layer */
    meshx_err_t err = MESHX_SUCCESS;
    /* Register the ESP Generic Server callback */
    esp_err_t esp_err = esp_ble_mesh_register_lighting_server_callback(
        (MESHX_LIGHT_SRV_CB)&meshx_ble_lightness_server_cb
    );
    if (esp_err != ESP_OK)
        err = MESHX_ERR_PLAT;

    return err;
}

/**
 * @brief Delete a Light CTL Server instance.
 *
 * This function releases resources associated with a Light CTL Server model.
 *
 * @param[in,out] p_pub    Pointer to the publication context to be deleted.
 * @param[in,out] p_ctl_srv Pointer to the Light CTL Server instance to be deleted.
 *
 * @return
 *      - MESHX_SUCCESS on success.
 *      - Appropriate error code on failure.
 */
meshx_err_t meshx_plat_light_srv_delete(meshx_ptr_t *p_pub, meshx_ptr_t *p_ctl_srv)
{
    if (p_ctl_srv)
    {
        if(*p_ctl_srv && ((MESHX_LIGHT_CTL_SRV *)*p_ctl_srv)->state)
        {
            MESHX_FREE(((MESHX_LIGHT_CTL_SRV *)*p_ctl_srv)->state);
            ((MESHX_LIGHT_CTL_SRV *)*p_ctl_srv)->state = NULL;
        }
        MESHX_FREE(*p_ctl_srv);
        *p_ctl_srv = NULL;
    }

    return meshx_plat_del_model_pub(p_pub);
}

/**
 * @brief Creates and initializes a Light CTL (Color Temperature Lightness) Setup Server model instance.
 *
 * This function sets up the Light CTL Setup Server for a given model, configuring publication and server context pointers.
 *
 * @param[in]  p_model    Pointer to the parent model instance.
 * @param[out] p_pub      Pointer to the publication context to be initialized.
 * @param[out] p_ctl_srv  Pointer to the Light CTL Setup Server context to be initialized.
 *
 * @return meshx_err_t    Error code indicating the result of the operation.
 *                       - MESHX_OK on success
 *                       - Appropriate error code otherwise
 */
meshx_err_t meshx_plat_light_ctl_setup_srv_create(meshx_ptr_t p_model, meshx_ptr_t *p_pub, meshx_ptr_t *p_ctl_srv)
{
    if (!p_model || !p_pub || !p_ctl_srv)
        return MESHX_INVALID_ARG;

    meshx_err_t err = MESHX_SUCCESS;

    err = meshx_plat_create_model_pub(p_pub, 1);
    if (err)
        return meshx_plat_del_model_pub(p_pub);

    *p_ctl_srv = (MESHX_LIGHT_CTL_SETUP_SRV *)MESHX_CALOC(1, sizeof(MESHX_LIGHT_CTL_SETUP_SRV));
    if (!*p_ctl_srv)
        return MESHX_NO_MEM;

    /* SIG CTL Setup Server initialisation */
    memcpy(p_model, &light_ctl_setup_sig_template, sizeof(MESHX_MODEL));

    ((MESHX_LIGHT_CTL_SETUP_SRV *)*p_ctl_srv)->rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;
    ((MESHX_LIGHT_CTL_SETUP_SRV *)*p_ctl_srv)->rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;

    ((MESHX_LIGHT_CTL_SETUP_SRV *)*p_ctl_srv)->state = (MESHX_LIGHT_CTL_STATE *)MESHX_CALOC(1, sizeof(MESHX_LIGHT_CTL_STATE));
    if (!((MESHX_LIGHT_CTL_SETUP_SRV *)*p_ctl_srv)->state)
    {
        MESHX_FREE(*p_ctl_srv);
        return MESHX_NO_MEM;
    }

    ((MESHX_LIGHT_CTL_SETUP_SRV *)*p_ctl_srv)->state->temperature_range_max = 0;
    ((MESHX_LIGHT_CTL_SETUP_SRV *)*p_ctl_srv)->state->temperature_range_min = 0;

    ((MESHX_MODEL *)p_model)->user_data = *p_ctl_srv;

    // Update the publication pointer with proper const handling
    *((meshx_ptr_t *)((uint8_t *)p_model + offsetof(MESHX_MODEL, pub))) = *p_pub;

    return err;
}

/**
 * @brief Create a Light CTL Server instance.
 *
 * This function initializes and allocates resources for a Light CTL Server model.
 *
 * @param[in]  p_model  Pointer to the model instance.
 * @param[out] p_pub    Pointer to the publication context.
 * @param[out] p_ctl_srv Pointer to the Light CTL Server instance.
 *
 * @return
 *      - MESHX_SUCCESS on success.
 *      - Appropriate error code on failure.
 */
meshx_err_t meshx_plat_light_ctl_srv_create(meshx_ptr_t p_model, meshx_ptr_t *p_pub, meshx_ptr_t *p_ctl_srv)
{
    if (!p_model || !p_pub || !p_ctl_srv)
        return MESHX_INVALID_ARG;

    meshx_err_t err = MESHX_SUCCESS;

    err = meshx_plat_create_model_pub(p_pub, 1);
    if (err)
        return meshx_plat_del_model_pub(p_pub);

    *p_ctl_srv = (MESHX_LIGHT_CTL_SRV *)MESHX_CALOC(1, sizeof(MESHX_LIGHT_CTL_SRV));
    if (!*p_ctl_srv)
        return MESHX_NO_MEM;

    /* SIG ON OFF initialisation */

    memcpy(p_model, &light_ctl_sig_template, sizeof(MESHX_MODEL));

    ((MESHX_LIGHT_CTL_SRV *)*p_ctl_srv)->rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;
    ((MESHX_LIGHT_CTL_SRV *)*p_ctl_srv)->rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;

    ((MESHX_LIGHT_CTL_SRV *)*p_ctl_srv)->state = (MESHX_LIGHT_CTL_STATE *)MESHX_CALOC(1, sizeof(MESHX_LIGHT_CTL_STATE));
    if (!((MESHX_LIGHT_CTL_SRV *)*p_ctl_srv)->state)
    {
        MESHX_FREE(*p_ctl_srv);
        return MESHX_NO_MEM;
    }

    ((MESHX_LIGHT_CTL_SRV *)*p_ctl_srv)->state->temperature = 0;
    ((MESHX_LIGHT_CTL_SRV *)*p_ctl_srv)->state->lightness = 0;
    ((MESHX_LIGHT_CTL_SRV *)*p_ctl_srv)->state->delta_uv = 0;
    ((MESHX_LIGHT_CTL_SRV *)*p_ctl_srv)->state->temperature_range_min = 0;
    ((MESHX_LIGHT_CTL_SRV *)*p_ctl_srv)->state->temperature_range_max = 0;

    ((MESHX_MODEL *)p_model)->user_data = *p_ctl_srv;
    // Update the publication pointer with proper const handling
    *((meshx_ptr_t *)((uint8_t *)p_model + offsetof(MESHX_MODEL, pub))) = *p_pub;

    return err;
}

/**
 * @brief Create a Light Lightness Server instance.
 *
 * This function initializes and allocates resources for a Light Lightness Server model.
 *
 * @param[in]  p_model  Pointer to the model instance.
 * @param[out] p_pub    Pointer to the publication context.
 * @param[out] p_lightness_srv Pointer to the Light Lightness Server instance.
 *
 * @return
 *      - MESHX_SUCCESS on success.
 *      - Appropriate error code on failure.
 */
meshx_err_t meshx_plat_light_lightness_srv_create(meshx_ptr_t p_model, meshx_ptr_t *p_pub, meshx_ptr_t *p_lightness_srv)
{
    if (!p_model || !p_pub || !p_lightness_srv)
        return MESHX_INVALID_ARG;

    meshx_err_t err = MESHX_SUCCESS;

    err = meshx_plat_create_model_pub(p_pub, 1);
    if (err)
        return meshx_plat_del_model_pub(p_pub);

    *p_lightness_srv = (MESHX_LIGHT_LIGHTNESS_SRV *)MESHX_CALOC(1, sizeof(MESHX_LIGHT_LIGHTNESS_SRV));
    if (!*p_lightness_srv)
        return MESHX_NO_MEM;

    memcpy(p_model, &light_lightness_sig_template, sizeof(MESHX_MODEL));

    ((MESHX_LIGHT_LIGHTNESS_SRV *)*p_lightness_srv)->rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;
    ((MESHX_LIGHT_LIGHTNESS_SRV *)*p_lightness_srv)->rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;

    ((MESHX_LIGHT_LIGHTNESS_SRV *)*p_lightness_srv)->state = (MESHX_LIGHT_LIGHTNESS_STATE *)MESHX_CALOC(1, sizeof(MESHX_LIGHT_LIGHTNESS_STATE));
    if (!((MESHX_LIGHT_LIGHTNESS_SRV *)*p_lightness_srv)->state)
    {
        MESHX_FREE(*p_lightness_srv);
        return MESHX_NO_MEM;
    }

    ((MESHX_MODEL *)p_model)->user_data = *p_lightness_srv;
    // Update the publication pointer with proper const handling
    *((meshx_ptr_t *)((uint8_t *)p_model + offsetof(MESHX_MODEL, pub))) = *p_pub;

    return err;
}

/**
 * @brief Create a Light HSL Server instance.
 *
 * This function initializes and allocates resources for a Light HSL Server model.
 *
 * @param[in]  p_model  Pointer to the model instance.
 * @param[out] p_pub    Pointer to the publication context.
 * @param[out] p_hsl_srv Pointer to the Light HSL Server instance.
 *
 * @return
 *      - MESHX_SUCCESS on success.
 *      - Appropriate error code on failure.
 */
meshx_err_t meshx_plat_light_hsl_srv_create(meshx_ptr_t p_model, meshx_ptr_t *p_pub, meshx_ptr_t *p_hsl_srv)
{
    if (!p_model || !p_pub || !p_hsl_srv)
        return MESHX_INVALID_ARG;

    meshx_err_t err = MESHX_SUCCESS;

    err = meshx_plat_create_model_pub(p_pub, 1);
    if (err)
        return meshx_plat_del_model_pub(p_pub);

    *p_hsl_srv = (MESHX_LIGHT_HSL_SRV *)MESHX_CALOC(1, sizeof(MESHX_LIGHT_HSL_SRV));
    if (!*p_hsl_srv)
        return MESHX_NO_MEM;

    memcpy(p_model, &light_hsl_sig_template, sizeof(MESHX_MODEL));

    ((MESHX_LIGHT_HSL_SRV *)*p_hsl_srv)->rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;
    ((MESHX_LIGHT_HSL_SRV *)*p_hsl_srv)->rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;

    ((MESHX_LIGHT_HSL_SRV *)*p_hsl_srv)->state = (MESHX_LIGHT_HSL_STATE *)MESHX_CALOC(1, sizeof(MESHX_LIGHT_HSL_STATE));
    if (!((MESHX_LIGHT_HSL_SRV *)*p_hsl_srv)->state)
    {
        MESHX_FREE(*p_hsl_srv);
        return MESHX_NO_MEM;
    }

    ((MESHX_MODEL *)p_model)->user_data = *p_hsl_srv;
    // Update the publication pointer with proper const handling
    *((meshx_ptr_t *)((uint8_t *)p_model + offsetof(MESHX_MODEL, pub))) = *p_pub;

    return err;
}

/**
 * @brief Create a Light xyL Server instance.
 *
 * This function initializes and allocates resources for a Light xyL Server model.
 *
 * @param[in]  p_model  Pointer to the model instance.
 * @param[out] p_pub    Pointer to the publication context.
 * @param[out] p_xyl_srv Pointer to the Light xyL Server instance.
 *
 * @return
 *      - MESHX_SUCCESS on success.
 *      - Appropriate error code on failure.
 */
meshx_err_t meshx_plat_light_xyl_srv_create(meshx_ptr_t p_model, meshx_ptr_t *p_pub, meshx_ptr_t *p_xyl_srv)
{
    if (!p_model || !p_pub || !p_xyl_srv)
        return MESHX_INVALID_ARG;

    meshx_err_t err = MESHX_SUCCESS;

    err = meshx_plat_create_model_pub(p_pub, 1);
    if (err)
        return meshx_plat_del_model_pub(p_pub);

    *p_xyl_srv = (MESHX_LIGHT_XYL_SRV *)MESHX_CALOC(1, sizeof(MESHX_LIGHT_XYL_SRV));
    if (!*p_xyl_srv)
        return MESHX_NO_MEM;

    memcpy(p_model, &light_xyl_sig_template, sizeof(MESHX_MODEL));

    ((MESHX_LIGHT_XYL_SRV *)*p_xyl_srv)->rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;
    ((MESHX_LIGHT_XYL_SRV *)*p_xyl_srv)->rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;

    ((MESHX_LIGHT_XYL_SRV *)*p_xyl_srv)->state = (MESHX_LIGHT_XYL_STATE *)MESHX_CALOC(1, sizeof(MESHX_LIGHT_XYL_STATE));
    if (!((MESHX_LIGHT_XYL_SRV *)*p_xyl_srv)->state)
    {
        MESHX_FREE(*p_xyl_srv);
        return MESHX_NO_MEM;
    }

    ((MESHX_MODEL *)p_model)->user_data = *p_xyl_srv;
    // Update the publication pointer with proper const handling
    *((meshx_ptr_t *)((uint8_t *)p_model + offsetof(MESHX_MODEL, pub))) = *p_pub;

    return err;
}

/**
 * @brief Create a Light LC Server instance.
 *
 * This function initializes and allocates resources for a Light LC (Light Control) Server model.
 *
 * @param[in]  p_model  Pointer to the model instance.
 * @param[out] p_pub    Pointer to the publication context.
 * @param[out] p_lc_srv Pointer to the Light LC Server instance.
 *
 * @return
 *      - MESHX_SUCCESS on success.
 *      - Appropriate error code on failure.
 */
meshx_err_t meshx_plat_light_lc_srv_create(meshx_ptr_t p_model, meshx_ptr_t *p_pub, meshx_ptr_t *p_lc_srv)
{
    if (!p_model || !p_pub || !p_lc_srv)
        return MESHX_INVALID_ARG;

    meshx_err_t err = MESHX_SUCCESS;

    err = meshx_plat_create_model_pub(p_pub, 1);
    if (err)
        return meshx_plat_del_model_pub(p_pub);

    *p_lc_srv = (MESHX_LIGHT_LC_SRV *)MESHX_CALOC(1, sizeof(MESHX_LIGHT_LC_SRV));
    if (!*p_lc_srv)
        return MESHX_NO_MEM;

    memcpy(p_model, &light_lc_sig_template, sizeof(MESHX_MODEL));

    ((MESHX_LIGHT_LC_SRV *)*p_lc_srv)->rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;
    ((MESHX_LIGHT_LC_SRV *)*p_lc_srv)->rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;

    ((MESHX_LIGHT_LC_SRV *)*p_lc_srv)->lc = (MESHX_LIGHT_LC_STATE *)MESHX_CALOC(1, sizeof(MESHX_LIGHT_LC_STATE));
    if (!((MESHX_LIGHT_LC_SRV *)*p_lc_srv)->lc)
    {
        MESHX_FREE(*p_lc_srv);
        return MESHX_NO_MEM;
    }

    ((MESHX_MODEL *)p_model)->user_data = *p_lc_srv;
    // Update the publication pointer with proper const handling
    *((meshx_ptr_t *)((uint8_t *)p_model + offsetof(MESHX_MODEL, pub))) = *p_pub;

    return err;
}

/**
 * @brief Restore the Light Server model state from persistent storage.
 *
 * This function restores the Light Server model state from persistent storage.
 *
 * @param[in] p_model    Pointer to the model instance to be restored.
 * @param[in] state      Pointer to the state data to be restored.
 * @param[in] state_len  Length of the state data in bytes.
 *
 * @return meshx_err_t Returns an error code indicating success or failure of the operation.
 *         - MESHX_SUCCESS on success.
 *         - MESHX_ERR_INVALID_ARG if any parameter is invalid.
 *         - MESHX_ERR_INVALID_STATE if the state data is corrupted.
 *         - Other error codes for implementation-specific failures.
 */
meshx_err_t meshx_plat_light_srv_restore(meshx_ptr_t p_model, const meshx_lighting_server_state_t *state, uint16_t state_len)
{
    if (!p_model || !state)
        return MESHX_INVALID_ARG;

    MESHX_MODEL *model = (MESHX_MODEL *)p_model;
    MESHX_LIGHT_CTL_SRV *srv = (MESHX_LIGHT_CTL_SRV *)model->user_data;

    if(!srv)
        return MESHX_INVALID_STATE;

    meshx_ptr_t state_ptr = (meshx_ptr_t)&srv->state;

    memcpy(state_ptr, state, state_len);

    return MESHX_SUCCESS;
}

/**
 * @brief Set the state of the Light CTL Server.
 *
 * This function updates the state of the Light CTL Server with the provided parameters.
 *
 * @param[in] p_model         Pointer to the model instance.
 * @param[in] delta_uv        Delta UV value.
 * @param[in] lightness       Lightness value.
 * @param[in] temperature     Temperature value.
 * @param[in] temp_range_max  Maximum temperature range.
 * @param[in] temp_range_min  Minimum temperature range.
 *
 * @return
 *      - MESHX_SUCCESS on success.
 *      - Appropriate error code on failure.
 */
meshx_err_t meshx_plat_set_light_ctl_srv_state(meshx_ptr_t p_model,
                                               uint16_t delta_uv,
                                               uint16_t lightness,
                                               uint16_t temperature,
                                               uint16_t temp_range_max,
                                               uint16_t temp_range_min)
{
    if (!p_model)
        return MESHX_INVALID_ARG;

    MESHX_MODEL *model = (MESHX_MODEL *)p_model;
    MESHX_LIGHT_CTL_SRV *srv = (MESHX_LIGHT_CTL_SRV *)model->user_data;
    srv->state->delta_uv = delta_uv;
    srv->state->lightness = lightness;
    srv->state->temperature = temperature;
    srv->state->temperature_range_min = temp_range_min;
    srv->state->temperature_range_max = temp_range_max;

    return MESHX_SUCCESS;
}

/**
 * This function restores the state of the Light CTL Server with the provided parameters.
 *
 * @param[in] p_model         Pointer to the model instance.
 * @param[in] delta_uv        Delta UV value.
 * @param[in] lightness       Lightness value.
 * @param[in] temperature     Temperature value.
 * @param[in] temp_range_max  Maximum temperature range.
 * @param[in] temp_range_min  Minimum temperature range.
 *
 * @return
 *      - MESHX_SUCCESS on success.
 *      - Appropriate error code on failure.
 */
meshx_err_t meshx_plat_light_ctl_srv_restore(meshx_ptr_t p_model,
                                             uint16_t delta_uv,
                                             uint16_t lightness,
                                             uint16_t temperature,
                                             uint16_t temp_range_max,
                                             uint16_t temp_range_min)
{
    return meshx_plat_set_light_ctl_srv_state(
        p_model,
        delta_uv,
        lightness,
        temperature,
        temp_range_max,
        temp_range_min
    );
}
