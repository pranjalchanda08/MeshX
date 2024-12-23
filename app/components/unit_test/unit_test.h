/**
 * @file unit_test.h
 * @brief Header file for the production console unit test functionality.
 *
 * This file contains the declarations and definitions for initializing the
 * production console and registering unit test callbacks for different modules.
 *
 * @author [Pranjal Chanda]
 */
#ifndef __PROD_UNIT_TEST_H__
#define __PROD_UNIT_TEST_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "module_id.h"

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
esp_err_t init_prod_console(void);

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

#endif /* __PROD_UNIT_TEST_H__ */
