#include <board.h>
#include <app_common.h>
#include <prod_prov.h>
#include <config_server.h>
#include <relay_server_model.h>

#define TAG "APP"
#define CID_ESP 0x02E5

#define MAX_ELE_CNT 1 + CONFIG_RELAY_SERVER_COUNT

void app_cfg_srv_app_key_bind_hook(esp_ble_mesh_cfg_server_cb_param_t *param);
void app_prod_prov_cb(esp_ble_mesh_prov_cb_param_t *param, prod_prov_evt_t evt);

static uint8_t dev_uuid[16] = {0xdd, 0xdd};

static config_server_params_t cfg_srv = {
    .on_app_key_cb = app_cfg_srv_app_key_bind_hook
};

static prov_params_t prod_prov_cfg = {
    .uuid = dev_uuid,
    .cb_reg = app_prod_prov_cb
};

static esp_ble_mesh_model_t root_model[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&PROD_CONFIG_SERVER_INSTANCE),
};

static esp_ble_mesh_elem_t elements[MAX_ELE_CNT] = {
    ESP_BLE_MESH_ELEMENT(0, root_model, ESP_BLE_MESH_MODEL_NONE),
#if CONFIG_RELAY_SERVER_COUNT > 0
    ESP_BLE_MESH_ELEMENT(0, RELAY_SRV_ELEMENT_INSTANCE(0), ESP_BLE_MESH_MODEL_NONE),
#endif /* CONFIG_RELAY_SERVER_COUNT */
#if CONFIG_RELAY_SERVER_COUNT > 1
    ESP_BLE_MESH_ELEMENT(0, RELAY_SRV_ELEMENT_INSTANCE(1), ESP_BLE_MESH_MODEL_NONE),
#endif /* CONFIG_RELAY_SERVER_COUNT */
#if CONFIG_RELAY_SERVER_COUNT > 2
    ESP_BLE_MESH_ELEMENT(0, RELAY_SRV_ELEMENT_INSTANCE(2), ESP_BLE_MESH_MODEL_NONE),
#endif /* CONFIG_RELAY_SERVER_COUNT */
#if CONFIG_RELAY_SERVER_COUNT > 3
    ESP_BLE_MESH_ELEMENT(0, RELAY_SRV_ELEMENT_INSTANCE(3), ESP_BLE_MESH_MODEL_NONE),
#endif /* CONFIG_RELAY_SERVER_COUNT */
#if CONFIG_RELAY_SERVER_COUNT > 4
    ESP_BLE_MESH_ELEMENT(0, RELAY_SRV_ELEMENT_INSTANCE(4), ESP_BLE_MESH_MODEL_NONE),
#endif /* CONFIG_RELAY_SERVER_COUNT */
#if CONFIG_RELAY_SERVER_COUNT > 5
    ESP_BLE_MESH_ELEMENT(0, RELAY_SRV_ELEMENT_INSTANCE(5), ESP_BLE_MESH_MODEL_NONE),
#endif /* CONFIG_RELAY_SERVER_COUNT */
#if CONFIG_RELAY_SERVER_COUNT > 6
    ESP_BLE_MESH_ELEMENT(0, RELAY_SRV_ELEMENT_INSTANCE(6), ESP_BLE_MESH_MODEL_NONE),
#endif /* CONFIG_RELAY_SERVER_COUNT */
#if CONFIG_RELAY_SERVER_COUNT > 7
    ESP_BLE_MESH_ELEMENT(0, RELAY_SRV_ELEMENT_INSTANCE(7), ESP_BLE_MESH_MODEL_NONE),
#endif /* CONFIG_RELAY_SERVER_COUNT */
};

static esp_ble_mesh_comp_t composition = {
    .cid = CID_ESP,
    .element_count = MAX_ELE_CNT,
    .elements = elements,
};

void app_cfg_srv_app_key_bind_hook(esp_ble_mesh_cfg_server_cb_param_t *param)
{
    hw_init(param->model->element->element_addr - esp_ble_mesh_get_primary_element_address());
}

void app_prod_prov_cb(esp_ble_mesh_prov_cb_param_t *param, prod_prov_evt_t evt)
{
    return;
}

static esp_err_t ble_mesh_init(void)
{
    esp_err_t err = ESP_OK;

    err = prod_init_prov(&prod_prov_cfg);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize Prov server (err %d)", err);
        return err;
    }
    err = prod_init_config_server(&cfg_srv);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize config server (err %d)", err);
        return err;
    }
    err = prod_gen_srv_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize prod server (err %d)", err);
        return err;
    }
    err = esp_ble_mesh_init(&PROD_PROV_INSTANCE, &composition);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize mesh stack (err %d)", err);
        return err;
    }

    err = esp_ble_mesh_node_prov_enable((esp_ble_mesh_prov_bearer_t)(ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to enable mesh node (err %d)", err);
        return err;
    }

    ESP_LOGI(TAG, "BLE Mesh Node initialized");

    return err;
}

void app_main(void)
{
    esp_err_t err;

    ESP_LOGI(TAG, "Initializing...");
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    err = bluetooth_init();
    if (err)
    {
        ESP_LOGE(TAG, "esp32_bluetooth_init failed (err %d)", err);
        return;
    }
    ble_mesh_get_dev_uuid(dev_uuid);
    /* Initialize the Bluetooth Mesh Subsystem */
    err = ble_mesh_init();
    if (err)
    {
        ESP_LOGE(TAG, "Bluetooth mesh init failed (err %d)", err);
    }
}
