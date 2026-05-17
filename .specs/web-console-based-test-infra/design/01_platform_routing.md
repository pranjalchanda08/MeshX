# Design Page 01: Platform Routing Layer & APIs

This document details the firmware implementation of the dynamic platform serial routing abstraction layer and query APIs.

---

## 1. Interface Signature Changes (`meshx_platform.h`)

We expose the query and control interfaces in [meshx_platform.h](../../../main/component/meshx/interface/meshx_platform.h):

```c
/**
 * @brief Console channel interface types.
 */
typedef enum {
    MESHX_PLATFORM_CONSOLE_CHANNEL_UART,     /**< Standard physical logging UART */
    MESHX_PLATFORM_CONSOLE_CHANNEL_USB_CDC   /**< Native USB-to-UART or JTAG/CDC Bridge */
} meshx_platform_console_channel_t;

/**
 * @brief Expose console detection capabilities.
 * @return Active console channel type configuration.
 */
meshx_platform_console_channel_t meshx_platform_get_console_channel(void);

/**
 * @brief Get the current dynamic MXSP routing target.
 * @return True if MXSP is currently multiplexed over console channel, False if routed to UART1.
 */
bool meshx_platform_get_mxsp_use_console(void);

/**
 * @brief Set the dynamic MXSP routing target.
 * @param[in] enable Set to true to route MXSP over active console log channel.
 */
void meshx_platform_set_mxsp_use_console(bool enable);
```

---

## 2. Platform Driver Implementation (`esp_platform.c`)

We modify [esp_platform.c](../../../port/platform/esp/esp_idf/utils/esp_platform.c) to implement these routing abstractions:

```c
#include "interface/meshx_platform.h"

static bool g_mxsp_use_console = false;

meshx_platform_console_channel_t meshx_platform_get_console_channel(void) {
#if defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    return MESHX_PLATFORM_CONSOLE_CHANNEL_USB_CDC;
#else
    return MESHX_PLATFORM_CONSOLE_CHANNEL_UART;
#endif
}

bool meshx_platform_get_mxsp_use_console(void) {
    return g_mxsp_use_console;
}

void meshx_platform_set_mxsp_use_console(bool enable) {
    g_mxsp_use_console = enable;
    MESHX_LOGI(MODULE_ID_COMMON, "Serial MXSP route dynamically changed to: %s",
               enable ? (meshx_platform_get_console_channel() == MESHX_PLATFORM_CONSOLE_CHANNEL_USB_CDC ? "USB CDC" : "UART Console")
                      : "Dedicated UART1");
}

int32_t meshx_platform_serial_write(const uint8_t *data, uint16_t length) {
    if (g_mxsp_use_console) {
        return meshx_platform_console_write(data, length);
    } else {
        // Physical UART1 peripheral write implementation
        return meshx_platform_uart_write(UART_NUM_1, data, length);
    }
}

int32_t meshx_platform_serial_read(uint8_t *data, uint16_t max_length) {
    if (g_mxsp_use_console) {
        return meshx_platform_console_read(data, max_length);
    } else {
        // Physical UART1 peripheral read implementation
        return meshx_platform_uart_read(UART_NUM_1, data, max_length);
    }
}
```

---

## 3. CLI Unit Test Command Handler (`unit_test.c`)

To fulfill **REQ-002**, register the toggle switch subcommand within [unit_test.c](../../../main/component/unit_test/src/unit_test.c):

```c
#define MODULE_ID_COMMON 8
#define SUB_CMD_SET_ROUTING 1

static int common_ut_callback(int cmd_id, int argc, char **argv) {
    if (cmd_id == SUB_CMD_SET_ROUTING) {
        if (argc < 1) {
            MESHX_LOGE(MODULE_ID_COMMON, "Routing value argument missing");
            return MESHX_INVALID_ARG;
        }
        int route_to_console = atoi(argv[0]);
        meshx_platform_set_mxsp_use_console(route_to_console != 0);
        return MESHX_SUCCESS;
    }
    return MESHX_NOT_SUPPORTED;
}
```
During initialization inside `init_unit_test_console()`, register the Common Module callback:
```c
register_unit_test(MODULE_ID_COMMON, common_ut_callback);
```
