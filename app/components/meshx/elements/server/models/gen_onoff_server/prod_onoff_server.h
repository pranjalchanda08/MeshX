/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file prod_onoff_server.h
 * @brief Header file for the On/Off Server model in the BLE Mesh Node application.
 *
 * This file contains the function declarations and necessary includes for the
 * On/Off Server model used in the BLE Mesh Node application.
 *
 */

#ifndef __PROD_ONOFF_SERVER__
#define __PROD_ONOFF_SERVER__

#include "app_common.h"
#include "prod_gen_server.h"
#include "meshx_control_task.h"

/**
 * @brief Initialize the On/Off Server model.
 *
 * This function initializes the On/Off Server model, setting up necessary
 * configurations and state variables.
 *
 * @return
 *    - ESP_OK: Success
 *    - ESP_FAIL: Failure
 */
esp_err_t prod_on_off_server_init(void);

#endif /* __PROD_ONOFF_SERVER__ */
