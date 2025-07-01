// SPDX-License-Identifier: MIT
/**
 * @file test_reg_get_phy.c
 * @brief Tests for register map representation and handling.
 * @author Jakob Kastelic
 * @copyright Copyright (c) 2025 Stanford Research Systems, Inc.
 */

/**
 * To force re-reading the physical device, all maps in this test suite have all
 * fields set to REG_VOLATILE.
 */

#include "tests/test_common.h"
#include "tests/test_reg.h"
#include "utils/reg.h"
#include <stddef.h>
#include <stdint.h>

static uint32_t mock_read_data[16];

static uint32_t mock_read_fn(int arg, size_t reg)
{
   (void)arg;
   return mock_read_data[reg];
}

static int mock_write_fn(int arg, size_t reg, uint32_t val)
{
   (void)arg;
   (void)reg;
   (void)val;
   return 0;
}

/**
 * @brief test reg_get returns correct values for simple 1-bit and 64-bit
 * fields
 */
static int test_reg_read_basic(void)
{
   static const struct reg_field fields[] = {
       {"bit1",  0, 0, 1,  REG_VOLATILE},
       {"bit64", 0, 0, 64, REG_VOLATILE},
       {NULL,    0, 0, 0,  REG_VOLATILE}
   };

   uint32_t data[3] = {0};

   struct reg_dev dev = {
       .reg_width = 32,
       .reg_num   = 3,
       .field_map = fields,
       .data      = data,
       .read_fn   = mock_read_fn,
       .write_fn  = mock_write_fn,
   };

   mock_read_data[0] = 0xFFFFFFFF;
   mock_read_data[1] = 0xFFFFFFFF;
   mock_read_data[2] = 0x00000000;

   // read "bit1" via reg_get and reg_get
   uint64_t val_read = reg_get(&dev, "bit1");
   if (val_read != 1ULL) {
      TEST_FAIL("reg_get bit1 returned 0x%llx, expected 0x1",
                (unsigned long long)val_read);
      return -1;
   }
   uint64_t val_get = reg_get(&dev, "bit1");
   if (val_get != val_read) {
      TEST_FAIL("reg_get bit1 returned 0x%llx, expected same as reg_get 0x%llx",
                (unsigned long long)val_get, (unsigned long long)val_read);
      return -1;
   }

   // read "bit64" via reg_get and reg_get
   val_read = reg_get(&dev, "bit64");
   if (val_read != UINT64_MAX) {
      TEST_FAIL("reg_get bit64 returned 0x%llx, expected 0x%llx",
                (unsigned long long)val_read, (unsigned long long)UINT64_MAX);
      return -1;
   }
   val_get = reg_get(&dev, "bit64");
   if (val_get != val_read) {
      TEST_FAIL(
          "reg_get bit64 returned 0x%llx, expected same as reg_get 0x%llx",
          (unsigned long long)val_get, (unsigned long long)val_read);
      return -1;
   }

   // verify that data[] was updated with the mock reads
   if (data[0] != 0xFFFFFFFF || data[1] != 0xFFFFFFFF ||
       data[2] != 0x00000000) {
      TEST_FAIL("reg_get did not update device data correctly");
      return -1;
   }

   return 0;
}

/**
 * @brief test reg_get for fields crossing register boundaries
 */
static int test_reg_read_cross_boundary(void)
{
   static const struct reg_field fields[] = {
       {"cross17", 0, 31, 17, REG_VOLATILE},
       {NULL,      0, 0,  0,  REG_VOLATILE}
   };

   uint32_t data[3] = {0};

   struct reg_dev dev = {
       .reg_width = 32,
       .reg_num   = 3,
       .field_map = fields,
       .data      = data,
       .read_fn   = mock_read_fn,
       .write_fn  = mock_write_fn,
   };

   mock_read_data[0] = 0x80000000; // bit 31 set
   mock_read_data[1] = 0x00000001; // bit 32 set (lowest bit of reg 1)
   mock_read_data[2] = 0x00000000;

   uint64_t val_read = reg_get(&dev, "cross17");
   if (val_read != 0x3ULL) {
      TEST_FAIL("reg_get cross17 returned 0x%llx, expected 0x10001",
                (unsigned long long)val_read);
      return -1;
   }

   uint64_t val_get = reg_get(&dev, "cross17");
   if (val_get != val_read) {
      TEST_FAIL("reg_get cross17 returned 0x%llx, expected same as reg_get "
                "0x%llx",
                (unsigned long long)val_get, (unsigned long long)val_read);
      return -1;
   }

   // verify cached data updated correctly
   if (data[0] != 0x80000000 || data[1] != 0x00000001 ||
       data[2] != 0x00000000) {
      TEST_FAIL(
          "reg_get did not update device data correctly on cross boundary");
      return -1;
   }

   return 0;
}

/**
 * @brief test reg_get returns 0 and error on invalid field name
 */
static int test_reg_read_invalid_field(void)
{
   static const struct reg_field fields[] = {
       {"bit4", 0, 0, 4, REG_VOLATILE},
       {NULL,   0, 0, 0, REG_VOLATILE}
   };

   uint32_t data[1] = {0};

   struct reg_dev dev = {
       .reg_width = 32,
       .reg_num   = 1,
       .field_map = fields,
       .data      = data,
       .read_fn   = mock_read_fn,
       .write_fn  = mock_write_fn,
   };

   uint64_t val_read = reg_get(&dev, "nonexistent");
   if (val_read != 0) {
      TEST_FAIL("reg_get returned non-zero for invalid field");
      return -1;
   }

   uint64_t val_get = reg_get(&dev, "nonexistent");
   if (val_get != 0) {
      TEST_FAIL("reg_get returned non-zero for invalid field");
      return -1;
   }

   return 0;
}

/**
 * @brief test reg_get returns 0 and error on NULL device pointer
 */
static int test_reg_read_null_device(void)
{
   uint64_t val_read = reg_get(NULL, "bit4");
   if (val_read != 0) {
      TEST_FAIL("reg_get returned non-zero for NULL device");
      return -1;
   }

   uint64_t val_get = reg_get(NULL, "bit4");
   if (val_get != 0) {
      TEST_FAIL("reg_get returned non-zero for NULL device");
      return -1;
   }

   return 0;
}

/**
 * @brief test reg_get returns 0 and error on NULL field pointer
 */
static int test_reg_read_null_field(void)
{
   static const struct reg_field fields[] = {
       {"bit4", 0, 0, 4, REG_VOLATILE},
       {NULL,   0, 0, 0, REG_VOLATILE}
   };

   uint32_t data[1] = {0};

   struct reg_dev dev = {
       .reg_width = 32,
       .reg_num   = 1,
       .field_map = fields,
       .data      = data,
       .read_fn   = mock_read_fn,
       .write_fn  = mock_write_fn,
   };

   uint64_t val_read = reg_get(&dev, NULL);
   if (val_read != 0) {
      TEST_FAIL("reg_get returned non-zero for NULL field");
      return -1;
   }

   uint64_t val_get = reg_get(&dev, NULL);
   if (val_get != 0) {
      TEST_FAIL("reg_get returned non-zero for NULL field");
      return -1;
   }

   return 0;
}

/**
 * @brief test reg_get returns 0 and error on missing read_fn pointer
 */
static int test_reg_read_missing_read_fn(void)
{
   static const struct reg_field fields[] = {
       {"bit4", 0, 0, 4, REG_VOLATILE},
       {NULL,   0, 0, 0, REG_VOLATILE}
   };

   uint32_t data[1] = {0};

   struct reg_dev dev = {
       .reg_width = 32,
       .reg_num   = 1,
       .field_map = fields,
       .data      = data,
       .read_fn   = NULL,
       .write_fn  = mock_write_fn,
   };

   uint64_t val_read = reg_get(&dev, "bit4");
   if (val_read != 0) {
      TEST_FAIL("reg_get returned non-zero with NULL read_fn");
      return -1;
   }

   uint64_t val_get = reg_get(&dev, "bit4");
   if (val_get != 0) {
      TEST_FAIL("reg_get returned non-zero for missing read_fn case");
      return -1;
   }

   return 0;
}

int test_reg_get_phy(void)
{
   static int (*valid_fn[])(void) = {test_reg_read_cross_boundary,
                                     test_reg_read_basic, NULL};

   static int (*invalid_fn[])(void) = {
       test_reg_read_invalid_field, test_reg_read_null_device,
       test_reg_read_null_field, test_reg_read_missing_read_fn, NULL};

   if (test_runner(valid_fn, invalid_fn) != 0)
      return -1;

   TEST_SUCCESS();
   return 0;
}

// end file test_reg_get_phy.c
