/**
 * @copyright Copyright © 2024 - 2025 MeshX
 *
 * @file meshx.c
 * @brief meshX application file for ESP BLE Mesh node.
 *
 * This file contains initialization routines for BLE Mesh provisioning, configuration,
 * and light control servers, as well as the main application entry point.
 *
 * @author Pranjal Chanda
 *
 */
#include "meshx.h"

/**
 * @def ROOT_MODEL_VEN_CNT
 * @brief Defines the vendor count for the root model.
 */
#define ROOT_MODEL_VEN_CNT 0

/**
 * @def FRESHBOOT_TIMEOUT_MS
 * @brief Defines the timeout duration in milliseconds for a fresh boot.
 */
#define FRESHBOOT_TIMEOUT_MS 1500


/**
 * @var meshX_banner
 * A static constant character array that contains the banner information for
 * the MeshX component. This banner is used for display purposes to indicate
 * the presence of the MeshX component in the application.
 */
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
static meshx_os_timer_t *g_boot_timer;

extern size_t get_root_sig_models_count(void);
extern MESHX_MODEL * get_root_sig_models(void);
extern meshx_err_t create_ble_mesh_element_composition(dev_struct_t *p_dev, meshx_config_t const *config);

/**
 * @brief Initializes BLE Mesh elements.
 *
 * @param[in] p_dev     Pointer to the device structure.
 * @param[in] config    Pointer to the meshX configuration.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
static meshx_err_t ble_mesh_element_init(dev_struct_t *p_dev, meshx_config_t const *config)
{
    if (!p_dev)
        return MESHX_INVALID_STATE;

    meshx_err_t err = MESHX_SUCCESS;
    err = meshx_create_plat_composition(&p_dev->composition);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to create platform composition: (%d)", err);
        return err;
    }

    /* Initialize root model */
    err = meshx_plat_add_element_to_composition(
        0,
        p_dev->elements,
        get_root_sig_models(),
        NULL,
        (uint8_t) get_root_sig_models_count(),
        (uint8_t) ROOT_MODEL_VEN_CNT
    );
    if(err)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to add element to composition: (%d)", err);
        return err;
    }
    p_dev->element_idx++;
    /* Root to be used with fixed format */

    err = create_ble_mesh_element_composition(p_dev, config);
    if(err)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to create BLE Mesh Element Composition: (%d)", err);
        return err;
    }
    err = meshx_plat_composition_init(
        p_dev->composition,
        p_dev->elements,
        config->cid,
        config->pid,
        (uint16_t)p_dev->element_idx
    );
    if(err)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to initialise MeshX Composition: (%d)", err);
        return err;
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Initializes application tasks.
 *
 * @param[in] pdev Pointer to the device structure.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
static meshx_err_t meshx_tasks_init(dev_struct_t * pdev)
{
    meshx_err_t err;

    err = create_control_task(pdev);
    MESHX_ERR_PRINT_RET("Failed to create control task", err);

    return err;
}

/**
 * @brief Restore the device state from the NVS.
 *
 * @param[in] pdev Pointer to the device structure.
 * @param[in] config Pointer to the meshX configuration.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 *
 */
static meshx_err_t meshx_dev_restore(dev_struct_t *pdev, meshx_config_t const *config)
{
    meshx_err_t err = MESHX_SUCCESS;

    err = meshx_nvs_open(config->cid, config->pid, config->meshx_nvs_save_period);
    MESHX_ERR_PRINT_RET("MeshX NVS Open failed", err);

    err = meshx_nvs_get(MESHX_NVS_STORE, &pdev->meshx_store, sizeof(pdev->meshx_store));
    MESHX_ERR_PRINT_RET("Failed to restore meshx device state", err);

    return err;
}

/**
 * @brief Initializes the BLE Mesh subsystem.
 *
 * This function sets up provisioning, configuration servers, and BLE Mesh stack initialization.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
static meshx_err_t ble_mesh_init(meshx_config_t const *config)
{
    if(config == NULL || config->product_name == NULL || strlen(config->product_name) > ESP_BLE_MESH_DEVICE_NAME_MAX_LEN)
        return MESHX_INVALID_ARG;

    meshx_err_t err;

    err = meshx_platform_bt_init();
    MESHX_ERR_PRINT_RET("Platform BT init failed", err);

    meshx_dev_restore(&g_dev, config);

    err = ble_mesh_element_init(&g_dev, config);
    MESHX_ERR_PRINT_RET("Failed to initialize BLE Elements", err);

    err = esp_ble_mesh_init(&MESHX_PROV_INSTANCE, g_dev.composition);
    MESHX_ERR_PRINT_RET("Failed to initialize mesh stack", err);

    err = esp_ble_mesh_set_unprovisioned_device_name(config->product_name);
    MESHX_ERR_PRINT_RET("Name Set Error", err);

    err = esp_ble_mesh_node_prov_enable((esp_ble_mesh_prov_bearer_t)(ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT));
    MESHX_ERR_PRINT_RET("Failed to enable mesh node", err);

    MESHX_LOGI(MODULE_ID_COMMON, "BLE Mesh Node initialized");

    return MESHX_SUCCESS;
}

/**
 * @brief Callback function for the boot timer.
 *
 * This function is called when the boot timer expires.
 *
 * @param[in] p_timer Pointer to the timer structure.
 */
static void meshx_init_boot_timer_arm_cb(const meshx_os_timer_t* p_timer)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Fresh Boot Timer Expired");

    meshx_err_t err = control_task_msg_publish(
        CONTROL_TASK_MSG_CODE_SYSTEM,
        CONTROL_TASK_MSG_EVT_SYSTEM_FRESH_BOOT,
        p_timer,
        OS_TIMER_SIZE
    );
    if(err)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to publish fresh boot event: (%d)", err);
    }
}

/**
 * @brief Initializes the boot timer.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
static meshx_err_t meshx_init_boot_timer(void)
{
    meshx_err_t err = meshx_os_timer_create("boot_timer",
        FRESHBOOT_TIMEOUT_MS,
        false,
        meshx_init_boot_timer_arm_cb,
        &g_boot_timer
    );
    MESHX_ERR_PRINT_RET("Failed to create boot timer", err);

    err = meshx_os_timer_start(g_boot_timer);
    MESHX_ERR_PRINT_RET("Failed to start boot timer", err);

    return err;
}

/**
 * @brief MeshX initialisation function
 *
 * This function initialises the MeshX stack with the given configuration.
 *
 * @param[in] config Pointer to the configuration structure
 *
 * @return MESHX_SUCCESS, Success
 */
meshx_err_t meshx_init(meshx_config_t const *config)
{
    /* Check if the configuration is valid */
    if(!config)
        return MESHX_INVALID_ARG;

    meshx_err_t err = MESHX_SUCCESS;

    /* Copy the configuration to the global config structure */
    memcpy(&g_config, config, sizeof(meshx_config_t));

    meshx_logging_t logging_cfg;

    logging_cfg.def_log_level = config->meshx_log_level == MESHX_LOG_VERBOSE ?
        CONFIG_MESHX_DEFAULT_LOG_LEVEL : config->meshx_log_level;

    err = meshx_logging_init(&logging_cfg);
    MESHX_ERR_PRINT_RET("Logging init failed", err);

    /* Initialise Platform deps */
    err = meshx_platform_init();
    MESHX_ERR_PRINT_RET("Platform init failed", err);

    /* Initialize OS timer */
    err = meshx_os_timer_init();
    MESHX_ERR_PRINT_RET("OS Timer Init failed", err);

    /* Initialize MeshX NVS */
    err = meshx_nvs_init();
    MESHX_ERR_PRINT_RET("MeshX NVS Init failed", err);

    /* Initialize application tasks */
    err = meshx_tasks_init(&g_dev);
    MESHX_ERR_PRINT_RET("Tasks initialization failed", err);

    /* Initialize boot timer */
    err = meshx_init_boot_timer();
    MESHX_ERR_PRINT_RET("Boot Timer Init failed", err);

    /* Register application element callback */
    err = meshx_app_reg_element_callback(g_config.app_element_cb);
    MESHX_ERR_PRINT_RET("Failed to register app element callback", err);

    /* Register application control callback */
    err = meshx_app_reg_system_events_callback(g_config.app_ctrl_cb);
    MESHX_ERR_PRINT_RET("Failed to register app control callback", err);

    /* Initialize the Bluetooth Mesh Subsystem */
    err = ble_mesh_init(&g_config);
    MESHX_ERR_PRINT_RET("Bluetooth mesh init failed", err);

    /* Print the MeshX banner */
    printf(LOG_ANSI_COLOR_REGULAR(LOG_ANSI_COLOR_CYAN) "%s" LOG_ANSI_COLOR_RESET, meshX_banner);

#if CONFIG_ENABLE_UNIT_TEST
    /* Register unit test command */
    err = register_ut_command();
    MESHX_ERR_PRINT_RET("Failed to register unit test command", err);

    /* Initialize unit test console */
    err = init_unit_test_console();
    MESHX_ERR_PRINT_RET("Failed to initialize production console", err);
#endif /* CONFIG_ENABLE_UNIT_TEST */

    return err;
}

