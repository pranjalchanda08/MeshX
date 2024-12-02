
#include "relay_server_model.h"

#if CONFIG_RELAY_SERVER_ELEMENT_NOS > 0

#define RELAY_OFF 0
#define RELAY_ON !RELAY_OFF

#define TAG "REL_SRV"

static relay_elements_t relay_element_init_ctrl;

static esp_ble_mesh_model_t relay_sig_template = ESP_BLE_MESH_SIG_MODEL(
            ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_SRV, NULL, NULL, NULL);

static esp_err_t dev_create_relay_model_space(dev_struct_t *pdev, uint16_t n_max)
{
    if (!pdev)
        return ESP_ERR_INVALID_STATE;

    /* Assign Spaces for Model List, Publish List and onoff gen list */
    relay_element_init_ctrl.model_cnt = n_max;

    for (size_t relay_model_id = 0; relay_model_id < n_max; relay_model_id++)
    {
        /* Perform memcpy to setup the constants */
        memcpy( &relay_element_init_ctrl.relay_server_sig_model_list[relay_model_id][0],
                &relay_sig_template,
                sizeof(esp_ble_mesh_model_t)
            );
        /* Set the dynamic spaces for the model */
        void ** temp = (void**) &relay_element_init_ctrl.relay_server_sig_model_list[relay_model_id][0].pub;
        *temp = relay_element_init_ctrl.relay_server_pub_list + relay_model_id;
        relay_element_init_ctrl.relay_server_sig_model_list[relay_model_id][0].user_data =
            relay_element_init_ctrl.relay_server_onoff_gen_list + relay_model_id;
    }
    return ESP_OK;
}

static esp_err_t dev_add_relay_srv_model_to_element_list(dev_struct_t *pdev, uint16_t *start_idx, uint16_t n_max)
{
    if (!pdev /*|| !pdev->elements*/)
        return ESP_ERR_INVALID_STATE;

    esp_ble_mesh_elem_t *elements = pdev->elements;

    for (size_t i = *start_idx; i < (n_max + *start_idx); i++)
    {
        if (i == 0)
        {
            /* Insert the first SIG model in root model to save element virtual addr space */
            memcpy(&elements[i].sig_models[1],
                   relay_element_init_ctrl.relay_server_sig_model_list[i - *start_idx],
                   sizeof(esp_ble_mesh_model_t));
            uint8_t *ref_ptr = (uint8_t *)&elements[i].sig_model_count;
            (*ref_ptr)++;
        }
        else
        {
            elements[i].sig_models = relay_element_init_ctrl.relay_server_sig_model_list[i - *start_idx];
            elements[i].vnd_models = ESP_BLE_MESH_MODEL_NONE;
            uint8_t *ref_ptr = (uint8_t *)&elements[i].sig_model_count;
            *ref_ptr = RELAY_SRV_MODEL_SIG_CNT;
            ref_ptr = (uint8_t *)&elements[i].vnd_model_count;
            *ref_ptr = RELAY_SRV_MODEL_VEN_CNT;
        }
    }
    /* Increment the index for further registrations */
    *start_idx = *start_idx + n_max;
    return ESP_OK;
}

esp_err_t create_relay_elements(dev_struct_t *pdev)
{
    esp_err_t err;
    err = dev_create_relay_model_space(pdev, CONFIG_RELAY_SERVER_ELEMENT_NOS);
    if (err)
    {
        ESP_LOGE(TAG, "Relay Model create failed: (%d)", err);
        return err;
    }
    err = dev_add_relay_srv_model_to_element_list(pdev, (uint16_t *)&pdev->element_idx, CONFIG_RELAY_SERVER_ELEMENT_NOS);
    if (err)
    {
        ESP_LOGE(TAG, "Relay Model create failed: (%d)", err);
        return err;
    }
    return ESP_OK;
}
#endif /* CONFIG_RELAY_SERVER_ELEMENT_NOS > 0*/

