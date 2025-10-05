/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file unit_test.c
 * @brief Unit test module for the production console.
 *
 * This file contains the implementation of the unit test module for the production console.
 * It provides functions to register unit test commands and initialize the console for
 * production use.
 */

#include "unit_test.h"
#include "meshx_log.h"

#if CONFIG_ENABLE_UNIT_TEST

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
 *      - MESHX_SUCCESS: If the unit test callback was successfully invoked.
 *      - MESHX_INVALID_ARG: If the number of arguments is insufficient.
 *      - MESHX_NOT_FOUND: If no unit test is registered for the specified module ID.
 */
static meshx_err_t ut_command_handler(int argc, char **argv) {
    if (argc < UT_CMD_MIN_ARGS) {
        MESHX_LOGE("Insufficient arguments");
        return MESHX_INVALID_ARG;
    }

    int cmd_id = UT_GET_ARG(2, uint16_t, argv);
    int parsed_argc = UT_GET_ARG(3, uint16_t, argv);
    module_id_t module_id = UT_GET_ARG(1, uint16_t, argv);
    ESP_LOGD(TAG, "Unit Test: Params -> argc: %d, Module: %d, cmd_id: %d", parsed_argc, cmd_id , module_id);
    if (parsed_argc > (argc - UT_CMD_MIN_ARGS))
    {
        MESHX_LOGE("Insufficient module arguments");
        return MESHX_INVALID_ARG;
    }

    for (size_t i = 0; i < parsed_argc; i++)
    {
        MESHX_LOGD("argv[%d]: %s", i, argv[i + UT_CMD_MIN_ARGS]);
    }

    if (module_id >= MODULE_ID_MAX) {
        MESHX_LOGE("Module ID %d unknown", module_id);
        return MESHX_INVALID_ARG;
    }

    if(NULL != callback_list[module_id].callback)
    {
        return callback_list[module_id].callback(cmd_id, parsed_argc, (argv + UT_CMD_MIN_ARGS));
    }

    MESHX_LOGE("No unit test registered for module ID %d", module_id);
    return MESHX_NOT_FOUND;
}

/**
 * @brief Registers the unit test (ut) command with the ESP console.
 *
 * This function creates a new console command "ut" which is used for running unit tests.
 * The command is registered with the ESP console using the esp_console_cmd_register function.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - Other error codes: Failure
 */
meshx_err_t register_ut_command() {
    const esp_console_cmd_t cmd = {
        .command = "ut",
        .help = "Run unit tests",
        .hint = NULL,
        .func = (esp_console_cmd_func_t)&ut_command_handler,
    };
    return esp_console_cmd_register(&cmd);
}

 /* @brief Registers the unit test (ut) command with the ESP console.
 *
 * This function creates a new console command "ut" which is used for running unit tests.
 * The command is registered with the ESP console using the esp_console_cmd_register function.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - Other error codes: Failure
 */
meshx_err_t init_unit_test_console() {
    // Initialize the console
    meshx_err_t err = MESHX_SUCCESS;
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();

    repl_config.prompt = "X>";

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
    if (err != MESHX_SUCCESS) {
        return err;
    }

    err = esp_console_start_repl(repl);
    if (err != MESHX_SUCCESS) {
        return err;
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Register a unit test for a specific module.
 *
 * This function registers a unit test callback for the given module ID.
 *
 * @param[in] module_id     The ID of the module for which the unit test is being registered.
 * @param[in] callback      The callback function to be called for the unit test.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_INVALID_ARG: Invalid arguments
 *     - MESHX_FAIL: Failed to register the unit test
 */
meshx_err_t register_unit_test(module_id_t module_id, module_callback_t callback) {
    if(module_id >= MODULE_ID_MAX)
        return MESHX_INVALID_ARG;

    callback_list[module_id].callback = callback;
    return MESHX_SUCCESS;
}

#endif /* CONFIG_ENABLE_UNIT_TEST */
