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

#define TAG __func__
static unit_test_callback_t *callback_list = NULL;

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
static int ut_command_handler(int argc, char **argv) {
    if (argc < 2) {
        ESP_LOGE(TAG, "Insufficient arguments");
        return ESP_ERR_INVALID_ARG;
    }

    module_id_t module_id = atoi(argv[1]);
    if(module_id >= MODULE_ID_MAX)
    {
        ESP_LOGE(TAG, "Module ID %d unknown", module_id);
        return ESP_ERR_INVALID_ARG;
    }
    unit_test_callback_t *current = callback_list;

    while (current != NULL) {
        if (current->module_id == module_id) {
            current->callback(argc, argv);
            return ESP_OK;
        }
        current = current->next;
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
static esp_err_t register_ut_command() {
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
    unit_test_callback_t *new_node = (unit_test_callback_t *)malloc(sizeof(unit_test_callback_t));
    if (new_node == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for new callback node");
        return ESP_ERR_NO_MEM;
    }
    new_node->module_id = module_id;
    new_node->callback = callback;
    new_node->next = callback_list;
    callback_list = new_node;
    return ESP_OK;
}
