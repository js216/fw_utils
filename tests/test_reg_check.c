// SPDX-License-Identifier: MIT
/**
 * @file test_reg_check.c
 * @brief Tests for register map representation and handling.
 * @author Jakob Kastelic
 * @copyright Copyright (c) 2025 Stanford Research Systems, Inc.
 */

#include "tcase_reg_check.h"
#include "tests/test_common.h"
#include "tests/test_reg.h"
#include "utils/debug.h"
#include "utils/reg.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static uint32_t data[TCASE_CHECK_REG_NUM];

static uint32_t test_read_fn(int arg, size_t reg)
{
   (void)arg;
   (void)reg;
   return 0;
}

static int test_write_fn(int arg, size_t reg, uint32_t val)
{
   (void)arg;
   (void)reg;
   (void)val;
   return 0;
}

static int test_reg_fwidth(struct reg_dev *dev)
{
   // skip missing maps
   if (!dev->field_map)
      return 0;

   const struct reg_field *f = &dev->field_map[0];
   while (f && f->name) {
      // skip underscore fields, since they can all have the same name
      if (f->name[0] != '_')
         if (reg_fwidth(dev, f->name) != f->width) {
            TEST_FAIL("Width mismatch for field %s: %d != %d", f->name,
                      reg_fwidth(dev, f->name), f->width);
            return -1;
         }
      f++;
   }
   return 0;
}

static int test_reg_check_map(const struct map_test *mt, size_t len)
{
   for (size_t i = 0; i < len; i++) {
      if (!mt || !mt->desc) {
         TEST_FAIL("NULL pointers passed int struct map_test");
         return -1;
      }

      if (mt[i].expect_ok)
         debug_silent(false);
      else
         debug_silent(true);

      const struct reg_dev *dev = &mt[i].dev;

      // patch in pointers, unless null test
      struct reg_dev patched = *dev;
      if (mt[i].desc[0] != 'n') { // crude check for "null device pointer"
         patched.field_map = mt[i].map;
         patched.data      = data;
         patched.read_fn   = test_read_fn;
         patched.write_fn  = test_write_fn;
      }

      // test reg_fwidth()
      if (test_reg_fwidth(&patched)) {
         TEST_FAIL("error in testing fwidth");
         return -1;
      }

      // run reg_check()
      int ret = reg_check(mt[i].desc[0] == 'n' ? NULL : &patched);
      if ((ret == 0) != mt[i].expect_ok) {
         TEST_FAIL("case %zu: %s", i, mt[i].desc);
         return -1;
      }
   }

   return 0;
}

int test_reg_check(void)
{
   if (test_reg_check_map(mf, sizeof(mf) / sizeof(mf[0])))
      return -1;

   TEST_SUCCESS();
   return 0;
}

// end file test_reg_check.c
