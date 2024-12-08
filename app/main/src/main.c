#include <app_common.h>
#include <prod_prov.h>
#include <config_server.h>
#if CONFIG_ENABLE_SERVER_COMMON
#include <prod_onoff_server.h>
#endif /* CONFIG_ENABLE_SERVER_COMMON */

#if CONFIG_ENABLE_CLIENT_COMMON
#include <prod_client.h>
#endif /* CONFIG_ENABLE_CLIENT_COMMON */

#define ESP_ERR_PRINT_RET(_e_str, _err)            \
    if (_err != ESP_OK)                            \
    {                                              \
        ESP_LOGE(TAG, _e_str " (err 0x%x)", _err); \
        return _err;                               \
    }

#define TAG "APP"
#define CID_ESP CONFIG_CID_ID

dev_struct_t g_dev;

#define ROOT_MODEL_SIG_CNT 1
#define ROOT_MODEL_VEN_CNT 0

void app_cfg_srv_app_key_bind_hook(esp_ble_mesh_cfg_server_cb_param_t *param);
void app_prod_prov_cb(esp_ble_mesh_prov_cb_param_t *param, prod_prov_evt_t evt);

static uint8_t dev_uuid[16] = {0xdd, 0xdd};

static config_server_params_t cfg_srv = {
    .on_app_key_cb = app_cfg_srv_app_key_bind_hook};

static prov_params_t prod_prov_cfg = {
    .uuid = dev_uuid,
    .cb_reg = app_prod_prov_cb};

static esp_ble_mesh_model_t root_model[2] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&PROD_CONFIG_SERVER_INSTANCE),
};

void app_cfg_srv_app_key_bind_hook(esp_ble_mesh_cfg_server_cb_param_t *param)
{
    return;
}

void app_prod_prov_cb(esp_ble_mesh_prov_cb_param_t *param, prod_prov_evt_t evt)
{
    return;
}

static esp_err_t ble_mesh_composition_init(dev_struct_t *p_dev)
{
    if (!p_dev)
        return ESP_ERR_INVALID_STATE;

    p_dev->composition.cid = CID_ESP;
    p_dev->composition.pid = CONFIG_PID_ID;
    p_dev->composition.element_count = p_dev->element_idx;
    p_dev->composition.elements = p_dev->elements;

    return ESP_OK;
}

extern esp_err_t create_ble_mesh_element_composition(dev_struct_t *p_dev);

static esp_err_t ble_mesh_element_init(dev_struct_t *p_dev)
{
    if (!p_dev)
        return ESP_ERR_INVALID_STATE;

    /* Initialise Root model */
    p_dev->elements[0].sig_models = root_model;
    p_dev->elements[0].vnd_models = ESP_BLE_MESH_MODEL_NONE;
    memset((void *)&p_dev->elements[0].sig_model_count, ROOT_MODEL_SIG_CNT, sizeof(p_dev->elements[0].sig_model_count));
    memset((void *)&p_dev->elements[0].vnd_model_count, ROOT_MODEL_VEN_CNT, sizeof(p_dev->elements[0].vnd_model_count));

    return create_ble_mesh_element_composition(p_dev);
}

static esp_err_t app_tasks_init()
{
    esp_err_t err;

    err = create_control_task();

    return err;
}

static esp_err_t ble_mesh_init(void)
{
    esp_err_t err;

    err = ble_mesh_element_init(&g_dev);
    ESP_ERR_PRINT_RET("Failed to initialize BLE Elements", err);

    err = ble_mesh_composition_init(&g_dev);
    ESP_ERR_PRINT_RET("Failed to initialize BLE Composition", err);

    err = prod_init_prov(&prod_prov_cfg);
    ESP_ERR_PRINT_RET("Failed to initialize Prov server", err);

    err = prod_init_config_server(&cfg_srv);
    ESP_ERR_PRINT_RET("Failed to initialize config server", err);

    err = esp_ble_mesh_init(&PROD_PROV_INSTANCE, &g_dev.composition);

    ESP_ERR_PRINT_RET("Failed to initialize mesh stack", err);

    err = esp_ble_mesh_node_prov_enable((esp_ble_mesh_prov_bearer_t)(ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT));

    ESP_ERR_PRINT_RET("Failed to enable mesh node", err);

    err = esp_ble_mesh_set_unprovisioned_device_name(CONFIG_PRODUCT_NAME);
    ESP_ERR_PRINT_RET("Name Set Error", err);

    ESP_LOGI(TAG, "BLE Mesh Node initialized");

    return ESP_OK;
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
    err = app_tasks_init();
    if (err)
    {
        ESP_LOGE(TAG, "Tasks initialisation failed (err 0x%x)", err);
    }
    err = bluetooth_init();
    if (err)
    {
        ESP_LOGE(TAG, "esp32_bluetooth_init failed (err 0x%x)", err);
        return;
    }
    ble_mesh_get_dev_uuid(dev_uuid);
    /* Initialize the Bluetooth Mesh Subsystem */
    err = ble_mesh_init();
    if (err)
    {
        ESP_LOGE(TAG, "Bluetooth mesh init failed (err 0x%x)", err);
    }
}
