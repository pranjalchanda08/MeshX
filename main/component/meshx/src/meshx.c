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
 * @def ROOT_ELEMENT_IDX
 * @brief Defines the index for the root element.
 */
#define ROOT_ELEMENT_IDX 0

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

meshx_prov_params_t g_prov_cfg = {
    .uuid = MESHX_UUID_EMPTY,  /**< UUID for the provisioning device */
    .node_name = NULL           /**< Node name for the provisioning device */
};

extern size_t get_root_sig_models_count(void);
extern size_t get_root_ven_models_count(void);
extern meshx_ptr_t get_root_sig_models(void);
extern meshx_ptr_t get_root_ven_models(void);
extern meshx_err_t meshx_create_element_composition(dev_struct_t *p_dev, meshx_config_t const *config);

#include <meshx_builder_api.h>

static MESHX_ELEMENT g_legacy_elements[MAX_ELE_CNT];

/**
 * @brief Initializes BLE Mesh elements.
 *
 * @param[in] p_dev     Pointer to the device structure.
 * @param[in] config    Pointer to the meshX configuration.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
static meshx_err_t meshx_element_init(dev_struct_t *p_dev, meshx_config_t const *config)
{
    if (!p_dev)
        return MESHX_INVALID_STATE;

    meshx_err_t err = MESHX_SUCCESS;

    /**
     * @brief Detect if Dynamic Composition Builder was used
     * @note If the builder is active, we bake the dynamic composition and skip legacy init
     */
    if (meshx_builder_is_active()) {
        MESHX_LOGI(MODULE_ID_COMMON, "Dynamic Composition detected. Baking...");
        err = meshx_builder_bake(p_dev, config->cid, config->pid, config->vid);
        if (err != MESHX_SUCCESS) return err;

        // Dynamic comp already initialized plat composition and models
        return MESHX_SUCCESS;
    }

    /* Legacy Fallback Path */
    p_dev->elements = g_legacy_elements;
    p_dev->element_cnt = MAX_ELE_CNT;
    memset(g_legacy_elements, 0, sizeof(g_legacy_elements));

    meshx_ptr_t meshx_sig_root_model_arr = NULL;
    meshx_ptr_t meshx_ven_root_model_arr = NULL;

    err = meshx_create_plat_composition(&p_dev->composition);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to create platform composition: (%d)", err);
        return err;
    }

    /* Start with element index 1 as 0 is root element */
    p_dev->element_idx = 1;

    err = meshx_create_element_composition(p_dev, config);
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

    meshx_sig_root_model_arr = get_root_sig_models();
    if(meshx_sig_root_model_arr == NULL)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to get root SIG models");
        return MESHX_FAIL;
    }

    /**
     * @brief Initialise the root element
     * @note We initialise the root element later as root element's models needs
     *       to be choosen based on the composition of the other elements
     */
    err = meshx_plat_add_element_to_composition(
        ROOT_ELEMENT_IDX,
        p_dev->elements,
        meshx_sig_root_model_arr,
        meshx_ven_root_model_arr,
        (uint8_t) get_root_sig_models_count(),
        (uint8_t) get_root_ven_models_count()
    );
    if(err)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to add element to composition: (%d)", err);
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

#if CONFIG_TXCM_ENABLE
    err = meshx_txcm_init(pdev);
    MESHX_ERR_PRINT_RET("Failed to create Tx Control Module", err);
#endif /* CONFIG_TXCM_ENABLE */
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

    err = meshx_nvs_get(MESHX_NVS_STORE, &pdev->meshx_store, sizeof(meshx_app_store_t));
    if (err != MESHX_SUCCESS) {
        MESHX_LOGW(MODULE_ID_COMMON, "No meshx device state found in NVS, starting fresh (err 0x%x)", err);
        memset(&pdev->meshx_store, 0, sizeof(meshx_app_store_t));
        return MESHX_SUCCESS; // Non-fatal
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Initializes the BLE Mesh subsystem.
 *
 * This function sets up provisioning, configuration servers, and BLE Mesh stack initialization.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
static meshx_err_t meshx_ble_mesh_init(meshx_config_t *config)
{
    if(config == NULL || config->product_name == NULL)
        return MESHX_INVALID_ARG;

    meshx_err_t err;
    g_prov_cfg.node_name = (uint8_t *)config->product_name;

    /* Copy the UUID to the global device structure for visualization and management */
    memcpy(g_dev.uuid, config->meshx_uuid_addr, sizeof(g_dev.uuid));

    err = meshx_platform_bt_init(g_dev.uuid);
    MESHX_ERR_PRINT_RET("Platform BT init failed", err);

    g_prov_cfg.uuid = g_dev.uuid;
    g_prov_cfg.freshboot_timeout_ms = FRESHBOOT_TIMEOUT_MS;

    err = meshx_element_init(&g_dev, config);
    MESHX_ERR_PRINT_RET("Failed to initialize BLE Elements", err);

    err = meshx_init_prov(&g_dev, &g_prov_cfg);
    MESHX_ERR_PRINT_RET("Failed to initialize provisioning", err);

    err = meshx_plat_ble_mesh_init(&g_prov_cfg, g_dev.composition);
    MESHX_ERR_PRINT_RET("Failed to initialize BLE Mesh stack", err);

    /* Publish system ready event to notify lower layers */
    control_task_msg_publish(CONTROL_TASK_MSG_CODE_PROVISION, CONTROL_TASK_MSG_EVT_SYSTEM_STACK_READY, NULL, 0);

    return MESHX_SUCCESS;
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

    /* Restore the device state and open NVS (Required before element constructors run) */
    err = meshx_dev_restore(&g_dev, config);
    MESHX_ERR_PRINT_RET("Device restore failed", err);

    /**
     * @brief OOB Dynamic Composition Automation
     * If the configuration provides an element composition array,
     * we automatically trigger the dynamic builder.
     */
    if (config->element_comp_arr && config->element_comp_arr_len > 0)
    {
        meshx_builder_begin();
        for (uint16_t i = 0; i < config->element_comp_arr_len; i++)
        {
            if (config->element_comp_arr[i].element_cnt > 0)
            {
                meshx_builder_add_element(config->element_comp_arr[i].type,
                                          config->element_comp_arr[i].element_cnt);
            }
        }
        meshx_builder_commit();
    }

    /* Initialize application tasks */
    err = meshx_tasks_init(&g_dev);
    MESHX_ERR_PRINT_RET("Tasks initialization failed", err);

    /* Register application element callback */
    err = meshx_app_reg_element_callback(g_config.app_element_cb);
    MESHX_ERR_PRINT_RET("Failed to register app element callback", err);

    /* Register application control callback */
    err = meshx_app_reg_system_events_callback(g_config.app_ctrl_cb);
    MESHX_ERR_PRINT_RET("Failed to register app control callback", err);

    /* Initialize the Bluetooth Mesh Subsystem */
    err = meshx_ble_mesh_init(&g_config);
    MESHX_ERR_PRINT_RET("Bluetooth mesh init failed", err);

    /* Initialize Hosted Serial Protocol */
    err = meshx_serial_init();
    MESHX_ERR_PRINT_RET("Serial init failed", err);

    /* Print the MeshX banner */
    CONFIG_MESHX_LOG_PRINTF(LOG_ANSI_COLOR_REGULAR(LOG_ANSI_COLOR_CYAN) "%s" LOG_ANSI_COLOR_RESET, meshX_banner);

#if CONFIG_ENABLE_UNIT_TEST
    /* Initialize unit test console (this also registers the 'ut' command) */
    err = init_unit_test_console();
    MESHX_ERR_PRINT_RET("Failed to initialize unit test console", err);
#endif /* CONFIG_ENABLE_UNIT_TEST */

    return err;
}

