/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_shell.c
 * @brief Core implementation of the MeshX standalone shell.
 */

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>

#include "interface/utils/meshx_shell.h"
#include "interface/meshx_platform.h"
#include "interface/rtos/meshx_rtos_utils.h"
#include "interface/rtos/meshx_task.h"
#include "interface/logging/meshx_log.h"
#include "interface/utils/meshx_tiny_printf.h"

#define MESHX_SHELL_MAX_COMMANDS 2
#define SHELL_MAX_LINE_LEN       128U
#define SHELL_MAX_ARGS           16U
#define SHELL_PROMPT             ""

#define MESHX_SHELL_TASK_STACK_SIZE 4096U
#define MESHX_SHELL_TASK_PRIORITY   4U
#define MESHX_SHELL_TASK_NAME      "meshx_shell"

typedef struct
{
    uint8_t cmd_count;
    uint8_t line_ptr;
    bool initialized;
    char line_buffer[SHELL_MAX_LINE_LEN];
    meshx_shell_cmd_t commands[MESHX_SHELL_MAX_COMMANDS];
} shell_ctx_t;

static shell_ctx_t *shell_ctx = NULL;

void meshx_shell_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    meshx_tiny_vprintf(fmt, args);
    va_end(args);
}

static void shell_help_cmd(void)
{
    meshx_shell_printf("\nAvailable Commands:\n");
    for (int i = 0; i < shell_ctx->cmd_count; i++)
    {
        meshx_shell_printf("  %-12s : %s\n", shell_ctx->commands[i].command, shell_ctx->commands[i].help);
    }
}

static void shell_process_line(char *line)
{
    char *argv[SHELL_MAX_ARGS];
    int argc = 0;

    // Simple tokenizer
    char *token = strtok(line, " \t\n\r");
    while (token != NULL && argc < SHELL_MAX_ARGS)
    {
        argv[argc++] = token;
        token = strtok(NULL, " \t\n\r");
    }

    if (argc == 0)
        return;

    if (strcmp(argv[0], "help") == 0 || strcmp(argv[0], "?") == 0)
    {
        shell_help_cmd();
        return;
    }

    for (int i = 0; i < shell_ctx->cmd_count; i++)
    {
        if (strcmp(argv[0], shell_ctx->commands[i].command) == 0)
        {
            int ret = shell_ctx->commands[i].func(argc, argv);
            if (ret != 0)
            {
                MESHX_LOGE(MODULE_ID_COMMON, "Command %s returned error: %d\n", argv[0], ret);
            }
            return;
        }
    }

    meshx_shell_printf("Unknown command: %s. Type 'help' for available commands.\n", argv[0]);
}

extern bool meshx_serial_is_hosted_mode_enabled(void);

static void shell_task(void *pvParameters)
{
    uint8_t c;
    meshx_shell_printf("\n%s", SHELL_PROMPT);

    while (true)
    {
        if (meshx_platform_get_mxsp_use_console() && meshx_serial_is_hosted_mode_enabled())
        {
            meshx_task_delay(100);
            continue;
        }
        if (meshx_platform_console_read(&c, 1) > 0)
        {
            if (c == '\r' || c == '\n')
            {
                shell_ctx->line_buffer[shell_ctx->line_ptr] = '\0';
                if (shell_ctx->line_ptr > 0)
                {
                    shell_process_line(shell_ctx->line_buffer);
                }
                shell_ctx->line_ptr = 0;
                meshx_shell_printf(SHELL_PROMPT);
            }
            else if (c == '\b' || c == 0x7F)    /* Backspace */
            {
                if (shell_ctx->line_ptr > 0)
                {
                    shell_ctx->line_ptr--;
                }
            }
            else if (shell_ctx->line_ptr < SHELL_MAX_LINE_LEN - 1)
            {
                if (isprint(c))
                {
                    shell_ctx->line_buffer[shell_ctx->line_ptr++] = c;
                }
            }
        }
        meshx_task_delay(10); // Small delay to prevent tight loop
    }
}

meshx_err_t meshx_shell_init(void)
{
    if (MESHX_SUCCESS != MESHX_CALLOC(shell_ctx, 1, sizeof(shell_ctx_t)))
    {
        return MESHX_NO_MEM;
    }
    if (shell_ctx->initialized)
        return MESHX_SUCCESS;

    MESHX_LOGD(MODULE_ID_COMMON, "Initializing MeshX Shell...");

    meshx_err_t err = meshx_platform_console_init();
    if (err != MESHX_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Platform console init failed: %d", err);
        return err;
    }

    shell_ctx->initialized = true;
    MESHX_LOGD(MODULE_ID_COMMON, "MeshX Shell Initialized");
    return MESHX_SUCCESS;
}

meshx_err_t meshx_shell_deinit(void)
{
    return MESHX_FREE(shell_ctx);
}

meshx_err_t meshx_shell_register_command(const meshx_shell_cmd_t *cmd)
{
    if (!shell_ctx->initialized)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Shell not initialized, cannot register command '%s'", cmd->command);
        return MESHX_ERR_NOT_INIT;
    }
    if (shell_ctx->cmd_count >= MESHX_SHELL_MAX_COMMANDS)
        return MESHX_FAIL;

    shell_ctx->commands[shell_ctx->cmd_count++] = *cmd;
    return MESHX_SUCCESS;
}

meshx_err_t meshx_shell_start(void)
{
    meshx_task_t task_cfg = {
        .arg        = NULL,
        .task_cb    = shell_task,
        .task_name  = MESHX_SHELL_TASK_NAME,
        .priority   = MESHX_SHELL_TASK_PRIORITY,
        .stack_size = MESHX_SHELL_TASK_STACK_SIZE,
    };

    return meshx_task_create(&task_cfg);
}
