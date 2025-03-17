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
#ifndef CONFIG_GEN_ONOFF_CLIENT_COUNT
#define CONFIG_GEN_ONOFF_CLIENT_COUNT           2
#endif /* CONFIG_GEN_ONOFF_CLIENT_COUNT */


/**
 * @brief Gen OnOff Client Counts
 */
#ifndef CONFIG_GEN_ONOFF_SERVER_COUNT
#define CONFIG_GEN_ONOFF_SERVER_COUNT           2
#endif /* CONFIG_GEN_ONOFF_SERVER_COUNT */


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
 * @brief Enable Unit Test
 */
#ifndef CONFIG_ENABLE_UNIT_TEST
#define CONFIG_ENABLE_UNIT_TEST                 1
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
    #if !CONFIG_GEN_ONOFF_SERVER_COUNT
    #error "Enable this to use Relay Server Element"
    #endif
#endif /* CONFIG_RELAY_SERVER_COUNT */

#if CONFIG_RELAY_CLIENT_COUNT
    #if !CONFIG_GEN_ONOFF_CLIENT_COUNT
    #error "Enable this to use Relay Client Element"
    #endif
#endif /* CONFIG_RELAY_SERVER_COUNT */

#if CONFIG_LIGHT_CWWW_SRV_COUNT
    #if !CONFIG_GEN_ONOFF_SERVER_COUNT && !CONFIG_ENABLE_LIGHT_CTL_SERVER
    #error "Enable this to use CWWW Server Element"
    #endif
#endif /* CONFIG_LIGHT_CWWW_SRV_COUNT */

#if CONFIG_LIGHT_CWWW_CLIENT_COUNT
    #if !CONFIG_GEN_ONOFF_CLIENT_COUNT && !CONFIG_ENABLE_LIGHT_CTL_CLIENT
    #error "Enable this to use CWWW Client Element"
    #endif
#endif /* CONFIG_LIGHT_CWWW_CLIENT_COUNT */

/**
 * @brief Model Level Config Check
 */

#if CONFIG_GEN_ONOFF_SERVER_COUNT
    #if !CONFIG_ENABLE_GEN_SERVER
    #error "Enable this to use OnOff Server Element"
    #endif
#endif /* CONFIG_GEN_ONOFF_SERVER_COUNT */

#if CONFIG_ENABLE_LIGHT_CTL_SERVER
    #if !CONFIG_ENABLE_LIGHT_SERVER
    #error "Enable this to use Light Server Element"
    #endif
#endif /* CONFIG_ENABLE_LIGHT_CTL_SERVER */

#endif /* __MESHX_CONFIG_INTERNAL_H__ */
