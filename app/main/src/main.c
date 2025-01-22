/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file main.c
 * @brief Main application file for initializing MeshX on ESP32.
 *
 * This file contains the entry point for the application which initializes
 * the MeshX library and handles any initialization errors.
 */


#include "meshx.h"

#define TAG __func__

 /**
 * @brief Main application entry point.
 *
 * This function initializes the MeshX library and logs an error message
 * if the initialization fails.
 */
void app_main(void)
{
    esp_err_t err;

    err = meshx_init();
    if(err)
    {
        ESP_LOGE(TAG, "MeshX Init failed (err: 0x%x)", err);
        return;
    }
}
