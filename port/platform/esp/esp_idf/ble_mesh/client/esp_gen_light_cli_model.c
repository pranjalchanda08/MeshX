/**
 * Copyright (c) 2024 - 2025 MeshX
 *
 * @file esp_gen_light_cli_model.c
 * @brief Implementation of the Generic Light Client model for BLE Mesh.
 *        This file contains the initialization, resource management, and
 *        message handling logic for the Generic Light Client model in
 *        the MeshX platform.
 *
 *        The Generic Light Client model is responsible for sending requests
 *        and receiving responses related to the light state of devices in a
 *        BLE Mesh network. It manages the client instance, publication context,
 *        and interacts with the MeshX BLE Mesh stack.
 *
 * @author Pranjal Chanda
 *
 */
/*****************************************************************************************************************
 * INCLUDES
 ****************************************************************************************************************/
#include "meshx_control_task.h"
#include "interface/ble_mesh/client/meshx_ble_mesh_light_cli.h"

/*****************************************************************************************************************
 * DEFINES
 ****************************************************************************************************************/
#define MESHX_CLIENT_INIT_MAGIC_NO 0x3728 // Magic number to indicate initialization state

/*****************************************************************************************************************
 * STATIC VARIABLES
 ****************************************************************************************************************/
static uint16_t meshx_client_init = 0; // Magic number to indicate initialization state

/**
 * @brief Mapping of BLE Mesh client state events to string representations.
 */
static const char *client_state_str[] =
{
    [ESP_BLE_MESH_LIGHT_CLIENT_PUBLISH_EVT] = "PUBLISH_EVT",
    [ESP_BLE_MESH_LIGHT_CLIENT_TIMEOUT_EVT] = "TIMEOUT_EVT",
    [ESP_BLE_MESH_LIGHT_CLIENT_GET_STATE_EVT] = "GET_STATE_EVT",
    [ESP_BLE_MESH_LIGHT_CLIENT_SET_STATE_EVT] = "SET_STATE_EVT",
};

/*****************************************************************************************************************
 * STATIC FUNCTION PROTOTYPES
 ****************************************************************************************************************/
static void esp_ble_mesh_light_client_cb(MESHX_GEN_LIGHT_CLI_CB_EVT event,
                                         MESHX_GEN_LIGHT_CLI_CB_PARAM *param);
/*****************************************************************************************************************
 * STATIC FUNCTION DEFINITIONS
****************************************************************************************************************/

/**
 * @brief Callback function for BLE Mesh Light Client events.
 *
 * This function is invoked to handle events related to the Generic Light Client model
 * in the BLE Mesh stack. It processes various client events and their associated parameters.
 *
 * @param event The event type received by the Light Client.
 * @param param Pointer to the structure containing event-specific parameters.
 */
static void esp_ble_mesh_light_client_cb(MESHX_GEN_LIGHT_CLI_CB_EVT event,
                                         MESHX_GEN_LIGHT_CLI_CB_PARAM *param)
{
    MESHX_UNUSED(client_state_str);
    MESHX_LOGD(MODULE_ID_MODEL_CLIENT, "%s, err|op|src|dst: %d|%04" PRIx32 "|%04x|%04x",
            client_state_str[event], param->error_code,
            param->params->ctx.recv_op, param->params->ctx.addr, param->params->ctx.recv_dst);

    meshx_gen_light_cli_cb_param_t pub_param = {
        .ctx = {
            .net_idx    = param->params->ctx.net_idx,
            .app_idx    = param->params->ctx.app_idx,
            .dst_addr   = param->params->ctx.recv_dst,
            .src_addr   = param->params->ctx.addr,
            .opcode     = param->params->ctx.recv_op,
            .p_ctx      = &param->params->ctx
        },
        .model = {
            .pub_addr   = param->params->model->pub->publish_addr,
            .model_id   = param->params->model->model_id,
            .el_id      = param->params->model->element_idx,
            .p_model    = param->params->model
        },
        .evt = MESHX_BIT(event)
    };

    /* Copy the msg data from BLE Layer to MeshX Layer */
    memcpy(&pub_param.status, &param->status_cb, sizeof(meshx_gen_light_client_status_cb_t));

    /* Publish the event to the control task message queue */
    meshx_err_t err = control_task_msg_publish(
        CONTROL_TASK_MSG_CODE_FRM_BLE,
        pub_param.model.model_id,
        &pub_param, sizeof(meshx_gen_light_cli_cb_param_t));
    if (err != MESHX_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Failed to publish Generic Light Client event: %d", err);
    }
}

/*****************************************************************************************************************
 * PUBLIC FUNCTION DEFINITIONS
****************************************************************************************************************/
/**
 * @brief Initialize the Generic Light Client Model.
 *        This function sets up the necessary parameters and resources
 *        for the Generic Light Client Model to operate correctly.
 *
 * @return meshx_err_t Returns MESHX_SUCCESS on successful initialization,
 *                     or an appropriate error code on failure.
 */
meshx_err_t meshx_plat_gen_light_client_init(void)
{
    meshx_err_t err = MESHX_SUCCESS;
    if (meshx_client_init == MESHX_CLIENT_INIT_MAGIC_NO)
    {
        return MESHX_SUCCESS; // Already initialized
    }
    meshx_client_init = MESHX_CLIENT_INIT_MAGIC_NO;

    // Register the callback for handling messages sent to the BLE layer
    esp_err_t esp_err = esp_ble_mesh_register_light_client_callback(
        (MESHX_GEN_LIGHT_CLI_CB)&esp_ble_mesh_light_client_cb);
    if (esp_err != ESP_OK)
        err = MESHX_ERR_PLAT;
    return err;
}

/**
 * @brief Creates and initializes a Light CTL (Color Temperature Light) client model instance.
 *
 * This function sets up the Light CTL client model for use in the BLE Mesh network.
 * It associates the client model with the provided model pointer and optionally sets up
 * publication and client context pointers.
 *
 * @param[in]  p_model         Pointer to the mesh model structure to associate with the Light CTL client.
 * @param[out] p_pub           Pointer to a publication context pointer to be initialized (can be NULL if not used).
 * @param[out] p_light_ctl_cli Pointer to a Light CTL client context pointer to be initialized.
 *
 * @return meshx_err_t         Returns MESHX_OK on success, or an appropriate error code on failure.
 */
meshx_err_t meshx_plat_light_ctl_client_create(meshx_ptr_t p_model, meshx_ptr_t* p_pub, meshx_ptr_t* p_light_ctl_cli)
{
    if (!p_model || !p_pub || !p_light_ctl_cli)
    {
        return MESHX_INVALID_ARG; // Invalid arguments
    }
    /* SIG Light CTL Init */
    uint16_t model_id = ESP_BLE_MESH_MODEL_ID_LIGHT_CTL_CLI;
    memcpy((meshx_ptr_t)&(((MESHX_MODEL *)p_model)->model_id), &model_id, sizeof(model_id));

    return meshx_plat_client_create(p_model, p_pub, p_light_ctl_cli);
}

/**
 * @brief Deletes the Light client instance and its associated publication context.
 *
 * This function is responsible for cleaning up and freeing resources associated with the Light client model,
 * including its publication context.
 *
 * @param[in] p_pub Pointer to the publication context to be deleted.
 * @param[in] p_cli Pointer to the client instance to be deleted.
 *
 * @return meshx_err_t Returns an error code indicating the result of the delete operation.
 */
meshx_err_t meshx_plat_light_client_delete(meshx_ptr_t* p_pub, meshx_ptr_t* p_cli)
{
    if (p_cli)
    {
        MESHX_FREE(*p_cli);
        *p_cli = NULL;
    }

    return meshx_plat_del_model_pub(p_pub);
}

/**
 * @brief Sends a Light Client message over BLE Mesh.
 *
 * This function constructs and sends a Light Client message using the specified model,
 * set state parameters, opcode, destination address, network index, and application index.
 *
 * @param[in] p_model   Pointer to the BLE Mesh model instance.
 * @param[in] p_set     Pointer to the structure containing the light client set state parameters.
 * @param[in] opcode    Opcode of the message to be sent.
 * @param[in] addr      Destination address for the message.
 * @param[in] net_idx   Network index to be used for sending the message.
 * @param[in] app_idx   Application index to be used for sending the message.
 * @param[in] is_get_opcode Indicates whether the opcode is a GET type (true) or SET type (false).
 *
 * @return meshx_err_t  Result of the message send operation.
 */
meshx_err_t meshx_plat_light_client_send_msg(
    meshx_ptr_t p_model, meshx_light_client_set_state_t *p_set,
    uint16_t opcode, uint16_t addr,
    uint16_t net_idx, uint16_t app_idx,
    bool is_get_opcode
)
{
    if (!p_model || !p_set)
    {
        return MESHX_INVALID_ARG;
    }

    esp_ble_mesh_client_common_param_t common = {0};
    common.model        = p_model;
    common.opcode       = opcode;
    common.ctx.addr     = addr;
    common.ctx.net_idx  = net_idx;
    common.ctx.app_idx  = app_idx;
    common.ctx.send_ttl = BLE_MESH_TTL_DEFAULT;
    common.msg_timeout  = 0; /* 0 indicates that timeout value from menuconfig will be used */

    // Send the message using the appropriate BLE Mesh API
    if (!is_get_opcode)
    {
        esp_err_t esp_err = esp_ble_mesh_light_client_set_state(
            &common,
            (esp_ble_mesh_light_client_set_state_t *)p_set
        );
        if (esp_err != ESP_OK)
        {
            MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Failed to send Light Client message: %d", esp_err);
            return MESHX_ERR_PLAT;
        }
    }
    else
    {
        esp_err_t esp_err = esp_ble_mesh_light_client_get_state(
            &common,
            (esp_ble_mesh_light_client_get_state_t *)p_set
        );
        if (esp_err != ESP_OK)
        {
            MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Failed to send Light Client GET message: %d", esp_err);
            return MESHX_ERR_PLAT;
        }
    }
    return MESHX_SUCCESS;
}
