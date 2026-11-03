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
#include "hal/usb_serial_jtag_ll.h"
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
#include "interface/rtos/meshx_sem.h"

static bool g_mxsp_use_console = false;
static meshx_sem_t s_console_tx_sem = {
    .sem_name = "cons_tx_sem",
    .max_count = 1,
    .init_count = 1,
    .__sem_handle = NULL
};
static bool s_console_tx_sem_init = false;

static void ensure_tx_sem_init(void)
{
    if (!s_console_tx_sem_init) {
        meshx_sem_create(&s_console_tx_sem);
        s_console_tx_sem_init = true;
    }
}

bool meshx_platform_console_tx_lock(meshx_platform_tx_prio_t prio, uint32_t timeout_ms)
{
    if (!g_mxsp_use_console) {
        return true;
    }
    ensure_tx_sem_init();
    uint32_t wait_ms = (prio == MESHX_PLATFORM_TX_PRIO_HIGH) ? timeout_ms : 0;
    return (meshx_sem_take(&s_console_tx_sem, wait_ms) == MESHX_SUCCESS);
}

void meshx_platform_console_tx_unlock(void)
{
    if (!g_mxsp_use_console) {
        return;
    }
    ensure_tx_sem_init();
    meshx_sem_give(&s_console_tx_sem);
}

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
    if (g_mxsp_use_console)
    {
        if (meshx_platform_console_tx_lock(MESHX_PLATFORM_TX_PRIO_HIGH, 500))
        {
            meshx_platform_console_write((const char *)data, len);
            meshx_platform_console_tx_unlock();
        }
    }
    else
    {
        uart_write_bytes(CONFIG_MXSP_UART_PORT, (const char *)data, len);
    }
}

/**
 * @brief Reads data from the serial interface.
 */
int32_t meshx_platform_serial_read(uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    if (g_mxsp_use_console)
    {
        return meshx_platform_console_read(data, len, timeout_ms);
    }
    else
    {
        return uart_read_bytes(CONFIG_MXSP_UART_PORT, data, len, timeout_ms / portTICK_PERIOD_MS);
    }
}

meshx_err_t meshx_platform_console_init(void)
{
    static bool console_initialized = false;
    if (console_initialized) return MESHX_SUCCESS;

#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    usb_serial_jtag_driver_config_t usj_cfg = {
        .rx_buffer_size = 2048,
        .tx_buffer_size = 2048,
    };
    usb_serial_jtag_driver_install(&usj_cfg);
    usb_serial_jtag_vfs_use_driver();
    ESP_LOGD("MESHX_PLAT", "Console initialized via USB Serial JTAG ISR driver");

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
    console_initialized = true;
    return MESHX_SUCCESS;
}

void meshx_platform_console_write(const char *data, uint16_t len)
{
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    usb_serial_jtag_write_bytes(data, len, pdMS_TO_TICKS(50));
#elif CONFIG_ESP_CONSOLE_UART
    uart_write_bytes(CONFIG_MESHX_CONSOLE_UART_PORT, data, len);
#else
    // Fallback to standard printf/fwrite if needed
    fwrite(data, 1, len, stdout);
#endif
}

int32_t meshx_platform_console_read(uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    TickType_t ticks = (timeout_ms == 0) ? 0 : pdMS_TO_TICKS(timeout_ms);
    if (timeout_ms > 0 && ticks == 0) ticks = 1;
    return usb_serial_jtag_read_bytes(data, len, ticks);
#elif CONFIG_ESP_CONSOLE_UART
    TickType_t ticks = (timeout_ms == 0) ? 0 : pdMS_TO_TICKS(timeout_ms);
    if (timeout_ms > 0 && ticks == 0) {
        ticks = 1;
    }
    return uart_read_bytes(CONFIG_MESHX_CONSOLE_UART_PORT, data, len, ticks);
#else
    return -1;
#endif
}

/**
 * @brief Get the active console channel type.
 * @return The active console channel type.
 */
meshx_platform_console_channel_t meshx_platform_get_console_channel(void)
{
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    return MESHX_PLATFORM_CONSOLE_CHANNEL_USB_CDC;
#else
    return MESHX_PLATFORM_CONSOLE_CHANNEL_UART;
#endif
}

/**
 * @brief Get the current dynamic MXSP routing target.
 * @return True if MXSP is currently multiplexed over console channel, False if routed to UART1.
 */
bool meshx_platform_get_mxsp_use_console(void)
{
    return g_mxsp_use_console;
}

/**
 * @brief Set the dynamic MXSP routing target.
 * @param[in] enable Set to true to route MXSP over active console log channel.
 */
void meshx_platform_set_mxsp_use_console(bool enable)
{
    g_mxsp_use_console = enable;
}

