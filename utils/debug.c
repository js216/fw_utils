// SPDX-License-Identifier: MIT
/**
 * @file debug.c
 * @brief Callback-based error handling.
 *
 * @author Jakob Kastelic
 * @copyright Copyright (c) 2025 Stanford Research Systems, Inc.
 */

#include "utils/debug.h"
#include <stdbool.h>

static bool silence_errors;

static void (*debug_error_cb)(const char *fn, const char *file, int line,
                              const char *msg);

void debug_set_error_cb(void (*cb)(const char *fn, const char *file, int line,
                                   const char *msg))
{
   debug_error_cb = cb;
}

// cppcheck-suppress unusedFunction
void debug_error(const char *fn, const char *file, int line, const char *msg)
{
   if (silence_errors)
      return;

   if (debug_error_cb)
      (*debug_error_cb)(fn, file, line, msg);
}

void debug_silent(bool silent)
{
   silence_errors = silent;
}

// end file debug.c
