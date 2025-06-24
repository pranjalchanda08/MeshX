/**
 * Copyright (c) 2024 - 2025 MeshX
 *
 * @file esp_gen_cli_model.c
 * @brief Implementation of the Generic OnOff Client model for BLE Mesh.
 *        This file contains the initialization, resource management, and
 *        message handling logic for the Generic OnOff Client model in
 *        the MeshX platform.
 *
 *        The Generic OnOff Client model is responsible for sending requests
 *        and receiving responses related to the on/off state of devices in a
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
#include "interface/ble_mesh/client/meshx_ble_mesh_gen_cli.h"

/*****************************************************************************************************************
 * DEFINES
 ****************************************************************************************************************/
#define MESHX_SERVER_INIT_MAGIC_NO 0x1121 // Magic number to indicate initialization state

#define CONTROL_TASK_MSG_EVT_TO_BLE_GEN_CLI_MASK \
    CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF

/*****************************************************************************************************************
 * STATIC VARIABLES
 ****************************************************************************************************************/
static uint16_t meshx_client_init = 0; // Magic number to indicate initialization state

/**
 * @brief Mapping of BLE Mesh client state events to string representations.
 */
static const char *client_state_str[] =
{
    [ESP_BLE_MESH_GENERIC_CLIENT_PUBLISH_EVT] = "PUBLISH_EVT",
    [ESP_BLE_MESH_GENERIC_CLIENT_TIMEOUT_EVT] = "TIMEOUT_EVT",
    [ESP_BLE_MESH_GENERIC_CLIENT_GET_STATE_EVT] = "GET_STATE_EVT",
    [ESP_BLE_MESH_GENERIC_CLIENT_SET_STATE_EVT] = "SET_STATE_EVT",
};

/*****************************************************************************************************************
 * STATIC FUNCTION PROTOTYPES
 ****************************************************************************************************************/
static meshx_err_t meshx_plat_gen_cli_create(meshx_ptr_t p_model, meshx_ptr_t* p_pub, meshx_ptr_t*p_cli);

static void esp_ble_mesh_generic_client_cb(MESHX_GEN_CLI_CB_EVT event, MESHX_GEN_CLI_CB_PARAM *param);
/*****************************************************************************************************************
 * STATIC FUNCTION DEFINITIONS
 ****************************************************************************************************************/

/**
 * @brief Callback function for handling BLE Mesh Generic Client events.
 *
 * This function is invoked when a BLE Mesh Generic Client event occurs. It logs the event details,
 * prepares a parameter structure for the MeshX layer, copies relevant status data, and publishes
 * the event to the control task message queue.
 *
 * @param[in] event The type of Generic Client callback event (MESHX_GEN_CLI_CB_EVT).
 * @param[in] param Pointer to the callback parameter structure containing event-specific data (MESHX_GEN_CLI_CB_PARAM).
 *
 * The function performs the following steps:
 *  - Logs the event and associated error code, operation code, source, and destination addresses.
 *  - Populates a meshx_gen_cli_cb_param_t structure with context and model information.
 *  - Copies status callback data from the BLE layer to the MeshX layer.
 *  - Publishes the event to the control task using control_task_msg_publish().
 *  - Logs an error if publishing fails.
 */

static void esp_ble_mesh_generic_client_cb(MESHX_GEN_CLI_CB_EVT event,
                                           MESHX_GEN_CLI_CB_PARAM *param)
{
    MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "%s, err|op|src|dst: %d|%04" PRIx32 "|%04x|%04x",
               client_state_str[event], param->error_code,
               param->params->ctx.recv_op, param->params->ctx.addr, param->params->ctx.recv_dst);

    meshx_gen_cli_cb_param_t pub_param = {
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
    memcpy(&pub_param.status, &param->status_cb, sizeof(meshx_gen_onoff_status_cb_t));

    meshx_err_t err = control_task_msg_publish(
        CONTROL_TASK_MSG_CODE_FRM_BLE,
        pub_param.model.model_id,
        &pub_param,
        sizeof(meshx_gen_cli_cb_param_t));
    if (err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to publish to control task");
    }
}

/**
 * @brief Creates and initializes a generic client model for BLE Mesh.
 *
 * This function sets up the necessary structures and resources for a generic client model
 * in the BLE Mesh stack. It initializes the model, publication context, and the on/off client instance.
 *
 * @param[in]  p_model      Pointer to the model structure to be initialized.
 * @param[out] p_pub        Pointer to a location where the address of the publication context will be stored.
 * @param[out] p_cli  Pointer to a location where the address of the on/off client instance will be stored.
 *
 * @return meshx_err_t      Returns an error code indicating the result of the operation.
 *                          Typically, MESHX_OK on success or an appropriate error code on failure.
 */
static meshx_err_t meshx_plat_gen_cli_create(meshx_ptr_t p_model, meshx_ptr_t* p_pub, meshx_ptr_t* p_cli)
{
    if (!p_model || !p_pub || !p_cli)
    {
        return MESHX_INVALID_ARG; // Invalid arguments
    }
    meshx_err_t err = MESHX_SUCCESS;

    // Create the publication context for the model
    err = meshx_plat_create_model_pub(p_pub, 1);
    if (err)
    {
        return meshx_plat_del_model_pub(p_pub); // Clean up on error
    }

    // Allocate memory for the OnOff client model
    *p_cli = (MESHX_GEN_CLI *)MESHX_CALOC(1, sizeof(MESHX_GEN_CLI));
    if (!*p_cli)
    {
        return MESHX_NO_MEM; // Memory allocation failed
    }

    // Initialize the OnOff client model
    ((MESHX_MODEL *)p_model)->user_data = *p_cli;

    meshx_ptr_t*temp = (meshx_ptr_t*)&((MESHX_MODEL *)p_model)->pub;

    *temp = *p_pub;

    return MESHX_SUCCESS; // Successfully created the model and publication context
}

/*****************************************************************************************************************
 * INTERFACE FUNCTION DEFINITIONS
 ****************************************************************************************************************/
/**
 * @brief Initialize the meshxuction generic server.
 *
 * This function sets up the necessary configurations and initializes the
 * meshxuction generic server for the BLE mesh node.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_FAIL: Failed to initialize the server
 */
meshx_err_t meshx_plat_gen_cli_init(void)
{

    meshx_err_t err = MESHX_SUCCESS;
    if (meshx_client_init == MESHX_SERVER_INIT_MAGIC_NO)
    {
        return MESHX_SUCCESS; // Already initialized
    }
    meshx_client_init = MESHX_SERVER_INIT_MAGIC_NO;

    // Register the callback for handling messages sent to the BLE layer
    esp_err_t esp_err = esp_ble_mesh_register_generic_client_callback(
        (MESHX_GEN_CLI_CB)&esp_ble_mesh_generic_client_cb);
    if (esp_err != ESP_OK)
        err = MESHX_ERR_PLAT;

    return err;
}

/**
 * @brief Creates a Generic OnOff client model and its publication context.
 *
 * This function initializes the Generic OnOff client model, its publication
 * context, and allocates memory for the client instance. It checks for
 * invalid arguments and handles memory allocation failures.
 *
 * @param[out] p_model Pointer to the model structure to be created.
 * @param[out] p_pub Pointer to the publication context to be created.
 * @param[out] p_onoff_cli Pointer to the OnOff client instance to be allocated.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully created the model and publication context.
 *     - MESHX_INVALID_ARG: One or more arguments are invalid.
 *     - MESHX_NO_MEM: Memory allocation failed.
 */
meshx_err_t meshx_plat_on_off_gen_cli_create(meshx_ptr_t p_model, meshx_ptr_t* p_pub, meshx_ptr_t* p_onoff_cli)
{
    if (!p_model || !p_pub || !p_onoff_cli)
    {
        return MESHX_INVALID_ARG; // Invalid arguments
    }
    /* SIG On OFF Init */
    uint16_t model_id = ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_CLI;
    memcpy((meshx_ptr_t)&(((MESHX_MODEL *)p_model)->model_id), &model_id, sizeof(model_id));

    return meshx_plat_gen_cli_create(p_model, p_pub, p_onoff_cli);
}

/**
 * @brief Deletes the Generic OnOff Client model and its associated resources.
 *
 * This function frees the memory allocated for the Generic OnOff Client
 * and sets the pointer to NULL. It also deletes the model publication
 * resources associated with the client.
 *
 * @param[in,out] p_pub Pointer to the publication structure to be deleted.
 * @param[in,out] p_cli Pointer to the OnOff Client structure to be freed.
 *
 * @return
 *     - MESHX_SUCCESS: Model and publication deleted successfully.
 *     - MESHX_FAIL: Failed to delete the model or publication.
 */
meshx_err_t meshx_plat_gen_cli_delete(meshx_ptr_t* p_pub, meshx_ptr_t* p_cli)
{
    if (p_cli)
    {
        MESHX_FREE(*p_cli);
        *p_cli = NULL;
    }

    return meshx_plat_del_model_pub(p_pub);
}

/**
 * @brief Sends a Generic Client message over BLE Mesh.
 *
 * This function sends a message from a Generic Client model to a specified address
 * within the BLE Mesh network, using the provided opcode and parameters.
 *
 * @param[in] p_model   Pointer to the Generic Client model instance.
 * @param[in] p_set     Pointer to the structure containing the message parameters to set.
 * @param[in] opcode    Operation code specifying the type of message to send.
 * @param[in] addr      Destination address within the BLE Mesh network.
 * @param[in] net_idx   Network index identifying the subnet to use.
 * @param[in] app_idx   Application key index to encrypt the message.
 *
 * @return meshx_err_t  Result of the operation. Returns MESHX_OK on success or an error code on failure.
 */
meshx_err_t meshx_plat_gen_cli_send_msg(
    meshx_ptr_t p_model, meshx_gen_cli_set_t *p_set,
    uint16_t opcode, uint16_t addr,
    uint16_t net_idx, uint16_t app_idx
)
{
    if (!p_model || !p_set)
    {
        return MESHX_INVALID_ARG; // Invalid arguments
    }

    esp_ble_mesh_client_common_param_t common = {0};
    common.model        = p_model;
    common.opcode       = opcode;
    common.ctx.addr     = addr;
    common.ctx.net_idx  = net_idx;
    common.ctx.app_idx  = app_idx;
    common.ctx.send_ttl = 3;
    common.msg_timeout  = 0; /* 0 indicates that timeout value from menuconfig will be used */

    esp_err_t err = esp_ble_mesh_generic_client_set_state(
        &common,
        (esp_ble_mesh_generic_client_set_state_t*) p_set);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Send Generic OnOff failed");
        return MESHX_FAIL;
    }

    return MESHX_SUCCESS;
}
