// SPDX-License-Identifier: MIT
/**
 * @file test_common.c
 * @brief Routines for error handling etc.
 *
 * @author Jakob Kastelic
 * @copyright Copyright (c) 2025 Stanford Research Systems, Inc.
 */

#include "tests/test_common.h"
#include "utils/debug.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

int test_runner(int (*valid_fn[])(void), int (*invalid_fn[])(void))
{
   // test valid behavior
   debug_silent(false);
   for (int (**fn)(void) = valid_fn; *fn != NULL; fn++)
      if ((*fn)() != 0)
         return -1;

   // test invalid behavor
   debug_silent(true);
   for (int (**fn)(void) = invalid_fn; *fn != NULL; fn++)
      if ((*fn)() != 0)
         return -1;

   // restore error handling
   debug_silent(false);

   return 0;
}

void printout_buffer(const uint32_t *data, const size_t len)
{
   for (size_t i = 0; i < len; i++) {
      printf("   data[%zu] = 0x%x\n", i, data[i]);
   }
}

// end file test_common.c
