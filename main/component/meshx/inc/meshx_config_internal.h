/**
 * @file meshx_config_internal.h
 * @brief Internal configuration settings for MeshX.
 *
 * This header file defines default configuration macros for MeshX, including
 * mandatory fixed configurations, element counts, server and client counts,
 * and enabling various features such as unit testing and light servers.
 * It also includes error checks to ensure valid configuration settings.
 *
 */
#ifndef __MESHX_CONFIG_INTERNAL_H__
#define __MESHX_CONFIG_INTERNAL_H__

#ifndef DOXYGEN
#if defined(__has_include)
  #if __has_include("meshx_config.h")
    #include "meshx_config.h"
  #endif
#else
  #include "meshx_config.h"
#endif
#endif
/**
 * @brief Mandatory Fixed Configs
 */
#ifndef CONFIG_MESHX_DEFAULT
#define CONFIG_MESHX_DEFAULT                    1
#endif
#ifndef CONFIG_ENABLE_SERVER_COMMON
#define CONFIG_ENABLE_SERVER_COMMON             1
#endif
#ifndef CONFIG_ENABLE_PROVISIONING
#define CONFIG_ENABLE_PROVISIONING              1
#endif
#ifndef CONFIG_ENABLE_CONFIG_SERVER
#define CONFIG_ENABLE_CONFIG_SERVER             1
#endif

/**
 * @brief Application Main
 * @note: This needs to be defined based on platform
 */
#ifndef CONFIG_APP_MAIN
#error "Define CONFIG_APP_MAIN to the application main function"
// #define CONFIG_APP_MAIN                         main
#endif /* CONFIG_APP_MAIN */

#ifndef CONFIG_CID_ID
#define CONFIG_CID_ID                           0x7908
#endif /* CONFIG_CID_ID */

#ifndef CONFIG_PID_ID
#define CONFIG_PID_ID                           0x4
#endif /* CONFIG_PID_ID */

#ifndef CONFIG_PRODUCT_NAME
#define CONFIG_PRODUCT_NAME                     "all_in_one"
#endif /* CONFIG_PRODUCT_NAME */

/**
 * @brief Enable Unit Test
 */
#ifndef CONFIG_ENABLE_UNIT_TEST
#define CONFIG_ENABLE_UNIT_TEST                 0
#endif /* CONFIG_ENABLE_UNIT_TEST */

#define CONFIG_TXCM_ENABLE                     1

/**
 * @brief Enable raw GPIO remote control/test APIs over MXCP.
 * Enabled by default for testing/development builds. Can be disabled
 * for production releases (e.g. via -DCONFIG_MESHX_ENABLE_GPIO_TEST_API=0 or
 * when CONFIG_MESHX_PRODUCTION_BUILD is defined).
 */
#ifndef CONFIG_MESHX_ENABLE_GPIO_TEST_API
#ifdef CONFIG_MESHX_PRODUCTION_BUILD
#define CONFIG_MESHX_ENABLE_GPIO_TEST_API       0
#else
#define CONFIG_MESHX_ENABLE_GPIO_TEST_API       1
#endif
#endif

/**
 * @brief Check if element variant is a Client
 */
#ifndef MESHX_ELEMENT_TYPE_IS_CLIENT
#define MESHX_ELEMENT_TYPE_IS_CLIENT(variant) \
    (((variant) == MESHX_ELEMENT_TYPE_RELAY_CLIENT) || \
     ((variant) == MESHX_ELEMENT_TYPE_LIGHT_CWWW_CLIENT) || \
     ((variant) == MESHX_ELEMENT_TYPE_LIGHT_HSL_CLIENT) || \
     ((variant) == MESHX_ELEMENT_TYPE_SENSOR_CLIENT))
#endif

/**
 * @brief Configurable Timer & Timeout Intervals (milliseconds)
 */
#ifndef CONFIG_MESHX_COMP_RESET_DEFAULT_DELAY_MS
#define CONFIG_MESHX_COMP_RESET_DEFAULT_DELAY_MS     500   /**< Default reboot delay following composition update */
#endif
#ifndef MESHX_COMP_RESET_DEFAULT_DELAY_MS
#define MESHX_COMP_RESET_DEFAULT_DELAY_MS            CONFIG_MESHX_COMP_RESET_DEFAULT_DELAY_MS
#endif

#ifndef CONFIG_MESHX_KR_BURST_INITIAL_DELAY_MS
#define CONFIG_MESHX_KR_BURST_INITIAL_DELAY_MS       2000  /**< Initial burst timer period for Key Rotation */
#endif
#ifndef MESHX_KR_BURST_INITIAL_DELAY_MS
#define MESHX_KR_BURST_INITIAL_DELAY_MS              CONFIG_MESHX_KR_BURST_INITIAL_DELAY_MS
#endif

#ifndef CONFIG_MESHX_KR_BURST_JITTER_BASE_MS
#define CONFIG_MESHX_KR_BURST_JITTER_BASE_MS         2000  /**< Minimum jitter backoff delay (2s) */
#endif
#ifndef MESHX_KR_BURST_JITTER_BASE_MS
#define MESHX_KR_BURST_JITTER_BASE_MS                CONFIG_MESHX_KR_BURST_JITTER_BASE_MS
#endif

#ifndef CONFIG_MESHX_KR_BURST_JITTER_RANGE_MS
#define CONFIG_MESHX_KR_BURST_JITTER_RANGE_MS        3000  /**< Jitter randomization window (+0..3s) */
#endif
#ifndef MESHX_KR_BURST_JITTER_RANGE_MS
#define MESHX_KR_BURST_JITTER_RANGE_MS               CONFIG_MESHX_KR_BURST_JITTER_RANGE_MS
#endif

#ifndef CONFIG_MESHX_KR_OVERLAP_WINDOW_MS
#define CONFIG_MESHX_KR_OVERLAP_WINDOW_MS            240000 /**< 4-minute dual-key coexistence window */
#endif
#ifndef MESHX_KR_OVERLAP_WINDOW_MS
#define MESHX_KR_OVERLAP_WINDOW_MS                   CONFIG_MESHX_KR_OVERLAP_WINDOW_MS
#endif

#ifndef CONFIG_MESHX_REDUNDANCY_HB_INTERVAL_MS
#define CONFIG_MESHX_REDUNDANCY_HB_INTERVAL_MS       5000  /**< 5s periodic Master redundancy heartbeat */
#endif
#ifndef MESHX_REDUNDANCY_HB_INTERVAL_MS
#define MESHX_REDUNDANCY_HB_INTERVAL_MS              CONFIG_MESHX_REDUNDANCY_HB_INTERVAL_MS
#endif

#ifndef CONFIG_MESHX_REDUNDANCY_WD_INTERVAL_MS
#define CONFIG_MESHX_REDUNDANCY_WD_INTERVAL_MS       5000  /**< 5s periodic Standby failover watchdog period */
#endif
#ifndef MESHX_REDUNDANCY_WD_INTERVAL_MS
#define MESHX_REDUNDANCY_WD_INTERVAL_MS              CONFIG_MESHX_REDUNDANCY_WD_INTERVAL_MS
#endif

#ifndef CONFIG_MESHX_REDUNDANCY_MAX_MISSED_BEATS
#define CONFIG_MESHX_REDUNDANCY_MAX_MISSED_BEATS     3     /**< Number of missed heartbeats before promoting Standby to Master */
#endif
#ifndef MESHX_REDUNDANCY_MAX_MISSED_BEATS
#define MESHX_REDUNDANCY_MAX_MISSED_BEATS            CONFIG_MESHX_REDUNDANCY_MAX_MISSED_BEATS
#endif

#ifndef CONFIG_MESHX_ORPHAN_PULL_INITIAL_DELAY_MS
#define CONFIG_MESHX_ORPHAN_PULL_INITIAL_DELAY_MS    2000  /**< Initial recovery pull timer period */
#endif
#ifndef MESHX_ORPHAN_PULL_INITIAL_DELAY_MS
#define MESHX_ORPHAN_PULL_INITIAL_DELAY_MS           CONFIG_MESHX_ORPHAN_PULL_INITIAL_DELAY_MS
#endif

#ifndef CONFIG_MESHX_ORPHAN_PULL_JITTER_BASE_MS
#define CONFIG_MESHX_ORPHAN_PULL_JITTER_BASE_MS      1000  /**< Minimum pull jitter delay (1s) */
#endif
#ifndef MESHX_ORPHAN_PULL_JITTER_BASE_MS
#define MESHX_ORPHAN_PULL_JITTER_BASE_MS             CONFIG_MESHX_ORPHAN_PULL_JITTER_BASE_MS
#endif

#ifndef CONFIG_MESHX_ORPHAN_PULL_JITTER_RANGE_MS
#define CONFIG_MESHX_ORPHAN_PULL_JITTER_RANGE_MS     4000  /**< Pull jitter randomization window (+0..4s) */
#endif
#ifndef MESHX_ORPHAN_PULL_JITTER_RANGE_MS
#define MESHX_ORPHAN_PULL_JITTER_RANGE_MS            CONFIG_MESHX_ORPHAN_PULL_JITTER_RANGE_MS
#endif

#ifndef CONFIG_MESHX_PROV_OOB_WATCHDOG_TIMEOUT_MS
#define CONFIG_MESHX_PROV_OOB_WATCHDOG_TIMEOUT_MS    30000 /**< 30s watchdog for OOB authentication input */
#endif
#ifndef MESHX_PROV_OOB_WATCHDOG_TIMEOUT_MS
#define MESHX_PROV_OOB_WATCHDOG_TIMEOUT_MS           CONFIG_MESHX_PROV_OOB_WATCHDOG_TIMEOUT_MS
#endif

#ifndef CONFIG_MESHX_PROXIMITY_BEACON_INTERVAL_MS
#define CONFIG_MESHX_PROXIMITY_BEACON_INTERVAL_MS    500   /**< Proximity announcement periodic beacon interval (ms) */
#endif
#ifndef MESHX_PROXIMITY_BEACON_INTERVAL_MS
#define MESHX_PROXIMITY_BEACON_INTERVAL_MS           CONFIG_MESHX_PROXIMITY_BEACON_INTERVAL_MS
#endif

#ifndef CONFIG_MESHX_PROXIMITY_DEFAULT_WINDOW_SEC
#define CONFIG_MESHX_PROXIMITY_DEFAULT_WINDOW_SEC    30    /**< Default proximity pairing window (seconds) */
#endif
#ifndef MESHX_PROXIMITY_DEFAULT_WINDOW_SEC
#define MESHX_PROXIMITY_DEFAULT_WINDOW_SEC           CONFIG_MESHX_PROXIMITY_DEFAULT_WINDOW_SEC
#endif

#endif /* __MESHX_CONFIG_INTERNAL_H__ */
