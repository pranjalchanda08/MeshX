/**
 * Copyright (c) 2024 - 2026 MeshX
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file meshx_platform_zephyr.c
 * @brief Platform-specific initialization and console/serial I/O for Zephyr RTOS.
 *        Supports dynamic channel detection (Physical UART vs. USB CDC ACM / USB Serial JTAG).
 */

#include "interface/meshx_platform.h"
#include "interface/logging/meshx_log.h"
#include "meshx_config_internal.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#if defined(CONFIG_REBOOT)
#include <zephyr/sys/reboot.h>
#endif
#if defined(CONFIG_USB_DEVICE_STACK)
#include <zephyr/usb/usb_device.h>
#endif
#include <zephyr/sys/printk.h>

static bool g_mxsp_use_console = false;

meshx_err_t meshx_platform_init(void)
{
    return meshx_platform_console_init();
}

__attribute__((noreturn)) void meshx_platform_reset(void)
{
#if defined(CONFIG_REBOOT)
    sys_reboot(SYS_REBOOT_COLD);
#else
    MESHX_LOGI(MODULE_ID_COMMON, "Platform reset requested");
#endif
    while (1)
    {
        k_busy_wait(1000000);
    }
}

void meshx_platform_console_write(const char *data, uint16_t len)
{
    if (!data || len == 0)
    {
        return;
    }

#if DT_HAS_CHOSEN(zephyr_console)
    const struct device *dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
    if (device_is_ready(dev))
    {
        for (uint16_t i = 0; i < len; i++)
        {
            uart_poll_out(dev, (unsigned char)data[i]);
        }
        return;
    }
#endif

    for (uint16_t i = 0; i < len; i++)
    {
        printk("%c", data[i]);
    }
}

int32_t meshx_platform_console_read(uint8_t *data, uint16_t len)
{
    if (!data || len == 0)
    {
        return 0;
    }

#if DT_HAS_CHOSEN(zephyr_console)
    const struct device *dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
    if (!device_is_ready(dev))
    {
        return -1;
    }

    uint16_t read_bytes = 0;
    while (read_bytes < len)
    {
        unsigned char c;
        if (uart_poll_in(dev, &c) == 0)
        {
            data[read_bytes++] = (uint8_t)c;
        }
        else
        {
            break;
        }
    }
    if (read_bytes == 0)
    {
        k_msleep(10);
    }
    return read_bytes;
#else
    return 0;
#endif
}

meshx_err_t meshx_platform_console_init(void)
{
#if defined(CONFIG_USB_DEVICE_STACK)
    /* Enable Zephyr USB device controller for CDC ACM */
    int ret = usb_enable(NULL);
    if (ret != 0 && ret != -EALREADY)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to enable Zephyr USB device: %d", ret);
        return MESHX_ERR_PLAT;
    }
#endif
    return MESHX_SUCCESS;
}

meshx_platform_console_channel_t meshx_platform_get_console_channel(void)
{
#if DT_HAS_CHOSEN(zephyr_console)
#if DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart) || \
    DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), espressif_esp32_usb_serial_jtag)
    return MESHX_PLATFORM_CONSOLE_CHANNEL_USB_CDC;
#else
    return MESHX_PLATFORM_CONSOLE_CHANNEL_UART;
#endif
#elif defined(CONFIG_USB_CDC_ACM) || defined(CONFIG_UART_ESP32_USB)
    return MESHX_PLATFORM_CONSOLE_CHANNEL_USB_CDC;
#else
    return MESHX_PLATFORM_CONSOLE_CHANNEL_UART;
#endif
}

meshx_err_t meshx_platform_serial_init(void)
{
    return MESHX_SUCCESS;
}

int32_t meshx_platform_serial_read(uint8_t *data, uint16_t max_len)
{
    if (g_mxsp_use_console)
    {
        return meshx_platform_console_read(data, max_len);
    }
    k_msleep(20);
    return 0;
}

void meshx_platform_serial_write(const uint8_t *data, uint16_t len)
{
    if (g_mxsp_use_console)
    {
        meshx_platform_console_write((const char *)data, len);
    }
}

bool meshx_platform_get_mxsp_use_console(void)
{
    return g_mxsp_use_console;
}

void meshx_platform_set_mxsp_use_console(bool enable)
{
    g_mxsp_use_console = enable;
}
