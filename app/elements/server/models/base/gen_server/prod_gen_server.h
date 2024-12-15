#ifndef __PROD_GEN_SERVER_H__
#define __PROD_GEN_SERVER_H__

#include <server_common.h>
#include <esp_ble_mesh_generic_model_api.h>

#ifndef CONFIG_MAX_PROD_SERVER_CB
#define CONFIG_MAX_PROD_SERVER_CB   2
#endif

typedef esp_err_t (* prod_server_cb) (esp_ble_mesh_generic_server_cb_param_t *param);

typedef struct prod_gen_ctx
{
    uint8_t state;
    uint8_t tid;
    uint16_t pub_addr;
    uint16_t net_id;
    uint16_t app_id;
}prod_gen_ctx_t;

typedef struct prod_server_cb_reg
{
    uint32_t model_id;
    prod_server_cb cb;
}prod_server_cb_reg_t;

esp_err_t prod_gen_srv_reg_cb(uint32_t model_id, prod_server_cb cb);
esp_err_t prod_gen_srv_init(void);

#endif /* __PROD_GEN_SERVER_H__ */
