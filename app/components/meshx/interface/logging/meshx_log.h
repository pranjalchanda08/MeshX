/**
 * @file meshx_log.h
 * @brief Logging interface for MeshX with color-coded output
 */

#ifndef MESHX_LOG_H
#define MESHX_LOG_H

#include <stdio.h>
#include "meshx_config_internal.h"
#include "module_id.h"
#include "meshx_err.h"

/**
 * @brief Macro to define printf function
 * @note this can be overriden by compile time Macro def
 */
#ifndef CONFIG_MESHX_LOG_PRINTF
#define CONFIG_MESHX_LOG_PRINTF printf
#endif /* CONFIG_MESHX_LOG_PRINTF */

#ifndef CONFIG_MESHX_DEFAULT_LOG_LEVEL
#define CONFIG_MESHX_DEFAULT_LOG_LEVEL MESHX_LOG_INFO
#endif /* CONFIG_MESHX_DEFAULT_LOG_LEVEL */

/* ANSI Color Codes */
#define MESHX_LOG_COLOR_BLACK "\033[0;30m"
#define MESHX_LOG_COLOR_RED "\033[0;31m"
#define MESHX_LOG_COLOR_GREEN "\033[0;32m"
#define MESHX_LOG_COLOR_YELLOW "\033[0;33m"
#define MESHX_LOG_COLOR_BLUE "\033[0;34m"
#define MESHX_LOG_COLOR_PURPLE "\033[0;35m"
#define MESHX_LOG_COLOR_CYAN "\033[0;36m"
#define MESHX_LOG_COLOR_WHITE "\033[0;37m"
#define MESHX_LOG_COLOR_RESET "\033[0m"

/* Log levels */

#define MESHX_LOG_VERBOSE 0 /* White */
#define MESHX_LOG_DEBUG 1   /* Blue */
#define MESHX_LOG_INFO 2    /* Green */
#define MESHX_LOG_WARN 3    /* Yellow */
#define MESHX_LOG_ERROR 4   /* Red */
#define MESHX_LOG_NONE 5    /* Blue */
#define MESHX_LOG_MAX 6

typedef unsigned meshx_log_level_t;

typedef unsigned int (*millis_t)(void);

typedef struct meshx_logging
{
    unsigned def_log_level;
} meshx_logging_t;

/* Get color for log level */
#define MESHX_LOG_LEVEL_COLOR(level) (                                                                    \
    (level) == MESHX_LOG_ERROR ? MESHX_LOG_COLOR_RED : (level) == MESHX_LOG_WARN ? MESHX_LOG_COLOR_YELLOW \
                                                   : (level) == MESHX_LOG_INFO   ? MESHX_LOG_COLOR_GREEN  \
                                                   : (level) == MESHX_LOG_DEBUG  ? MESHX_LOG_COLOR_BLUE   \
                                                                                 : MESHX_LOG_COLOR_RESET)

/* Logging macro with colors */
#define MESHX_LOG(module_id, level, format, ...) meshx_log_printf(module_id, level, __FILENAME__, __LINE__, format, ##__VA_ARGS__)

#if CONFIG_MESHX_DEFAULT_LOG_LEVEL < MESHX_LOG_MAX
#define MESHX_LOGE(module_id, format, ...)                            \
    do                                                                \
    {                                                                 \
        MESHX_LOG(module_id, MESHX_LOG_ERROR, format, ##__VA_ARGS__); \
    } while (0)
#else
#define MESHX_LOGE(module_id, format, ...) \
    do                                     \
    {                                      \
    } while (0) // Prevent empty macro expansion warning

#endif

#if CONFIG_MESHX_DEFAULT_LOG_LEVEL < MESHX_LOG_ERROR
#define MESHX_LOGW(module_id, format, ...)                           \
    do                                                               \
    {                                                                \
        MESHX_LOG(module_id, MESHX_LOG_WARN, format, ##__VA_ARGS__); \
    } while (0)
#else
#define MESHX_LOGW(module_id, format, ...) \
    do                                     \
    {                                      \
    } while (0)
#endif

#if CONFIG_MESHX_DEFAULT_LOG_LEVEL < MESHX_LOG_WARN
#define MESHX_LOGI(module_id, format, ...)                           \
    do                                                               \
    {                                                                \
        MESHX_LOG(module_id, MESHX_LOG_INFO, format, ##__VA_ARGS__); \
    } while (0)
#else
#define MESHX_LOGI(module_id, format, ...) \
    do                                     \
    {                                      \
    } while (0)
#endif

#if CONFIG_MESHX_DEFAULT_LOG_LEVEL < MESHX_LOG_INFO
#define MESHX_LOGD(module_id, format, ...)                            \
    do                                                                \
    {                                                                 \
        MESHX_LOG(module_id, MESHX_LOG_DEBUG, format, ##__VA_ARGS__); \
    } while (0)
#else
#define MESHX_LOGD(module_id, format, ...) \
    do                                     \
    {                                      \
    } while (0)
#endif

/**
 * @brief Initializes the MeshX logging system with the provided configuration.
 *
 * This function sets up the logging system by assigning the default log level
 * and the callback function for retrieving the current time in milliseconds.
 * It validates the configuration parameters and returns an error if they are
 * invalid.
 *
 * @param[in] config Pointer to the logging configuration structure.
 *
 * @return MESHX_SUCCESS on successful initialization, MESHX_INVALID_ARG if
 *         the configuration is invalid.
 */
meshx_err_t meshx_logging_init(const meshx_logging_t *config);

/**
 * @brief Logs a formatted message for a specified module and log level.
 *
 * @note This is a weak function, can be redefined to override in app layer
 *
 * This function checks if the provided module ID and log level are valid
 * and above the current global and module-specific log levels. If valid,
 * it processes the variable arguments to format and log the message.
 *
 * @note This is a weak function defination and can be overriden by any other version
 *       of defination required as per platform
 *
 * @param[in] module_id     The ID of the module for which the log is generated.
 * @param[in] log_level     The log level of the message.
 * @param[in] func          The name of the function where the log is called.
 * @param[in] line_no       The line number in the source code where the log is called.
 * @param[in] fmt           The format string for the log message.
 * @param[in] ...           Additional arguments for the format string.
 */
MESHX_WEEK void meshx_log_printf(module_id_t module_id, meshx_log_level_t log_level,
                      const char *func, int line_no, const char *fmt, ...);

#endif /* MESHX_LOG_H */
