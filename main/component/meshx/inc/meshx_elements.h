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

#include <meshx_common.h>

#if CONFIG_ENABLE_PROVISIONING
#include <meshx_prov_srv.h>
#endif /* CONFIG_ENABLE_PROVISIONING */

#if CONFIG_ENABLE_CONFIG_SERVER
#include <meshx_config_server.h>
#endif /* CONFIG_ENABLE_CONFIG_SERVER */
