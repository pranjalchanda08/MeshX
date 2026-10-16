/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_proximity_pair.c
 * @brief Autonomous 1-Hop Proximity & Multi-Hop Relay Pairing implementation.
 *
 * @author Pranjal Chanda
 * @date 2026-10-16
 */

#include <string.h>
#include <meshx_api.h>
#include <meshx_config.h>
#include <nvs/meshx_nvs.h>
#include <meshx_builder_api.h>
#include <os/meshx_os_timer.h>
#include <pairing/meshx_manifest.h>
#include <pairing/meshx_unpair_mgr.h>
#include <pairing/meshx_compat_matrix.h>
#include <interface/logging/meshx_log.h>
#include <pairing/meshx_proximity_pair.h>
#include <pairing/meshx_pair_orchestrator.h>
#include <interface/ble_mesh/meshx_ble_mesh_cmn_def.h>

#define TAG MODULE_ID_COMMON

#ifndef MESHX_MGMT_VIRTUAL_ADDR
#define MESHX_MGMT_VIRTUAL_ADDR 0x8F42
#endif

#define DEDUP_CACHE_SIZE 8

typedef struct
{
    uint16_t unicast;
    uint8_t seq;
} dedup_entry_t;

static uint8_t                  s_dedup_idx     = 0;
static uint8_t                  s_seq_counter   = 0;
static meshx_os_timer_t         *s_window_timer = NULL;
static meshx_os_timer_t         *s_beacon_timer = NULL;
static meshx_proximity_led_cb_t s_led_cb        = NULL;
static meshx_proximity_state_t  s_state         = MESHX_PROXIMITY_STATE_IDLE;
static dedup_entry_t            s_dedup_cache[DEDUP_CACHE_SIZE] = {0};

static void broadcast_pair_announce(void);
static void on_window_timer_expired(const meshx_os_timer_t *timer);
static void on_beacon_timer_expired(const meshx_os_timer_t *timer);
static void on_orchestrator_pair_status(meshx_err_t status, uint16_t peer, uint8_t elem, uint16_t group);

/**
 * @brief Initialize the proximity and relayed pairing state machine.
 */
meshx_err_t meshx_proximity_init(void)
{
    s_state = MESHX_PROXIMITY_STATE_IDLE;
    s_led_cb = NULL;
    s_seq_counter = 0;
    s_dedup_idx = 0;
    memset(s_dedup_cache, 0, sizeof(s_dedup_cache));

    /* Register callback with pair orchestrator */
    meshx_pair_register_cb(on_orchestrator_pair_status);

    return MESHX_SUCCESS;
}

/**
 * @brief Register callback for LED feedback notifications.
 */
meshx_err_t meshx_proximity_register_led_cb(meshx_proximity_led_cb_t cb)
{
    s_led_cb = cb;
    return MESHX_SUCCESS;
}

/**
 * @brief Trigger entry into autonomous pairing mode.
 */
meshx_err_t meshx_proximity_start(uint16_t window_sec)
{
    meshx_err_t err = MESHX_SUCCESS;
    uint32_t window_ms = (window_sec ? window_sec : MESHX_PROXIMITY_DEFAULT_WINDOW_SEC) * 1000;

    if (s_state == MESHX_PROXIMITY_STATE_WINDOW_ACTIVE)
    {
        MESHX_LOGI(TAG, "Proximity: Pairing window already active");
        return MESHX_SUCCESS;
    }

    s_state = MESHX_PROXIMITY_STATE_WINDOW_ACTIVE;
    MESHX_LOGI(TAG, "Proximity: Pairing window opened for %lu ms", (unsigned long)window_ms);

    if (s_led_cb)
    {
        s_led_cb(MESHX_LED_PATTERN_AMBER_PULSE);
    }

    /* Create or start 30s window timer */
    if (!s_window_timer)
    {
        err = meshx_os_timer_create("prox_win", window_ms, false, on_window_timer_expired, &s_window_timer);
        if (err == MESHX_SUCCESS)
        {
            meshx_os_timer_start(s_window_timer);
        }
    }
    else
    {
        meshx_os_timer_start(s_window_timer);
    }

    /* Broadcast initial announcement */
    broadcast_pair_announce();

    /* Start 500ms periodic beacon timer */
    if (!s_beacon_timer)
    {
        err = meshx_os_timer_create("prox_bcn", 500, true, on_beacon_timer_expired, &s_beacon_timer);
        if (err == MESHX_SUCCESS)
        {
            meshx_os_timer_start(s_beacon_timer);
        }
    }
    else
    {
        meshx_os_timer_start(s_beacon_timer);
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Cancel / stop active pairing window immediately.
 */
meshx_err_t meshx_proximity_stop(void)
{
    if (s_window_timer)
    {
        meshx_os_timer_stop(s_window_timer);
    }
    if (s_beacon_timer)
    {
        meshx_os_timer_stop(s_beacon_timer);
    }

    s_state = MESHX_PROXIMITY_STATE_IDLE;
    if (s_led_cb)
    {
        s_led_cb(MESHX_LED_PATTERN_OFF);
    }

    MESHX_LOGI(TAG, "Proximity: Pairing window stopped");
    return MESHX_SUCCESS;
}

/**
 * @brief Get current operational state of the proximity engine.
 */
meshx_proximity_state_t meshx_proximity_get_state(void)
{
    return s_state;
}

/**
 * @brief Handle an incoming MESHX_SYS_CMD_PAIR_ANNOUNCE wire beacon.
 */
meshx_err_t meshx_proximity_handle_announce(const meshx_uvp_pair_announce_t *announce,
                                            uint16_t src_unicast,
                                            int8_t rssi,
                                            uint8_t rx_ttl)
{
    uint16_t local_unicast = meshx_get_node_addr();
    uint8_t local_elem_count = meshx_comp_get_element_count();
    uint8_t hop_distance = (rx_ttl <= 7) ? (7 - rx_ttl) : 0;

    if (!announce)
    {
        return MESHX_INVALID_ARG;
    }

    if (s_state != MESHX_PROXIMITY_STATE_WINDOW_ACTIVE)
    {
        return MESHX_SUCCESS; /* Drop if pairing window closed */
    }

    /* Drop own announcements */
    if (announce->unicast_addr == local_unicast || src_unicast == local_unicast)
    {
        return MESHX_SUCCESS;
    }

    /* Deduplication check */
    for (size_t i = 0; i < DEDUP_CACHE_SIZE; i++)
    {
        if (s_dedup_cache[i].unicast == announce->unicast_addr &&
            s_dedup_cache[i].seq == announce->seq_num)
        {
            return MESHX_SUCCESS; /* Duplicate frame */
        }
    }
    s_dedup_cache[s_dedup_idx].unicast = announce->unicast_addr;
    s_dedup_cache[s_dedup_idx].seq = announce->seq_num;
    s_dedup_idx = (s_dedup_idx + 1) % DEDUP_CACHE_SIZE;

    /* Mode A Proximity check: If direct 1-hop (hop_distance == 0), verify RSSI */
    if (hop_distance == 0 && rssi < announce->min_rssi)
    {
        MESHX_LOGD(TAG, "Proximity: RSSI %d dBm below threshold %d dBm, dropping candidate 0x%04X",
                   rssi, announce->min_rssi, announce->unicast_addr);
        return MESHX_SUCCESS;
    }

    MESHX_LOGI(TAG, "Proximity: Candidate captured -> Addr 0x%04X:el%u (Var: 0x%04X, Role: %u, RSSI: %d dBm, Hops: %u)",
               announce->unicast_addr, announce->elem_idx, announce->variant,
               announce->role, rssi, hop_distance);

    /* If peer is Consumer (Sink) and we are Producer (Source), match traits and pair */
    if (announce->role == MESHX_ROLE_CONSUMER)
    {
        for (uint8_t local_el = 1; local_el < local_elem_count; local_el++)
        {
            uint16_t local_var = meshx_comp_get_element_variant(local_el);
            uint16_t local_funcs[8];
            uint8_t num_funcs = meshx_comp_get_element_func_ids(local_el, local_funcs, 8);

            for (uint8_t f = 0; f < num_funcs; f++)
            {
                uint8_t local_role = 0;
                uint16_t local_trait = 0;
                uint8_t peer_role = 0;
                uint16_t peer_trait = 0;
                meshx_manifest_resolve_role_trait(local_var, &local_role, &local_trait);
                meshx_manifest_resolve_role_trait(announce->variant, &peer_role, &peer_trait);

                if (meshx_verify_trait_compatibility((meshx_trait_type_t)local_trait,
                                                     (meshx_trait_type_t)peer_trait,
                                                     local_funcs[f]) == MESHX_SUCCESS)
                {
                    MESHX_LOGI(TAG, "Proximity: Compatible trait match! Local El%u -> Peer 0x%04X:el%u on func 0x%04X",
                               local_el, announce->unicast_addr, announce->elem_idx, local_funcs[f]);

                    s_state = MESHX_PROXIMITY_STATE_PAIRING;
                    if (s_beacon_timer)
                    {
                        meshx_os_timer_stop(s_beacon_timer);
                    }

                    return meshx_pair_initiate(local_el,
                                               announce->unicast_addr,
                                               announce->elem_idx,
                                               local_funcs[f]);
                }
            }
        }
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Unpair all local links.
 */
meshx_err_t meshx_proximity_unpair_all(void)
{
    meshx_nvs_link_table_t table;

    MESHX_LOGW(TAG, "Proximity: Unpairing all local element links");

    if (meshx_nvs_link_table_get(&table) == MESHX_SUCCESS)
    {
        for (size_t i = 0; i < MESHX_MAX_PERSISTED_LINKS; i++)
        {
            if (table.links[i].active)
            {
                meshx_unpair_element(table.links[i].local_element_idx,
                                     table.links[i].target_unicast_addr,
                                     1,
                                     table.links[i].func_id);
            }
        }
    }

    if (s_led_cb)
    {
        s_led_cb(MESHX_LED_PATTERN_RED_FLASH);
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Broadcast autonomous pairing announcement beacon frame.
 *
 * Packs and transmits MESHX_SYS_CMD_PAIR_ANNOUNCE to MESHX_MGMT_VIRTUAL_ADDR
 * with the primary application element variant, role, and minimum RSSI threshold.
 */
static void broadcast_pair_announce(void)
{
    uint16_t local_unicast = meshx_get_node_addr();
    uint8_t el_count = meshx_comp_get_element_count();
    uint8_t app_el = (el_count > 1) ? 1 : 0;
    uint16_t var = meshx_comp_get_element_variant(app_el);

    meshx_uvp_pair_announce_t ann;
    memset(&ann, 0, sizeof(ann));
    ann.msg_type = MESHX_SYS_CMD_PAIR_ANNOUNCE;
    ann.unicast_addr = local_unicast;
    ann.elem_idx = app_el;
    ann.variant = var;
    uint8_t local_role = 0;
    uint16_t local_trait = 0;
    meshx_manifest_resolve_role_trait(var, &local_role, &local_trait);
    ann.role = local_role;
    ann.min_rssi = MESHX_PROXIMITY_RSSI_THRESHOLD;
    ann.seq_num = s_seq_counter++;

    meshx_comp_send_uvp(0,
                        MESHX_MGMT_VIRTUAL_ADDR,
                        MESHX_FUNC_ID_SYS_PAIRING,
                        &ann,
                        sizeof(ann),
                        false);
}

/**
 * @brief Pairing window timer expiration callback.
 *
 * Invoked when the 30-second proximity pairing window closes without
 * any compatible peer handshake. Transitions state to TIMEOUT and flashes red LED.
 *
 * @param[in] timer Pointer to expired OS timer instance.
 */
static void on_window_timer_expired(const meshx_os_timer_t *timer)
{
    MESHX_UNUSED(timer);
    if (s_state == MESHX_PROXIMITY_STATE_WINDOW_ACTIVE)
    {
        MESHX_LOGW(TAG, "Proximity: 30s pairing window expired without match");
        meshx_proximity_stop();
        s_state = MESHX_PROXIMITY_STATE_TIMEOUT;
        if (s_led_cb)
        {
            s_led_cb(MESHX_LED_PATTERN_RED_FLASH);
        }
    }
}

/**
 * @brief Periodic beacon timer expiration callback.
 *
 * Invoked every 500ms while the pairing window is active to repeatedly
 * broadcast pairing announcements.
 *
 * @param[in] timer Pointer to periodic OS timer instance.
 */
static void on_beacon_timer_expired(const meshx_os_timer_t *timer)
{
    MESHX_UNUSED(timer);
    if (s_state == MESHX_PROXIMITY_STATE_WINDOW_ACTIVE)
    {
        broadcast_pair_announce();
    }
}

/**
 * @brief Pair orchestrator completion status callback.
 *
 * Handles SUCCESS or error notifications from the pair orchestrator.
 * Transitions state to SUCCESS or TIMEOUT, stops beaconing, and updates LED feedback.
 *
 * @param[in] status Result status of pairing transaction (MESHX_SUCCESS or error code).
 * @param[in] peer   Unicast address of paired peer.
 * @param[in] elem   Element index on peer.
 * @param[in] group  Assigned group address, or MESHX_ADDR_UNASSIGNED.
 */
static void on_orchestrator_pair_status(meshx_err_t status, uint16_t peer, uint8_t elem, uint16_t group)
{
    MESHX_UNUSED(peer);
    MESHX_UNUSED(elem);
    MESHX_UNUSED(group);

    if (status == MESHX_SUCCESS)
    {
        MESHX_LOGI(TAG, "Proximity: Pairing transaction SUCCESS!");
        s_state = MESHX_PROXIMITY_STATE_SUCCESS;
        meshx_proximity_stop();
        if (s_led_cb)
        {
            s_led_cb(MESHX_LED_PATTERN_GREEN_SOLID);
        }
    }
    else
    {
        MESHX_LOGW(TAG, "Proximity: Pairing transaction FAILED");
        s_state = MESHX_PROXIMITY_STATE_TIMEOUT;
        meshx_proximity_stop();
        if (s_led_cb)
        {
            s_led_cb(MESHX_LED_PATTERN_RED_FLASH);
        }
    }
}
