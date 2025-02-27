/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshX.c
 * @brief meshX application file for ESP BLE Mesh node.
 *
 * This file contains initialization routines for BLE Mesh provisioning, configuration,
 * and light control servers, as well as the main application entry point.
 *
 */

#include "meshx.h"

#define ROOT_MODEL_VEN_CNT 0
#define FRESHBOOT_TIMEOUT_MS 1500

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

static meshx_config_t g_config;
static os_timer_t *g_boot_timer;

extern size_t get_root_sig_models_count(void);
extern esp_ble_mesh_model_t * get_root_sig_models(void);
extern esp_err_t create_ble_mesh_element_composition(dev_struct_t *p_dev, meshx_config_t const *config);
/**
 * @brief Initializes BLE Mesh elements.
 *
 * @param[in] p_dev Pointer to the device structure.
 * @param[in] config Pointer to the meshX configuration.
 *
 * @return ESP_OK on success, error code otherwise.
 */
static esp_err_t ble_mesh_element_init(dev_struct_t *p_dev, meshx_config_t const *config)
{
    if (!p_dev)
        return ESP_ERR_INVALID_STATE;

    esp_err_t err = ESP_OK;
    /* Initialize root model */
    p_dev->elements[0].sig_models = get_root_sig_models();
    p_dev->elements[0].vnd_models = ESP_BLE_MESH_MODEL_NONE;
    memset((void *)&p_dev->elements[0].sig_model_count, get_root_sig_models_count(), sizeof(p_dev->elements[0].sig_model_count));
    memset((void *)&p_dev->elements[0].vnd_model_count, ROOT_MODEL_VEN_CNT, sizeof(p_dev->elements[0].vnd_model_count));

    ESP_LOGI(TAG, "Root: SIG : %d, VEN: %d", p_dev->elements[0].sig_model_count, p_dev->elements[0].vnd_model_count);
    /* Root to be used with fixed format */
    p_dev->element_idx++;

    err = create_ble_mesh_element_composition(p_dev, config);
    if(err)
    {
        ESP_LOGE(TAG, "Failed to create BLE Mesh Element Composition: (%d)", err);
        return err;
    }
    p_dev->composition.cid = config->cid;
    p_dev->composition.pid = config->pid;
    p_dev->composition.element_count = p_dev->element_idx;
    p_dev->composition.elements = p_dev->elements;

    return ESP_OK;
}

/**
 * @brief Initializes application tasks.
 *
 * @param[in] pdev Pointer to the device structure.
 *
 * @return ESP_OK on success, error code otherwise.
 */
static esp_err_t meshx_tasks_init(dev_struct_t * pdev)
{
    esp_err_t err;

    err = create_control_task(pdev);
    ESP_ERR_PRINT_RET("Failed to create control task", err);

    return err;
}

/**
 * @brief Restore the device state from the NVS.
 *
 * @param[in] pdev Pointer to the device structure.
 * @return ESP_OK on success, error code otherwise.
 *
 */
static esp_err_t meshx_dev_restore(dev_struct_t *pdev, meshx_config_t const *config)
{
    esp_err_t err = ESP_OK;

    err = meshx_nvs_open(config->cid, config->pid, config->meshx_nvs_save_period);
    ESP_ERR_PRINT_RET("MeshX NVS Open failed", err);

    err = meshx_nvs_get(MESHX_NVS_STORE, &pdev->meshx_store, sizeof(pdev->meshx_store));
    ESP_ERR_PRINT_RET("Failed to restore meshx device state", err);

    return err;
}

/**
 * @brief Initializes the BLE Mesh subsystem.
 *
 * This function sets up provisioning, configuration servers, and BLE Mesh stack initialization.
 *
 * @return ESP_OK on success, error code otherwise.
 */
static esp_err_t ble_mesh_init(meshx_config_t const *config)
{
    if(config == NULL || config->product_name == NULL || strlen(config->product_name) > ESP_BLE_MESH_DEVICE_NAME_MAX_LEN)
        return ESP_ERR_INVALID_ARG;

    esp_err_t err;

    meshx_dev_restore(&g_dev, config);

    err = ble_mesh_element_init(&g_dev, config);
    ESP_ERR_PRINT_RET("Failed to initialize BLE Elements", err);

    err = esp_ble_mesh_init(&MESHX_PROV_INSTANCE, &g_dev.composition);
    ESP_ERR_PRINT_RET("Failed to initialize mesh stack", err);

    err = esp_ble_mesh_set_unprovisioned_device_name(config->product_name);
    ESP_ERR_PRINT_RET("Name Set Error", err);

    err = esp_ble_mesh_node_prov_enable((esp_ble_mesh_prov_bearer_t)(ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT));
    ESP_ERR_PRINT_RET("Failed to enable mesh node", err);

    ESP_LOGI(TAG, "BLE Mesh Node initialized");

    return ESP_OK;
}

/**
 * @brief Callback function for the boot timer.
 *
 * This function is called when the boot timer expires.
 *
 * @param[in] p_timer Pointer to the timer structure.
 */
static void meshx_init_boot_timer_arm_cb(const os_timer_t* p_timer)
{
    ESP_LOGD(TAG, "Fresh Boot Timer Expired");

    esp_err_t err = control_task_msg_publish(
        CONTROL_TASK_MSG_CODE_SYSTEM,
        CONTROL_TASK_MSG_EVT_SYSTEM_FRESH_BOOT,
        p_timer,
        OS_TIMER_SIZE
    );
    if(err)
    {
        ESP_LOGE(TAG, "Failed to publish fresh boot event: (%d)", err);
    }
}

/**
 * @brief Initializes the boot timer.
 *
 * @return ESP_OK on success, error code otherwise.
 */
static esp_err_t meshx_init_boot_timer(void)
{
    esp_err_t err = os_timer_create("boot_timer",
        FRESHBOOT_TIMEOUT_MS,
        false,
        meshx_init_boot_timer_arm_cb,
        &g_boot_timer
    );
    ESP_ERR_PRINT_RET("Failed to create boot timer", err);

    err = os_timer_start(g_boot_timer);
    ESP_ERR_PRINT_RET("Failed to start boot timer", err);

    return err;
}

/**
 * @brief MeshX initialisation function
 *
 * This function initialises the MeshX stack with the given configuration.
 * @param config Pointer to the configuration structure
 *
 * @return ESP_OK, Success
 */
esp_err_t meshx_init(meshx_config_t const *config)
{
    if(!config)
        return ESP_ERR_INVALID_ARG;

    esp_err_t err = ESP_OK;

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    ESP_ERROR_CHECK(err);

    memcpy(&g_config, config, sizeof(meshx_config_t));

    err = os_timer_init();
    ESP_ERR_PRINT_RET("OS Timer Init failed", err);


    err = meshx_nvs_init();
    ESP_ERR_PRINT_RET("MeshX NVS Init failed", err);

    err = meshx_tasks_init(&g_dev);
    ESP_ERR_PRINT_RET("Tasks initialization failed", err);

    err = meshx_init_boot_timer();
    ESP_ERR_PRINT_RET("Boot Timer Init failed", err);

    err = meshx_app_reg_element_callback(g_config.app_element_cb);
    ESP_ERR_PRINT_RET("Failed to register app element callback", err);

    err = meshx_app_reg_system_events_callback(g_config.app_ctrl_cb);
    ESP_ERR_PRINT_RET("Failed to register app control callback", err);

    err = bluetooth_init();
    ESP_ERR_PRINT_RET("esp32_bluetooth_init failed", err);

    esp_log_level_set("BLE_MESH", ESP_LOG_ERROR);

    /* Initialize the Bluetooth Mesh Subsystem */
    err = ble_mesh_init(&g_config);
    ESP_ERR_PRINT_RET("Bluetooth mesh init failed", err);

    printf(LOG_ANSI_COLOR_REGULAR(LOG_ANSI_COLOR_CYAN) "%s" LOG_ANSI_COLOR_RESET, meshX_banner);

#if CONFIG_ENABLE_UNIT_TEST
    err = register_ut_command();
    ESP_ERR_PRINT_RET("Failed to register unit test command", err);

    err = init_unit_test_console();
    ESP_ERR_PRINT_RET("Failed to initialize production console", err);
#endif /* CONFIG_ENABLE_UNIT_TEST */

    return err;
}

