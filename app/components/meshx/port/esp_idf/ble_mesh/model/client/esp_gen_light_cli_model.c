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

static void esp_ble_mesh_light_client_cb(MESHX_GEN_LIGHT_CLI_CB_EVT event,
                                         MESHX_GEN_LIGHT_CLI_CB_PARAM *param)
{
    MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "%s, err|op|src|dst: %d|%04" PRIx32 "|%04x|%04x",
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
meshx_err_t meshx_plat_gen_light_cli_init(void)
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
