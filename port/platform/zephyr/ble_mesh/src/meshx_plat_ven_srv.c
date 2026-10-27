/**
 * Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_plat_ven_srv.c
 * @brief Implementation of MeshX Vendor Server model handling for Zephyr Bluetooth Mesh.
 *        Processes incoming Unified Vendor Protocol (UVP) messages and dispatches them
 *        to the centralized MeshX Control Task.
 *
 * @author Pranjal Chanda
 */

#include "meshx_control_task.h"
#include "meshx_uvp.h"
#include "meshx_platform_ble_mesh.h"
#include "interface/ble_mesh/meshx_ble_mesh_cmn.h"
#include "interface/logging/meshx_log.h"
#include "meshx_virtual_topo.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/mesh.h>

/**
 * @brief Zephyr opcode handler for incoming UVP vendor model frames
 */
static int zephyr_uvp_rx_handler(const struct bt_mesh_model *model,
                                 struct bt_mesh_msg_ctx *ctx,
                                 struct net_buf_simple *buf)
{
    if (model == NULL || ctx == NULL || buf == NULL) {
        return -EINVAL;
    }

    uint16_t length = (uint16_t)buf->len;
    uint8_t *msg = buf->data;

    MESHX_LOGD(MODULE_ID_MODEL_SERVER, "Zephyr UVP RX: op|src|dst: %04x|%04x, len: %d",
               ctx->addr, ctx->recv_dst, length);

    if (length < MESHX_UVP_HEADER_SIZE) {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Zephyr UVP: Packet too small (%d < %d)",
                   length, MESHX_UVP_HEADER_SIZE);
        return -EMSGSIZE;
    }

    meshx_uvp_header_t *header = (meshx_uvp_header_t *)msg;
    uint8_t rx_el_id = (model->rt != NULL) ? model->rt->elem_idx : 0;

#if CONFIG_MESHX_ENABLE_VIRTUAL_TOPO
    int8_t effective_rssi = ctx->recv_rssi;
    if (!meshx_virtual_topo_filter_rx(ctx->addr, &effective_rssi)) {
        MESHX_LOGD(MODULE_ID_MODEL_SERVER, "Virtual Topo: Dropped packet from 0x%04X", ctx->addr);
        return 0; /* Silently dropped */
    }
    ctx->recv_rssi = effective_rssi;
#endif

    MESHX_LOGI(MODULE_ID_MODEL_SERVER, "Zephyr UVP Frame -> TID: %d, EL_IDX: %d, TYPE_ID: 0x%04x, Payload: %d bytes",
               header->tid, rx_el_id, header->type_id, (int)(length - MESHX_UVP_HEADER_SIZE));

    control_task_uvp_meta_t uvp_ctx = {
        .src_addr = ctx->addr,
        .dst_addr = ctx->recv_dst,
        .rx_el_id = rx_el_id,
        .uvp_header = *header
    };

    control_task_msg_publish_uvp(
        CONTROL_TASK_MSG_CODE_FRM_BLE,
        MESHX_MODEL_ID_UVP,
        &uvp_ctx,
        msg + MESHX_UVP_HEADER_SIZE,
        length - MESHX_UVP_HEADER_SIZE
    );

    return 0;
}

/**
 * @brief Opcode table for MeshX UVP Vendor Model under Zephyr
 */
static const struct bt_mesh_model_op g_meshx_uvp_op[] = {
    { BT_MESH_MODEL_OP_3(MESHX_UVP_OPCODE_BASE, MESHX_COMPANY_ID_UVP), MESHX_UVP_HEADER_SIZE, zephyr_uvp_rx_handler },
    BT_MESH_MODEL_OP_END,
};

meshx_err_t meshx_plat_ven_srv_init(void)
{
    MESHX_LOGD(MODULE_ID_MODEL_SERVER, "Zephyr UVP Vendor Model Handler Initialized");
    return MESHX_SUCCESS;
}

meshx_err_t meshx_plat_bind_uvp_opcodes(void *p_model)
{
    if (p_model == NULL) {
        return MESHX_INVALID_ARG;
    }

    struct bt_mesh_model *model = (struct bt_mesh_model *)p_model;
    *(const struct bt_mesh_model_op **)&model->op = g_meshx_uvp_op;

    return MESHX_SUCCESS;
}

meshx_err_t meshx_plat_model_sub_add(void *p_model, uint16_t sub_addr)
{
    if (p_model == NULL) {
        return MESHX_INVALID_ARG;
    }

    struct bt_mesh_model *model = (struct bt_mesh_model *)p_model;
    if (model->groups == NULL) {
        return MESHX_NO_MEM;
    }

    for (size_t i = 0; i < CONFIG_BT_MESH_MODEL_GROUP_COUNT; i++) {
        if (model->groups[i] == BT_MESH_ADDR_UNASSIGNED) {
            model->groups[i] = sub_addr;
            return MESHX_SUCCESS;
        }
    }

    return MESHX_NO_MEM;
}

meshx_err_t meshx_plat_model_sub_del(void *p_model, uint16_t sub_addr)
{
    if (p_model == NULL) {
        return MESHX_INVALID_ARG;
    }

    struct bt_mesh_model *model = (struct bt_mesh_model *)p_model;
    if (model->groups == NULL) {
        return MESHX_NOT_FOUND;
    }

    for (size_t i = 0; i < CONFIG_BT_MESH_MODEL_GROUP_COUNT; i++) {
        if (model->groups[i] == sub_addr) {
            model->groups[i] = BT_MESH_ADDR_UNASSIGNED;
            return MESHX_SUCCESS;
        }
    }

    return MESHX_NOT_FOUND;
}

meshx_err_t meshx_uvp_send(void *p_model,
                           uint16_t dst_addr,
                           uint16_t type_id,
                           const void *payload,
                           uint16_t payload_len,
                           bool ack_req,
                           uint16_t app_idx)
{
    struct bt_mesh_model *model = (struct bt_mesh_model *)p_model;
    if (!model || (!payload && payload_len > 0)) {
        return MESHX_INVALID_ARG;
    }
    if (payload_len > MESHX_UVP_MAX_PAYLOAD) {
        return MESHX_INVALID_ARG;
    }

    uint16_t total_len = MESHX_UVP_HEADER_SIZE + payload_len;
    uint8_t buffer[MESHX_UVP_MAX_PAYLOAD + MESHX_UVP_HEADER_SIZE];

    static uint8_t g_uvp_tid = 0;
    meshx_uvp_header_t *header = (meshx_uvp_header_t *)buffer;
    header->tid = g_uvp_tid++;
    header->ack_req = ack_req ? 1 : 0;
    header->rfu = 0;
    header->type_id = type_id;

    if (payload_len > 0) {
        memcpy(buffer + MESHX_UVP_HEADER_SIZE, payload, payload_len);
    }

    struct bt_mesh_msg_ctx ctx = {
        .net_idx = BT_MESH_KEY_ANY,
        .app_idx = app_idx,
        .addr = dst_addr,
        .send_ttl = BT_MESH_TTL_DEFAULT,
    };

    BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_MODEL_OP_3(MESHX_UVP_OPCODE_BASE, MESHX_COMPANY_ID_UVP), total_len);
    bt_mesh_model_msg_init(&msg, BT_MESH_MODEL_OP_3(MESHX_UVP_OPCODE_BASE, MESHX_COMPANY_ID_UVP));
    net_buf_simple_add_mem(&msg, buffer, total_len);

    int err = bt_mesh_model_send(model, &ctx, &msg, NULL, NULL);
    if (err) {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Zephyr UVP send failed: %d", err);
        return MESHX_ERR_PLAT;
    }

    return MESHX_SUCCESS;
}

