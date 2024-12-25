/**
 * @file main.c
 * @brief Main application file for ESP BLE Mesh node.
 *
 * This file contains initialization routines for BLE Mesh provisioning, configuration,
 * and light control servers, as well as the main application entry point.
 *
 * @author [Pranjal Chanda]
 */

#include "main.h"

#define ROOT_MODEL_SIG_CNT ARRAY_SIZE(app_root_model)
#define ROOT_MODEL_VEN_CNT 0

/**
 * @brief Hook for application-specific AppKey binding handling.
 *
 * @param[in] param Pointer to BLE Mesh configuration server callback parameters.
 */
void app_cfg_srv_app_key_bind_hook(const esp_ble_mesh_cfg_server_cb_param_t *param);

/**
 * @brief Callback for provisioning events.
 *
 * @param[in] param Pointer to BLE Mesh provisioning callback parameters.
 * @param[in] evt Provisioning event type.
 */
void app_prod_prov_cb(const esp_ble_mesh_prov_cb_param_t *param, prod_prov_evt_t evt);

/** Device UUID for provisioning. */
static uint8_t dev_uuid[16] = {0xdd, 0xdd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

dev_struct_t g_dev;

#if CONFIG_ENABLE_PROVISIONING
/** Provisioning parameters for BLE Mesh. */
static prov_params_t prod_prov_cfg = {
    .uuid = dev_uuid,
    .cb_reg = app_prod_prov_cb};
#endif

#if CONFIG_ENABLE_LIGHT_CTL_SERVER
/** Light CTL state. */
esp_ble_mesh_light_ctl_state_t ctl_state;
ESP_BLE_MESH_MODEL_PUB_DEFINE(ctl_setup_pub, 2 + 6, ROLE_NODE);

/** Light CTL setup server model. */
static esp_ble_mesh_light_ctl_setup_srv_t ctl_setup_server = {
    .rsp_ctrl = {
        .get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
        .set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
    },
    .state = &ctl_state,
};
#endif

/** Root models for BLE Mesh elements. */
static esp_ble_mesh_model_t app_root_model[] = {
#if CONFIG_ENABLE_CONFIG_SERVER
    ESP_BLE_MESH_MODEL_CFG_SRV(&PROD_CONFIG_SERVER_INSTANCE),
#endif /* CONFIG_ENABLE_CONFIG_SERVER */
#if CONFIG_ENABLE_LIGHT_CTL_SERVER
    ESP_BLE_MESH_MODEL_LIGHT_CTL_SETUP_SRV(&ctl_setup_pub, &ctl_setup_server),
#endif
};

#if CONFIG_ENABLE_CONFIG_SERVER
void app_cfg_srv_app_key_bind_hook(const esp_ble_mesh_cfg_server_cb_param_t *param)
{
    ESP_UNUSED(param);
    return;
}
#endif

#if CONFIG_ENABLE_PROVISIONING
void app_prod_prov_cb(const esp_ble_mesh_prov_cb_param_t *param, prod_prov_evt_t evt)
{
    ESP_UNUSED(param);
    ESP_UNUSED(evt);
    return;
}
#endif

/**
 * @brief Initializes BLE Mesh composition data.
 *
 * @param[in] p_dev Pointer to the device structure.
 * @return ESP_OK on success, error code otherwise.
 */
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

/**
 * @brief Initializes BLE Mesh elements.
 *
 * @param[in] p_dev Pointer to the device structure.
 * @return ESP_OK on success, error code otherwise.
 */
static esp_err_t ble_mesh_element_init(dev_struct_t *p_dev)
{
    if (!p_dev)
        return ESP_ERR_INVALID_STATE;

    /* Initialize root model */
    p_dev->elements[0].sig_models = app_root_model;
    p_dev->elements[0].vnd_models = ESP_BLE_MESH_MODEL_NONE;
    memset((void *)&p_dev->elements[0].sig_model_count, ROOT_MODEL_SIG_CNT, sizeof(p_dev->elements[0].sig_model_count));
    memset((void *)&p_dev->elements[0].vnd_model_count, ROOT_MODEL_VEN_CNT, sizeof(p_dev->elements[0].vnd_model_count));

    ESP_LOGI(TAG, "Root: SIG : %d, VEN: %d", p_dev->elements[0].sig_model_count, p_dev->elements[0].vnd_model_count);
    /* Root to be used with fixed format */
    p_dev->element_idx++;
    return create_ble_mesh_element_composition(p_dev);
}

/**
 * @brief Initializes application tasks.
 *
 * @return ESP_OK on success, error code otherwise.
 */
static esp_err_t app_tasks_init(dev_struct_t * pdev)
{
    esp_err_t err;

    err = create_control_task(pdev);
    ESP_ERR_PRINT_RET("Failed to create control task", err);

    return err;
}

/**
 * @brief Initializes the BLE Mesh subsystem.
 *
 * This function sets up provisioning, configuration servers, and BLE Mesh stack initialization.
 *
 * @return ESP_OK on success, error code otherwise.
 */
static esp_err_t ble_mesh_init(void)
{
    esp_err_t err;

    err = ble_mesh_element_init(&g_dev);
    ESP_ERR_PRINT_RET("Failed to initialize BLE Elements", err);

    err = ble_mesh_composition_init(&g_dev);
    ESP_ERR_PRINT_RET("Failed to initialize BLE Composition", err);

    err = prod_init_prov(&prod_prov_cfg);
    ESP_ERR_PRINT_RET("Failed to initialize Prov server", err);

    err = prod_init_config_server();
    ESP_ERR_PRINT_RET("Failed to initialize config server", err);

    err = esp_ble_mesh_init(&PROD_PROV_INSTANCE, &g_dev.composition);
    ESP_ERR_PRINT_RET("Failed to initialize mesh stack", err);

    err = esp_ble_mesh_set_unprovisioned_device_name(CONFIG_PRODUCT_NAME);
    ESP_ERR_PRINT_RET("Name Set Error", err);

    err = esp_ble_mesh_node_prov_enable((esp_ble_mesh_prov_bearer_t)(ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT));
    ESP_ERR_PRINT_RET("Failed to enable mesh node", err);

    ESP_LOGI(TAG, "BLE Mesh Node initialized");

    return ESP_OK;
}

/**
 * @brief Main application entry point.
 *
 * Initializes NVS, Bluetooth, and BLE Mesh subsystems.
 */
void app_main(void)
{
    esp_err_t err;

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    err = app_tasks_init(&g_dev);
    if (err)
    {
        ESP_LOGE(TAG, "Tasks initialization failed (err 0x%x)", err);
        return;
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
        return;
    }

#if CONFIG_ENABLE_UNIT_TEST
    err = register_ut_command();
    if (err)
    {
        ESP_LOGE(TAG, "Failed to register unit test command (err 0x%x)", err);
        return;
    }

    err = init_prod_console();
    if (err)
    {
        ESP_LOGE(TAG, "Failed to initialize production console (err 0x%x)", err);
        return;
    }
#endif /* CONFIG_ENABLE_UNIT_TEST */

}
