/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file unit_test.h
 * @brief Header file for the production console unit test functionality.
 *
 * This file contains the declarations and definitions for initializing the
 * production console and registering unit test callbacks for different modules.
 *
 *
 */
#ifndef __MESHX_UNIT_TEST_H__
#define __MESHX_UNIT_TEST_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "module_id.h"

/**
 * @brief Macro to extract an argument from the argument list.
 */
#define UT_GET_ARG(_x, _type, _argv)   (_type) atoi( _argv [_x] )

/**
 * @brief Callback function for unit test modules.
 *
 * This function is used to define the callback function signature for unit test modules.
 * The callback function is invoked when a unit test command is received by the production console.
 *
 * @param cmd_id The command ID to be processed.
 * @param argc The number of arguments provided.
 * @param argv The array of arguments.
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 *     - Other error codes depending on the implementation
 */
typedef esp_err_t (*module_callback_t)(int cmd_id, int argc, char **argv);

typedef struct callback_node {
    module_callback_t callback;
} unit_test_callback_t;

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
esp_err_t register_ut_command();

/**
 * @brief Initialize the production console.
 *
 * This function initializes the production console for the application.
 *
 * @return
 *    - ESP_OK: Success
 *    - ESP_FAIL: Initialization failed
 */
esp_err_t init_unit_test_console(void);

/**
 * @brief Register a unit test for a specific module.
 *
 * This function registers a unit test callback for a given module.
 *
 * @param module_id The ID of the module for which the unit test is being registered.
 * @param callback The callback function to be called for the unit test.
 *
 * @return
 *    - ESP_OK: Success
 *    - ESP_FAIL: Registration failed
 */
esp_err_t register_unit_test(module_id_t module_id, module_callback_t callback) ;

#endif /* __MESHX_UNIT_TEST_H__ */
