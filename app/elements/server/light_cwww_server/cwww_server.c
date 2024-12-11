#include <cwww_server_model.h>
#include <string.h>

#if CONFIG_LIGHT_CWWW_SRV_COUNT > 0

#define CWWW_TEMP_MIN   2700
#define CWWW_TEMP_MAX   6500

static cwww_elements_t cwww_element_init_ctrl;

static esp_ble_mesh_model_t cwww_sig_template[CWWW_SRV_MODEL_SIG_CNT] =
{
    ESP_BLE_MESH_SIG_MODEL(ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_SRV, NULL, NULL, NULL),
    ESP_BLE_MESH_SIG_MODEL(ESP_BLE_MESH_MODEL_ID_LIGHT_CTL_SRV, NULL, NULL, NULL),
}; 

static esp_err_t dev_create_cwww_model_space(uint16_t n_max)
{
    /* Assign Spaces for Model List, Publish List and onoff gen list */
    cwww_element_init_ctrl.model_cnt = n_max;
    void ** temp;
    for (size_t cwww_model_id = 0; cwww_model_id < n_max; cwww_model_id++)
    {
#if CONFIG_GEN_ONOFF_SERVER_COUNT
        cwww_element_init_ctrl.cwww_server_onoff_gen_list[cwww_model_id].rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;
        cwww_element_init_ctrl.cwww_server_onoff_gen_list[cwww_model_id].rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;

        /* Perform memcpy to setup the constants */
        memcpy( &cwww_element_init_ctrl.cwww_server_sig_model_list[cwww_model_id][CWWW_SIG_ONOFF_MODEL_ID],
                &cwww_sig_template[CWWW_SIG_ONOFF_MODEL_ID],
                sizeof(esp_ble_mesh_model_t)
            );
        /* Set the dynamic spaces for the model */
        temp = (void**) &cwww_element_init_ctrl.cwww_server_sig_model_list[cwww_model_id][CWWW_SIG_ONOFF_MODEL_ID].pub;
        *temp = &cwww_element_init_ctrl.cwww_server_pub_list[cwww_model_id][CWWW_SIG_ONOFF_MODEL_ID];
        cwww_element_init_ctrl.cwww_server_sig_model_list[cwww_model_id][CWWW_SIG_ONOFF_MODEL_ID].user_data =
            cwww_element_init_ctrl.cwww_server_onoff_gen_list + cwww_model_id;
#endif /* CONFIG_GEN_ONOFF_SERVER_COUNT */

#if CONFIG_ENABLE_LIGHT_CTL_SERVER
        cwww_element_init_ctrl.cwww_server_light_ctl_list[cwww_model_id].rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;
        cwww_element_init_ctrl.cwww_server_light_ctl_list[cwww_model_id].rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;
        cwww_element_init_ctrl.cwww_server_light_ctl_list[cwww_model_id].state = &cwww_element_init_ctrl.cwww_light_ctl_state[cwww_model_id];

        memcpy( &cwww_element_init_ctrl.cwww_server_sig_model_list[cwww_model_id][CWWW_SIG_L_CTL_MODEL_ID],
                &cwww_sig_template[CWWW_SIG_L_CTL_MODEL_ID],
                sizeof(esp_ble_mesh_model_t)
            );
        temp = (void**) &cwww_element_init_ctrl.cwww_server_sig_model_list[cwww_model_id][CWWW_SIG_L_CTL_MODEL_ID].pub;
        *temp = &cwww_element_init_ctrl.cwww_server_pub_list[cwww_model_id][CWWW_SIG_L_CTL_MODEL_ID];
        cwww_element_init_ctrl.cwww_server_sig_model_list[cwww_model_id][CWWW_SIG_L_CTL_MODEL_ID].user_data =
            cwww_element_init_ctrl.cwww_server_light_ctl_list + cwww_model_id;
#endif /* CONFIG_ENABLE_LIGHT_CTL_SERVER */
    }
    return ESP_OK;
}

static esp_err_t dev_add_cwww_srv_model_to_element_list(dev_struct_t *pdev, uint16_t *start_idx, uint16_t n_max)
{
    if (!pdev)
        return ESP_ERR_INVALID_STATE;

    if ((n_max + *start_idx) > CONFIG_MAX_ELEMENT_COUNT)
    {
        ESP_LOGE(TAG, "No of elements limit reached");
        return ESP_ERR_NO_MEM;
    }

    esp_ble_mesh_elem_t *elements = pdev->elements;

    for (size_t i = *start_idx; i < (n_max + *start_idx); i++)
    {
        if (i == 0)
        {
            /* Insert the first SIG model in root model to save element virtual addr space */
            memcpy(&elements[i].sig_models[1],
                   cwww_element_init_ctrl.cwww_server_sig_model_list[i - *start_idx],
                   sizeof(esp_ble_mesh_model_t));
            uint8_t *ref_ptr = (uint8_t *)&elements[i].sig_model_count;
            (*ref_ptr)++;
        }
        else
        {
            elements[i].sig_models = cwww_element_init_ctrl.cwww_server_sig_model_list[i - *start_idx];
            elements[i].vnd_models = ESP_BLE_MESH_MODEL_NONE;
            uint8_t *ref_ptr = (uint8_t *)&elements[i].sig_model_count;
            *ref_ptr = CWWW_SRV_MODEL_SIG_CNT;
            ref_ptr = (uint8_t *)&elements[i].vnd_model_count;
            *ref_ptr = CWWW_SRV_MODEL_VEN_CNT;
        }
    }
    /* Increment the index for further registrations */
    *start_idx += n_max;
    return ESP_OK;
}

esp_err_t create_cwww_elements(dev_struct_t *pdev)
{
    esp_err_t err;
    err = dev_create_cwww_model_space(CONFIG_LIGHT_CWWW_SRV_COUNT);
    if (err)
    {
        ESP_LOGE(TAG, "CWWW Model create failed: (%d)", err);
        return err;
    }
    err = dev_add_cwww_srv_model_to_element_list(pdev, (uint16_t *)&pdev->element_idx, CONFIG_LIGHT_CWWW_SRV_COUNT);
    if (err)
    {
        ESP_LOGE(TAG, "CWWW Model create failed: (%d)", err);
     
        return err;
    }
    err = prod_on_off_server_init();
    if (err)
    {
        ESP_LOGE(TAG, "prod_on_off_server_init failed: (%d)", err);
     
        return err;
    }
    err = prod_light_ctl_server_init();
    if (err)
    {
        ESP_LOGE(TAG, "prod_light_ctl_server_init failed: (%d)", err);
     
        return err;
    }
    return ESP_OK;
}
#endif /* CONFIG_LIGHT_CWWW_SRV_COUNT > 0*/
