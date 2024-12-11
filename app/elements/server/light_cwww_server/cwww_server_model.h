#ifndef __CWWW_SERVER_MODEL_H__
#define __CWWW_SERVER_MODEL_H__

#include <prod_onoff_server.h>
#include <prod_light_ctl_srv.h>
#include <app_common.h>

#define CWWW_SERVER_ELEMENT_NOS_DEF 1

#ifndef CONFIG_LIGHT_CWWW_SRV_COUNT
#define CONFIG_LIGHT_CWWW_SRV_COUNT CWWW_SERVER_ELEMENT_NOS_DEF
#endif

#define CWWW_SRV_MODEL_SIG_CNT CWWW_SIG_ID_MAX   // No of SIG models in a cwww model element
#define CWWW_SRV_MODEL_VEN_CNT 0                    // No of VEN models in a cwww model element

typedef enum{
    CWWW_SIG_ONOFF_MODEL_ID,
    CWWW_SIG_L_CTL_MODEL_ID,
    CWWW_SIG_ID_MAX
}cwww_sig_id_t;

typedef struct cwww_element
{
    size_t model_cnt;
    esp_ble_mesh_model_t cwww_server_sig_model_list[CONFIG_LIGHT_CWWW_SRV_COUNT][CWWW_SRV_MODEL_SIG_CNT];
    esp_ble_mesh_model_pub_t cwww_server_pub_list[CONFIG_LIGHT_CWWW_SRV_COUNT][CWWW_SRV_MODEL_SIG_CNT];
    esp_ble_mesh_gen_onoff_srv_t cwww_server_onoff_gen_list[CONFIG_LIGHT_CWWW_SRV_COUNT];
    esp_ble_mesh_light_ctl_srv_t cwww_server_light_ctl_list[CONFIG_LIGHT_CWWW_SRV_COUNT];
    esp_ble_mesh_light_ctl_state_t cwww_light_ctl_state[CONFIG_LIGHT_CWWW_SRV_COUNT];
} cwww_elements_t;

/**
 * @brief Create Dynamic CWWW Server Model Elements
 *
 * @param[in]       pdev    Pointer to device structure
 *
 * @return esp_err_t
 */
esp_err_t create_cwww_elements(dev_struct_t *pdev);

#endif /*__CWWW_SERVER_MODEL_H__*/
