// SPDX-License-Identifier: MIT
/**
 * @file test_reg_virt_check.c
 * @brief Tests for register map representation and handling.
 * @author Jakob Kastelic
 * @copyright Copyright (c) 2025 Stanford Research Systems, Inc.
 */

#include "tests/tcase_reg_virt_check.h"
#include "tests/test_common.h"
#include "tests/test_reg.h"
#include "utils/debug.h"
#include "utils/reg.h"
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static uint32_t dev_data[TCASE_VIRT_MAX_REGS];
static uint64_t virt_data[TCASE_VIRT_MAX_FIELDS];
static struct reg_virt vdev;

static int dev_load_fn(int arg, int id)
{
   (void)arg;
   (void)id;
   return 0;
}

static uint32_t dev_read_fn(int arg, size_t reg)
{
   (void)arg;
   (void)reg;
   return 0;
}

static int dev_write_fn(int arg, size_t reg, uint32_t val)
{
   (void)arg;
   (void)reg;
   (void)val;
   return 0;
}

int test_reg_virt_check(void)
{
   for (size_t i = 0; i < sizeof(mvt) / sizeof(mvt[0]); i++) {
      // do we expect success or failure?
      if (mvt[i].good)
         debug_silent(false);
      else
         debug_silent(true);

      // load the test case
      vdev               = mvt[i].vdev;
      vdev.base.read_fn  = dev_read_fn;
      vdev.base.write_fn = dev_write_fn;
      vdev.base.data     = dev_data;
      vdev.load_fn       = dev_load_fn;
      vdev.data          = virt_data;

      // check for expected result
      const int exp = (mvt[i].good) ? 0 : -1;
      if (reg_verify(&vdev) != exp) {
         TEST_FAIL("failed on case: %s", mvt[i].desc);
         return -1;
      }
   }

   // restore error handling
   debug_silent(false);

   TEST_SUCCESS();
   return 0;
}

// end file test_reg_virt_check.c
