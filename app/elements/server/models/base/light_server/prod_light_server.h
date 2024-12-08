#ifndef __PROD_LIGHT_SERVER_H__
#define __PROD_LIGHT_SERVER_H__

#include <server_common.h>
#include "esp_ble_mesh_lighting_model_api.h"

#ifndef CONFIG_MAX_PROD_LIGHTING_SRV_CB
#define CONFIG_MAX_PROD_LIGHTING_SRV_CB   10
#endif

typedef esp_err_t (* prod_lighting_server_cb) (esp_ble_mesh_lighting_server_cb_param_t *param);

typedef struct prod_lighting_server_cb_reg
{
    uint32_t model_id;
    prod_lighting_server_cb cb;
}prod_lighting_server_cb_reg_t;

esp_err_t prod_lighting_reg_cb(uint32_t model_id, prod_lighting_server_cb cb);
esp_err_t prod_lighting_srv_init(void);

#endif /* __PROD_LIGHT_SERVER_H__ */
