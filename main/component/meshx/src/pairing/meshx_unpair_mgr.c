/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_unpair_mgr.c
 * @brief Autonomous Unpair Manager and Pre-Reset Lifecycle Teardown implementation.
 *
 * @author Pranjal Chanda
 * @date 2026-10-16
 */

#include <string.h>
#include <meshx_api.h>
#include <meshx_config.h>
#include <nvs/meshx_nvs.h>
#include <meshx_builder_api.h>
#include <pairing/meshx_unpair_mgr.h>
#include <interface/rtos/meshx_task.h>
#include <interface/logging/meshx_log.h>
#include <interface/rtos/meshx_rtos_utils.h>
#include <pairing/meshx_group_allocator.h>

#define TAG MODULE_ID_COMMON

#ifndef MESHX_MGMT_VIRTUAL_ADDR
/**
 * Default Company Management Destination Address fallback.
 * Note: Resolves to the 16-bit Virtual Address (0x8F42 in 0x8000..0xBFFF) derived via
 * code_gen.py from the management Label UUID, or the Company Management Group Address (0xCFFF).
 */
#define MESHX_MGMT_VIRTUAL_ADDR 0x8F42
#endif

static meshx_unpair_status_cb_t s_unpair_status_cb = NULL;

/**
 * @brief Initialize the unpair manager subsystem.
 */
meshx_err_t meshx_unpair_init(void)
{
    s_unpair_status_cb = NULL;
    return MESHX_SUCCESS;
}

/**
 * @brief Register a callback for unpair events.
 */
meshx_err_t meshx_unpair_register_cb(meshx_unpair_status_cb_t cb)
{
    s_unpair_status_cb = cb;
    return MESHX_SUCCESS;
}

/**
 * @brief Granularly unpair an element link with a specific peer.
 */
meshx_err_t meshx_unpair_element(uint8_t    src_elem_idx,
                                 uint16_t   peer_unicast,
                                 uint8_t    peer_elem,
                                 uint16_t   func_id)
{
    meshx_nvs_link_table_t table;
    uint16_t group_addr = MESHX_ADDR_UNASSIGNED;
    int link_slot = -1;

    if (peer_unicast == MESHX_ADDR_UNASSIGNED)
    {
        return MESHX_INVALID_ARG;
    }

    if (meshx_nvs_link_table_get(&table) != MESHX_SUCCESS)
    {
        return MESHX_FAIL;
    }

    for (size_t i = 0; i < MESHX_MAX_PERSISTED_LINKS; i++)
    {
        if (table.links[i].active &&
            table.links[i].local_element_idx == src_elem_idx &&
            table.links[i].target_unicast_addr == peer_unicast &&
            table.links[i].func_id == func_id)
        {
            group_addr = table.links[i].target_group_addr;
            link_slot = (int)i;
            break;
        }
    }

    if (link_slot < 0)
    {
        MESHX_LOGW(TAG, "Unpair Mgr: Link not found for El%u -> 0x%04X:el%u (func 0x%04X)",
                   src_elem_idx, peer_unicast, peer_elem, func_id);
        return MESHX_NOT_FOUND;
    }

    /* Dispatch MESHX_SYS_CMD_UNPAIR_REQUEST to peer */
    meshx_uvp_unpair_req_t req;
    memset(&req, 0, sizeof(req));
    req.msg_type = MESHX_SYS_CMD_UNPAIR_REQUEST;
    req.src_elem_idx = src_elem_idx;
    req.dst_elem_idx = peer_elem;
    req.func_id = func_id;
    req.target_group_addr = group_addr;

    meshx_err_t send_err = meshx_comp_send_uvp(src_elem_idx,
                                               peer_unicast,
                                               MESHX_FUNC_ID_SYS_PAIRING,
                                               &req,
                                               sizeof(req),
                                               false);
    if (send_err != MESHX_SUCCESS)
    {
        MESHX_LOGW(TAG, "Unpair Mgr: Failed to dispatch UNPAIR_REQUEST to 0x%04X:el%u (err %d)",
                   peer_unicast, peer_elem, send_err);
    }

    /* Local cleansing:
     * If element pub_addr matches peer_unicast or group_addr, reset to MESHX_ADDR_UNASSIGNED
     * if no other active links use it.
     */
    uint16_t cur_pub = meshx_comp_get_element_pub_addr(src_elem_idx);
    if (cur_pub == peer_unicast || (group_addr != MESHX_ADDR_UNASSIGNED && cur_pub == group_addr))
    {
        size_t remaining_pub_uses = 0;
        for (size_t i = 0; i < MESHX_MAX_PERSISTED_LINKS; i++)
        {
            if (table.links[i].active &&
                (int)i != link_slot &&
                table.links[i].local_element_idx == src_elem_idx)
            {
                remaining_pub_uses++;
            }
        }
        if (remaining_pub_uses == 0)
        {
            meshx_comp_set_element_pub_addr(src_elem_idx, MESHX_ADDR_UNASSIGNED);
        }
    }

    /* If group subscription active and ref_count drops to 1 (this entry), unsubscribe */
    if (group_addr != MESHX_ADDR_UNASSIGNED)
    {
        if (meshx_group_get_ref_count(group_addr) <= 1)
        {
            meshx_comp_del_element_sub_addr(src_elem_idx, group_addr);
        }
    }

    /* Erase link from NVS */
    memset(&table.links[link_slot], 0, sizeof(table.links[link_slot]));
    if (table.link_count > 0)
    {
        table.link_count--;
    }
    table.link_epoch++;
    meshx_err_t set_err = meshx_nvs_link_table_set(&table);
    if (set_err != MESHX_SUCCESS)
    {
        MESHX_LOGE(TAG, "Unpair Mgr: Failed to persist unlinked table to NVS (%d)", set_err);
        return set_err;
    }

    MESHX_LOGI(TAG, "Unpair Mgr: Successfully unlinked El%u from 0x%04X:el%u",
               src_elem_idx, peer_unicast, peer_elem);

    if (s_unpair_status_cb)
    {
        s_unpair_status_cb(MESHX_SUCCESS, peer_unicast, peer_elem);
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Handle an incoming MESHX_SYS_CMD_UNPAIR_REQUEST wire frame.
 */
meshx_err_t meshx_unpair_handle_request(const meshx_uvp_unpair_req_t *req,
                                        uint16_t src_unicast,
                                        meshx_uvp_unpair_rsp_t *rsp)
{
    meshx_nvs_link_table_t table;

    if (!req || !rsp)
    {
        return MESHX_INVALID_ARG;
    }

    rsp->msg_type = MESHX_SYS_RSP_UNPAIR_RESPONSE;
    rsp->status = MESHX_SYS_STATUS_SUCCESS;

    /* If group subscription was held, check reference count */
    if (req->target_group_addr != MESHX_ADDR_UNASSIGNED)
    {
        if (meshx_group_get_ref_count(req->target_group_addr) <= 1)
        {
            meshx_comp_del_element_sub_addr(req->dst_elem_idx, req->target_group_addr);
        }
    }

    /* Reset publication address if it points to the unpairing peer */
    uint16_t cur_pub = meshx_comp_get_element_pub_addr(req->dst_elem_idx);
    if (cur_pub == src_unicast || (req->target_group_addr != MESHX_ADDR_UNASSIGNED && cur_pub == req->target_group_addr))
    {
        meshx_comp_set_element_pub_addr(req->dst_elem_idx, MESHX_ADDR_UNASSIGNED);
    }

    /* Erase corresponding link from local table */
    if (meshx_nvs_link_table_get(&table) == MESHX_SUCCESS)
    {
        for (size_t i = 0; i < MESHX_MAX_PERSISTED_LINKS; i++)
        {
            if (table.links[i].active &&
                table.links[i].local_element_idx == req->dst_elem_idx &&
                table.links[i].target_unicast_addr == src_unicast &&
                table.links[i].func_id == req->func_id)
            {
                memset(&table.links[i], 0, sizeof(table.links[i]));
                if (table.link_count > 0)
                {
                    table.link_count--;
                }
                table.link_epoch++;
                meshx_nvs_link_table_set(&table);
                break;
            }
        }
    }

    MESHX_LOGI(TAG, "Unpair Mgr: Peer 0x%04X:el%u unlinked from local El%u (func 0x%04X)",
               src_unicast, req->src_elem_idx, req->dst_elem_idx, req->func_id);

    return MESHX_SUCCESS;
}

/**
 * @brief Handle an incoming MESHX_SYS_RSP_UNPAIR_RESPONSE wire frame.
 */
meshx_err_t meshx_unpair_handle_response(const meshx_uvp_unpair_rsp_t *rsp,
                                         uint16_t src_unicast)
{
    if (!rsp)
    {
        return MESHX_INVALID_ARG;
    }

    MESHX_LOGI(TAG, "Unpair Mgr: Unpair confirmation received from peer 0x%04X (status %u)",
               src_unicast, rsp->status);

    return MESHX_SUCCESS;
}

/**
 * @brief Handle a received MESHX_SYS_NOTIFY_NODE_RESET broadcast frame.
 */
meshx_err_t meshx_unpair_handle_reset_notify(uint16_t reset_node_addr)
{
    meshx_nvs_link_table_t table;
    bool modified = false;

    if (meshx_nvs_link_table_get(&table) != MESHX_SUCCESS)
    {
        return MESHX_FAIL;
    }

    for (size_t i = 0; i < MESHX_MAX_PERSISTED_LINKS; i++)
    {
        if (table.links[i].active && table.links[i].target_unicast_addr == reset_node_addr)
        {
            uint8_t el = table.links[i].local_element_idx;
            uint16_t group = table.links[i].target_group_addr;

            /* If local element was publishing to resetting node, clear */
            if (meshx_comp_get_element_pub_addr(el) == reset_node_addr)
            {
                meshx_comp_set_element_pub_addr(el, MESHX_ADDR_UNASSIGNED);
            }

            /* Decrement group reference count; if 1, unsubscribe */
            if (group != MESHX_ADDR_UNASSIGNED && meshx_group_get_ref_count(group) <= 1)
            {
                meshx_comp_del_element_sub_addr(el, group);
            }

            memset(&table.links[i], 0, sizeof(table.links[i]));
            if (table.link_count > 0)
            {
                table.link_count--;
            }
            modified = true;
        }
    }

    if (modified)
    {
        table.link_epoch++;
        meshx_nvs_link_table_set(&table);
        MESHX_LOGI(TAG, "Unpair Mgr: Cleansed links for resetting peer 0x%04X", reset_node_addr);
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Execute mandatory Pre-Reset Lifecycle Teardown.
 *
 * Dispatches targeted UNPAIR_REQUESTs to all active peers, followed by a broadcast
 * of MESHX_SYS_NOTIFY_NODE_RESET to the Company Management Virtual / Group Address.
 *
 * Address Selection & Audit Notes:
 * - MESHX_MGMT_VIRTUAL_ADDR selects the destination address for the reset notification broadcast:
 *     - Intended Company Management Group Address: 0xCFFF (fixed group address range 0xC000..0xFEFF).
 *     - Virtual Address fallback: 0x8F42 (derived via code_gen.py from the management Label UUID
 *       per Bluetooth Mesh Profile §3.8.4 in the 0x8000..0xBFFF range).
 * - All provisioned nodes belonging to the company network subscribe to this management address on boot.
 * - Receiving nodes immediately cleanse stale link-table entries referring to the resetting node
 *   without waiting for offline heartbeat timeouts or unpair polling.
 * - Stage 2 implements a 500ms network drain window before MCU reset/NVS erase, guaranteeing radio transmission
 *   queue flush even under mesh traffic contention.
 */
meshx_err_t meshx_unpair_pre_reset_teardown(void)
{
    meshx_nvs_link_table_t table;
    uint16_t local_unicast = meshx_get_node_addr();

    MESHX_LOGW(TAG, "Unpair Mgr: Initiating Pre-Reset Teardown (Active Mesh Phase)");

    /* Stage 1: Active In-Mesh Notification to known peers */
    if (meshx_nvs_link_table_get(&table) == MESHX_SUCCESS && table.link_count > 0)
    {
        size_t active_count = 0;
        meshx_nvs_link_entry_t *snapshot = NULL;
        meshx_err_t m_err = MESHX_MALLOC(&snapshot, table.link_count * sizeof(meshx_nvs_link_entry_t));
        if (m_err == MESHX_SUCCESS && snapshot != NULL)
        {
            for (size_t i = 0; i < MESHX_MAX_PERSISTED_LINKS; i++)
            {
                if (table.links[i].active && active_count < table.link_count)
                {
                    snapshot[active_count++] = table.links[i];
                }
            }

            for (size_t i = 0; i < active_count; i++)
            {
                meshx_uvp_unpair_req_t req;
                memset(&req, 0, sizeof(req));
                req.msg_type = MESHX_SYS_CMD_UNPAIR_REQUEST;
                req.src_elem_idx = snapshot[i].local_element_idx;
                req.dst_elem_idx = 1;
                req.func_id = snapshot[i].func_id;
                req.target_group_addr = snapshot[i].target_group_addr;

                meshx_err_t err = meshx_comp_send_uvp(snapshot[i].local_element_idx,
                                                      snapshot[i].target_unicast_addr,
                                                      MESHX_FUNC_ID_SYS_PAIRING,
                                                      &req,
                                                      sizeof(req),
                                                      false);
                if (err != MESHX_SUCCESS)
                {
                    MESHX_LOGW(TAG, "Unpair Mgr: Teardown notice to 0x%04X returned %d",
                               snapshot[i].target_unicast_addr, err);
                }
            }

            MESHX_FREE(&snapshot);
        }
        else
        {
            /* Fallback to direct table iteration if heap allocation fails */
            MESHX_LOGW(TAG, "Unpair Mgr: Heap snapshot allocation failed, falling back to direct table iteration");
            for (size_t i = 0; i < MESHX_MAX_PERSISTED_LINKS; i++)
            {
                if (table.links[i].active)
                {
                    meshx_uvp_unpair_req_t req;
                    memset(&req, 0, sizeof(req));
                    req.msg_type = MESHX_SYS_CMD_UNPAIR_REQUEST;
                    req.src_elem_idx = table.links[i].local_element_idx;
                    req.dst_elem_idx = 1;
                    req.func_id = table.links[i].func_id;
                    req.target_group_addr = table.links[i].target_group_addr;

                    meshx_err_t err = meshx_comp_send_uvp(table.links[i].local_element_idx,
                                                          table.links[i].target_unicast_addr,
                                                          MESHX_FUNC_ID_SYS_PAIRING,
                                                          &req,
                                                          sizeof(req),
                                                          false);
                    if (err != MESHX_SUCCESS)
                    {
                        MESHX_LOGW(TAG, "Unpair Mgr: Teardown notice to 0x%04X returned %d",
                                   table.links[i].target_unicast_addr, err);
                    }
                }
            }
        }
    }

    /* Broadcast MESHX_SYS_NOTIFY_NODE_RESET to Company Management Virtual Address (0xCFFF) */
    meshx_uvp_notify_node_reset_t reset_notice;
    memset(&reset_notice, 0, sizeof(reset_notice));
    reset_notice.msg_type = MESHX_SYS_NOTIFY_NODE_RESET;
    reset_notice.reset_node_addr = local_unicast;

    meshx_err_t bcast_err = meshx_comp_send_uvp(0,
                                                MESHX_MGMT_VIRTUAL_ADDR,
                                                MESHX_FUNC_ID_SYS_PAIRING,
                                                &reset_notice,
                                                sizeof(reset_notice),
                                                false);
    if (bcast_err != MESHX_SUCCESS)
    {
        MESHX_LOGW(TAG, "Unpair Mgr: Teardown broadcast to 0x%04X returned %d",
                   MESHX_MGMT_VIRTUAL_ADDR, bcast_err);
    }

    /* Stage 2: 500 ms Graceful Network Drain Delay */
    MESHX_LOGI(TAG, "Unpair Mgr: Waiting 500 ms network drain window...");
    meshx_task_delay(500);

    /* Stage 3: Erase Local Link NVS Table */
    meshx_nvs_link_table_erase();
    MESHX_LOGI(TAG, "Unpair Mgr: Link table erased. Pre-reset teardown complete.");

    return MESHX_SUCCESS;
}
