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
#include "interface/ble_mesh/meshx_ble_mesh_cmn.h" /* meshx_model_t */
#include "meshx_api.h"                             /* meshx_get_net_key_id */
#include "meshx_virtual_topo.h"

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
    if (event != ESP_BLE_MESH_MODEL_OPERATION_EVT)
    {
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
    if (length < MESHX_UVP_HEADER_SIZE)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "UVP: Packet too small (%d < %d)",
                   length, MESHX_UVP_HEADER_SIZE);
        return;
    }

    meshx_uvp_header_t *header = (meshx_uvp_header_t *)msg;

    /* Resolve the local receiving element index from the model instance */
    uint8_t rx_el_id = param->model_operation.model->element_idx;

#if CONFIG_MESHX_ENABLE_VIRTUAL_TOPO
    int8_t effective_rssi = ctx->recv_rssi;
    if (!meshx_virtual_topo_filter_rx(ctx->addr, &effective_rssi)) {
        MESHX_LOGD(MODULE_ID_MODEL_SERVER, "Virtual Topo: Dropped packet from 0x%04X", ctx->addr);
        return;
    }
    ctx->recv_rssi = effective_rssi;
#endif

    MESHX_LOGI(MODULE_ID_MODEL_SERVER, "UVP Frame -> TID: %d, EL_IDX: %d, TYPE_ID: 0x%04x, Payload: %d bytes",
               header->tid, rx_el_id, header->type_id, (int)(length - MESHX_UVP_HEADER_SIZE));

    /*
     * Publish UVP message to Control Task for centralized processing.
     * msg_evt: 0x0001 (UVP Vendor Model ID)
     * resolved_rx_el_id: The physical element receiving this packet
     * uvp_header: Extracted 4-byte header
     * params: Payload after the 4-byte header
     */
    control_task_uvp_meta_t uvp_ctx = {
        .src_addr = ctx->addr,
        .dst_addr = ctx->recv_dst,
        .rx_el_id = rx_el_id,
        .uvp_header = *header};

    control_task_msg_publish_uvp(
        CONTROL_TASK_MSG_CODE_FRM_BLE,
        MESHX_MODEL_ID_UVP,
        &uvp_ctx,
        msg + MESHX_UVP_HEADER_SIZE,
        length - MESHX_UVP_HEADER_SIZE);
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
        (MESHX_VND_SRV_CB)esp_ble_mesh_vendor_server_cb);

    if (err != ESP_OK)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to register vendor callback: 0x%x", err);
        return MESHX_ERR_PLAT;
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Bind the platform-specific UVP opcodes to a model instance.
 *
 * @param p_model Pointer to the platform model structure.
 * @return MESHX_SUCCESS on success, or an error code.
 */
meshx_err_t meshx_plat_bind_uvp_opcodes(void *p_model)
{
    if (!p_model)
    {
        return MESHX_INVALID_ARG;
    }

    esp_ble_mesh_model_t *esp_model = (esp_ble_mesh_model_t *)p_model;

    /* Initialize keys and groups arrays with correct ESP-IDF sentinels.
     * C++ dynamic builder zero-initializes, which ESP-IDF misinterprets as bound elements! */
    for (int i = 0; i < CONFIG_BLE_MESH_MODEL_KEY_COUNT; i++)
    {
        esp_model->keys[i] = ESP_BLE_MESH_KEY_UNUSED;
    }
    for (int i = 0; i < CONFIG_BLE_MESH_MODEL_GROUP_COUNT; i++)
    {
        esp_model->groups[i] = ESP_BLE_MESH_ADDR_UNASSIGNED;
    }

    /* Dynamically allocate the opcodes array using active product profile opcode */
    esp_ble_mesh_model_op_t init_opcodes[] = {
        ESP_BLE_MESH_MODEL_OP(meshx_uvp_get_opcode(), MESHX_UVP_HEADER_SIZE),
        ESP_BLE_MESH_MODEL_OP_END,
    };

    esp_ble_mesh_model_op_t *uvp_opcodes = NULL;
    meshx_err_t err = MESHX_MALLOC(&uvp_opcodes, sizeof(init_opcodes));
    if (err != MESHX_SUCCESS || !uvp_opcodes)
    {
        return MESHX_NO_MEM;
    }

    memcpy(uvp_opcodes, init_opcodes, sizeof(init_opcodes));

    esp_model->op = uvp_opcodes;

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
                           uint16_t type_id,
                           const void *payload,
                           uint16_t payload_len,
                           bool ack_req,
                           uint16_t app_idx)
{
    esp_ble_mesh_model_t *esp_model = (esp_ble_mesh_model_t *)p_model;
    uint8_t *buffer = NULL;
    if (!esp_model || (!payload && payload_len > 0))
    {
        return MESHX_INVALID_ARG;
    }

    if (payload_len > MESHX_UVP_MAX_PAYLOAD)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "UVP Payload too large (%d > %d)", payload_len, MESHX_UVP_MAX_PAYLOAD);
        return MESHX_INVALID_ARG;
    }

    /* Allocate buffer for Header + Payload */
    uint16_t total_len = MESHX_UVP_HEADER_SIZE + payload_len;
    meshx_err_t m_err = MESHX_MALLOC(&buffer, total_len);
    if (m_err != MESHX_SUCCESS || !buffer)
    {
        return MESHX_NO_MEM;
    }

    /* Populate UVP Header */
    meshx_uvp_header_t *header = (meshx_uvp_header_t *)buffer;
    header->tid = g_uvp_tid++;
    header->ack_req = ack_req ? 1 : 0;
    header->rfu = 0;
    header->type_id = type_id;

    /* Copy Payload */
    if (payload_len > 0)
    {
        memcpy(buffer + MESHX_UVP_HEADER_SIZE, payload, payload_len);
    }

    uint8_t el_idx = esp_model->element_idx;

    uint16_t net_idx = meshx_get_net_key_id();

    /* Prepare Context */
    esp_ble_mesh_msg_ctx_t ctx = {
        .net_idx = net_idx,
        .app_idx = app_idx,
        .addr = dst_addr,
        .send_ttl = ESP_BLE_MESH_TTL_DEFAULT,
        .send_cred = 0,
        .send_tag = BIT1,
    };

    /* Send via ESP-BLE-MESH stack */
    esp_err_t err = esp_ble_mesh_server_model_send_msg(
        esp_model,
        &ctx,
        MESHX_UVP_OPCODE,
        total_len,
        buffer);

    uint8_t tid = header->tid;
    MESHX_FREE(&buffer);

    if (err != ESP_OK)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "UVP Send failed: 0x%x", err);
        return MESHX_ERR_PLAT;
    }

    MESHX_LOGD(MODULE_ID_MODEL_SERVER, "UVP Sent -> DST: 0x%04x, TID: %d, EL: %d, TYPE: 0x%04x",
               dst_addr, tid, el_idx, type_id);

    return MESHX_SUCCESS;
}

/**
 * @brief Add a group subscription address to a platform model.
 *
 * @param[in] p_model   Pointer to the platform model instance.
 * @param[in] sub_addr  Group address to subscribe to (0xC000 - 0xC3FF).
 *
 * @return MESHX_SUCCESS on success, MESHX_ERR_NO_MEM if table full, or error code.
 */
meshx_err_t meshx_plat_model_sub_add(void *p_model, uint16_t sub_addr)
{
    esp_ble_mesh_model_t *esp_model = (esp_ble_mesh_model_t *)p_model;
    if (!esp_model || sub_addr == ESP_BLE_MESH_ADDR_UNASSIGNED)
    {
        return MESHX_INVALID_ARG;
    }

    /* Check if already subscribed */
    for (int i = 0; i < CONFIG_BLE_MESH_MODEL_GROUP_COUNT; i++)
    {
        if (esp_model->groups[i] == sub_addr)
        {
            return MESHX_SUCCESS;
        }
    }

    /* Find empty slot */
    for (int i = 0; i < CONFIG_BLE_MESH_MODEL_GROUP_COUNT; i++)
    {
        if (esp_model->groups[i] == ESP_BLE_MESH_ADDR_UNASSIGNED)
        {
            esp_model->groups[i] = sub_addr;
            MESHX_LOGI(MODULE_ID_MODEL_SERVER, "Model 0x%04x subscribed to group 0x%04x (slot %d)",
                       esp_model->model_id, sub_addr, i);
            return MESHX_SUCCESS;
        }
    }

    MESHX_LOGW(MODULE_ID_MODEL_SERVER, "Model 0x%04x subscription table full (%d)",
               esp_model->model_id, CONFIG_BLE_MESH_MODEL_GROUP_COUNT);
    return MESHX_NO_MEM;
}

/**
 * @brief Delete a group subscription address from a platform model.
 *
 * @param[in] p_model   Pointer to the platform model instance.
 * @param[in] sub_addr  Group address to remove.
 *
 * @return MESHX_SUCCESS on success, MESHX_NOT_FOUND if not present, or error code.
 */
meshx_err_t meshx_plat_model_sub_del(void *p_model, uint16_t sub_addr)
{
    esp_ble_mesh_model_t *esp_model = (esp_ble_mesh_model_t *)p_model;
    if (!esp_model || sub_addr == ESP_BLE_MESH_ADDR_UNASSIGNED)
    {
        return MESHX_INVALID_ARG;
    }

    for (int i = 0; i < CONFIG_BLE_MESH_MODEL_GROUP_COUNT; i++)
    {
        if (esp_model->groups[i] == sub_addr)
        {
            esp_model->groups[i] = ESP_BLE_MESH_ADDR_UNASSIGNED;
            MESHX_LOGI(MODULE_ID_MODEL_SERVER, "Model 0x%04x unsubscribed from group 0x%04x (slot %d)",
                       esp_model->model_id, sub_addr, i);
            return MESHX_SUCCESS;
        }
    }

    return MESHX_NOT_FOUND;
}
