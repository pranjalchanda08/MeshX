/**
 * @file prod_light_ctl_srv.h
 * @brief Header file for the Light CTL Server module.
 *
 * This file contains the function declarations and necessary includes for
 * initializing and managing the Light CTL Server in the BLE mesh network.
 *
 * @note This module is part of the BLE mesh node application.
 */

#ifndef __PROD_LIGHT_CTL_SRV_H__
#define __PROD_LIGHT_CTL_SRV_H__

#include "app_common.h"
#include <prod_light_server.h>
#include "control_task.h"

/**
 * @brief Initialize the Light CTL Server.
 *
 * This function initializes the Light CTL Server, setting up necessary
 * configurations and preparing it to handle light control operations
 * within the BLE mesh network.
 *
 * @return
 *    - ESP_OK: Success
 *    - ESP_FAIL: Initialization failed
 */
esp_err_t prod_light_ctl_server_init(void);

#endif /*__PROD_LIGHT_CTL_SRV_H__*/
