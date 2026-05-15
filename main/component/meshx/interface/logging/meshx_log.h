/**
 * @file meshx_log.h
 * @brief Logging interface for MeshX with color-coded output
 */

#ifndef MESHX_LOG_H
#define MESHX_LOG_H

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include "meshx_err.h"
#include "module_id.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CONFIG_MESHX_LOG_THREADED
#define CONFIG_MESHX_LOG_THREADED 0
#endif

/* Log levels matching esp_log.h levels for compatibility where needed */
#define MESHX_LOG_NONE    0
#define MESHX_LOG_ERROR   1
#define MESHX_LOG_WARN    2
#define MESHX_LOG_INFO    3
#define MESHX_LOG_DEBUG   4
#define MESHX_LOG_VERBOSE 5
#define MESHX_LOG_MAX     6

typedef uint8_t meshx_log_level_t;

/* Logging control structure */
typedef struct {
    meshx_log_level_t def_log_level;
} meshx_logging_t;

/* Color codes for console output (used by host decoder or fallback) */
#define MESHX_LOG_COLOR_BLACK   "30"
#define MESHX_LOG_COLOR_RED     "31"
#define MESHX_LOG_COLOR_GREEN   "32"
#define MESHX_LOG_COLOR_BROWN   "33"
#define MESHX_LOG_COLOR_BLUE    "34"
#define MESHX_LOG_COLOR_PURPLE  "35"
#define MESHX_LOG_COLOR_CYAN    "36"
#define MESHX_LOG_COLOR_WHITE   "37"
#define MESHX_LOG_COLOR_RESET   "\033[0m"
#define MESHX_LOG_COLOR(COLOR)  "\033[0;" COLOR "m"

#define MESHX_LOG_COLOR_E       MESHX_LOG_COLOR(MESHX_LOG_COLOR_RED)
#define MESHX_LOG_COLOR_W       MESHX_LOG_COLOR(MESHX_LOG_COLOR_BROWN)
#define MESHX_LOG_COLOR_I       MESHX_LOG_COLOR(MESHX_LOG_COLOR_GREEN)
#define MESHX_LOG_COLOR_D       MESHX_LOG_COLOR(MESHX_LOG_COLOR_BLUE)
#define MESHX_LOG_COLOR_V       MESHX_LOG_COLOR(MESHX_LOG_COLOR_RESET)

#define MESHX_LOG_GET_COLOR(level) ((level) == MESHX_LOG_ERROR ? MESHX_LOG_COLOR_E : \
                                    (level) == MESHX_LOG_WARN  ? MESHX_LOG_COLOR_W : \
                                    (level) == MESHX_LOG_INFO  ? MESHX_LOG_COLOR_I : \
                                    (level) == MESHX_LOG_DEBUG ? MESHX_LOG_COLOR_BLUE : \
                                    MESHX_LOG_COLOR_RESET)

#ifndef CONFIG_MESHX_DEFAULT_LOG_LEVEL
#define CONFIG_MESHX_DEFAULT_LOG_LEVEL MESHX_LOG_INFO
#endif

#define MESHX_LOG_STR_ATTR __attribute__((section(".meshx_log_str"), used))
#define MESHX_LOG_SYNC_WORD 0xDEAD

#define MESHX_LOG_CONCAT_INNER(a, b) a ## b
#define MESHX_LOG_CONCAT(a, b) MESHX_LOG_CONCAT_INNER(a, b)

#ifdef __COUNTER__
#define MESHX_LOG_UNIQUE_ID __COUNTER__
#else
#define MESHX_LOG_UNIQUE_ID __LINE__
#endif

/**
 * @brief TLV Log Packet structure
 * @note Packed for transmission efficiency
 */
typedef struct {
    uint16_t sync;      /* 0xDEAD */
    uint8_t  len;       /* Length of payload (everything after this field) */
    uint8_t  level;     /* Log level */
    uint8_t  module_id; /* Module ID */
    uint32_t timestamp; /* System time in ms */
    uint32_t fmt_addr;  /* Address of format string in ELF */
    uint32_t file_addr; /* Address of file name in ELF */
    uint16_t line_no;   /* Line number */
    uint32_t args[16];  /* 16 variadic arguments */
    char     inline_str[32]; /* Space for one dynamic string (up to 32 chars) */
    uint8_t  parity;    /* XOR parity at the end */
} __attribute__((packed)) meshx_log_packet_t;

#define MESHX_LOG(module_id, level, format, ...) \
    MESHX_LOG_INTERNAL(MESHX_LOG_UNIQUE_ID, module_id, level, format, ##__VA_ARGS__, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)

#define MESHX_STR_INNER(x) #x
#define MESHX_STR(x) MESHX_STR_INNER(x)

#define MESHX_LOG_INTERNAL(id, module_id, level, format, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13, arg14, arg15, ...) \
    do                                                                \
    {                                                                 \
        static const char MESHX_LOG_CONCAT(_fmt, id)[] __attribute__((section(".meshx_log_str." MESHX_STR(id)), used)) = format; \
        static const char MESHX_LOG_CONCAT(_file, id)[] __attribute__((section(".meshx_log_str." MESHX_STR(id)), used)) = __FILE__; \
        meshx_log_packet(module_id, level, MESHX_LOG_CONCAT(_file, id), __LINE__, MESHX_LOG_CONCAT(_fmt, id), arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13, arg14, arg15); \
    } while (0)

#if CONFIG_MESHX_DEFAULT_LOG_LEVEL < MESHX_LOG_MAX
#define MESHX_LOGE(module_id, format, ...)                            \
    do                                                                \
    {                                                                 \
        MESHX_LOG(module_id, MESHX_LOG_ERROR, format, ##__VA_ARGS__); \
    } while (0)

#define MESHX_LOGW(module_id, format, ...)                            \
    do                                                                \
    {                                                                 \
        MESHX_LOG(module_id, MESHX_LOG_WARN, format, ##__VA_ARGS__);  \
    } while (0)

#define MESHX_LOGI(module_id, format, ...)                            \
    do                                                                \
    {                                                                 \
        MESHX_LOG(module_id, MESHX_LOG_INFO, format, ##__VA_ARGS__);  \
    } while (0)

#define MESHX_LOGD(module_id, format, ...)                            \
    do                                                                \
    {                                                                 \
        MESHX_LOG(module_id, MESHX_LOG_DEBUG, format, ##__VA_ARGS__); \
    } while (0)

#define MESHX_LOGV(module_id, format, ...)                            \
    do                                                                \
    {                                                                 \
        MESHX_LOG(module_id, MESHX_LOG_VERBOSE, format, ##__VA_ARGS__);\
    } while (0)

#else
#define MESHX_LOGE(module_id, format, ...) do {} while (0)
#define MESHX_LOGW(module_id, format, ...) do {} while (0)
#define MESHX_LOGI(module_id, format, ...) do {} while (0)
#define MESHX_LOGD(module_id, format, ...) do {} while (0)
#define MESHX_LOGV(module_id, format, ...) do {} while (0)
#endif

/* Exported functions */
meshx_err_t meshx_logging_init(const meshx_logging_t *config);
void meshx_module_set_log_level(module_id_t module_id, meshx_log_level_t log_level);

/**
 * @brief Logs a packetized binary log message.
 * @note This is called by the MESHX_LOG macro.
 */
void meshx_log_packet(module_id_t module_id, meshx_log_level_t log_level,
                      const char *file, int line_no, const char *fmt, ...);

/**
 * @brief Legacy printf-style logging for external components.
 */
void meshx_log_printf(module_id_t module_id, meshx_log_level_t log_level,
                      const char *func, int line_no, const char *fmt, ...);

#ifndef CONFIG_MESHX_LOG_PRINTF
#define CONFIG_MESHX_LOG_PRINTF printf
#endif

#ifdef __cplusplus
}
#endif

#endif /* MESHX_LOG_H */
