/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_shell.h
 * @brief Platform-agnostic shell interface for MeshX.
 */

#ifndef __MESHX_SHELL_H__
#define __MESHX_SHELL_H__

#include "meshx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Shell command function pointer type.
 */
typedef int (*meshx_shell_func_t)(int argc, char **argv);

/**
 * @brief Shell command structure.
 */
typedef struct {
    const char *command; /**< Command name */
    const char *help;    /**< Help text */
    const char *hint;    /**< Hint text (optional) */
    meshx_shell_func_t func; /**< Command function */
} meshx_shell_cmd_t;

#define MESHX_SHELL_MAX_COMMANDS 32

/**
 * @brief Initializes the MeshX shell core and the underlying platform console.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_shell_init(void);

/**
 * @brief Registers a command with the MeshX shell.
 *
 * @param[in] cmd Pointer to the command structure.
 * @return meshx_err_t MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_shell_register_command(const meshx_shell_cmd_t *cmd);

/**
 * @brief Starts the MeshX shell task.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_shell_start(void);

/**
 * @brief Prints a message to the shell console.
 *
 * @param[in] fmt Format string.
 * @param[in] ... Arguments.
 */
void meshx_shell_printf(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_SHELL_H__ */
