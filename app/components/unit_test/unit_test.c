/**
 * @file unit_test.c
 * @brief Unit test module for the production console.
 *
 * This file contains the implementation of the unit test module for the production console.
 * It provides functions to register unit test commands and initialize the console for
 * production use.
 *
 * @author [Pranjal Chanda]
 */
#include "unit_test.h"
#include "esp_log.h"

#define TAG "unit_test"
#define UT_CMD_MIN_ARGS 4

static unit_test_callback_t callback_list[MODULE_ID_MAX];

/**
 * @brief Handles unit test commands by invoking the appropriate callback based on the module ID.
 *
 * This function processes unit test commands by parsing the provided arguments and invoking
 * the corresponding callback function registered for the specified module ID.
 *
 * @param argc The number of arguments passed to the command.
 * @param argv An array of strings representing the arguments.
 *
 * @return
 *      - ESP_OK: If the unit test callback was successfully invoked.
 *      - ESP_ERR_INVALID_ARG: If the number of arguments is insufficient.
 *      - ESP_ERR_NOT_FOUND: If no unit test is registered for the specified module ID.
 */
static esp_err_t ut_command_handler(int argc, char **argv) {
    if (argc < UT_CMD_MIN_ARGS) {
        ESP_LOGE(TAG, "Insufficient arguments");
        return ESP_ERR_INVALID_ARG;
    }

    int cmd_id = atoi(argv[2]);
    int parsed_argc = atoi(argv[3]);
    module_id_t module_id = atoi(argv[1]);
    ESP_LOGD(TAG, "Unit Test: Params -> argc: %d, Module: %d, cmd_id: %d", parsed_argc, cmd_id , module_id);
    if (parsed_argc > (argc - UT_CMD_MIN_ARGS))
    {
        ESP_LOGE(TAG, "Insufficient module arguments");
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < parsed_argc; i++)
    {
        ESP_LOGD(TAG, "argv[%d]: %s", i, argv[i + UT_CMD_MIN_ARGS]);
    }

    if (module_id >= MODULE_ID_MAX) {
        ESP_LOGE(TAG, "Module ID %d unknown", module_id);
        return ESP_ERR_INVALID_ARG;
    }

    if(NULL != callback_list[module_id].callback)
    {
        return callback_list[module_id].callback(cmd_id, parsed_argc, (argv + UT_CMD_MIN_ARGS));
    }

    ESP_LOGE(TAG, "No unit test registered for module ID %d", module_id);
    return ESP_ERR_NOT_FOUND;
}

/**
 * @brief Registers the unit test (ut) command with the ESP console.
 *
 * This function creates a new console command "ut" which is used for running unit tests.
 * The command is registered with the ESP console using the esp_console_cmd_register function.
 *
 * @return
 *     - ESP_OK: Success
 *     - Other error codes: Failure
 */
esp_err_t register_ut_command() {
    const esp_console_cmd_t cmd = {
        .command = "ut",
        .help = "Run unit tests",
        .hint = NULL,
        .func = &ut_command_handler,
    };
    return esp_console_cmd_register(&cmd);
}

 /* @brief Registers the unit test (ut) command with the ESP console.
 *
 * This function creates a new console command "ut" which is used for running unit tests.
 * The command is registered with the ESP console using the esp_console_cmd_register function.
 *
 * @return
 *     - ESP_OK: Success
 *     - Other error codes: Failure
 */
esp_err_t init_prod_console() {
    // Initialize the console
    esp_err_t err = ESP_OK;
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();

    repl_config.prompt = "ble_node>";

    // install console REPL environment
#if CONFIG_ESP_CONSOLE_UART
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));
#endif
#if CONFIG_ESP_CONSOLE_USB_CDC
    esp_console_dev_usb_cdc_config_t cdc_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&cdc_config, &repl_config, &repl));
#endif
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    esp_console_dev_usb_serial_jtag_config_t usbjtag_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&usbjtag_config, &repl_config, &repl));
#endif

    // Register the unit test command
    err = register_ut_command();
    if (err != ESP_OK) {
        return err;
    }

    err = esp_console_start_repl(repl);
    if (err != ESP_OK) {
        return err;
    }

    return ESP_OK;
}

/**
 * @brief Register a unit test for a specific module.
 *
 * This function registers a unit test callback for the given module ID.
 *
 * @param module_id The ID of the module for which the unit test is being registered.
 * @param callback The callback function to be called for the unit test.
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 *     - ESP_FAIL: Failed to register the unit test
 */
esp_err_t register_unit_test(module_id_t module_id, module_callback_t callback) {
    if(module_id >= MODULE_ID_MAX)
        return ESP_ERR_INVALID_ARG;

    callback_list[module_id].callback = callback;
    return ESP_OK;
}
