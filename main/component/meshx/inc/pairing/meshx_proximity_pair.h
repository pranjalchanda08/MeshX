/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_proximity_pair.h
 * @brief Autonomous 1-Hop Proximity & Multi-Hop Relay Pairing Engine.
 *
 * Implements the 30-second pairing window state machine, RSSI threshold
 * filtering (>= -50 dBm), multi-hop relay hop calculations, duplicate sequence
 * suppression, and push-button/LED feedback integration hooks.
 *
 * @author Pranjal Chanda
 * @date 2026-10-16
 */

#ifndef __MESHX_PROXIMITY_PAIR_H__
#define __MESHX_PROXIMITY_PAIR_H__

#include <stdint.h>
#include <stdbool.h>
#include <meshx_common.h>
#include <meshx_uvp.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MESHX_PROXIMITY_DEFAULT_WINDOW_SEC
#define MESHX_PROXIMITY_DEFAULT_WINDOW_SEC  30      /**< Default 30s pairing window */
#endif
#define MESHX_PROXIMITY_RSSI_THRESHOLD      (-50)   /**< Minimum RSSI in dBm for 1-hop proximity */

/**
 * @brief Proximity pairing engine operational states.
 */
typedef enum {
    MESHX_PROXIMITY_STATE_IDLE          = 0x00, /**< Idle, normal runtime */
    MESHX_PROXIMITY_STATE_WINDOW_ACTIVE = 0x01, /**< 30s pairing window active, announcing */
    MESHX_PROXIMITY_STATE_PAIRING       = 0x02, /**< Candidate matched, handshake in progress */
    MESHX_PROXIMITY_STATE_SUCCESS       = 0x03, /**< Successfully paired */
    MESHX_PROXIMITY_STATE_TIMEOUT       = 0x04, /**< Window timed out without pairing */
} meshx_proximity_state_t;

/**
 * @brief LED Feedback visual patterns.
 */
typedef enum {
    MESHX_LED_PATTERN_OFF               = 0x00, /**< Turn off pairing LED */
    MESHX_LED_PATTERN_AMBER_PULSE       = 0x01, /**< Amber pulse: pairing window open */
    MESHX_LED_PATTERN_GREEN_SOLID       = 0x02, /**< Solid green: pairing succeeded */
    MESHX_LED_PATTERN_RED_FLASH         = 0x03, /**< Red flash: timeout or unpair */
} meshx_proximity_led_pattern_t;

/**
 * @brief Callback for visual LED feedback.
 *
 * @param[in] pattern Requested visual pattern.
 */
typedef void (*meshx_proximity_led_cb_t)(meshx_proximity_led_pattern_t pattern);

/**
 * @brief Initialize the proximity and relayed pairing state machine.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_proximity_init(void);

/**
 * @brief Register callback for LED feedback notifications.
 *
 * @param[in] cb LED callback function.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_proximity_register_led_cb(meshx_proximity_led_cb_t cb);

/**
 * @brief Trigger entry into autonomous pairing mode.
 *
 * Opens the 30-second pairing window, starts periodic beacon announcements,
 * and enables candidate capture. Typically bound to a 3-second button hold.
 *
 * @param[in] window_sec Duration of the pairing window in seconds (default 30).
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_proximity_start(uint16_t window_sec);

/**
 * @brief Cancel / stop active pairing window immediately.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_proximity_stop(void);

/**
 * @brief Get current operational state of the proximity engine.
 *
 * @return Current meshx_proximity_state_t.
 */
meshx_proximity_state_t meshx_proximity_get_state(void);

/**
 * @brief Handle an incoming MESHX_SYS_CMD_PAIR_ANNOUNCE wire beacon.
 *
 * Filters by RSSI threshold (>= -50 dBm in 1-hop), computes hop distance,
 * discards duplicate sequences, and triggers pairing if a compatible peer is matched.
 *
 * @param[in] announce    Received beacon announcement payload.
 * @param[in] src_unicast Unicast address of sender node.
 * @param[in] rssi        Received Signal Strength Indication (dBm).
 * @param[in] rx_ttl      Received Time-To-Live for hop distance calculation.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_proximity_handle_announce(const meshx_uvp_pair_announce_t *announce,
                                            uint16_t                         src_unicast,
                                            int8_t                           rssi,
                                            uint8_t                          rx_ttl);

/**
 * @brief Unpair all local links.
 *
 * Bound to 10-second button hold. Dismantles all peer associations.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_proximity_unpair_all(void);

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_PROXIMITY_PAIR_H__ */
