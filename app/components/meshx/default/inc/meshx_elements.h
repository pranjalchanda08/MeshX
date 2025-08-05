/**
 * @file meshx_elements.h
 * @brief Header file for MeshX elements.
 *
 * This file includes the necessary headers for various MeshX elements based on the configuration.
 * The included headers depend on the enabled features and the count of different server and client elements.
 *
 * @author Pranjal Chanda
 */

#pragma once

#include <app_common.h>

#if CONFIG_ENABLE_PROVISIONING
#include <meshx_provisioning_server.h>
#endif /* CONFIG_ENABLE_PROVISIONING */

#if CONFIG_ENABLE_CONFIG_SERVER
#include <meshx_config_server.h>
#endif /* CONFIG_ENABLE_CONFIG_SERVER */

#if CONFIG_RELAY_SERVER_COUNT
#include <meshx_relay_server_element.h>
#endif /* CONFIG_RELAY_SERVER_COUNT */

#if CONFIG_RELAY_CLIENT_COUNT
#include <meshx_relay_client_element.h>
#endif /* CONFIG_RELAY_CLIENT_COUNT */

#if CONFIG_LIGHT_CWWW_SRV_COUNT
#include <meshx_cwww_server_element.h>
#endif /* CONFIG_LIGHT_CWWW_SRV_COUNT */

#if CONFIG_LIGHT_CWWW_CLIENT_COUNT
#include <meshx_light_cwww_client_element.h>
#endif /* CONFIG_LIGHT_CWWW_CLIENT_COUNT */

#include "esp_err.h"
