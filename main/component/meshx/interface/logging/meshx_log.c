/**
 * @file meshx_log.c
 * @brief Implementation of the MeshX logging system.
 *
 * This file contains the implementation of the logging system for the MeshX
 * application. It provides functions to initialize the logging system, set
 * logging levels for specific modules, and log formatted messages with
 * different log levels and colors.
 */

#include "meshx_log.h"
#include "interface/meshx_platform.h"
#include "module_id.h"
#include "interface/rtos/meshx_rtos_utils.h"
#include "interface/rtos/meshx_msg_q.h"
#include "interface/rtos/meshx_task.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#ifndef CONFIG_MESHX_LOG_PRINTF
#define CONFIG_MESHX_LOG_PRINTF printf
#endif

#ifndef CONFIG_MESHX_LOG_BUF_SIZE
#define CONFIG_MESHX_LOG_BUF_SIZE 128
#endif

#if CONFIG_MESHX_LOG_THREADED
#include "meshx_control_task.h"

typedef struct {
    uint8_t data[CONFIG_MESHX_LOG_BUF_SIZE];
    uint16_t len;
} meshx_log_msg_t;

static meshx_msg_q_t log_msg_q = {
    .max_msg_depth = sizeof(meshx_log_msg_t),  /* Size of one message (bytes) */
    .max_msg_length = CONFIG_MESHX_LOG_QUEUE_LEN /* Max number of messages in queue */
};

static bool log_threaded_init_done = false;

static void meshx_log_task_handler(void *args)
{
    meshx_log_msg_t recv_msg;
    (void)args;

    // CONFIG_MESHX_LOG_PRINTF("[LOG] Log task started\n");

    while (true)
    {
        if (meshx_msg_q_recv(&log_msg_q, &recv_msg, UINT32_MAX) == MESHX_SUCCESS)
        {
            /* Check for sync word 0xDEAD (LE: 0xAD, 0xDE) */
            if (recv_msg.len >= 2 && recv_msg.data[0] == 0xAD && recv_msg.data[1] == 0xDE)
            {
                /* Raw binary output for host decoder */
                meshx_platform_console_write((const char*)recv_msg.data, recv_msg.len);
            }
            else
            {
                /* Fallback for legacy strings if any */
                CONFIG_MESHX_LOG_PRINTF("%s", (char*)recv_msg.data);
            }
        }
    }
}
#endif

static meshx_log_level_t module_log_level[MODULE_ID_MAX];
static meshx_logging_t meshx_logging_ctrl;

/**
 * @brief Calculates XOR parity for a buffer
 */
static uint8_t calculate_xor_parity(const uint8_t *data, uint16_t len)
{
    uint8_t parity = 0;
    for (uint16_t i = 0; i < len; i++)
    {
        parity ^= data[i];
    }
    return parity;
}

meshx_err_t meshx_logging_init(const meshx_logging_t *config)
{
    if (!config)
        return MESHX_INVALID_ARG;

    meshx_logging_ctrl.def_log_level = config->def_log_level;
    for (size_t i = 0; i < MODULE_ID_MAX; i++)
    {
        module_log_level[i] = config->def_log_level;
    }

#if CONFIG_MESHX_LOG_THREADED
    if (!log_threaded_init_done)
    {
        meshx_err_t err = meshx_msg_q_create(&log_msg_q);
        if (err != MESHX_SUCCESS)
            return err;

        static meshx_task_t log_task;
        log_task.task_cb = meshx_log_task_handler;
        log_task.task_name = "meshx_log_task";
        log_task.stack_size = CONFIG_MESHX_LOG_STACK_SIZE;
        log_task.priority = CONFIG_MESHX_LOG_TASK_PRIO;
        log_task.arg = NULL;

        err = meshx_task_create(&log_task);
        if (err != MESHX_SUCCESS)
            return err;

        log_threaded_init_done = true;
    }
#endif

    return MESHX_SUCCESS;
}

void meshx_module_set_log_level(module_id_t module_id, meshx_log_level_t log_level)
{
    if (module_id < MODULE_ID_MAX)
    {
        module_log_level[module_id] = log_level;
    }
}

static void vmeshx_log_packet(module_id_t module_id, meshx_log_level_t log_level,
                              const char *file, int line_no, const char *fmt, va_list args)
{
    /* Validate module ID and log level */
    if (module_id >= MODULE_ID_MAX || log_level > meshx_logging_ctrl.def_log_level ||
        module_log_level[module_id] < log_level)
    {
        return;
    }

#if CONFIG_MESHX_LOG_THREADED
    meshx_log_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    meshx_log_packet_t *pkt = (meshx_log_packet_t *)msg.data;
    msg.len = sizeof(meshx_log_packet_t);
#else
    uint8_t buf[sizeof(meshx_log_packet_t)];
    meshx_log_packet_t *pkt = (meshx_log_packet_t *)buf;
    memset(buf, 0, sizeof(buf));
#endif

    unsigned int timestamp = 0;
    meshx_rtos_get_sys_time(&timestamp);

    pkt->sync = MESHX_LOG_SYNC_WORD;
    pkt->level = (uint8_t)log_level;
    pkt->module_id = (uint8_t)module_id;
    pkt->timestamp = (uint32_t)timestamp;
    pkt->fmt_addr = (uint32_t)fmt;
    pkt->file_addr = (uint32_t)file;
    pkt->line_no = (uint16_t)line_no;

    /* Pack arguments */
    va_list args_copy;
    va_copy(args_copy, args);
    for (int i = 0; i < 16; i++) {
        pkt->args[i] = va_arg(args_copy, uint32_t);
    }
    va_end(args_copy);

    /* Simple scan for %s to support dynamic string inlining */
    const char *p = fmt;
    int arg_idx = 0;
    bool found_s = false;
    while (*p) {
        if (*p == '%' && *(p+1) != '%') {
            p++;
            // Basic skip of common modifiers
            while (*p && (isdigit((int)*p) || strchr("-+ #.0123456789", *p))) p++;
            while (*p && strchr("lhjzL", *p)) p++;
            
            if (*p == 's') {
                found_s = true;
                break;
            }
            arg_idx++;
            if (arg_idx >= 16) break;
        }
        p++;
    }

    if (found_s && arg_idx < 16) {
        uintptr_t str_ptr = (uintptr_t)pkt->args[arg_idx];
        /* 
         * ESP32-C3 Memory Map Check:
         * DRAM: 0x3FC80000 - 0x3FCE0000
         * DROM (Flash strings): 0x3C000000 - 0x3C800000
         */
        bool is_valid_ptr = (str_ptr >= 0x3FC80000 && str_ptr < 0x3FCE0000) || 
                            (str_ptr >= 0x3C000000 && str_ptr < 0x3C800000);

        if (is_valid_ptr) {
            /* Safely copy the string, ensuring null termination */
            strncpy(pkt->inline_str, (const char *)str_ptr, sizeof(pkt->inline_str) - 1);
            pkt->inline_str[sizeof(pkt->inline_str) - 1] = '\0';
        } else {
            /* Not a valid string pointer or out of expected bounds */
            strncpy(pkt->inline_str, "<invalid>", sizeof(pkt->inline_str) - 1);
        }
    }

    pkt->len = sizeof(meshx_log_packet_t) - 3; // Length excluding sync and len

    pkt->parity = calculate_xor_parity((uint8_t *)pkt, sizeof(meshx_log_packet_t) - 1);

#if CONFIG_MESHX_LOG_THREADED
    if (log_threaded_init_done)
    {
        /* Use a small wait to avoid dropping logs if queue is momentarily full */
        if (meshx_msg_q_send(&log_msg_q, &msg, sizeof(msg), 10) == MESHX_SUCCESS) {
            return;
        }
    }
#endif

    /* Fallback: direct output if not threaded or queue full/not ready */
    meshx_platform_console_write((const char *)pkt, sizeof(meshx_log_packet_t));
}

void meshx_log_packet(module_id_t module_id, meshx_log_level_t log_level,
                      const char *file, int line_no, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vmeshx_log_packet(module_id, log_level, file, line_no, fmt, args);
    va_end(args);
}

void meshx_log_printf(module_id_t module_id, meshx_log_level_t log_level,
                      const char *func, int line_no, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vmeshx_log_packet(module_id, log_level, func, line_no, fmt, args);
    va_end(args);
}
