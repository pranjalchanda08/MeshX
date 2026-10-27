/**
 * Copyright © 2024 - 2026 MeshX
 *
 * @file meshx_virtual_topo.c
 * @brief Dynamic Software-Defined Virtual Topology (SDVT) Switch implementation.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "meshx_virtual_topo.h"
#include "meshx_mxcp.h"
#include "interface/logging/meshx_log.h"
#include "interface/utils/meshx_tiny_printf.h"

#if CONFIG_MESHX_ENABLE_VIRTUAL_TOPO

static bool s_topo_initialized = false;

static meshx_virtual_topo_config_t s_topo_cfg = {
    .enabled             = false,
    .preset              = MESHX_TOPO_PRESET_NONE,
    .own_addr            = 0x0001,
    .default_policy_drop = false,
    .peer_count          = 0,
    .peers               = {{0}}
};

/* Simple pseudo-random generator for lossy drop calculation */
static uint32_t s_prng_seed = 123456789U;

static uint32_t topo_prng_next(void)
{
    s_prng_seed = s_prng_seed * 1103515245U + 12345U;
    return (s_prng_seed / 65536U) % 32768U;
}

meshx_err_t meshx_virtual_topo_set_own_addr(uint16_t own_addr)
{
    s_topo_cfg.own_addr = own_addr;
    MESHX_LOGI(MODULE_ID_COMMON, "Virtual Topo: Local node address set to 0x%04X", own_addr);
    return MESHX_SUCCESS;
}

meshx_err_t meshx_virtual_topo_init(void)
{
    s_topo_initialized = true;
    s_topo_cfg.enabled = true;
    s_topo_cfg.preset  = MESHX_TOPO_PRESET_NONE;
    s_topo_cfg.peer_count = 0;
    memset(s_topo_cfg.peers, 0, sizeof(s_topo_cfg.peers));

    MESHX_LOGI(MODULE_ID_COMMON, "Virtual Topo: Dynamic SDVT Switch Engine Initialized (Status: Active, Passthrough until rules applied)");
    return MESHX_SUCCESS;
}

meshx_err_t meshx_virtual_topo_deinit(void)
{
    s_topo_initialized = false;
    s_topo_cfg.enabled = false;
    s_topo_cfg.preset  = MESHX_TOPO_PRESET_NONE;
    s_topo_cfg.default_policy_drop = false;
    s_topo_cfg.peer_count = 0;
    memset(s_topo_cfg.peers, 0, sizeof(s_topo_cfg.peers));

    MESHX_LOGI(MODULE_ID_COMMON, "Virtual Topo: SDVT Switch Engine De-initialized (reverted to raw OTA passthrough)");
    return MESHX_SUCCESS;
}

bool meshx_virtual_topo_is_initialized(void)
{
    return s_topo_initialized;
}

meshx_err_t meshx_virtual_topo_set_enabled(bool enabled)
{
    s_topo_cfg.enabled = enabled;
    MESHX_LOGI(MODULE_ID_COMMON, "Virtual Topo: Filtering state set to %s", enabled ? "ENABLED" : "DISABLED (Passthrough)");
    return MESHX_SUCCESS;
}

bool meshx_virtual_topo_is_enabled(void)
{
    return s_topo_cfg.enabled;
}

meshx_err_t meshx_virtual_topo_clear(void)
{
    s_topo_cfg.enabled             = false;
    s_topo_cfg.preset              = MESHX_TOPO_PRESET_NONE;
    s_topo_cfg.default_policy_drop = false;
    s_topo_cfg.peer_count          = 0;
    memset(s_topo_cfg.peers, 0, sizeof(s_topo_cfg.peers));
    MESHX_LOGI(MODULE_ID_COMMON, "Virtual Topo: Cleared all topology rules");
    return MESHX_SUCCESS;
}

meshx_err_t meshx_virtual_topo_set_peer(uint16_t peer_addr, int8_t rssi_offset_db, uint8_t drop_rate_pct, bool blocked)
{
    /* Check if peer already exists */
    for (uint8_t i = 0; i < s_topo_cfg.peer_count; i++) {
        if (s_topo_cfg.peers[i].peer_addr == peer_addr) {
            s_topo_cfg.peers[i].rssi_offset_db = rssi_offset_db;
            s_topo_cfg.peers[i].drop_rate_pct  = drop_rate_pct;
            s_topo_cfg.peers[i].blocked        = blocked;
            MESHX_LOGD(MODULE_ID_COMMON, "Virtual Topo: Updated peer 0x%04X (offset=%d dB, drop=%d%%, blocked=%d)",
                       peer_addr, rssi_offset_db, drop_rate_pct, blocked);
            return MESHX_SUCCESS;
        }
    }

    if (s_topo_cfg.peer_count >= MESHX_VIRTUAL_TOPO_MAX_PEERS) {
        MESHX_LOGE(MODULE_ID_COMMON, "Virtual Topo: Peer table full (max %u)", MESHX_VIRTUAL_TOPO_MAX_PEERS);
        return MESHX_NO_MEM;
    }

    s_topo_cfg.peers[s_topo_cfg.peer_count].peer_addr      = peer_addr;
    s_topo_cfg.peers[s_topo_cfg.peer_count].rssi_offset_db = rssi_offset_db;
    s_topo_cfg.peers[s_topo_cfg.peer_count].drop_rate_pct  = drop_rate_pct;
    s_topo_cfg.peers[s_topo_cfg.peer_count].delay_ms       = 0;
    s_topo_cfg.peers[s_topo_cfg.peer_count].blocked        = blocked;
    s_topo_cfg.peer_count++;

    MESHX_LOGI(MODULE_ID_COMMON, "Virtual Topo: Added peer 0x%04X (offset=%d dB, drop=%d%%, blocked=%d)",
               peer_addr, rssi_offset_db, drop_rate_pct, blocked);
    return MESHX_SUCCESS;
}

meshx_err_t meshx_virtual_topo_remove_peer(uint16_t peer_addr)
{
    for (uint8_t i = 0; i < s_topo_cfg.peer_count; i++) {
        if (s_topo_cfg.peers[i].peer_addr == peer_addr) {
            for (uint8_t j = i; j < s_topo_cfg.peer_count - 1; j++) {
                s_topo_cfg.peers[j] = s_topo_cfg.peers[j + 1];
            }
            s_topo_cfg.peer_count--;
            MESHX_LOGI(MODULE_ID_COMMON, "Virtual Topo: Removed peer 0x%04X", peer_addr);
            return MESHX_SUCCESS;
        }
    }
    return MESHX_NOT_FOUND;
}

meshx_err_t meshx_virtual_topo_set_partition_state(bool is_partitioned, const uint16_t *bridge_addrs, uint8_t bridge_count)
{
    if (bridge_addrs == NULL || bridge_count == 0) {
        return MESHX_INVALID_ARG;
    }

    for (uint8_t b = 0; b < bridge_count; b++) {
        uint16_t target_bridge = bridge_addrs[b];
        for (uint8_t i = 0; i < s_topo_cfg.peer_count; i++) {
            if (s_topo_cfg.peers[i].peer_addr == target_bridge) {
                s_topo_cfg.peers[i].blocked = is_partitioned;
                MESHX_LOGI(MODULE_ID_COMMON, "Virtual Topo: Bridge 0x%04X %s (Partition State: %s)",
                           target_bridge, is_partitioned ? "SEVERED" : "HEALED",
                           is_partitioned ? "SPLIT" : "JOINED");
                break;
            }
        }
    }
    return MESHX_SUCCESS;
}

meshx_err_t meshx_virtual_topo_apply_preset(meshx_topo_preset_t preset, uint16_t own_addr)
{
    meshx_virtual_topo_clear();
    s_topo_cfg.preset   = preset;
    s_topo_cfg.own_addr = own_addr;

    switch (preset) {
    case MESHX_TOPO_PRESET_NONE:
        s_topo_cfg.enabled = false;
        s_topo_cfg.default_policy_drop = false;
        break;

    case MESHX_TOPO_PRESET_LINEAR:
        /* Daisy chain: Node i connects only to i-1 and i+1 */
        s_topo_cfg.enabled = true;
        s_topo_cfg.default_policy_drop = true; /* Whitelist mode */
        if (own_addr > 0x0001) {
            meshx_virtual_topo_set_peer(own_addr - 1, 0, 0, false);
        }
        meshx_virtual_topo_set_peer(own_addr + 1, 0, 0, false);
        break;

    case MESHX_TOPO_PRESET_CLIQUE:
        /* All nodes in direct line of sight; distance simulated via RSSI offsets */
        s_topo_cfg.enabled = true;
        s_topo_cfg.default_policy_drop = false; /* Allow all, apply RSSI modifiers */
        for (uint16_t peer = 0x0001; peer <= 0x0008; peer++) {
            if (peer != own_addr) {
                int16_t dist = (int16_t)peer - (int16_t)own_addr;
                if (dist < 0) dist = -dist;
                int8_t offset = (int8_t)(-(dist * 8)); /* e.g. -8 dB per logical rank */
                meshx_virtual_topo_set_peer(peer, offset, 0, false);
            }
        }
        break;

    case MESHX_TOPO_PRESET_TREE:
        /* Master (0x0001) -> Routers (0x0002, 0x0003) -> Leaves */
        s_topo_cfg.enabled = true;
        s_topo_cfg.default_policy_drop = true;
        if (own_addr == 0x0001) {
            meshx_virtual_topo_set_peer(0x0002, 0, 0, false);
            meshx_virtual_topo_set_peer(0x0003, 0, 0, false);
        } else if (own_addr == 0x0002) {
            meshx_virtual_topo_set_peer(0x0001, 0, 0, false);
            meshx_virtual_topo_set_peer(0x0004, 0, 0, false);
            meshx_virtual_topo_set_peer(0x0005, 0, 0, false);
        } else if (own_addr == 0x0003) {
            meshx_virtual_topo_set_peer(0x0001, 0, 0, false);
            meshx_virtual_topo_set_peer(0x0006, 0, 0, false);
            meshx_virtual_topo_set_peer(0x0007, 0, 0, false);
        } else if (own_addr == 0x0004 || own_addr == 0x0005) {
            meshx_virtual_topo_set_peer(0x0002, 0, 0, false);
        } else if (own_addr == 0x0006 || own_addr == 0x0007) {
            meshx_virtual_topo_set_peer(0x0003, 0, 0, false);
        }
        break;

    case MESHX_TOPO_PRESET_DUMBBELL:
        /* Partition A (0x0001, 0x0002) <-> Bridge (0x0005) <-> Partition B (0x0003, 0x0004, 0x0006) */
        s_topo_cfg.enabled = true;
        s_topo_cfg.default_policy_drop = true;
        if (own_addr == 0x0001 || own_addr == 0x0002) {
            meshx_virtual_topo_set_peer(0x0001, 0, 0, false);
            meshx_virtual_topo_set_peer(0x0002, 0, 0, false);
            meshx_virtual_topo_set_peer(0x0005, 0, 0, false); /* Bridge link */
        } else if (own_addr == 0x0005) { /* Bridge */
            meshx_virtual_topo_set_peer(0x0001, 0, 0, false);
            meshx_virtual_topo_set_peer(0x0002, 0, 0, false);
            meshx_virtual_topo_set_peer(0x0003, 0, 0, false);
            meshx_virtual_topo_set_peer(0x0004, 0, 0, false);
            meshx_virtual_topo_set_peer(0x0006, 0, 0, false);
        } else { /* Partition B */
            meshx_virtual_topo_set_peer(0x0003, 0, 0, false);
            meshx_virtual_topo_set_peer(0x0004, 0, 0, false);
            meshx_virtual_topo_set_peer(0x0006, 0, 0, false);
            meshx_virtual_topo_set_peer(0x0005, 0, 0, false); /* Bridge link */
        }
        break;

    case MESHX_TOPO_PRESET_DIAMOND:
        /* Node 0x0001 -> Fast Relay (0x0003) -> Target (0x0007)
         *             -> Slow Relay A (0x0004) -> Slow Relay B (0x0005) -> Target (0x0007) */
        s_topo_cfg.enabled = true;
        s_topo_cfg.default_policy_drop = true;
        if (own_addr == 0x0001) {
            meshx_virtual_topo_set_peer(0x0003, 0, 0, false);
            meshx_virtual_topo_set_peer(0x0004, 0, 0, false);
        } else if (own_addr == 0x0003) {
            meshx_virtual_topo_set_peer(0x0001, 0, 0, false);
            meshx_virtual_topo_set_peer(0x0007, 0, 0, false);
        } else if (own_addr == 0x0004) {
            meshx_virtual_topo_set_peer(0x0001, 0, 0, false);
            meshx_virtual_topo_set_peer(0x0005, 0, 0, false);
        } else if (own_addr == 0x0005) {
            meshx_virtual_topo_set_peer(0x0004, 0, 0, false);
            meshx_virtual_topo_set_peer(0x0007, 0, 0, false);
        } else if (own_addr == 0x0007) {
            meshx_virtual_topo_set_peer(0x0003, 0, 0, false);
            meshx_virtual_topo_set_peer(0x0005, 0, 0, false);
        }
        break;

    default:
        s_topo_cfg.enabled = false;
        break;
    }

    MESHX_LOGI(MODULE_ID_COMMON, "Virtual Topo: Applied Preset %d for Node 0x%04X (Peers: %u)",
               preset, own_addr, s_topo_cfg.peer_count);
    return MESHX_SUCCESS;
}

meshx_err_t meshx_virtual_topo_get_config(meshx_virtual_topo_config_t *out_cfg)
{
    if (out_cfg == NULL) {
        return MESHX_INVALID_ARG;
    }
    memcpy(out_cfg, &s_topo_cfg, sizeof(meshx_virtual_topo_config_t));
    return MESHX_SUCCESS;
}

bool meshx_virtual_topo_filter_rx(uint16_t src_addr, int8_t *inout_rssi)
{
    if (!s_topo_initialized || !s_topo_cfg.enabled) {
        return true; /* Passthrough mode: uninitialized or disabled */
    }

    /* Search peer rule */
    for (uint8_t i = 0; i < s_topo_cfg.peer_count; i++) {
        if (s_topo_cfg.peers[i].peer_addr == src_addr) {
            /* 1. Hard Cut / Partition Severance Check */
            if (s_topo_cfg.peers[i].blocked) {
                return false; /* Dropped */
            }

            /* 2. Probabilistic Lossy Link Check */
            if (s_topo_cfg.peers[i].drop_rate_pct > 0) {
                uint32_t roll = topo_prng_next() % 100U;
                if (roll < s_topo_cfg.peers[i].drop_rate_pct) {
                    return false; /* Dropped due to loss rate */
                }
            }

            /* 3. Virtual Attenuation / RSSI Offset Injection */
            if (inout_rssi != NULL && s_topo_cfg.peers[i].rssi_offset_db != 0) {
                int16_t modified = (int16_t)(*inout_rssi) + (int16_t)(s_topo_cfg.peers[i].rssi_offset_db);
                if (modified < -127) modified = -127;
                if (modified > 0)    modified = 0;
                *inout_rssi = (int8_t)modified;
            }

            return true; /* Packet permitted */
        }
    }

    /* Peer not found in table: apply default policy */
    if (s_topo_cfg.default_policy_drop) {
        return false; /* Whitelist mode: drop unlisted peers */
    }

    return true; /* Blacklist mode: allow unlisted peers */
}

/* =========================================================================
 * MXCP Event Notification Transmitter
 * ========================================================================= */

meshx_err_t meshx_virtual_topo_notify_status(uint8_t action, uint8_t status_code)
{
    uint8_t buf[sizeof(mxcp_evt_virtual_topo_status_t) + MESHX_VIRTUAL_TOPO_MAX_PEERS * sizeof(mxcp_topo_peer_entry_t)];
    mxcp_evt_virtual_topo_status_t *hdr = (mxcp_evt_virtual_topo_status_t *)buf;

    hdr->status_code = status_code;
    hdr->action      = action;
    hdr->initialized = s_topo_initialized ? 1 : 0;
    hdr->enabled     = s_topo_cfg.enabled ? 1 : 0;
    hdr->preset      = (uint8_t)s_topo_cfg.preset;
    hdr->own_addr    = s_topo_cfg.own_addr;
    hdr->peer_count  = s_topo_cfg.peer_count;

    uint8_t total_len = sizeof(mxcp_evt_virtual_topo_status_t);

    for (uint8_t i = 0; i < s_topo_cfg.peer_count && i < MESHX_VIRTUAL_TOPO_MAX_PEERS; i++) {
        mxcp_topo_peer_entry_t *entry = (mxcp_topo_peer_entry_t *)(buf + total_len);
        entry->peer_addr      = s_topo_cfg.peers[i].peer_addr;
        entry->rssi_offset_db = s_topo_cfg.peers[i].rssi_offset_db;
        entry->drop_rate_pct  = s_topo_cfg.peers[i].drop_rate_pct;
        entry->blocked        = s_topo_cfg.peers[i].blocked ? 1 : 0;
        total_len += sizeof(mxcp_topo_peer_entry_t);
    }

    MESHX_LOGD(MODULE_ID_COMMON, "Virtual Topo: Sending MXCP status evt (action=%u, status=%u, peers=%u, len=%u)",
               action, status_code, s_topo_cfg.peer_count, total_len);

    return mxcp_send_event(MXCP_EVT_VIRTUAL_TOPO_STATUS, buf, total_len);
}

#else /* !CONFIG_MESHX_ENABLE_VIRTUAL_TOPO */

typedef int __meshx_virtual_topo_empty_tu_dummy;

#endif /* CONFIG_MESHX_ENABLE_VIRTUAL_TOPO */
