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
#include "interface/meshx_platform.h"
#include <meshx.h>
#include <meshx_serial.h>
#include <meshx_ro_cfg.h>
#include <meshx_uvp_dispatcher.hpp>

#include <stdio.h>
#include <string.h>
#include "interface/utils/meshx_tiny_printf.h"
#include "interface/gpio/meshx_gpio.h"

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

static dev_struct_t g_dev;
static meshx_config_t g_config;
static char g_product_name[32];
static uint16_t g_cid;
static uint16_t g_pid;
static uint16_t g_vid;
static meshx_uuid_addr_t g_uuid;

meshx_prov_params_t g_prov_cfg = {
    .uuid = g_uuid,
    .node_name = (uint8_t *)g_product_name
};

extern size_t get_root_sig_models_count(void);
extern size_t get_root_ven_models_count(void);
extern meshx_ptr_t get_root_sig_models(void);
extern meshx_ptr_t get_root_ven_models(void);
extern meshx_err_t meshx_create_element_composition(dev_struct_t *p_dev, meshx_config_t const *config);

#include <meshx_builder_api.h>

#if 0
static MESHX_ELEMENT g_legacy_elements[MAX_ELE_CNT];
#endif /* 0 */

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

    if (meshx_builder_is_active()) {
        MESHX_LOGD(MODULE_ID_COMMON, "Dynamic Composition detected. Baking...");
        err = meshx_builder_bake(p_dev, g_cid, g_pid, g_vid);
        if (err != MESHX_SUCCESS) return err;

        err = meshx_restore_all_element_ctx();
        if (err != MESHX_SUCCESS) {
            MESHX_LOGW(MODULE_ID_COMMON, "Element NVS ctx restore failed: 0x%x", err);
        }

        return MESHX_SUCCESS;
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

    /**
     * @brief Enable TXCM if dynamic composition has client elements or if legacy flags are set
     */
#if CONFIG_TXCM_ENABLE
    bool txcm_needed = meshx_builder_is_txcm_active();
    txcm_needed = true;
    if (txcm_needed) {
        err = meshx_txcm_init(pdev);
        MESHX_ERR_PRINT_RET("Failed to create Tx Control Module", err);
    }
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

    err = meshx_nvs_open(g_cid, g_pid, config->meshx_nvs_save_period);
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
    if(config == NULL)
        return MESHX_INVALID_ARG;

    meshx_err_t err;
    g_prov_cfg.node_name = (uint8_t *)g_product_name;

    /* Copy the UUID to the global device structure for visualization and management */
    memcpy(g_dev.uuid, g_uuid, sizeof(g_dev.uuid));

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
 * @brief Load persistent read-only configuration from flash and apply it.
 *
 * This function attempts to load product identification (CID, PID, Name) and
 * device UUID from the `meshx_cfg` partition. If the partition is missing or
 * the data is invalid, it falls back to the defaults provided in the config struct.
 *
 * @param config Pointer to the initial configuration provided by the application.
 * @return meshx_err_t MESHX_SUCCESS on success, or an error code on fatal failure.
 */
static meshx_err_t meshx_load_persistent_config(meshx_config_t const *config)
{
    meshx_err_t err;
    uint16_t loaded_cid = 0xFFFF;
    uint16_t loaded_pid = 0xFFFF;
    uint8_t loaded_uuid[16] = {0};

    /* Load Persistent Read-Only Configuration */
    err = meshx_ro_cfg_init(&loaded_cid, &loaded_pid, g_product_name, sizeof(g_product_name), loaded_uuid);
    if (err != MESHX_SUCCESS) {
        if (err == MESHX_NOT_FOUND || err == MESHX_ERR_RO_CFG_FORMAT) {
            MESHX_LOGW(MODULE_ID_COMMON, "No valid read-only config found (err 0x%x). Using legacy defaults.", err);

            // Fallback product name
            if (g_product_name[0] == '\0') {
                strncpy(g_product_name, CONFIG_PRODUCT_NAME, sizeof(g_product_name) - 1);
                g_product_name[sizeof(g_product_name)-1] = '\0';
            }

            // Fallback CID/PID
            loaded_cid = CONFIG_CID_ID;
            loaded_pid = CONFIG_PID_ID;
        } else {
            return err;
        }
    }

    g_cid = loaded_cid;
    g_pid = loaded_pid;
    g_vid = loaded_cid;

    // Check if UUID was loaded (if it's not all zeros)
    bool uuid_loaded = false;
    for (int i = 0; i < 16; i++) {
        if (loaded_uuid[i] != 0) {
            uuid_loaded = true;
            break;
        }
    }

    if (uuid_loaded) {
        memcpy(g_uuid, loaded_uuid, 16);
        MESHX_LOGD(MODULE_ID_COMMON, "Using Persistent UUID from config");
    } else {
        /* Fallback to empty UUID (will be generated by stack if needed) */
        memset(g_uuid, 0, 16);
        MESHX_LOGI(MODULE_ID_COMMON, "Using Empty UUID (Stack will handle generation)");
    }

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

    /* Initialise Platform deps */
    err = meshx_platform_init();
    MESHX_ERR_PRINT_RET("Platform init failed", err);

    meshx_logging_t logging_cfg;
    logging_cfg.def_log_level = config->meshx_log_level == MESHX_LOG_VERBOSE ?
        CONFIG_MESHX_DEFAULT_LOG_LEVEL : config->meshx_log_level;

    err = meshx_logging_init(&logging_cfg);
    MESHX_ERR_PRINT_RET("Logging init failed", err);

    MESHX_LOGD(MODULE_ID_COMMON, "MeshX Logging Initialized successfully");

    /* Load Persistent Configuration and apply UUID/Product Info */
    err = meshx_load_persistent_config(config);
    MESHX_ERR_PRINT_RET("Failed to load persistent configuration", err);

    /* Initialise Control Task messaging early (required for NVS and OS Timers) */
    err = control_task_init();
    MESHX_ERR_PRINT_RET("Control Task Init failed", err);

    /* Initialize OS timer */
    err = meshx_os_timer_init();
    MESHX_ERR_PRINT_RET("OS Timer Init failed", err);

    /* Initialize Unified Vendor Protocol Dispatcher */
    err = meshx_uvp_dispatcher_init();
    MESHX_ERR_PRINT_RET("UVP Dispatcher Init failed", err);

    /* Initialize GPIO subsystem */
    err = meshx_gpio_init();
    MESHX_ERR_PRINT_RET("GPIO Init failed", err);

    /* Initialize MeshX NVS */
    err = meshx_nvs_init();
    MESHX_ERR_PRINT_RET("MeshX NVS Init failed", err);

    /* Restore the device state and open NVS (Required before element constructors run) */
    err = meshx_dev_restore(&g_dev, &g_config);
    MESHX_ERR_PRINT_RET("Device restore failed", err);

    /* Initialize application tasks */
    err = meshx_tasks_init(&g_dev);
    MESHX_ERR_PRINT_RET("Tasks initialization failed", err);

    /* Register application element callback */
    err = meshx_api_register_data_cb(g_config.app_element_cb);
    MESHX_ERR_PRINT_RET("Failed to register app element callback", err);

    /* Register application control callback */
    err = meshx_api_register_ctrl_cb(g_config.app_ctrl_cb);
    MESHX_ERR_PRINT_RET("Failed to register app control callback", err);

    /* Initialize the Bluetooth Mesh Subsystem */
    err = meshx_ble_mesh_init(&g_config);
    MESHX_ERR_PRINT_RET("Bluetooth mesh init failed", err);

    /* Initialize Hosted Serial Protocol */
    err = meshx_serial_init();
    MESHX_ERR_PRINT_RET("Serial init failed", err);

#if CONFIG_ENABLE_UNIT_TEST
    /* Initialize unit test console (this also registers the 'ut' command) */
    err = init_unit_test_console();
    MESHX_ERR_PRINT_RET("Failed to initialize unit test console", err);
#endif /* CONFIG_ENABLE_UNIT_TEST */

    return err;
}

uint16_t meshx_get_net_key_id(void)
{
    return g_dev.meshx_store.net_key_id;
}

uint16_t meshx_get_node_addr(void)
{
    return g_dev.meshx_store.node_addr;
}

size_t meshx_get_element_count(void)
{
    return g_dev.element_cnt;
}

