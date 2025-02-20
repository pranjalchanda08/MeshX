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

#define CONFIG_MESHX_NVS_SAVE_PERIOD_MS    1000

/**
 * @brief Array of element components with their respective types and counts.
 *
 * This array holds the configuration for different types of elements in the MeshX application.
 * Each entry in the array consists of an element type and the corresponding count defined in the configuration.
 *
 */
static element_comp_t element_comp_arr [] = {
    {MESHX_ELEMENT_TYPE_RELAY_SERVER, CONFIG_RELAY_SERVER_COUNT},
    {MESHX_ELEMENT_TYPE_RELAY_CLIENT, CONFIG_RELAY_CLIENT_COUNT},
    {MESHX_ELEMENT_TYPE_LIGHT_CWWW_SERVER, CONFIG_LIGHT_CWWW_SRV_COUNT},
    {MESHX_ELEMENT_TYPE_LIGHT_CWWW_CLIENT, CONFIG_LIGHT_CWWW_CLIENT_COUNT}
};

/**
 * @brief Configuration for the MeshX library.
 */
static const meshx_config_t meshx_config = {
    .cid = CONFIG_CID_ID,
    .pid = CONFIG_PID_ID,
    .product_name = CONFIG_PRODUCT_NAME,
    .element_comp_arr = element_comp_arr,
    .element_comp_arr_len = ARRAY_SIZE(element_comp_arr),
    .meshx_nvs_save_period = CONFIG_MESHX_NVS_SAVE_PERIOD_MS
};

 /**
 * @brief Main application entry point.
 *
 * This function initializes the MeshX library and logs an error message
 * if the initialization fails.
 */
void app_main(void)
{
    esp_err_t err;

    err = meshx_init(&meshx_config);
    if(err)
    {
        ESP_LOGE(TAG, "MeshX Init failed (err: 0x%x)", err);
        return;
    }
}
