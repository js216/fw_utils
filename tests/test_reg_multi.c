// SPDX-License-Identifier: MIT
/**
 * @file test_reg_multi.c
 * @brief Tests for register map representation and handling.
 * @author Jakob Kastelic
 * @copyright Copyright (c) 2025 Stanford Research Systems, Inc.
 */

#include "tests/test_common.h"
#include "tests/test_reg.h"
#include "utils/reg.h"
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define TEST_NUM_REGS 126U

static const struct reg_field test_dev_map[] = {
    // name              reg off wd  flags
    {"POWERDOWN",         0,  0,  1,  0},
    {"RESET",             0,  1,  1,  0},
    {"MUXOUT_LD_SEL",     0,  2,  1,  0},
    {"FCAL_EN",           0,  3,  1,  0},
    {"R0_RES1",           0,  4,  1,  0},
    {"FCAL_LPFD_ADJ",     0,  5,  2,  0},
    {"FCAL_HPFD_ADJ",     0,  7,  2,  0},
    {"OUT_MUTE",          0,  9,  1,  0},
    {"R0_RES2",           0,  10, 1,  0},
    {"ADD_HOLD",          0,  11, 1,  0},
    {"R0_RES3",           0,  12, 2,  0},
    {"VCO_PHASE_SYNC_EN", 0,  14, 1,  0},
    {"RAMP_EN",           0,  15, 1,  0},
    {"R34_RES",           34, 3,  13, 0},
    {"PLL_N_MSB",         34, 0,  3,  0},
    {"PLL_N_LSB",         36, 0,  16, 0},
    {"R37_RES2",          37, 0,  8,  0},
    {"PFD_DLY_SEL",       37, 8,  6,  0},
    {"R37_RES1",          37, 14, 1,  0},
    {"MASH_SEED_EN",      37, 15, 1,  0},
    {"PLL_NUM",           43, 0,  32, 0},
    {NULL,                0,  0,  0,  0}  // sentinel
};

static uint32_t dummy_phys_reg[2];

static int test_write_fn(int arg, size_t reg, uint32_t val)
{
   (void)arg;
   (void)reg;
   (void)val;

   // there are two 16-bit writes for 32-bit fields;
   // so we can keep track of which is which using this logic,
   // sensitive to writing order
   static int i = 1;
   i            = (i + 1) % 2;

   dummy_phys_reg[i] = val;

   //// TODO: remove this
   // printf("%s%zu : 0x%" PRIx32 "\n",
   //       i ? "" : "\n", reg, val);

   return 0;
}

static uint32_t test_read_fn(int arg, size_t reg)
{
   (void)arg;
   (void)reg;
   return 0;
}

static uint32_t test_dev_data[4][TEST_NUM_REGS] = {0};

static struct reg_dev *test_dev(const unsigned int ch)
{
   const uint8_t reg_width = 16;
   const uint16_t flags    = REG_DESCEND | REG_MSR_FIRST;

   static struct reg_dev test_dev_arr[4] = {
       {
        .reg_width = reg_width,
        .reg_num   = TEST_NUM_REGS,
        .field_map = test_dev_map,
        .data      = test_dev_data[0],
        .read_fn   = &test_read_fn,
        .write_fn  = &test_write_fn,
        .flags     = flags,
        },

       {
        .reg_width = reg_width,
        .reg_num   = TEST_NUM_REGS,
        .field_map = test_dev_map,
        .data      = test_dev_data[1],
        .read_fn   = &test_read_fn,
        .write_fn  = &test_write_fn,
        .flags     = flags,
        },

       {
        .reg_width = reg_width,
        .reg_num   = TEST_NUM_REGS,
        .field_map = test_dev_map,
        .data      = test_dev_data[2],
        .read_fn   = &test_read_fn,
        .write_fn  = &test_write_fn,
        .flags     = flags,
        },

       {
        .reg_width = reg_width,
        .reg_num   = TEST_NUM_REGS,
        .field_map = test_dev_map,
        .data      = test_dev_data[3],
        .read_fn   = &test_read_fn,
        .write_fn  = &test_write_fn,
        .flags     = flags,
        },
   };

   if (ch < 4)
      return &(test_dev_arr[ch]);

   // never reached
   TEST_FAIL("invalid device requested");
   return NULL;
}

static int test_init_one(const unsigned int ch, const uint32_t pattern)
{
   // initialize the registers data structure
   if (reg_check(test_dev(ch))) {
      TEST_FAIL("reg_check failed");
      return -1;
   }

   // write test pattern into the register
   for (size_t i = 0; i < TEST_NUM_REGS; i++) {
      if (reg_write(test_dev(ch), i, pattern)) {
         TEST_FAIL("test register write failed at i=%zu", i);
         return -1;
      }

      // disable writing to physical device for the readback
      const uint16_t flags = test_dev(ch)->flags;
      test_dev(ch)->flags |= REG_NOCOMM;

      // readback
      const uint32_t val = reg_read(test_dev(ch), i);
      if (val != pattern) {
         TEST_FAIL("dev %d: register %zu contains 0x%x", ch, i, val);
         return -1;
      }

      // restore original flags
      test_dev(ch)->flags = flags;
   }

   return 0;
}

static int test_init(void)
{
   const uint32_t pattern = 0xFFFFU;

   for (int ch = 0; ch < 4; ch++) {
      if (test_init_one(ch, pattern)) {
         TEST_FAIL("failed to init ch=%d", ch);
         return -1;
      }
   }

   // make sure all registers contain the correct pattern
   for (int ch = 0; ch < 4; ch++) {
      // disable writing to physical device
      // (otherwise reg_read would call phyiscal device read_fn)
      const uint16_t flags = test_dev(ch)->flags;
      test_dev(ch)->flags |= REG_NOCOMM;

      // check values of all registers
      for (size_t i = 0; i < TEST_NUM_REGS; i++) {
         const uint32_t val = reg_read(test_dev(ch), i);
         if (val != pattern) {
            TEST_FAIL("dev %d: register %zu contains 0x%x", ch, i, val);
            return -1;
         }
      }

      // restore original flags
      test_dev(ch)->flags = flags;
   }

   return 0;
}

static int test_rw32(int ch, const uint32_t pattern)
{
   const uint32_t lsb = pattern & 0xFFFFU;
   const uint32_t msb = pattern >> 16U;

   if (reg_set(test_dev(ch), "PLL_NUM", pattern)) {
      TEST_FAIL("reg_set(PLL_NUM) failed");
      return -1;
   }

   const uint32_t r42 = test_dev(ch)->data[42];
   if (r42 != msb) {
      TEST_FAIL("data[42] 0x%" PRIx32 ", should be 0x%" PRIx32, r42, msb);
      return -1;
   }

   const uint32_t r43 = test_dev(ch)->data[43];
   if (r43 != lsb) {
      TEST_FAIL("data[43] 0x%" PRIx32 ", should be 0x%" PRIx32, r43, lsb);
      return -1;
   }

   const uint32_t phi42 = dummy_phys_reg[0];
   if (phi42 != msb) {
      TEST_FAIL("MSB written as 0x%" PRIx32 ", should be 0x%" PRIx32, phi42,
                msb);
      return -1;
   }

   const uint32_t phi43 = dummy_phys_reg[1];
   if (phi43 != lsb) {
      TEST_FAIL("LSB written as 0x%" PRIx32 ", should be 0x%" PRIx32, phi43,
                lsb);
      return -1;
   }

   if (reg_get(test_dev(ch), "PLL_NUM") != pattern) {
      TEST_FAIL("reg_get(foo) returned wrong value");
      return -1;
   }
   return 0;
}

static int test_rw32_patterns(void)
{
   const uint32_t patterns[] = {0xFFFFU,    0xFFFFFU,    0xFFFFFFU,
                                0xFFFFFFFU, 0xFFFFFFFFU, 351562500U,
                                351562500U, 1210937500U};

   for (size_t i = 0; i < sizeof(patterns) / sizeof(patterns[0]); i++) {
      for (int ch = 0; ch < 4; ch++)
         if (test_rw32(ch, patterns[i])) {
            TEST_FAIL("pattern failed: 0x%" PRIx32, patterns[i]);
            return -1;
         }
   }

   return 0;
}

int test_reg_multi(void)
{
   static int (*valid_fn[])(void) = {test_init, test_rw32_patterns, NULL};

   static int (*invalid_fn[])(void) = {NULL};

   if (test_runner(valid_fn, invalid_fn)) {
      TEST_FAIL("all tests did not pass");
      return -1;
   }

   TEST_SUCCESS();
   return 0;
}

// end file test_reg_multi.c
