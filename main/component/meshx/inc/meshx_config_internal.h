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

#include "meshx_config.h"
/**
 * @brief Mandatory Fixed Configs
 */
#define CONFIG_MESHX_DEFAULT                    1
#define CONFIG_ENABLE_SERVER_COMMON             1
#define CONFIG_ENABLE_PROVISIONING              1
#define CONFIG_ENABLE_CONFIG_SERVER             1

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
 * @brief Total Element Count in the Composition
 * @note The number shall be number of elements + 1 (to include root model)
 */
#ifndef CONFIG_MAX_ELEMENT_COUNT
#define CONFIG_MAX_ELEMENT_COUNT                5
#endif /* CONFIG_MAX_ELEMENT_COUNT */

/**
 * @brief Relay Server Counts
 */
#ifndef CONFIG_RELAY_SERVER_COUNT
#define CONFIG_RELAY_SERVER_COUNT               1
#endif /* CONFIG_RELAY_SERVER_COUNT */


/**
 * @brief Relay Client Counts
 */
#ifndef CONFIG_RELAY_CLIENT_COUNT
#define CONFIG_RELAY_CLIENT_COUNT               1
#endif /* CONFIG_RELAY_CLIENT_COUNT */


/**
 * @brief CWWW Server Counts
 */
#ifndef CONFIG_LIGHT_CWWW_SRV_COUNT
#define CONFIG_LIGHT_CWWW_SRV_COUNT             1
#endif /* CONFIG_LIGHT_CWWW_SRV_COUNT */


/**
 * @brief CWWW Client Counts
 */
#ifndef CONFIG_LIGHT_CWWW_CLIENT_COUNT
#define CONFIG_LIGHT_CWWW_CLIENT_COUNT          1
#endif /* CONFIG_LIGHT_CWWW_CLIENT_COUNT */


/**
 * @brief Gen OnOff Server Counts
 */
#ifndef CONFIG_ENABLE_GEN_ONOFF_CLIENT
#define CONFIG_ENABLE_GEN_ONOFF_CLIENT           1
#endif /* CONFIG_ENABLE_GEN_ONOFF_CLIENT */

/**
 * @brief Gen OnOff Client Counts
 */
#ifndef CONFIG_ENABLE_GEN_ONOFF_SERVER
#define CONFIG_ENABLE_GEN_ONOFF_SERVER           1
#endif /* CONFIG_ENABLE_GEN_ONOFF_SERVER */

/**
 * @brief Gen Level Client Enable
 */
#ifndef CONFIG_ENABLE_GEN_LEVEL_CLIENT
#define CONFIG_ENABLE_GEN_LEVEL_CLIENT           1
#endif /* CONFIG_ENABLE_GEN_LEVEL_CLIENT */

/**
 * @brief Gen Level Server Enable
 */
#ifndef CONFIG_ENABLE_GEN_LEVEL_SERVER
#define CONFIG_ENABLE_GEN_LEVEL_SERVER           1
#endif /* CONFIG_ENABLE_GEN_LEVEL_SERVER */

/**
 * @brief Gen Power OnOff Client Enable
 */
#ifndef CONFIG_ENABLE_GEN_POWER_ONOFF_CLIENT
#define CONFIG_ENABLE_GEN_POWER_ONOFF_CLIENT     1
#endif /* CONFIG_ENABLE_GEN_POWER_ONOFF_CLIENT */

/**
 * @brief Gen Power OnOff Server Enable
 */
#ifndef CONFIG_ENABLE_GEN_POWER_ONOFF_SERVER
#define CONFIG_ENABLE_GEN_POWER_ONOFF_SERVER     1
#endif /* CONFIG_ENABLE_GEN_POWER_ONOFF_SERVER */

/**
 * @brief Gen Power OnOff Setup Server Enable
 */
#if CONFIG_ENABLE_GEN_POWER_ONOFF_SERVER
#define CONFIG_ENABLE_GEN_POWER_ONOFF_SETUP_SERVER 1
#endif /* CONFIG_ENABLE_GEN_POWER_ONOFF_SERVER */

/**
 * @brief Gen Power Level Client Enable
 */
#ifndef CONFIG_ENABLE_GEN_POWER_LEVEL_CLIENT
#define CONFIG_ENABLE_GEN_POWER_LEVEL_CLIENT     1
#endif /* CONFIG_ENABLE_GEN_POWER_LEVEL_CLIENT */

/**
 * @brief Gen Power Level Server Enable
 */
#ifndef CONFIG_ENABLE_GEN_POWER_LEVEL_SERVER
#define CONFIG_ENABLE_GEN_POWER_LEVEL_SERVER     1
#endif /* CONFIG_ENABLE_GEN_POWER_LEVEL_SERVER */

/**
 * @brief Gen Power Level Setup Server Enable
 */
#if CONFIG_ENABLE_GEN_POWER_LEVEL_SERVER
#define CONFIG_ENABLE_GEN_POWER_LEVEL_SETUP_SERVER 1
#endif /* CONFIG_ENABLE_GEN_POWER_LEVEL_SERVER */

/**
 * @brief Gen Default Transition Time Client Enable
 */
#ifndef CONFIG_ENABLE_GEN_DEF_TRANS_TIME_CLIENT
#define CONFIG_ENABLE_GEN_DEF_TRANS_TIME_CLIENT  1
#endif /* CONFIG_ENABLE_GEN_DEF_TRANS_TIME_CLIENT */

/**
 * @brief Gen Default Transition Time Server Enable
 */
#ifndef CONFIG_ENABLE_GEN_DEF_TRANS_TIME_SERVER
#define CONFIG_ENABLE_GEN_DEF_TRANS_TIME_SERVER  1
#endif /* CONFIG_ENABLE_GEN_DEF_TRANS_TIME_SERVER */

/**
 * @brief Gen Battery Client Enable
 */
#ifndef CONFIG_ENABLE_GEN_BATTERY_CLIENT
#define CONFIG_ENABLE_GEN_BATTERY_CLIENT         1
#endif /* CONFIG_ENABLE_GEN_BATTERY_CLIENT */

/**
 * @brief Gen Battery Server Enable
 */
#ifndef CONFIG_ENABLE_GEN_BATTERY_SERVER
#define CONFIG_ENABLE_GEN_BATTERY_SERVER         1
#endif /* CONFIG_ENABLE_GEN_BATTERY_SERVER */

/**
 * @brief Gen Location Client Enable
 */
#ifndef CONFIG_ENABLE_GEN_LOCATION_CLIENT
#define CONFIG_ENABLE_GEN_LOCATION_CLIENT        1
#endif /* CONFIG_ENABLE_GEN_LOCATION_CLIENT */

/**
 * @brief Gen Location Server Enable
 */
#ifndef CONFIG_ENABLE_GEN_LOCATION_SERVER
#define CONFIG_ENABLE_GEN_LOCATION_SERVER        1
#endif /* CONFIG_ENABLE_GEN_LOCATION_SERVER */

/**
 * @brief Gen Location Setup Server Enable
 */
#if CONFIG_ENABLE_GEN_LOCATION_SERVER
#define CONFIG_ENABLE_GEN_LOCATION_SETUP_SERVER  1
#endif /* CONFIG_ENABLE_GEN_LOCATION_SERVER */

/**
 * @brief Gen Property Client Enable
 */
#ifndef CONFIG_ENABLE_GEN_PROPERTY_CLIENT
#define CONFIG_ENABLE_GEN_PROPERTY_CLIENT        1
#endif /* CONFIG_ENABLE_GEN_PROPERTY_CLIENT */

/**
 * @brief Gen Admin Property Server Enable
 */
#ifndef CONFIG_ENABLE_GEN_ADMIN_PROPERTY_SERVER
#define CONFIG_ENABLE_GEN_ADMIN_PROPERTY_SERVER  1
#endif /* CONFIG_ENABLE_GEN_ADMIN_PROPERTY_SERVER */

/**
 * @brief Gen Manufacturer Property Server Enable
 */
#ifndef CONFIG_ENABLE_GEN_MANU_PROP_SERVER
#define CONFIG_ENABLE_GEN_MANU_PROP_SERVER 1
#endif /* CONFIG_ENABLE_GEN_MANU_PROP_SERVER */

/**
 * @brief Gen User Property Server Enable
 */
#ifndef CONFIG_ENABLE_GEN_USER_PROPERTY_SERVER
#define CONFIG_ENABLE_GEN_USER_PROPERTY_SERVER   1
#endif /* CONFIG_ENABLE_GEN_USER_PROPERTY_SERVER */

/**
 * @brief Gen Client Property Server Enable
 */
#ifndef CONFIG_ENABLE_GEN_CLIENT_PROPERTY_SERVER
#define CONFIG_ENABLE_GEN_CLIENT_PROPERTY_SERVER 1
#endif /* CONFIG_ENABLE_GEN_CLIENT_PROPERTY_SERVER */

/**
 * @brief Light CTL Server Counts
 */
#ifndef CONFIG_ENABLE_LIGHT_CTL_SERVER
#define CONFIG_ENABLE_LIGHT_CTL_SERVER          1
#endif /* CONFIG_ENABLE_LIGHT_CTL_SERVER */

/**
 * @brief Light CTL Client Counts
 */
#ifndef CONFIG_LIGHT_CTL_CLIENT_COUNT
#define CONFIG_LIGHT_CTL_CLIENT_COUNT           1
#endif /* CONFIG_LIGHT_CTL_CLIENT_COUNT */

/**
 * @brief Gen OnOff Client Enable
 */
#ifndef CONFIG_ENABLE_GEN_CLIENT
#define CONFIG_ENABLE_GEN_CLIENT                1
#endif /* CONFIG_ENABLE_GEN_CLIENT */

/**
 * @brief Gen OnOff Server Enable
 */
#ifndef CONFIG_ENABLE_GEN_SERVER
#define CONFIG_ENABLE_GEN_SERVER                1
#endif /* CONFIG_ENABLE_GEN_SERVER */

/**
 * @brief Gen Light Server
 */
#ifndef CONFIG_ENABLE_LIGHT_SERVER
#define CONFIG_ENABLE_LIGHT_SERVER              1
#endif /* CONFIG_ENABLE_LIGHT_SERVER */

/**
 * @brief Gen Light Client
 */
#ifndef CONFIG_ENABLE_LIGHT_CLIENT
#define CONFIG_ENABLE_LIGHT_CLIENT              1
#endif /* CONFIG_ENABLE_LIGHT_CLIENT */

/**
 * @brief Enable Unit Test
 */
#ifndef CONFIG_ENABLE_UNIT_TEST
#define CONFIG_ENABLE_UNIT_TEST                 0
#endif /* CONFIG_ENABLE_UNIT_TEST */

/**************************************************************************
 * CONFIG ERROR CHECK
 *************************************************************************/

 /**
  * @brief Element Level config check
  */

#if CONFIG_MAX_ELEMENT_COUNT < 1
    #error "Element Count must be atleast 1"
#endif

#if CONFIG_RELAY_SERVER_COUNT
    #if !CONFIG_ENABLE_GEN_ONOFF_SERVER
    #error "Enable this to use Relay Server Element"
    #endif
#endif /* CONFIG_RELAY_SERVER_COUNT */

#if CONFIG_RELAY_CLIENT_COUNT
    #if !CONFIG_ENABLE_GEN_CLIENT
    #error "Enable this to use Relay Client Element"
    #endif
#endif /* CONFIG_RELAY_SERVER_COUNT */

#if CONFIG_LIGHT_CWWW_SRV_COUNT
    #if !CONFIG_ENABLE_GEN_SERVER || !CONFIG_ENABLE_LIGHT_CTL_SERVER
    #error "Enable this to use CWWW Server Element"
    #endif
#endif /* CONFIG_LIGHT_CWWW_SRV_COUNT */

#if CONFIG_LIGHT_CWWW_CLIENT_COUNT
    #if !CONFIG_ENABLE_GEN_CLIENT || !CONFIG_ENABLE_LIGHT_CLIENT
    #error "Enable this to use CWWW Client Element"
    #endif
#endif /* CONFIG_LIGHT_CWWW_CLIENT_COUNT */

/**
 * @brief Model Level Config Check
 */

#if CONFIG_ENABLE_GEN_ONOFF_SERVER
    #if !CONFIG_ENABLE_GEN_SERVER
    #error "Enable this to use OnOff Server Element"
    #endif
#endif /* CONFIG_ENABLE_GEN_ONOFF_SERVER */

#if CONFIG_ENABLE_GEN_ONOFF_CLIENT
    #if !CONFIG_ENABLE_GEN_CLIENT
    #error "Enable GEN_CLIENT to use OnOff Client Element"
    #endif
#endif /* CONFIG_ENABLE_GEN_ONOFF_CLIENT */

#if CONFIG_ENABLE_GEN_LEVEL_SERVER
    #if !CONFIG_ENABLE_GEN_SERVER
    #error "Enable GEN_SERVER to use Level Server Element"
    #endif
#endif /* CONFIG_ENABLE_GEN_LEVEL_SERVER */

#if CONFIG_ENABLE_GEN_LEVEL_CLIENT
    #if !CONFIG_ENABLE_GEN_CLIENT
    #error "Enable GEN_CLIENT to use Level Client Element"
    #endif
#endif /* CONFIG_ENABLE_GEN_LEVEL_CLIENT */

#if CONFIG_ENABLE_GEN_BATTERY_SERVER
    #if !CONFIG_ENABLE_GEN_SERVER
    #error "Enable GEN_SERVER to use Battery Server Element"
    #endif
#endif /* CONFIG_ENABLE_GEN_BATTERY_SERVER */

#if CONFIG_ENABLE_GEN_BATTERY_CLIENT
    #if !CONFIG_ENABLE_GEN_CLIENT
    #error "Enable GEN_CLIENT to use Battery Client Element"
    #endif
#endif /* CONFIG_ENABLE_GEN_BATTERY_CLIENT */

#if CONFIG_ENABLE_GEN_POWER_LEVEL_SERVER
    #if !CONFIG_ENABLE_GEN_SERVER
    #error "Enable GEN_SERVER to use Power Level Server Element"
    #endif
#endif /* CONFIG_ENABLE_GEN_POWER_LEVEL_SERVER */

#if CONFIG_ENABLE_GEN_POWER_LEVEL_CLIENT
    #if !CONFIG_ENABLE_GEN_CLIENT
    #error "Enable GEN_CLIENT to use Power Level Client Element"
    #endif
#endif /* CONFIG_ENABLE_GEN_POWER_LEVEL_CLIENT */

#if CONFIG_ENABLE_GEN_POWER_ONOFF_SERVER
    #if !CONFIG_ENABLE_GEN_SERVER
    #error "Enable GEN_SERVER to use Power OnOff Server Element"
    #endif
#endif /* CONFIG_ENABLE_GEN_POWER_ONOFF_SERVER */

#if CONFIG_ENABLE_GEN_POWER_ONOFF_CLIENT
    #if !CONFIG_ENABLE_GEN_CLIENT
    #error "Enable GEN_CLIENT to use Power OnOff Client Element"
    #endif
#endif /* CONFIG_ENABLE_GEN_POWER_ONOFF_CLIENT */

#if CONFIG_ENABLE_GEN_DEF_TRANS_TIME_SERVER
    #if !CONFIG_ENABLE_GEN_SERVER
    #error "Enable GEN_SERVER to use Default Transition Time Server Element"
    #endif
#endif /* CONFIG_ENABLE_GEN_DEF_TRANS_TIME_SERVER */

#if CONFIG_ENABLE_GEN_DEF_TRANS_TIME_CLIENT
    #if !CONFIG_ENABLE_GEN_CLIENT
    #error "Enable GEN_CLIENT to use Default Transition Time Client Element"
    #endif
#endif /* CONFIG_ENABLE_GEN_DEF_TRANS_TIME_CLIENT */

#if CONFIG_ENABLE_GEN_LOCATION_SERVER
    #if !CONFIG_ENABLE_GEN_SERVER
    #error "Enable GEN_SERVER to use Location Server Element"
    #endif
#endif /* CONFIG_ENABLE_GEN_LOCATION_SERVER */

#if CONFIG_ENABLE_GEN_LOCATION_CLIENT
    #if !CONFIG_ENABLE_GEN_CLIENT
    #error "Enable GEN_CLIENT to use Location Client Element"
    #endif
#endif /* CONFIG_ENABLE_GEN_LOCATION_CLIENT */

/* Property Server checks require all property types */
#if CONFIG_ENABLE_GEN_ADMIN_PROPERTY_SERVER || CONFIG_ENABLE_GEN_MANU_PROP_SERVER || CONFIG_ENABLE_GEN_USER_PROPERTY_SERVER || CONFIG_ENABLE_GEN_CLIENT_PROPERTY_SERVER
    #if !CONFIG_ENABLE_GEN_SERVER
    #error "Enable GEN_SERVER to use Property Server Elements"
    #endif
#endif

#if CONFIG_ENABLE_GEN_PROPERTY_CLIENT
    #if !CONFIG_ENABLE_GEN_CLIENT
    #error "Enable GEN_CLIENT to use Property Client Element"
    #endif
#endif /* CONFIG_ENABLE_GEN_PROPERTY_CLIENT */

#if CONFIG_ENABLE_LIGHT_CTL_SERVER
    #if !CONFIG_ENABLE_LIGHT_SERVER
    #error "Enable this to use Light Server Element"
    #endif
#endif /* CONFIG_ENABLE_LIGHT_CTL_SERVER */

#define CONFIG_TXCM_ENABLE \
        CONFIG_RELAY_CLIENT_COUNT \
    ||  CONFIG_LIGHT_CWWW_CLIENT_COUNT

#endif /* __MESHX_CONFIG_INTERNAL_H__ */
