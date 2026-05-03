/**
 * Copyright (c) 2024 - 2025 MeshX
 *
 * @file esp_platform.c
 * @brief Platform-specific initialization and BLE Mesh provisioning for ESP32.
 *        This file contains functions to initialize the platform, Bluetooth,
 *        and BLE Mesh stack for the MeshX framework.
 *
 * @author Pranjal Chanda
 *
 */

#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/uart.h"

/* UART Configuration Macros */
#ifndef CONFIG_MXSP_UART_PORT
#define CONFIG_MXSP_UART_PORT UART_NUM_1
#endif

#ifndef CONFIG_MXSP_UART_BAUD
#define CONFIG_MXSP_UART_BAUD 115200
#endif

#ifndef CONFIG_MXSP_UART_TX_PIN
#define CONFIG_MXSP_UART_TX_PIN 1
#endif

#ifndef CONFIG_MXSP_UART_RX_PIN
#define CONFIG_MXSP_UART_RX_PIN 3
#endif

#include "interface/meshx_platform.h"
#include "interface/ble_mesh/server/meshx_ble_mesh_prov_srv.h"

/**
 * @brief Initializes the MeshX platform for the ESP32.
 *
 * This function sets up the necessary components and configurations
 * required for the MeshX platform to operate on the ESP32. It ensures
 * that the platform is ready for use by other components of the MeshX
 * system.
 *
 * @return
 *     - MESHX_OK on successful initialization.
 *     - Appropriate error code of type meshx_err_t on failure.
 */
meshx_err_t meshx_platform_init(void)
{
    esp_err_t err = ESP_OK;
    /* Initialize NVS flash */
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if(err)
        return MESHX_ERR_PLAT;

    /* Initialize Serial interface by default */
    meshx_platform_serial_init();

    /* Set log level for BLE Mesh */
    esp_log_level_set("BLE_MESH", ESP_LOG_ERROR);

    return MESHX_SUCCESS;
}

/**
 * @brief Resets the MeshX platform.
 * This function performs a system reset, restarting the platform.
 */
void meshx_platform_reset(void)
{
    esp_restart();
}

/**
 * @brief Initializes the serial interface.
 *
 * This function initializes the serial interface for the MeshX platform.
 * It configures the UART port, sets the baud rate, and enables the UART.
 *
 * @return
 *     - MESHX_OK on successful initialization.
 *     - Appropriate error code of type meshx_err_t on failure.
 */
meshx_err_t meshx_platform_serial_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = CONFIG_MXSP_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    int intr_alloc_flags = 0;

    esp_err_t ret = uart_driver_install(CONFIG_MXSP_UART_PORT, 1024 * 2, 0, 0, NULL, intr_alloc_flags);
    if (ret != ESP_OK) return MESHX_ERR_PLAT;

    ret = uart_param_config(CONFIG_MXSP_UART_PORT, &uart_config);
    if (ret != ESP_OK) return MESHX_ERR_PLAT;

    ret = uart_set_pin(CONFIG_MXSP_UART_PORT, CONFIG_MXSP_UART_TX_PIN, CONFIG_MXSP_UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) return MESHX_ERR_PLAT;

    return MESHX_SUCCESS;
}

/**
 * @brief Writes data to the serial interface.
 *
 * This function writes the given data to the serial interface.
 *
 * @param data Pointer to the data to be written.
 * @param len Length of the data to be written.
 */
void meshx_platform_serial_write(const uint8_t *data, uint16_t len)
{
    uart_write_bytes(CONFIG_MXSP_UART_PORT, (const char *)data, len);
}

/**
 * @brief Reads data from the serial interface.
 *
 * This function reads data from the serial interface.
 *
 * @param data Pointer to the buffer to store the read data.
 * @param len Maximum number of bytes to read.
 * @return int32_t Number of bytes read, or -1 if an error occurred.
 */
int32_t meshx_platform_serial_read(uint8_t *data, uint16_t len)
{
    return uart_read_bytes(CONFIG_MXSP_UART_PORT, data, len, 20 / portTICK_PERIOD_MS);
}
