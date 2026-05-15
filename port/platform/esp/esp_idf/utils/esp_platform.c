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

#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_vfs_dev.h"
#endif

#ifndef CONFIG_MESHX_CONSOLE_UART_PORT
#define CONFIG_MESHX_CONSOLE_UART_PORT UART_NUM_0
#endif

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

    /* Initialize Console interface by default for early logging */
    meshx_platform_console_init();

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
 */
void meshx_platform_serial_write(const uint8_t *data, uint16_t len)
{
    uart_write_bytes(CONFIG_MXSP_UART_PORT, (const char *)data, len);
}

/**
 * @brief Reads data from the serial interface.
 */
int32_t meshx_platform_serial_read(uint8_t *data, uint16_t len)
{
    return uart_read_bytes(CONFIG_MXSP_UART_PORT, data, len, 20 / portTICK_PERIOD_MS);
}

meshx_err_t meshx_platform_console_init(void)
{
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    /* Disable buffering on stdin and stdout */
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    /* Install USB-SERIAL-JTAG driver for interrupt-driven reads */
    usb_serial_jtag_driver_config_t usb_serial_jtag_config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    esp_err_t ret = usb_serial_jtag_driver_install(&usb_serial_jtag_config);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) return MESHX_ERR_PLAT;

    /* Tell VFS to use USB-SERIAL-JTAG driver */
    usb_serial_jtag_vfs_use_driver();
    ESP_LOGD("MESHX_PLAT", "Console initialized via USB Serial JTAG");

#elif CONFIG_ESP_CONSOLE_UART
    if (!uart_is_driver_installed(CONFIG_MESHX_CONSOLE_UART_PORT)) {
        uart_config_t uart_config = {
            .baud_rate = 115200,
            .data_bits = UART_DATA_8_BITS,
            .parity    = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .source_clk = UART_SCLK_DEFAULT,
        };
        ESP_ERROR_CHECK(uart_driver_install(CONFIG_MESHX_CONSOLE_UART_PORT, 1024, 0, 0, NULL, 0));
        ESP_ERROR_CHECK(uart_param_config(CONFIG_MESHX_CONSOLE_UART_PORT, &uart_config));
        ESP_LOGD("MESHX_PLAT", "Console initialized via UART%d", CONFIG_MESHX_CONSOLE_UART_PORT);
    }
#endif
    return MESHX_SUCCESS;
}

void meshx_platform_console_write(const char *data, uint16_t len)
{
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    usb_serial_jtag_write_bytes(data, len, portMAX_DELAY);
#elif CONFIG_ESP_CONSOLE_UART
    uart_write_bytes(CONFIG_MESHX_CONSOLE_UART_PORT, data, len);
#else
    // Fallback to standard printf/fwrite if needed
    fwrite(data, 1, len, stdout);
#endif
}

int32_t meshx_platform_console_read(uint8_t *data, uint16_t len)
{
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    return usb_serial_jtag_read_bytes(data, len, 10 / portTICK_PERIOD_MS);
#elif CONFIG_ESP_CONSOLE_UART
    return uart_read_bytes(CONFIG_MESHX_CONSOLE_UART_PORT, data, len, 10 / portTICK_PERIOD_MS);
#else
    return -1;
#endif
}
