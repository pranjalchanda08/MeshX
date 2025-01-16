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

static const char meshX_banner[] = {
"*********************************************************************************************************************\n"
"* MMMMMMMM               MMMMMMMM                                     hhhhhhh                 XXXXXXX       XXXXXXX *\n"
"* M:::::::M             M:::::::M                                     h:::::h                 X:::::X       X:::::X *\n"
"* M::::::::M           M::::::::M                                     h:::::h                 X:::::X       X:::::X *\n"
"* M:::::::::M         M:::::::::M                                     h:::::h                 X::::::X      X:::::X *\n"
"* M::::::::::M       M::::::::::M    eeeeeeeeeeee        ssssssssss   h:::: hhhhhh            XX:::::X     X:::::XX *\n"
"* M:::::::::::M     M:::::::::::M  ee::::::::::::ee    ss::::::::::s  h::::::::::hhh            X:::::X   X:::::X   *\n"
"* M:::::::M::::M   M::::M:::::::M e::::::eeeee:::::eess:::::::::::::s h::::::::::::::hh           X:::::X:::::X     *\n"
"* M::::::M M::::M M::::M M::::::Me::::::e     e:::::es::::::ssss:::::sh:::::::hhh::::::h           X:::::::::X      *\n"
"* M::::::M  M::::M::::M  M::::::Me:::::::eeeee::::::e s:::::s  ssssss h::::::h   h::::::h          X:::::::::X      *\n"
"* M::::::M   M:::::::M   M::::::Me:::::::::::::::::e    s::::::s      h:::::h     h:::::h         X:::::X:::::X     *\n"
"* M::::::M    M:::::M    M::::::Me::::::eeeeeeeeeee        s::::::s   h:::::h     h:::::h        X:::::X X:::::X    *\n"
"* M::::::M     MMMMM     M::::::Me:::::::e           ssssss   s:::::s h:::::h     h:::::h     XXX:::::X   X:::::XXX *\n"
"* M::::::M               M::::::Me::::::::e          s:::::ssss::::::sh:::::h     h:::::h     X::::::X     X::::::X *\n"
"* M::::::M               M::::::M e::::::::eeeeeeee  s::::::::::::::s h:::::h     h:::::h     X:::::X       X:::::X *\n"
"* M::::::M               M::::::M  ee:::::::::::::e   s:::::::::::ss  h:::::h     h:::::h     X:::::X       X:::::X *\n"
"* MMMMMMMM               MMMMMMMM    eeeeeeeeeeeeee    sssssssssss    hhhhhhh     hhhhhhh     XXXXXXX       XXXXXXX *\n" 
"*********************************************************************************************************************\n"
};

static dev_struct_t g_dev;

extern esp_err_t create_ble_mesh_element_composition(dev_struct_t *p_dev);
extern esp_err_t ble_mesh_composition_init(dev_struct_t *p_dev);
extern esp_ble_mesh_model_t * get_root_models(void);
extern size_t get_root_models_size(void);
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
    p_dev->elements[0].sig_models = get_root_models();
    p_dev->elements[0].vnd_models = ESP_BLE_MESH_MODEL_NONE;
    memset((void *)&p_dev->elements[0].sig_model_count, get_root_models_size(), sizeof(p_dev->elements[0].sig_model_count));
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

    err = os_timer_init();
    if (err)
    {
        ESP_LOGE(TAG, "OS Timer Init failed (err 0x%x)", err);
        return;
    }

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

    /* Initialize the Bluetooth Mesh Subsystem */
    err = ble_mesh_init();
    if (err)
    {
        ESP_LOGE(TAG, "Bluetooth mesh init failed (err 0x%x)", err);
        return;
    }

    printf(LOG_ANSI_COLOR_REGULAR(LOG_ANSI_COLOR_CYAN) "%s" LOG_ANSI_COLOR_RESET, meshX_banner);

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
