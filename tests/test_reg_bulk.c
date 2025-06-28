// SPDX-License-Identifier: MIT
/**
 * @file test_reg_bulk.c
 * @brief Tests for register map representation and handling.
 * @author Jakob Kastelic
 * @copyright Copyright (c) 2025 Stanford Research Systems, Inc.
 */

#include "tests/test_common.h"
#include "tests/test_reg.h"
#include "utils/reg.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Simple 3-word import and clear.
 */
static int test_reg_bulk_simple(void)
{
   uint32_t initial[3]   = {0x12345678, 0x9abcdef0, 0x0fedcba9};
   uint32_t temp[3]      = {0};
   struct reg_device dev = {.reg_width = 32,
                            .reg_num   = 3,
                            .field_map = NULL,
                            .data      = temp,
                            .write_fn  = NULL};

   if (reg_bulk(&dev, initial) != 0) {
      TEST_FAIL("reg_bulk failed on valid input");
      return -1;
   }

   for (size_t i = 0; i < 3; i++) {
      if (dev.data[i] != initial[i]) {
         TEST_FAIL("reg_bulk did not copy correct values");
         return -1;
      }
   }

   if (reg_bulk(&dev, NULL) != 0) {
      TEST_FAIL("reg_bulk failed on NULL input");
      return -1;
   }

   for (size_t i = 0; i < 3; i++) {
      if (dev.data[i] != 0) {
         TEST_FAIL("reg_bulk did not zero memory");
         return -1;
      }
   }

   return 0;
}

/**
 * @brief reg_bulk with reg_num = 0 should be a no-op.
 */
static int test_reg_bulk_zero_regs(void)
{
   struct reg_device dev = {.reg_width = 32,
                            .reg_num   = 0,
                            .field_map = NULL,
                            .data      = NULL,
                            .write_fn  = NULL};

   if (reg_bulk(&dev, NULL) != 0) {
      TEST_FAIL("reg_bulk failed on zero reg_num");
      return -1;
   }

   return 0;
}

/**
 * @brief reg_bulk with 1000 registers.
 */
static int test_reg_bulk_large(void)
{
   enum { N = 1000 };
   uint32_t initial[N];
   uint32_t buffer[N];

   for (size_t i = 0; i < N; i++) {
      initial[i] = (uint32_t)(i * 3 + 1);
      buffer[i]  = 0;
   }

   struct reg_device dev = {.reg_width = 32,
                            .reg_num   = N,
                            .field_map = NULL,
                            .data      = buffer,
                            .write_fn  = NULL};

   if (reg_bulk(&dev, initial) != 0) {
      TEST_FAIL("reg_bulk failed on large input");
      return -1;
   }

   for (size_t i = 0; i < N; i++) {
      if (dev.data[i] != initial[i]) {
         TEST_FAIL("reg_bulk large mismatch at index");
         return -1;
      }
   }

   return 0;
}

/**
 * @brief reg_bulk with NULL device pointer.
 */
static int test_reg_bulk_null_dev(void)
{
   if (reg_bulk(NULL, NULL) == 0) {
      TEST_FAIL("reg_bulk accepted NULL device");
      return -1;
   }

   return 0;
}

/**
 * @brief reg_bulk with NULL .data pointer in device.
 */
static int test_reg_bulk_null_storage(void)
{
   uint32_t input[2]     = {0xaa, 0xbb};
   struct reg_device dev = {.reg_width = 32,
                            .reg_num   = 2,
                            .field_map = NULL,
                            .data      = NULL,
                            .write_fn  = NULL};

   if (reg_bulk(&dev, input) == 0) {
      TEST_FAIL("reg_bulk accepted NULL dev.data");
      return -1;
   }

   return 0;
}

/**
 * @brief reg_bulk with NULL data and non-NULL .data but zero reg_width.
 */
static int test_reg_bulk_zero_width(void)
{
   uint32_t buf[2]       = {1, 2};
   struct reg_device dev = {.reg_width = 0,
                            .reg_num   = 2,
                            .field_map = NULL,
                            .data      = buf,
                            .write_fn  = NULL};

   // Depending on implementation policy, this may or may not be valid.
   // For now, assume it's invalid.
   if (reg_bulk(&dev, NULL) == 0) {
      TEST_FAIL("reg_bulk accepted zero reg_width");
      return -1;
   }

   return 0;
}

/**
 * @brief reg_bulk with 1 register and NULL input (clears it).
 */
static int test_reg_bulk_single_clear(void)
{
   uint32_t word         = 0xffffffff;
   struct reg_device dev = {.reg_width = 32,
                            .reg_num   = 1,
                            .field_map = NULL,
                            .data      = &word,
                            .write_fn  = NULL};

   if (reg_bulk(&dev, NULL) != 0) {
      TEST_FAIL("reg_bulk failed on single-word NULL input");
      return -1;
   }

   if (word != 0) {
      TEST_FAIL("reg_bulk did not zero the single register");
      return -1;
   }

   return 0;
}

/**
 * @brief reg_bulk with reg_width != 32 (e.g. 24 or 64) should still copy.
 */
static int test_reg_bulk_weird_width(void)
{
   uint32_t src[2]       = {0x11111111, 0x22222222};
   uint32_t dst[2]       = {0};
   struct reg_device dev = {.reg_width =
                                24, // Not 32, but irrelevant for copying
                            .reg_num   = 2,
                            .field_map = NULL,
                            .data      = dst,
                            .write_fn  = NULL};

   if (reg_bulk(&dev, src) != 0) {
      TEST_FAIL("reg_bulk failed on weird reg_width");
      return -1;
   }

   if (dst[0] != src[0] || dst[1] != src[1]) {
      TEST_FAIL("reg_bulk failed to copy on weird reg_width");
      return -1;
   }

   return 0;
}

/**
 * @brief reg_bulk with nonzero reg_num but data = NULL.
 */
static int test_reg_bulk_null_data_storage(void)
{
   struct reg_device dev = {.reg_width = 32,
                            .reg_num   = 2,
                            .field_map = NULL,
                            .data      = NULL,
                            .write_fn  = NULL};

   if (reg_bulk(&dev, NULL) == 0) {
      TEST_FAIL("reg_bulk accepted NULL dev.data with reg_num > 0");
      return -1;
   }

   return 0;
}

/**
 * @brief reg_bulk with huge reg_num but tiny data array.
 *
 * This test is technically UB if memory isn't reserved properly, but included
 * here as a conceptual case.
 */
static int test_reg_bulk_incomplete_input_data(void)
{
   // User is responsible for data size. reg_bulk does not validate bounds.
   // Here we demonstrate intent, but we use same-sized arrays for safety.
   uint32_t src[10];
   uint32_t dst[10];
   struct reg_device dev = {.reg_width = 32,
                            .reg_num   = 10,
                            .field_map = NULL,
                            .data      = dst,
                            .write_fn  = NULL};

   // Only partial data initialized (simulate caller bug)
   for (int i = 0; i < 5; i++)
      src[i] = i + 1;
   for (int i = 5; i < 10; i++)
      src[i] = 0xdeadbeef; // garbage

   if (reg_bulk(&dev, src) != 0) {
      TEST_FAIL("reg_bulk failed on valid-but-bad user input");
      return -1;
   }

   // Should copy whatever is in src blindly
   for (int i = 0; i < 10; i++) {
      if (dst[i] != src[i]) {
         TEST_FAIL("reg_bulk did not copy all elements");
         return -1;
      }
   }

   return 0;
}

int test_reg_bulk(void)
{
   static int (*valid_fn[])() = {
       test_reg_bulk_simple,      test_reg_bulk_zero_regs,
       test_reg_bulk_large,       test_reg_bulk_single_clear,
       test_reg_bulk_weird_width, NULL};

   static int (*invalid_fn[])() = {test_reg_bulk_null_dev,
                                   test_reg_bulk_null_storage,
                                   test_reg_bulk_zero_width,
                                   test_reg_bulk_null_data_storage,
                                   test_reg_bulk_incomplete_input_data,
                                   NULL};

   if (test_runner(valid_fn, invalid_fn) != 0)
      return -1;

   TEST_SUCCESS();
   return 0;
}

// end file test_reg_bulk.c
