// SPDX-License-Identifier: MIT
/**
 * @file test_reg_desc.c
 * @brief Tests for register map representation and handling.
 * @author Jakob Kastelic
 * @copyright Copyright (c) 2025 Stanford Research Systems, Inc.
 */

#include "tests/test_common.h"
#include "tests/test_reg.h"
#include "utils/reg.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define TEST_NUM_REGS 5U
#define TEST_REG_MAX  ((1U << 9U) - 1U)

static const struct reg_field test_dev_map[] = {
    // name     reg  offs width flags
    {"FIELD_UP", 0, 0, 9, REG_VOLATILE}, // (0 : 0--5) and (1 : 0--2)
    {"X",        1, 3, 3, 0           }, // (1 : 3--5)
    {"Y",        2, 3, 3, 0           }, // (2 : 3--5)
    {"FIELD_DN", 3, 0, 9, REG_DESCEND }, // (2 : 0--2) and (3 : 0--5)
    {"EMPTY",    4, 0, 6, 0           },
    {NULL,       0, 0, 0, 0           }
};

static int test_write_fn(size_t reg, uint32_t val)
{
   (void)reg;
   (void)val;
   return 0;
}

static uint32_t test_read_fn(size_t reg)
{
   (void)reg;
   return 0;
}

static uint32_t test_data[TEST_NUM_REGS];

static struct reg_device test_dev = {
    .reg_width = 6,
    .reg_num   = TEST_NUM_REGS,
    .field_map = test_dev_map,
    .data      = test_data,
    .read_fn   = test_read_fn,
    .write_fn  = test_write_fn,
    .flags     = REG_NOCOMM,
};

static int test_down(const uint32_t up, const uint32_t dn)
{
   if (reg_set(&test_dev, "FIELD_DN", dn)) {
      TEST_FAIL("fail to set FIELD_DN");
      return -1;
   }

   // split both into LSB and MSB
   const uint32_t up_lsb = up & ((1U << 6U) - 1U); // 6 bits
   const uint32_t dn_lsb = dn & ((1U << 6U) - 1U); // 6 bits
   const uint32_t up_msb = up >> 6U;               // 3 bits
   const uint32_t dn_msb = dn >> 6U;               // 3 bits

   // expected data in the buffer
   const uint32_t expect[] = {
       up_lsb, // reg 0
       up_msb, // reg 1
       dn_msb, // reg 2
       dn_lsb, // reg 3
       0x00U,  // reg 4
   };

   // is the underlying representation correct?
   for (size_t i = 0; i < TEST_NUM_REGS; i++) {
      if (test_data[i] != expect[i]) {
         TEST_FAIL("incorrect data in register buffer");
         printf("Testing up=%u, dn=%u:\n", up, dn);
         printf("Expected:\n");
         printout_buffer(expect, TEST_NUM_REGS);
         printf("\nActual:\n");
         printout_buffer(test_data, TEST_NUM_REGS);
         return -1;
      }
   }

   // test readback
   if (reg_get(&test_dev, "FIELD_UP") != up) {
      TEST_FAIL("fail to get FIELD_UP");
      return -1;
   }
   if (reg_get(&test_dev, "FIELD_DN") != dn) {
      TEST_FAIL("fail to get FIELD_DN");
      return -1;
   }

   return 0;
}

static int test_reg_desc_valid(void)
{
   if (reg_check(&test_dev)) {
      TEST_FAIL("device description not accepted");
      return -1;
   }

   for (uint32_t up = 0; up < TEST_REG_MAX; up++) {
      if (reg_set(&test_dev, "FIELD_UP", up)) {
         TEST_FAIL("fail to set FIELD_UP");
         return -1;
      }

      for (uint32_t dn = 0; dn < TEST_REG_MAX; dn++) {
         if (test_down(up, dn)) {
            TEST_FAIL("iteration failed");
            return -1;
         }
      }
   }

   return 0;
}

static int test_value_too_large(void)
{
   const uint64_t i_max = (1U << 9U);

   for (uint64_t i = i_max; i < 3 * i_max; i++)
      if (reg_set(&test_dev, "FIELD_DN", i) == 0) {
         TEST_FAIL("did not detect value that is too large");
         return -1;
      }

   return 0;
}

static int test_detect_overlap(void)
{
   static const struct reg_field map[] = {
       // name     reg  offs width flags
       {"FIELD_UP", 0, 0, 9, 0          }, // (0 : 0--5) and (1 : 0--2)
       {"FIELD_DN", 2, 5, 9, REG_DESCEND}, // (1 : 0--2) and (2 : 0--5)
       {"EMPTY",    3, 0, 6, 0          },
       {NULL,       0, 0, 0, 0          }
   };

   uint32_t data[4] = {0};

   struct reg_device dev = {
       .reg_width = 6,
       .reg_num   = 4,
       .field_map = map,
       .data      = data,
       .read_fn   = test_read_fn,
       .write_fn  = test_write_fn,
   };

   if (reg_check(&dev) == 0) {
      TEST_FAIL("did not detect field overlap");
      return -1;
   }

   return 0;
}

static int test_detect_dupl(void)
{
   static const struct reg_field map[] = {
       // name     reg  offs width flags
       {"FIELD_UP", 0, 0, 6, 0},
       {"FIELD_UP", 1, 0, 6, 0},
       {NULL,       0, 0, 0, 0}
   };

   uint32_t data[4] = {0};

   struct reg_device dev = {
       .reg_width = 6,
       .reg_num   = 2,
       .field_map = map,
       .data      = data,
       .read_fn   = test_read_fn,
       .write_fn  = test_write_fn,
   };

   if (reg_check(&dev) == 0) {
      TEST_FAIL("did not detect field duplicate field names");
      return -1;
   }

   return 0;
}

int test_reg_desc(void)
{
   if (reg_check(&test_dev)) {
      TEST_FAIL("test device considered invalid");
      return -1;
   }

   static int (*valid_fn[])(void) = {test_reg_desc_valid, NULL};

   static int (*invalid_fn[])(void) = {
       test_value_too_large, test_detect_overlap, test_detect_dupl, NULL};

   if (test_runner(valid_fn, invalid_fn) != 0) {
      TEST_FAIL("all tests did not pass");
      return -1;
   }

   TEST_SUCCESS();
   return 0;
}

// end file test_reg_desc.c
