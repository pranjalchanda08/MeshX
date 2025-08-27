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
#include "meshx_err.h"
#include "meshx_config_internal.h"

#if CONFIG_ENABLE_UNIT_TEST
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
 *     - MESHX_SUCCESS: Success
 *     - MESHX_INVALID_ARG: Invalid arguments
 *     - Other error codes depending on the implementation
 */
typedef meshx_err_t (*module_callback_t)(int cmd_id, int argc, char **argv);

/**
 * @brief Structure to hold the unit test callback function.
 */
typedef struct callback_node {
    module_callback_t callback;     /**< Callback function */
} unit_test_callback_t;

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
meshx_err_t register_ut_command();

/**
 * @brief Initialize the production console.
 *
 * This function initializes the production console for the application.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_FAIL: Initialization failed
 */
meshx_err_t init_unit_test_console(void);

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
meshx_err_t register_unit_test(module_id_t module_id, module_callback_t callback);

#endif /* CONFIG_ENABLE_UNIT_TEST */
#endif /* __MESHX_UNIT_TEST_H__ */
