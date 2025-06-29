// SPDX-License-Identifier: MIT
/**
 * @file test_reg_read.c
 * @brief Tests for register map representation and handling.
 * @author Jakob Kastelic
 * @copyright Copyright (c) 2025 Stanford Research Systems, Inc.
 */

#include "tests/test_common.h"
#include "tests/test_reg.h"
#include "utils/reg.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define MAX_REGS 4
#define TEST_ARG (-2000000000)

static uint32_t test_data[MAX_REGS];
static int read_fn_called;
static size_t last_read_reg;
static uint32_t read_return_val;
static int arg_called;

/** @brief Simulated read function for hardware register access. */
static uint32_t mock_read_fn(int arg, size_t reg)
{
   arg_called = arg;
   read_fn_called++;
   last_read_reg = reg;
   return read_return_val;
}

/**
 * @brief Valid call to reg_read() returns buffer content.
 */
static int test_valid_read(void)
{
   struct reg_dev dev = {
       .reg_width = 16,
       .reg_num   = 4,
       .arg       = TEST_ARG,
       .read_fn   = mock_read_fn,
       .data      = test_data,
   };

   memset(test_data, 0, sizeof(test_data));
   read_return_val = 0x1234U;
   read_fn_called  = 0;
   arg_called      = 0;

   uint32_t result = reg_read(&dev, 1);
   if (result != 0x1234U) {
      TEST_FAIL("reg_read did not return correct value from data buffer");
      return -1;
   }

   if (!read_fn_called || last_read_reg != 1) {
      TEST_FAIL("read_fn was not called with expected index");
      return -1;
   }

   if (dev.data[1] != read_return_val) {
      TEST_FAIL("device->data[1] was not updated correctly after reg_read");
      return -1;
   }

   if (arg_called != TEST_ARG) {
      TEST_FAIL("read_fn called with wrong argument");
      return -1;
   }

   return 0;
}

/**
 * @brief Reads register 0 correctly.
 */
static int test_reg_index_zero(void)
{
   struct reg_dev dev = {
       .reg_width = 16,
       .reg_num   = 2,
       .arg       = TEST_ARG,
       .read_fn   = mock_read_fn,
       .data      = test_data,
   };

   memset(test_data, 0, sizeof(test_data));
   read_return_val = 0x1ABCU;
   read_fn_called  = 0;
   arg_called      = 0;

   uint32_t result = reg_read(&dev, 0);
   if (result != 0x1ABCU) {
      TEST_FAIL("reg_read(0) did not return correct buffer value");
      return -1;
   }

   if (!read_fn_called || last_read_reg != 0) {
      TEST_FAIL("read_fn not called or incorrect reg index");
      return -1;
   }

   if (dev.data[0] != read_return_val) {
      TEST_FAIL("device->data[0] was not updated correctly after reg_read");
      return -1;
   }

   if (arg_called != TEST_ARG) {
      TEST_FAIL("read_fn called with wrong argument");
      return -1;
   }

   return 0;
}

/**
 * @brief reg_read accepts full 32-bit value.
 */
static int test_reg_width_32_valid(void)
{
   struct reg_dev dev = {
       .reg_width = 32,
       .reg_num   = 1,
       .arg       = TEST_ARG,
       .read_fn   = mock_read_fn,
       .data      = test_data,
   };

   memset(test_data, 0, sizeof(test_data));
   read_return_val = 0xFFFFFFFFU;
   arg_called      = 0;

   uint32_t result = reg_read(&dev, 0);
   if (result != 0xFFFFFFFFU) {
      TEST_FAIL("reg_read failed for full 32-bit width value");
      return -1;
   }

   if (dev.data[0] != read_return_val) {
      TEST_FAIL("device->data[0] was not updated correctly after reg_read");
      return -1;
   }

   if (arg_called != TEST_ARG) {
      TEST_FAIL("read_fn called with wrong argument");
      return -1;
   }

   return 0;
}

/**
 * @brief reg_read accepts value that fits in 1-bit width.
 */
static int test_reg_width_1_bit_valid(void)
{
   struct reg_dev dev = {
       .reg_width = 1,
       .reg_num   = 1,
       .arg       = TEST_ARG,
       .read_fn   = mock_read_fn,
       .data      = test_data,
   };

   memset(test_data, 0, sizeof(test_data));
   read_return_val = 1U;
   arg_called      = 0;

   if (reg_read(&dev, 0) != 1U) {
      TEST_FAIL("reg_read failed with valid 1-bit value");
      return -1;
   }

   if (dev.data[0] != read_return_val) {
      TEST_FAIL("device->data[0] was not updated correctly after reg_read");
      return -1;
   }

   if (arg_called != TEST_ARG) {
      TEST_FAIL("read_fn called with wrong argument");
      return -1;
   }

   return 0;
}

/**
 * @brief reg_read rejects value that exceeds 1-bit width.
 */
static int test_reg_width_1_bit_invalid(void)
{
   struct reg_dev dev = {
       .reg_width = 1,
       .reg_num   = 1,
       .arg       = TEST_ARG,
       .read_fn   = mock_read_fn,
       .data      = test_data,
   };

   read_return_val = 2U; // invalid (bit 1 set)
   memset(test_data, 0, sizeof(test_data));
   arg_called = 0;

   if (reg_read(&dev, 0) != 0) {
      TEST_FAIL("reg_read accepted value too large for 1-bit width");
      return -1;
   }

   if (dev.data[0] == read_return_val) {
      TEST_FAIL("device->data[0] was incorrectly updated on invalid read");
      return -1;
   }

   if (arg_called != TEST_ARG) {
      TEST_FAIL("read_fn called with wrong argument");
      return -1;
   }

   return 0;
}

/**
 * @brief reg_read accepts max 3-bit value.
 */
static int test_reg_width_3_valid(void)
{
   struct reg_dev dev = {
       .reg_width = 3,
       .reg_num   = 1,
       .arg       = TEST_ARG,
       .read_fn   = mock_read_fn,
       .data      = test_data,
   };

   read_return_val = 0x07U; // max 3 bits
   memset(test_data, 0, sizeof(test_data));
   arg_called = 0;

   if (reg_read(&dev, 0) != 0x07U) {
      TEST_FAIL("reg_read rejected valid 3-bit value");
      return -1;
   }

   if (dev.data[0] != read_return_val) {
      TEST_FAIL("device->data[0] was not updated correctly after reg_read");
      return -1;
   }

   if (arg_called != TEST_ARG) {
      TEST_FAIL("read_fn called with wrong argument");
      return -1;
   }

   return 0;
}

/**
 * @brief reg_read rejects value too large for 3-bit width.
 */
static int test_reg_width_3_invalid(void)
{
   struct reg_dev dev = {
       .reg_width = 3,
       .reg_num   = 1,
       .arg       = TEST_ARG,
       .read_fn   = mock_read_fn,
       .data      = test_data,
   };

   read_return_val = 0x08U; // invalid (bit 3 set)
   memset(test_data, 0, sizeof(test_data));
   arg_called = 0;

   if (reg_read(&dev, 0) != 0) {
      TEST_FAIL("reg_read accepted 4-bit value for 3-bit width");
      return -1;
   }

   if (dev.data[0] == read_return_val) {
      TEST_FAIL("device->data[0] was incorrectly updated on invalid read");
      return -1;
   }

   if (arg_called != TEST_ARG) {
      TEST_FAIL("read_fn called with wrong argument");
      return -1;
   }

   return 0;
}

/**
 * @brief reg_read accepts full 17-bit value.
 */
static int test_reg_width_17_valid(void)
{
   struct reg_dev dev = {
       .reg_width = 17,
       .reg_num   = 1,
       .arg       = TEST_ARG,
       .read_fn   = mock_read_fn,
       .data      = test_data,
   };

   read_return_val = 0x1FFFFU;
   memset(test_data, 0, sizeof(test_data));
   arg_called = 0;

   if (reg_read(&dev, 0) != 0x1FFFFU) {
      TEST_FAIL("reg_read rejected valid 17-bit value");
      return -1;
   }

   if (dev.data[0] != read_return_val) {
      TEST_FAIL("device->data[0] was not updated correctly after reg_read");
      return -1;
   }

   if (arg_called != TEST_ARG) {
      TEST_FAIL("read_fn called with wrong argument");
      return -1;
   }

   return 0;
}

/**
 * @brief reg_read rejects value too large for 17-bit width.
 */
static int test_reg_width_17_invalid(void)
{
   struct reg_dev dev = {
       .reg_width = 17,
       .reg_num   = 1,
       .arg       = TEST_ARG,
       .read_fn   = mock_read_fn,
       .data      = test_data,
   };

   read_return_val = 0x20000U; // invalid (bit 17 set)
   memset(test_data, 0, sizeof(test_data));
   arg_called = 0;

   if (reg_read(&dev, 0) != 0) {
      TEST_FAIL("reg_read accepted 18-bit value for 17-bit width");
      return -1;
   }

   if (dev.data[0] == read_return_val) {
      TEST_FAIL("device->data[0] was incorrectly updated on invalid read");
      return -1;
   }

   if (arg_called != TEST_ARG) {
      TEST_FAIL("read_fn called with wrong argument");
      return -1;
   }

   return 0;
}

/**
 * @brief reg_read rejects null device or missing read_fn.
 */
static int test_null_device_or_fn(void)
{
   struct reg_dev dev = {
       .reg_width = 16, .reg_num = 4, .data = test_data, .read_fn = NULL};

   if (reg_read(NULL, 0) != 0) {
      TEST_FAIL("reg_read(NULL) did not return 0");
      return -1;
   }

   if (reg_read(&dev, 0) != 0) {
      TEST_FAIL("reg_read with NULL read_fn did not return 0");
      return -1;
   }

   return 0;
}

/**
 * @brief reg_read rejects zero reg_width.
 */
static int test_zero_width(void)
{
   struct reg_dev dev = {
       .reg_width = 0,
       .reg_num   = 4,
       .arg       = TEST_ARG,
       .read_fn   = mock_read_fn,
       .data      = test_data,
   };

   if (reg_read(&dev, 0) != 0) {
      TEST_FAIL("reg_read accepted zero reg_width");
      return -1;
   }

   return 0;
}

/**
 * @brief reg_read rejects out-of-range register number.
 */
static int test_reg_out_of_range(void)
{
   struct reg_dev dev = {
       .reg_width = 16,
       .reg_num   = 4,
       .arg       = TEST_ARG,
       .read_fn   = mock_read_fn,
       .data      = test_data,
   };

   if (reg_read(&dev, 5) != 0) {
      TEST_FAIL("reg_read accepted out-of-range register index");
      return -1;
   }

   return 0;
}

/**
 * @brief reg_read rejects reg == reg_num.
 */
static int test_reg_equal_regnum(void)
{
   struct reg_dev dev = {
       .reg_width = 16,
       .reg_num   = 4,
       .arg       = TEST_ARG,
       .read_fn   = mock_read_fn,
       .data      = test_data,
   };

   if (reg_read(&dev, 4) != 0) {
      TEST_FAIL("reg_read accepted reg == reg_num");
      return -1;
   }

   return 0;
}

/**
 * @brief reg_read rejects read_fn value with too many bits set.
 */
static int test_read_fn_too_many_bits(void)
{
   struct reg_dev dev = {
       .reg_width = 12,
       .reg_num   = 2,
       .arg       = TEST_ARG,
       .read_fn   = mock_read_fn,
       .data      = test_data,
   };

   read_return_val = 0x1FFFU; // 13 bits set, invalid for 12-bit width
   memset(test_data, 0, sizeof(test_data));

   if (reg_read(&dev, 0) != 0) {
      TEST_FAIL("reg_read accepted too many bits for 12-bit width");
      return -1;
   }

   if (dev.data[0] == read_return_val) {
      TEST_FAIL("device->data[0] was incorrectly updated on invalid read");
      return -1;
   }

   return 0;
}

/**
 * @brief reg_read accepts highest allowed bit set at 4-bit width.
 */
static int test_read_fn_edge_bit(void)
{
   struct reg_dev dev = {
       .reg_width = 4,
       .reg_num   = 1,
       .arg       = TEST_ARG,
       .read_fn   = mock_read_fn,
       .data      = test_data,
   };

   read_return_val = 0x8U;
   memset(test_data, 0, sizeof(test_data));
   arg_called = 0;

   if (reg_read(&dev, 0) != 0x8U) {
      TEST_FAIL("reg_read failed with high bit set at 4-bit width");
      return -1;
   }

   if (dev.data[0] != read_return_val) {
      TEST_FAIL("device->data[0] was not updated correctly after reg_read");
      return -1;
   }

   if (arg_called != TEST_ARG) {
      TEST_FAIL("read_fn called with wrong argument");
      return -1;
   }

   return 0;
}

int test_reg_read(void)
{
   static int (*valid_fn[])(void) = {
       test_valid_read,         test_reg_index_zero,
       test_reg_width_32_valid, test_reg_width_1_bit_valid,
       test_reg_width_3_valid,  test_reg_width_17_valid,
       test_read_fn_edge_bit,   NULL};

   static int (*invalid_fn[])(void) = {test_null_device_or_fn,
                                       test_zero_width,
                                       test_reg_out_of_range,
                                       test_reg_equal_regnum,
                                       test_reg_width_1_bit_invalid,
                                       test_reg_width_3_invalid,
                                       test_reg_width_17_invalid,
                                       test_read_fn_too_many_bits,
                                       NULL};

   if (test_runner(valid_fn, invalid_fn) != 0)
      return -1;

   TEST_SUCCESS();
   return 0;
}

// end file test_reg_read.c
