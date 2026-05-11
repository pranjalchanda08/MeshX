/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_shell.c
 * @brief Core implementation of the MeshX standalone shell.
 */

#include "interface/utils/meshx_shell.h"
#include "interface/meshx_platform.h"
#include "interface/rtos/meshx_task.h"
#include "interface/logging/meshx_log.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>

#define SHELL_MAX_LINE_LEN 256
#define SHELL_MAX_ARGS     16
#define SHELL_PROMPT       "MeshX> "

static struct {
    meshx_shell_cmd_t commands[MESHX_SHELL_MAX_COMMANDS];
    uint8_t cmd_count;
    char line_buffer[SHELL_MAX_LINE_LEN];
    uint8_t line_ptr;
    bool initialized;
} shell_ctx;

void meshx_shell_printf(const char *fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len > 0) {
        meshx_platform_console_write(buf, (uint16_t)len);
    }
}

static void shell_help_cmd(void) {
    meshx_shell_printf("\nAvailable Commands:\n");
    for (int i = 0; i < shell_ctx.cmd_count; i++) {
        meshx_shell_printf("  %-12s : %s\n", shell_ctx.commands[i].command, shell_ctx.commands[i].help);
    }
}

static void shell_process_line(char *line) {
    char *argv[SHELL_MAX_ARGS];
    int argc = 0;

    // Simple tokenizer
    char *token = strtok(line, " \t\n\r");
    while (token != NULL && argc < SHELL_MAX_ARGS) {
        argv[argc++] = token;
        token = strtok(NULL, " \t\n\r");
    }

    if (argc == 0) return;

    if (strcmp(argv[0], "help") == 0 || strcmp(argv[0], "?") == 0) {
        shell_help_cmd();
        return;
    }

    for (int i = 0; i < shell_ctx.cmd_count; i++) {
        if (strcmp(argv[0], shell_ctx.commands[i].command) == 0) {
            int ret = shell_ctx.commands[i].func(argc, argv);
            if (ret != 0) {
                MESHX_LOGE(MODULE_ID_COMMON, "Command %s returned error: %d\n", argv[0], ret);
            }
            return;
        }
    }

    meshx_shell_printf("Unknown command: %s. Type 'help' for available commands.\n", argv[0]);
}

static void shell_task(void *pvParameters) {
    uint8_t c;
    meshx_shell_printf("\n%s", SHELL_PROMPT);

    while (1) {
        if (meshx_platform_console_read(&c, 1) > 0) {
            if (c == '\r' || c == '\n') {
                meshx_shell_printf("\n");
                shell_ctx.line_buffer[shell_ctx.line_ptr] = '\0';
                if (shell_ctx.line_ptr > 0) {
                    shell_process_line(shell_ctx.line_buffer);
                }
                shell_ctx.line_ptr = 0;
                meshx_shell_printf(SHELL_PROMPT);
            } else if (c == '\b' || c == 0x7F) { // Backspace
                if (shell_ctx.line_ptr > 0) {
                    shell_ctx.line_ptr--;
                    meshx_shell_printf("\b \b");
                }
            } else if (shell_ctx.line_ptr < SHELL_MAX_LINE_LEN - 1) {
                if (isprint(c)) {
                    shell_ctx.line_buffer[shell_ctx.line_ptr++] = c;
                    meshx_platform_console_write((const char *)&c, 1);
                }
            }
        }
        meshx_task_delay(10); // Small delay to prevent tight loop
    }
}

meshx_err_t meshx_shell_init(void) {
    if (shell_ctx.initialized) return MESHX_SUCCESS;

    memset(&shell_ctx, 0, sizeof(shell_ctx));
    MESHX_LOGD(MODULE_ID_COMMON, "Initializing MeshX Shell...");

    meshx_err_t err = meshx_platform_console_init();
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "Platform console init failed: %d", err);
        return err;
    }

    shell_ctx.initialized = true;
    MESHX_LOGD(MODULE_ID_COMMON, "MeshX Shell Initialized");
    return MESHX_SUCCESS;
}

meshx_err_t meshx_shell_register_command(const meshx_shell_cmd_t *cmd) {
    if (!shell_ctx.initialized) {
        MESHX_LOGE(MODULE_ID_COMMON, "Shell not initialized, cannot register command '%s'", cmd->command);
        return MESHX_ERR_NOT_INIT;
    }
    if (shell_ctx.cmd_count >= MESHX_SHELL_MAX_COMMANDS) return MESHX_FAIL;

    shell_ctx.commands[shell_ctx.cmd_count++] = *cmd;
    return MESHX_SUCCESS;
}

meshx_err_t meshx_shell_start(void) {
    meshx_task_t task_cfg = {
        .task_cb = shell_task,
        .task_name = "meshx_shell",
        .stack_size = 4096,
        .priority = 4,
        .arg = NULL
    };

    return meshx_task_create(&task_cfg);
}
