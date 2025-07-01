// SPDX-License-Identifier: MIT
/**
 * @file test_reg_virt.c
 * @brief Tests for register map representation and handling.
 * @author Jakob Kastelic
 * @copyright Copyright (c) 2025 Stanford Research Systems, Inc.
 */

#include "tests/tcase_reg_virt.h"
#include "tests/test_common.h"
#include "tests/test_reg.h"
#include "utils/reg.h"
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

uint32_t dev_data[TCASE_VIRT_MAX_REGS];
static uint64_t virt_data[TCASE_VIRT_MAX_FIELDS];
struct reg_virt vdev;

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

static void fill_out(const struct reg_virt vdev_template)
{
   vdev               = vdev_template;
   vdev.base.read_fn  = dev_read_fn;
   vdev.base.write_fn = dev_write_fn;
   vdev.base.data     = dev_data;
   vdev.load_fn       = dev_load_fn;
   vdev.data          = virt_data;
}

int test_check(const struct map_virt_test *const mvt)
{
   fill_out(mvt->vdev);

   if (reg_verify(&vdev)) {
      TEST_FAIL("virtual device failed verification");
      return -1;
   }

   return 0;
}

int test_access(const struct map_virt_test *const mvt)
{
   fill_out(mvt->vdev);

   // TODO

   return 0;
}

int test_reg_virt(void)
{
   for (size_t i = 0; i < sizeof(mvt) / sizeof(mvt[0]); i++) {
      if (test_check(&mvt[i]) || test_access(&mvt[i])) {
         TEST_FAIL("failed on case: %s", mvt[i].desc);
         return -1;
      }
   }

   TEST_SUCCESS();
   return 0;
}

// end file test_reg_virt.c
