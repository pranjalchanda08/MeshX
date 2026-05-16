/**
 * @file esp_ven_srv_model.c
 * @brief Implementation of the MeshX Vendor Server model for BLE Mesh.
 *        This file handles the Unified Vendor Protocol (UVP) messages
 *        encapsulated within vendor model opcodes.
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 */

#include "meshx_control_task.h"
#include "meshx_uvp.h"
#include "meshx_platform_ble_mesh.h"
#include "interface/ble_mesh/meshx_ble_mesh_cmn.h"  /* meshx_model_t */
#include "meshx_api.h"                               /* meshx_get_net_key_id */

static uint8_t g_uvp_tid = 0; /**< Monotonic Transaction ID for UVP messages */

/**
 * @brief Callback function for BLE Mesh Vendor Server events.
 *
 * This function processes incoming vendor-specific messages and implements
 * the UVP header parsing logic.
 *
 * @param event The vendor model event type.
 * @param param Pointer to the event parameters.
 */
static void esp_ble_mesh_vendor_server_cb(MESHX_VND_SRV_CB_EVT event,
                                          MESHX_VND_SRV_CB_PARAM *param)
{
    /* We only handle receive messages (operations) */
    if (event != ESP_BLE_MESH_MODEL_OPERATION_EVT) {
        return;
    }

    esp_ble_mesh_msg_ctx_t *ctx = param->model_operation.ctx;
    uint16_t length = param->model_operation.length;
    uint8_t *msg = param->model_operation.msg;

    MESHX_LOGD(MODULE_ID_MODEL_SERVER, "VND_SRV_CB, event: %d, op|src|dst:%06" PRIx32 "|%04x|%04x",
            event, ctx->recv_op, ctx->addr, ctx->recv_dst);

    /*
     * UVP Header Parsing:
     * Every vendor model message in MeshX must start with the 4-byte UVP header.
     */
    if (length < MESHX_UVP_HEADER_SIZE) {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "UVP: Packet too small (%d < %d)",
                   length, MESHX_UVP_HEADER_SIZE);
        return;
    }

    meshx_uvp_header_t *header = (meshx_uvp_header_t *)msg;

    MESHX_LOGI(MODULE_ID_MODEL_SERVER, "UVP Frame -> TID: %d, EL_IDX: %d, TYPE_ID: 0x%04x, Payload: %d bytes",
               header->tid, header->element_idx, header->type_id, (int)(length - MESHX_UVP_HEADER_SIZE));

    /*
     * Publish UVP message to Control Task for centralized processing.
     * msg_evt: 0x0001 (UVP Vendor Model ID)
     * uvp_header: Extracted 4-byte header
     * params: Payload after the 4-byte header
     */
    control_task_msg_publish_uvp(
        CONTROL_TASK_MSG_CODE_FRM_BLE,
        MESHX_MODEL_ID_UVP,
        ctx->addr,
        *header,
        msg + MESHX_UVP_HEADER_SIZE,
        length - MESHX_UVP_HEADER_SIZE
    );
}

/**
 * @brief Initialize the vendor server model platform resources.
 *
 * Registers the vendor model callback with the underlying BLE Mesh stack.
 *
 * @return MESHX_SUCCESS on success, or MESHX_ERR_PLAT on failure.
 */
meshx_err_t meshx_plat_ven_srv_init(void)
{
    MESHX_LOGD(MODULE_ID_MODEL_SERVER, "Initializing UVP Vendor Model Callback");

    esp_err_t err = esp_ble_mesh_register_custom_model_callback(
        (MESHX_VND_SRV_CB)esp_ble_mesh_vendor_server_cb
    );

    if (err != ESP_OK) {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to register vendor callback: 0x%x", err);
        return MESHX_ERR_PLAT;
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Send a UVP message.
 *
 * @param p_model       Pointer to the UVP Vendor Model instance (meshx_model_t*).
 * @param dst_addr      Destination unicast or group address.
 * @param el_idx        Target element index on the destination node.
 * @param type_id       Target element Type ID.
 * @param payload       Pointer to the TLV payload.
 * @param payload_len   Length of the TLV payload.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_uvp_send(void *p_model,
                           uint16_t dst_addr,
                           uint8_t el_idx,
                           uint16_t type_id,
                           const void *payload,
                           uint16_t payload_len)
{
    meshx_model_t *model = (meshx_model_t *)p_model;
    if (!model || (!payload && payload_len > 0)) {
        return MESHX_INVALID_ARG;
    }

    if (payload_len > MESHX_UVP_MAX_PAYLOAD) {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "UVP Payload too large (%d > %d)", payload_len, MESHX_UVP_MAX_PAYLOAD);
        return MESHX_INVALID_ARG;
    }

    /* Allocate buffer for Header + Payload */
    uint16_t total_len = MESHX_UVP_HEADER_SIZE + payload_len;
    uint8_t *buffer = (uint8_t *)MESHX_MALLOC(total_len);
    if (!buffer) {
        return MESHX_NO_MEM;
    }

    /* Populate UVP Header */
    meshx_uvp_header_t *header = (meshx_uvp_header_t *)buffer;
    header->tid = g_uvp_tid++;
    header->element_idx = el_idx;
    header->type_id = type_id;

    /* Copy Payload */
    if (payload_len > 0) {
        memcpy(buffer + MESHX_UVP_HEADER_SIZE, payload, payload_len);
    }

    esp_ble_mesh_model_t *esp_model = (esp_ble_mesh_model_t *)model->p_model;

    /*
     * Retrieve net_idx via the runtime-stored provisioning key ID.
     * app_idx is taken from the first bound AppKey on this model (index 0);
     * defaults to 0 if no key is bound yet.
     */
    uint16_t net_idx = meshx_get_net_key_id();
    uint16_t app_idx = (esp_model && esp_model->keys[0] != ESP_BLE_MESH_KEY_UNUSED)
                       ? esp_model->keys[0] : 0;

    /* Prepare Context */
    esp_ble_mesh_msg_ctx_t ctx = {
        .net_idx   = net_idx,
        .app_idx   = app_idx,
        .addr      = dst_addr,
        .send_ttl  = ESP_BLE_MESH_TTL_DEFAULT,
        .send_cred = 0,
        .send_tag  = BIT1,
    };

    /* Send via ESP-BLE-MESH stack */
    esp_err_t err = esp_ble_mesh_server_model_send_msg(
        (esp_ble_mesh_model_t *)model->p_model,
        &ctx,
        MESHX_UVP_OPCODE,
        total_len,
        buffer
    );

    uint8_t tid = header->tid;
    MESHX_FREE(buffer);

    if (err != ESP_OK) {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "UVP Send failed: 0x%x", err);
        return MESHX_ERR_PLAT;
    }

    MESHX_LOGD(MODULE_ID_MODEL_SERVER, "UVP Sent -> DST: 0x%04x, TID: %d, EL: %d, TYPE: 0x%04x",
               dst_addr, tid, el_idx, type_id);

    return MESHX_SUCCESS;
}
