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
 * @brief Check if element variant is a Client
 */
#ifndef MESHX_ELEMENT_TYPE_IS_CLIENT
#define MESHX_ELEMENT_TYPE_IS_CLIENT(variant) \
    (((variant) == MESHX_ELEMENT_TYPE_RELAY_CLIENT) || \
     ((variant) == MESHX_ELEMENT_TYPE_LIGHT_CWWW_CLIENT) || \
     ((variant) == MESHX_ELEMENT_TYPE_LIGHT_HSL_CLIENT) || \
     ((variant) == MESHX_ELEMENT_TYPE_SENSOR_CLIENT))
#endif

#endif /* __MESHX_CONFIG_INTERNAL_H__ */
