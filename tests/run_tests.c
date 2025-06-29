// SPDX-License-Identifier: MIT
/**
 * @file run_tests.c
 * @brief Run all host-side tests.
 *
 * NOT intended for running on the embedded target.
 *
 * @author Jakob Kastelic
 * @copyright Copyright (c) 2025 Stanford Research Systems, Inc.
 */

#include "tests/test_reg.h"
#include "utils/debug.h"
#include <stdio.h>

static void print_error(const char *fn, const char *fi, int l, const char *m)
{
   printf("\033[1;31merror:\033[0m %s in %s (line %d): %s\r\n", fn, fi, l, m);
}

int main(void)
{
   debug_set_error_cb(print_error);

   int ret = 0;

   // reg.c
   ret = ret || test_reg_check();
   ret = ret || test_reg_read();
   ret = ret || test_reg_write();
   ret = ret || test_reg_bulk();
   ret = ret || test_reg_get_set();
   ret = ret || test_reg_get_phy();
   ret = ret || test_reg_desc();
   ret = ret || test_reg_multi();

   return ret;
}

// end file run_tests.c
