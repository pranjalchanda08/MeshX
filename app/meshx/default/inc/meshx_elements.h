#pragma once

#include <app_common.h>

#if CONFIG_ENABLE_PROVISIONING
#include <prod_prov.h>
#endif /* CONFIG_ENABLE_PROVISIONING */

#if CONFIG_ENABLE_CONFIG_SERVER
#include <config_server.h>
#endif /* CONFIG_ENABLE_CONFIG_SERVER */

#if CONFIG_RELAY_SERVER_COUNT
#include <relay_server_element.h>
#endif /* CONFIG_RELAY_SERVER_COUNT */

#if CONFIG_RELAY_CLIENT_COUNT
#include <relay_client_element.h>
#endif /* CONFIG_RELAY_CLIENT_COUNT */

#if CONFIG_LIGHT_CWWW_SRV_COUNT
#include <cwww_server_element.h>
#endif /* CONFIG_LIGHT_CWWW_SRV_COUNT */

#if CONFIG_LIGHT_CWWW_CLIENT_COUNT
#include <light_cwww_client.h>
#endif /* CONFIG_LIGHT_CWWW_CLIENT_COUNT */
