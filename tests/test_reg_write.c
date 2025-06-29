// SPDX-License-Identifier: MIT
/**
 * @file test_reg.c
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

static uint32_t write_data[4]; // backing register array

static int mock_update_fn(int arg, size_t reg, uint32_t val) {
  (void)arg;
  (void)reg;
  (void)val;
  return 0;
}

/**
 * @brief Import valid register and verify result.
 *
 * Imports a value into register 2 and checks that the value appears.
 * No update function is triggered by reg_write.
 */
static int test_reg_write_valid(void) {
  struct reg_dev dev = {
      .reg_width = 32,
      .reg_num = 4,
      .field_map = NULL,
      .data = write_data,
      .write_fn = mock_update_fn,
  };

  memset(write_data, 0x55, sizeof(write_data));

  if (reg_write(&dev, 2, 0xaabbccdd) != 0) {
    TEST_FAIL("valid write failed");
    return -1;
  }

  if (write_data[2] != 0xaabbccdd) {
    TEST_FAIL("value not written to register");
    return -1;
  }

  return 0;
}

/**
 * @brief Try to write into a NULL device.
 */
static int test_reg_write_null_device(void) {
  if (reg_write(NULL, 0, 0x12345678) != -1) {
    TEST_FAIL("null device not rejected");
    return -1;
  }

  return 0;
}

/**
 * @brief Try to write with NULL data pointer.
 */
static int test_reg_write_null_data(void) {
  struct reg_dev dev = {
      .reg_width = 32,
      .reg_num = 2,
      .field_map = NULL,
      .data = NULL,
      .write_fn = mock_update_fn,
  };

  if (reg_write(&dev, 1, 0xdeadbeef) != -1) {
    TEST_FAIL("null data pointer not rejected");
    return -1;
  }

  return 0;
}

/**
 * @brief Try to write with zero-width registers.
 */
static int test_reg_write_zero_width(void) {
  struct reg_dev dev = {
      .reg_width = 0,
      .reg_num = 4,
      .field_map = NULL,
      .data = write_data,
      .write_fn = mock_update_fn,
  };

  if (reg_write(&dev, 1, 0xdeadbeef) != -1) {
    TEST_FAIL("zero-width register not rejected");
    return -1;
  }

  return 0;
}

/**
 * @brief Try to write into out-of-bounds register.
 */
static int test_reg_write_oob_register(void) {
  struct reg_dev dev = {
      .reg_width = 32,
      .reg_num = 3,
      .field_map = NULL,
      .data = write_data,
      .write_fn = mock_update_fn,
  };

  if (reg_write(&dev, 3, 0xdeadbeef) != -1) {
    TEST_FAIL("out-of-bounds register accepted");
    return -1;
  }

  return 0;
}

/**
 * @brief Import value into register 0 and verify it's written correctly.
 */
static int test_reg_write_first_register(void) {
  struct reg_dev dev = {
      .reg_width = 32,
      .reg_num = 4,
      .field_map = NULL,
      .data = write_data,
      .write_fn = mock_update_fn,
  };

  memset(write_data, 0, sizeof(write_data));

  if (reg_write(&dev, 0, 0x01020304) != 0) {
    TEST_FAIL("write into first register failed");
    return -1;
  }

  if (write_data[0] != 0x01020304) {
    TEST_FAIL("value not written to first register");
    return -1;
  }

  return 0;
}

/**
 * @brief Import value into last valid register and verify write.
 */
static int test_reg_write_last_register(void) {
  struct reg_dev dev = {
      .reg_width = 32,
      .reg_num = 4,
      .field_map = NULL,
      .data = write_data,
      .write_fn = mock_update_fn,
  };

  memset(write_data, 0, sizeof(write_data));

  if (reg_write(&dev, 3, 0xfeedface) != 0) {
    TEST_FAIL("write into last register failed");
    return -1;
  }

  if (write_data[3] != 0xfeedface) {
    TEST_FAIL("value not written to last register");
    return -1;
  }

  return 0;
}

/**
 * @brief Import a zero value and verify register is cleared.
 */
static int test_reg_write_zero_value(void) {
  struct reg_dev dev = {
      .reg_width = 32,
      .reg_num = 2,
      .field_map = NULL,
      .data = write_data,
      .write_fn = mock_update_fn,
  };

  memset(write_data, 0xFF, sizeof(write_data));

  if (reg_write(&dev, 1, 0x00000000) != 0) {
    TEST_FAIL("zero value write failed");
    return -1;
  }

  if (write_data[1] != 0x00000000) {
    TEST_FAIL("register not cleared to zero");
    return -1;
  }

  return 0;
}

/**
 * @brief Import all-ones value and verify it is written.
 */
static int test_reg_write_max_value(void) {
  struct reg_dev dev = {
      .reg_width = 32,
      .reg_num = 4,
      .field_map = NULL,
      .data = write_data,
      .write_fn = mock_update_fn,
  };

  memset(write_data, 0, sizeof(write_data));

  if (reg_write(&dev, 2, 0xFFFFFFFF) != 0) {
    TEST_FAIL("max value write failed");
    return -1;
  }

  if (write_data[2] != 0xFFFFFFFF) {
    TEST_FAIL("value not written to register");
    return -1;
  }

  return 0;
}

/**
 * @brief Try to write with reg = SIZE_MAX (wraparound index).
 */
static int test_reg_write_size_max_index(void) {
  struct reg_dev dev = {
      .reg_width = 32,
      .reg_num = 4,
      .field_map = NULL,
      .data = write_data,
      .write_fn = mock_update_fn,
  };

  if (reg_write(&dev, (size_t)-1, 0x12345678) != -1) {
    TEST_FAIL("SIZE_MAX register index not rejected");
    return -1;
  }

  return 0;
}

/**
 * @brief Try to write when device has no registers.
 */
static int test_reg_write_zero_registers(void) {
  struct reg_dev dev = {
      .reg_width = 32,
      .reg_num = 0,
      .field_map = NULL,
      .data = write_data,
      .write_fn = mock_update_fn,
  };

  if (reg_write(&dev, 0, 0x12345678) != -1) {
    TEST_FAIL("zero register device not rejected");
    return -1;
  }

  return 0;
}

/**
 * @brief Simple 8-bit register test.
 */
static int test_reg_write_value_too_large_8bit_val(void) {
  uint32_t data[1] = {0};
  struct reg_dev dev = {
      .reg_width = 8,
      .reg_num = 1,
      .field_map = NULL,
      .data = data,
      .write_fn = mock_update_fn,
  };

  // Value fits in 8 bits -> accepted
  if (reg_write(&dev, 0, 0x7F) != 0) {
    TEST_FAIL("valid 8-bit value rejected");
    return -1;
  }

  return 0;
}

/**
 * @brief Reject value too large for 8-bit register.
 */
static int test_reg_write_value_too_large_8bit_inv(void) {
  uint32_t data[1] = {0};
  struct reg_dev dev = {
      .reg_width = 8,
      .reg_num = 1,
      .field_map = NULL,
      .data = data,
      .write_fn = mock_update_fn,
  };

  // Value fits in 8 bits -> accepted
  if (reg_write(&dev, 0, 0x7F) != 0) {
    TEST_FAIL("valid 8-bit value rejected");
    return -1;
  }

  // Value does not fit (0x1FF > 8 bits) -> rejected
  if (reg_write(&dev, 0, 0x1FF) != -1) {
    TEST_FAIL("oversized 8-bit value accepted");
    return -1;
  }

  return 0;
}

/**
 * @brief Reject value too large for 16-bit register.
 */
static int test_reg_write_value_too_large_16bit(void) {
  uint32_t data[1] = {0};
  struct reg_dev dev = {
      .reg_width = 16,
      .reg_num = 1,
      .field_map = NULL,
      .data = data,
      .write_fn = mock_update_fn,
  };

  // Value fits in 16 bits -> accepted
  if (reg_write(&dev, 0, 0x7FFF) != 0) {
    TEST_FAIL("valid 16-bit value rejected");
    return -1;
  }

  // Value does not fit (0x1FFFF > 16 bits) -> rejected
  if (reg_write(&dev, 0, 0x1FFFF) != -1) {
    TEST_FAIL("oversized 16-bit value accepted");
    return -1;
  }

  return 0;
}

/**
 * @brief Reject value too large for 32-bit register.
 */
static int test_reg_write_value_too_large_32bit(void) {
  uint32_t data[1] = {0};
  struct reg_dev dev = {
      .reg_width = 32,
      .reg_num = 1,
      .field_map = NULL,
      .data = data,
      .write_fn = mock_update_fn,
  };

  // Any 32-bit value fits -> accepted
  if (reg_write(&dev, 0, 0xFFFFFFFF) != 0) {
    TEST_FAIL("valid 32-bit value rejected");
    return -1;
  }

  // There is no larger value for 32-bit uint, so no invalid test here

  return 0;
}

static int (*valid_fn[])(void) = {
    test_reg_write_valid,
    test_reg_write_first_register,
    test_reg_write_last_register,
    test_reg_write_zero_value,
    test_reg_write_max_value,
    test_reg_write_value_too_large_8bit_val,
    NULL,
};

static int (*invalid_fn[])(void) = {test_reg_write_null_device,
                                    test_reg_write_null_data,
                                    test_reg_write_zero_width,
                                    test_reg_write_oob_register,
                                    test_reg_write_size_max_index,
                                    test_reg_write_zero_registers,
                                    test_reg_write_value_too_large_8bit_inv,
                                    test_reg_write_value_too_large_16bit,
                                    test_reg_write_value_too_large_32bit,
                                    NULL};

int test_reg_write(void) {
  if (test_runner(valid_fn, invalid_fn) != 0)
    return -1;

  TEST_SUCCESS();
  return 0;
}

// end file test_reg.c
