/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_pair_orchestrator.c
 * @brief Autonomous Pairing Orchestration Engine implementation.
 *
 * @author Pranjal Chanda
 * @date 2026-10-16
 */

#include <string.h>
#include <meshx_api.h>
#include <nvs/meshx_nvs.h>
#include <meshx_builder_api.h>
#include <pairing/meshx_manifest.h>
#include <interface/logging/meshx_log.h>
#include <pairing/meshx_compat_matrix.h>
#include <pairing/meshx_group_allocator.h>
#include <pairing/meshx_pair_orchestrator.h>

#define TAG MODULE_ID_COMMON

/**
 * @brief Pending pairing transaction context.
 */
typedef struct
{
    bool     active;
    uint8_t  src_elem_idx;
    uint8_t  dst_elem_idx;
    uint16_t dst_unicast;
    uint16_t func_id;
    uint16_t proposed_group;
} meshx_pending_pair_t;

static meshx_pending_pair_t s_pending_pair;
static meshx_pair_status_cb_t s_pair_status_cb = NULL;

/**
 * @brief Helper to persist an entry in the link table and bump epoch.
 */
static meshx_err_t store_link_entry(const meshx_nvs_link_entry_t *entry)
{
    meshx_nvs_link_table_t table;
    int free_slot = -1;

    if (meshx_nvs_link_table_get(&table) != MESHX_SUCCESS)
    {
        memset(&table, 0, sizeof(table));
        table.magic = MESHX_NVS_LINK_MAGIC;
        table.version = MESHX_NVS_LINK_VERSION;
    }

    for (size_t i = 0; i < MESHX_MAX_PERSISTED_LINKS; i++)
    {
        if (table.links[i].active &&
            table.links[i].local_element_idx == entry->local_element_idx &&
            table.links[i].target_unicast_addr == entry->target_unicast_addr &&
            table.links[i].func_id == entry->func_id)
        {
            table.links[i] = *entry;
            table.link_epoch++;
            return meshx_nvs_link_table_set(&table);
        }
        if (!table.links[i].active && free_slot < 0)
        {
            free_slot = (int)i;
        }
    }

    if (free_slot < 0)
    {
        return MESHX_NO_MEM;
    }

    table.links[free_slot] = *entry;
    table.link_count++;
    table.link_epoch++;
    return meshx_nvs_link_table_set(&table);
}

/**
 * @brief Initialize the pairing orchestrator engine.
 */
meshx_err_t meshx_pair_init(void)
{
    memset(&s_pending_pair, 0, sizeof(s_pending_pair));
    s_pair_status_cb = NULL;
    return MESHX_SUCCESS;
}

/**
 * @brief Register a callback for pairing transaction events.
 */
meshx_err_t meshx_pair_register_cb(meshx_pair_status_cb_t cb)
{
    s_pair_status_cb = cb;
    return MESHX_SUCCESS;
}

/**
 * @brief Initiate an autonomous pairing transaction with a peer element.
 */
meshx_err_t meshx_pair_initiate(uint8_t src_elem_idx,
                                uint16_t dst_unicast,
                                uint8_t dst_elem_idx,
                                uint16_t func_id)
{
    meshx_nvs_link_table_t table;
    uint16_t local_unicast = 0;
    uint16_t local_variant = 0;
    uint16_t group_addr = MESHX_ADDR_UNASSIGNED;
    uint16_t anchor_addr = MESHX_ADDR_UNASSIGNED;
    size_t active_links_on_func = 0;
    int first_link_slot = -1;

    local_unicast = meshx_get_node_addr();
    if (local_unicast == MESHX_ADDR_UNASSIGNED)
    {
        MESHX_LOGE(TAG, "Pair Orchestrator: Node not provisioned (unicast unassigned)");
        return MESHX_INVALID_STATE;
    }

    local_variant = meshx_comp_get_element_variant(src_elem_idx);
    if (local_variant == 0)
    {
        MESHX_LOGE(TAG, "Pair Orchestrator: Invalid source element %u", src_elem_idx);
        return MESHX_INVALID_ARG;
    }

    if (!meshx_comp_element_has_func_id(src_elem_idx, func_id))
    {
        MESHX_LOGE(TAG, "Pair Orchestrator: Element %u lacks func_id 0x%04X", src_elem_idx, func_id);
        return MESHX_NOT_SUPPORTED;
    }

    /* Inspect current persistent link table */
    if (meshx_nvs_link_table_get(&table) == MESHX_SUCCESS)
    {
        for (size_t i = 0; i < MESHX_MAX_PERSISTED_LINKS; i++)
        {
            if (table.links[i].active &&
                table.links[i].local_element_idx == src_elem_idx &&
                table.links[i].func_id == func_id)
            {
                active_links_on_func++;
                if (first_link_slot < 0)
                {
                    first_link_slot = (int)i;
                }
            }
        }
    }

    /* Topology Decision:
     * If 0 existing peers on this func_id, use 1:1 direct unicast (group_addr = 0x0000).
     * If 1 or more existing peers, promote to 1:N group cluster.
     */
    if (active_links_on_func == 0)
    {
        group_addr = MESHX_ADDR_UNASSIGNED;
        MESHX_LOGI(TAG, "Pair Orchestrator: Initiating 1:1 direct unicast pairing to 0x%04X:el%u",
                   dst_unicast, dst_elem_idx);
    }
    else
    {
        anchor_addr = meshx_group_resolve_anchor_topology(local_unicast, dst_unicast,
                                                          MESHX_ROLE_PRODUCER, MESHX_ROLE_CONSUMER,
                                                          MESHX_TOPOLOGY_1_TO_N);
        group_addr = meshx_group_derive_from_anchor(anchor_addr);

        /* If previous single link was 1:1 unicast (group_addr == MESHX_ADDR_UNASSIGNED), promote it! */
        if (active_links_on_func == 1 && first_link_slot >= 0 &&
            table.links[first_link_slot].target_group_addr == MESHX_ADDR_UNASSIGNED)
        {
            meshx_uvp_upgrade_req_t upgrade;
            memset(&upgrade, 0, sizeof(upgrade));
            upgrade.msg_type = MESHX_SYS_CMD_TOPOLOGY_UPGRADE;
            upgrade.dst_elem_idx = 1;
            upgrade.func_id = func_id;
            upgrade.group_addr = group_addr;

            MESHX_LOGI(TAG, "Pair Orchestrator: Promoting 1:1 link to Group 0x%04X, notifying peer 0x%04X",
                       group_addr, table.links[first_link_slot].target_unicast_addr);

            meshx_comp_send_uvp(src_elem_idx,
                                table.links[first_link_slot].target_unicast_addr,
                                MESHX_FUNC_ID_SYS_PAIRING,
                                &upgrade,
                                sizeof(upgrade),
                                false);

            /* Update previous link in NVS to point to the new group */
            table.links[first_link_slot].target_group_addr = group_addr;
            table.link_epoch++;
            meshx_nvs_link_table_set(&table);
            meshx_comp_set_element_pub_addr(src_elem_idx, group_addr);
        }
    }

    /* Record pending context */
    s_pending_pair.active = true;
    s_pending_pair.src_elem_idx = src_elem_idx;
    s_pending_pair.dst_unicast = dst_unicast;
    s_pending_pair.dst_elem_idx = dst_elem_idx;
    s_pending_pair.func_id = func_id;
    s_pending_pair.proposed_group = group_addr;

    /* Transmit MESHX_SYS_CMD_PAIR_REQUEST */
    meshx_uvp_pair_req_t req;
    memset(&req, 0, sizeof(req));
    req.msg_type = MESHX_SYS_CMD_PAIR_REQUEST;
    req.src_elem_idx = src_elem_idx;
    req.dst_elem_idx = dst_elem_idx;
    req.func_id = func_id;
    req.target_group_addr = group_addr;
    req.sender_unicast = local_unicast;

    return meshx_comp_send_uvp(src_elem_idx,
                               dst_unicast,
                               MESHX_FUNC_ID_SYS_PAIRING,
                               &req,
                               sizeof(req),
                               false);
}

/**
 * @brief Handle an incoming MESHX_SYS_CMD_PAIR_REQUEST wire frame.
 */
meshx_err_t meshx_pair_handle_request(const meshx_uvp_pair_req_t *req,
                                      uint16_t src_unicast,
                                      meshx_uvp_pair_rsp_t *rsp)
{
    uint16_t local_variant = 0;
    meshx_err_t err = MESHX_SUCCESS;

    if (!req || !rsp)
    {
        return MESHX_INVALID_ARG;
    }

    rsp->msg_type = MESHX_SYS_RSP_PAIR_RESPONSE;
    rsp->target_group_addr = req->target_group_addr;

    local_variant = meshx_comp_get_element_variant(req->dst_elem_idx);
    if (local_variant == 0)
    {
        MESHX_LOGW(TAG, "Pair Orchestrator: Target element %u does not exist", req->dst_elem_idx);
        rsp->status = MESHX_SYS_STATUS_ERR_ELEMENT_MISSING;
        return MESHX_SUCCESS;
    }

    if (!meshx_comp_element_has_func_id(req->dst_elem_idx, req->func_id))
    {
        MESHX_LOGW(TAG, "Pair Orchestrator: Target element %u lacks func_id 0x%04X",
                   req->dst_elem_idx, req->func_id);
        rsp->status = MESHX_SYS_STATUS_ERR_INCOMPATIBLE;
        return MESHX_SUCCESS;
    }

    /* If target is Consumer and a group address is requested, subscribe */
    if (req->target_group_addr != MESHX_ADDR_UNASSIGNED)
    {
        err = meshx_comp_add_element_sub_addr(req->dst_elem_idx, req->target_group_addr);
        if (err != MESHX_SUCCESS)
        {
            MESHX_LOGE(TAG, "Pair Orchestrator: Failed to subscribe to group 0x%04X (%d)",
                       req->target_group_addr, err);
            rsp->status = MESHX_SYS_STATUS_ERR_TABLE_FULL;
            return MESHX_SUCCESS;
        }
    }

    /* Commit link entry to local NVS link table */
    meshx_nvs_link_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.active = 1;
    entry.local_element_idx = req->dst_elem_idx;
    entry.bound_variant = local_variant;
    entry.target_unicast_addr = req->sender_unicast ? req->sender_unicast : src_unicast;
    entry.target_group_addr = req->target_group_addr;
    entry.func_id = req->func_id;

    err = store_link_entry(&entry);
    if (err != MESHX_SUCCESS)
    {
        MESHX_LOGE(TAG, "Pair Orchestrator: Failed to persist link in NVS (%d)", err);
        rsp->status = MESHX_SYS_STATUS_ERR_TABLE_FULL;
        return MESHX_SUCCESS;
    }

    rsp->status = MESHX_SYS_STATUS_SUCCESS;

    MESHX_LOGI(TAG, "Pair Orchestrator: Successfully paired El%u with Peer 0x%04X:el%u (Group 0x%04X)",
               req->dst_elem_idx, entry.target_unicast_addr, req->src_elem_idx, req->target_group_addr);

    return MESHX_SUCCESS;
}

/**
 * @brief Handle an incoming MESHX_SYS_RSP_PAIR_RESPONSE wire frame.
 */
meshx_err_t meshx_pair_handle_response(const meshx_uvp_pair_rsp_t *rsp,
                                       uint16_t src_unicast)
{
    if (!rsp)
    {
        return MESHX_INVALID_ARG;
    }

    if (!s_pending_pair.active || s_pending_pair.dst_unicast != src_unicast)
    {
        MESHX_LOGW(TAG, "Pair Orchestrator: Unexpected pair response from 0x%04X", src_unicast);
        return MESHX_NOT_FOUND;
    }

    if (rsp->status == MESHX_SYS_STATUS_SUCCESS)
    {
        uint16_t pub_target = (rsp->target_group_addr != MESHX_ADDR_UNASSIGNED) ? rsp->target_group_addr : src_unicast;
        meshx_comp_set_element_pub_addr(s_pending_pair.src_elem_idx, pub_target);

        /* Commit link entry into initiator NVS table */
        meshx_nvs_link_entry_t entry;
        memset(&entry, 0, sizeof(entry));
        entry.active = 1;
        entry.local_element_idx = s_pending_pair.src_elem_idx;
        entry.bound_variant = meshx_comp_get_element_variant(s_pending_pair.src_elem_idx);
        entry.target_unicast_addr = src_unicast;
        entry.target_group_addr = rsp->target_group_addr;
        entry.func_id = s_pending_pair.func_id;

        store_link_entry(&entry);

        MESHX_LOGI(TAG, "Pair Orchestrator: Initiator link confirmed -> Pub target 0x%04X", pub_target);

        if (s_pair_status_cb)
        {
            s_pair_status_cb(MESHX_SUCCESS, src_unicast, s_pending_pair.dst_elem_idx, rsp->target_group_addr);
        }
    }
    else
    {
        MESHX_LOGW(TAG, "Pair Orchestrator: Peer 0x%04X rejected pair request (status %u)",
                   src_unicast, rsp->status);
        if (s_pair_status_cb)
        {
            s_pair_status_cb(MESHX_FAIL, src_unicast, s_pending_pair.dst_elem_idx, 0);
        }
    }

    s_pending_pair.active = false;
    return MESHX_SUCCESS;
}

/**
 * @brief Handle an incoming MESHX_SYS_CMD_TOPOLOGY_UPGRADE wire frame.
 */
meshx_err_t meshx_pair_handle_upgrade(const meshx_uvp_upgrade_req_t *upgrade,
                                      uint16_t src_unicast,
                                      meshx_uvp_upgrade_rsp_t *rsp)
{
    meshx_nvs_link_table_t table;
    meshx_err_t err = MESHX_SUCCESS;

    if (!upgrade || !rsp)
    {
        return MESHX_INVALID_ARG;
    }

    rsp->msg_type = MESHX_SYS_RSP_UPGRADE_ACK;
    rsp->status = MESHX_SYS_STATUS_SUCCESS;

    /* Subscribe local element to upgraded group */
    err = meshx_comp_add_element_sub_addr(upgrade->dst_elem_idx, upgrade->group_addr);
    if (err != MESHX_SUCCESS)
    {
        MESHX_LOGE(TAG, "Pair Orchestrator: Upgrade sub failed for group 0x%04X (%d)",
                   upgrade->group_addr, err);
        rsp->status = MESHX_SYS_STATUS_ERR_TABLE_FULL;
        return MESHX_SUCCESS;
    }

    /* Update existing link record in NVS */
    if (meshx_nvs_link_table_get(&table) == MESHX_SUCCESS)
    {
        for (size_t i = 0; i < MESHX_MAX_PERSISTED_LINKS; i++)
        {
            if (table.links[i].active &&
                table.links[i].target_unicast_addr == src_unicast &&
                table.links[i].func_id == upgrade->func_id)
            {
                table.links[i].target_group_addr = upgrade->group_addr;
                table.link_epoch++;
                meshx_nvs_link_table_set(&table);
                break;
            }
        }
    }

    MESHX_LOGI(TAG, "Pair Orchestrator: Upgraded link with 0x%04X to Group 0x%04X",
               src_unicast, upgrade->group_addr);

    return MESHX_SUCCESS;
}
