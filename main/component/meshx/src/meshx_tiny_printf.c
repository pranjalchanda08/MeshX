/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_tiny_printf.c
 * @brief Lightweight printf implementation.
 */

#include "interface/utils/meshx_tiny_printf.h"
#include "interface/meshx_platform.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

static void out_char(char **buf, size_t *size, char c) {
    if (*size > 1) {
        **buf = c;
        (*buf)++;
        (*size)--;
    }
}

static void out_str(char **buf, size_t *size, const char *s, int width, bool left_align) {
    if (!s) s = "(null)";
    int len = (int)strlen(s);
    int pad = width > len ? width - len : 0;

    if (!left_align) {
        while (pad-- > 0) out_char(buf, size, ' ');
    }

    while (*s) {
        out_char(buf, size, *s++);
    }

    if (left_align) {
        while (pad-- > 0) out_char(buf, size, ' ');
    }
}

static void out_uint(char **buf, size_t *size, uint32_t val, int base, int width, char pad, bool upper) {
    char tmp[11];
    int i = 0;
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";

    if (val == 0) {
        tmp[i++] = '0';
    } else {
        while (val > 0) {
            tmp[i++] = digits[val % base];
            val /= base;
        }
    }

    while (i < width) {
        out_char(buf, size, pad);
        width--;
    }

    while (i > 0) {
        out_char(buf, size, tmp[--i]);
    }
}

static void out_int(char **buf, size_t *size, int32_t val, int base, int width, char pad) {
    if (val < 0) {
        out_char(buf, size, '-');
        val = -val;
        if (width > 0) width--;
    }
    out_uint(buf, size, (uint32_t)val, base, width, pad, false);
}

int meshx_tiny_vsnprintf(char *buf, size_t size, const char *fmt, va_list args) {
    char *start = buf;
    size_t initial_size = size;

    if (size == 0) return 0;

    while (*fmt && size > 1) {
        if (*fmt == '%') {
            fmt++;
            bool left_align = false;
            if (*fmt == '-') {
                left_align = true;
                fmt++;
            }
            char pad = ' ';
            int width = 0;
            if (*fmt == '0') {
                pad = '0';
                fmt++;
            }
            while (isdigit((int)*fmt)) {
                width = width * 10 + (*fmt - '0');
                fmt++;
            }

            // Handle long modifiers (e.g., %ld, %lu, %lx)
            while (*fmt == 'l') fmt++; 

            switch (*fmt) {
                case 's':
                    out_str(&buf, &size, va_arg(args, const char *), width, left_align);
                    break;
                case 'd':
                case 'i':
                    out_int(&buf, &size, va_arg(args, int32_t), 10, width, pad);
                    break;
                case 'u':
                    out_uint(&buf, &size, va_arg(args, uint32_t), 10, width, pad, false);
                    break;
                case 'x':
                    out_uint(&buf, &size, va_arg(args, uint32_t), 16, width, pad, false);
                    break;
                case 'X':
                    out_uint(&buf, &size, va_arg(args, uint32_t), 16, width, pad, true);
                    break;
                case 'p':
                    out_str(&buf, &size, "0x", 0, false);
                    out_uint(&buf, &size, (uint32_t)(uintptr_t)va_arg(args, void *), 16, 8, '0', false);
                    break;
                case 'c':
                    out_char(&buf, &size, (char)va_arg(args, int));
                    break;
                case '%':
                    out_char(&buf, &size, '%');
                    break;
                default:
                    out_char(&buf, &size, '%');
                    out_char(&buf, &size, *fmt);
                    break;
            }
        } else {
            out_char(&buf, &size, *fmt);
        }
        fmt++;
    }

    if (size > 0) {
        *buf = '\0';
    } else if (initial_size > 0) {
        start[initial_size - 1] = '\0';
    }

    return (int)(buf - start);
}

int meshx_tiny_snprintf(char *buf, size_t size, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int res = meshx_tiny_vsnprintf(buf, size, fmt, args);
    va_end(args);
    return res;
}

int meshx_tiny_vprintf(const char *fmt, va_list args) {
    char buf[256];
    int res = meshx_tiny_vsnprintf(buf, sizeof(buf), fmt, args);
    if (res > 0) {
        meshx_platform_console_write(buf, (uint16_t)res);
    }
    return res;
}

int meshx_tiny_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int res = meshx_tiny_vprintf(fmt, args);
    va_end(args);
    return res;
}
