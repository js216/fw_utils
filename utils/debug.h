// SPDX-License-Identifier: MIT
/**
 * @file debug.h
 * @brief Callback-based error handling.
 *
 * @author Jakob Kastelic
 * @copyright Copyright (c) 2025 Stanford Research Systems, Inc.
 */

#ifndef DEBUG_H
#define DEBUG_H

#include <stdbool.h>

#define ERROR(x) debug_error(__func__, __FILE__, __LINE__, x) // no semicolon!

/**
 * @brief Set error callback function.
 * @param cb Error callback to use from now on.
 */
void debug_set_error_cb(void (*cb)(const char *fn, const char *file, int line,
                                   const char *msg));

/**
 * @brief Call the error callback function.
 * @param fn String to print (typically function name).
 * @param file String to print (typically file name).
 * @param line Number to print (typically line number).
 * @param msg String to print (typically descriptive message).
 */
void debug_error(const char *fn, const char *file, int line, const char *msg);

/**
 * @brief Suppress debugging/error messages (or not).
 * @param silent If true, suppress messages; if false, enable them.
 */
void debug_silent(bool silent);

#endif // DEBUG_H

// end file debug.h
