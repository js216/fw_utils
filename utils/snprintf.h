// SPDX-License-Identifier: MIT
/**
 * @file snprintf.h
 * @brief Settings for snprintf.c
 * @author Jakob Kastelic
 * @copyright Copyright (c) 2025 Stanford Research Systems, Inc.
 */

#include <limits.h>
#include <stdarg.h>
#include <stddef.h>

#define HAVE_STDARG_H               1
#define HAVE_STDDEF_H               1
#define HAVE_STDINT_H               1
#define HAVE_STDLIB_H               1
#define HAVE_FLOAT_H                1
#define HAVE_UNSIGNED_LONG_LONG_INT 1
#define HAVE_LONG_LONG_INT          1

__attribute__((format(printf, 3, 4))) int rpl_snprintf(char *str, size_t size,
                                                       const char *format, ...);

__attribute__((format(printf, 3, 0))) int
rpl_vsnprintf(char *str, size_t size, const char *format, va_list args);

// end file snprintf.h
