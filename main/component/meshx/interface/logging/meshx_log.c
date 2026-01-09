/**
 * @file meshx_log.c
 * @brief Implementation of the MeshX logging system.
 *
 * This file contains the implementation of the logging system for the MeshX
 * application. It provides functions to initialize the logging system, set
 * logging levels for specific modules, and log formatted messages with
 * different log levels and colors.
 *
 * The logging system supports different modules identified by module IDs,
 * and each module can have its own logging level. The log levels determine
 * the verbosity of the log messages. The system also supports colored log
 * messages for different log levels.
 *
 * The logging system requires a configuration structure to be initialized,
 * which includes a default log level and a callback function to retrieve
 * the current time in milliseconds.
 *
 * The main functions provided are:
 * - meshx_logging_init: Initializes the logging system with the provided
 *   configuration.
 * - meshx_module_set_log_level: Sets the logging level for a specified
 *   module.
 * - meshx_log_printf: Logs a formatted message for a specified module and
 *   log level.
 *
 * The file also defines macros for logging messages at different levels,
 * such as error, warning, info, and debug.
 *
 * @author Pranjal Chanda
 */

#include "meshx_log.h"
#include "module_id.h"
#include "interface/rtos/meshx_rtos_utils.h"
#include <stdio.h>
#include <stdarg.h>

static meshx_log_level_t module_log_level[MODULE_ID_MAX];

static const char * log_lvl_str [MESHX_LOG_MAX] =
{
    "", "D", "I", "W", "E"
};

static meshx_logging_t meshx_logging_ctrl;

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
meshx_err_t meshx_logging_init(const meshx_logging_t *config)
{
    if (!config)
        return MESHX_INVALID_ARG;

    meshx_logging_ctrl.def_log_level = config->def_log_level;
    for (size_t i = 0; i < MODULE_ID_MAX; i++)
    {
        module_log_level[i] = CONFIG_MESHX_DEFAULT_LOG_LEVEL;
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Sets the logging level for a specified module.
 *
 * This function assigns a new logging level to a given module
 * identified by its module ID. The logging level determines
 * the verbosity of log messages for that module.
 *
 * @param[in] module_id The ID of the module whose log level is to be set.
 * @param[in] log_level The new logging level to be assigned to the module.
 */
void meshx_module_set_log_level(module_id_t module_id, meshx_log_level_t log_level)
{
    module_log_level[module_id] = log_level;
}

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
void meshx_log_printf(module_id_t module_id, meshx_log_level_t log_level,
                      const char *func, int line_no, const char *fmt, ...)
{
    /* Validate module ID */
    if (module_id > MODULE_ID_MAX || log_level < meshx_logging_ctrl.def_log_level || module_log_level[module_id] < meshx_logging_ctrl.def_log_level)
        return;

    /* Get timestamp */
        unsigned int millis;
    unsigned int task_id;

    meshx_rtos_get_sys_time(&millis);
    meshx_rtos_get_curr_task_id_prio(&task_id);
    /* Get log level color */
    const char *color = MESHX_LOG_LEVEL_COLOR(log_level);

    /* Print timestamp and log */
    CONFIG_MESHX_LOG_PRINTF("\r%s[%s][%08u][%03x][%25s:%04d]\t", color, log_lvl_str[log_level], millis, task_id, func, line_no);

    /* Process variable arguments */
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    /* Reset color and add newline */
    CONFIG_MESHX_LOG_PRINTF("%s\n", MESHX_LOG_COLOR_RESET);
}
