#pragma once

#include <app_common.h>

#if CONFIG_ENABLE_PROVISIONING
#include <prod_prov.h>
#endif /* CONFIG_ENABLE_PROVISIONING */

#if CONFIG_ENABLE_CONFIG_SERVER
#include <config_server.h>
#endif /* CONFIG_ENABLE_CONFIG_SERVER */

#if CONFIG_GEN_ONOFF_SERVER_COUNT
#include <prod_onoff_server.h>
#endif /* CONFIG_GEN_ONOFF_SERVER_COUNT */

#if CONFIG_GEN_ONOFF_CLIENT_COUNT
#include <prod_onoff_client.h>
#endif /* CONFIG_GEN_ONOFF_CLIENT_COUNT */

#if CONFIG_ENABLE_LIGHT_CTL_SERVER
#include <prod_light_ctl_srv.h>
#endif /* CONFIG_ENABLE_LIGHT_CTL_SERVER */

#define ESP_ERR_PRINT_RET(_e_str, _err)            \
    if (_err != ESP_OK)                            \
    {                                              \
        ESP_LOGE(TAG, _e_str " (err 0x%x)", _err); \
        return _err;                               \
    }

#define CID_ESP CONFIG_CID_ID
