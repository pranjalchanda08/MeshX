/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_tiny_printf.h
 * @brief Lightweight printf implementation to reduce flash footprint.
 */

#ifndef MESHX_TINY_PRINTF_H
#define MESHX_TINY_PRINTF_H

#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Lightweight vsnprintf replacement.
 * Supports: %s, %d, %u, %x, %X, %p, %c, %%.
 * Supports padding with '0' and width specifier.
 */
int meshx_tiny_vsnprintf(char *buf, size_t size, const char *fmt, va_list args);

/**
 * @brief Lightweight snprintf replacement.
 */
int meshx_tiny_snprintf(char *buf, size_t size, const char *fmt, ...);

/**
 * @brief Lightweight printf that writes directly to console.
 */
int meshx_tiny_printf(const char *fmt, ...);
int meshx_tiny_vprintf(const char *fmt, va_list args);

#ifdef __cplusplus
}
#endif

#endif // MESHX_TINY_PRINTF_H
