// SPDX-License-Identifier: MIT
/**
 * @file test_reg_get_set.c
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
#include <stdlib.h>
#include <string.h>

static const struct reg_field test_fields[] = {
    {"foo",    0, 0,  8,  0}, // uses bits [7:0] of reg 0
    {"bar",    0, 8,  4,  0}, // uses bits [11:8] of reg 0
    {"wide",   1, 0,  32, 0}, // full 32-bit reg 1
    {"across", 2, 28, 8,  0}, // spans reg 2 (bits 28–31) and reg 3 (bits 0–3)
    {NULL,     0, 0,  0,  0}
};

static int update_log[8]; // track which registers write_fn was called for
static uint64_t update_val[8];
static int update_fn_called;

static int mock_update_fn(size_t reg, uint32_t val)
{
   if (reg >= 8)
      return -1;
   update_log[reg]++;
   update_val[reg] = val;
   update_fn_called++;
   return 0;
}

/**
 * @brief Test reg_set/reg_get for field "foo".
 *
 * Verifies correct data storage, value retrieval, and write_fn call
 * for a field located at bits 0–7 of register 0.
 */
static int test_reg_set_get_foo(void)
{
   uint32_t data[4]      = {0};
   struct reg_dev dev = {.reg_width = 32,
                            .reg_num   = 4,
                            .field_map = test_fields,
                            .data      = data,
                            .write_fn  = mock_update_fn};

   memset(update_log, 0, sizeof(update_log));
   update_fn_called = 0;

   if (reg_set(&dev, "foo", 0xab) != 0) {
      TEST_FAIL("reg_set(foo) failed");
      return -1;
   }

   if (data[0] != 0xab) {
      TEST_FAIL("data[0] incorrect after set foo");
      return -1;
   }

   if (reg_get(&dev, "foo") != 0xab) {
      TEST_FAIL("reg_get(foo) returned wrong value");
      return -1;
   }

   if (update_log[0] != 1 || update_val[0] != 0xab) {
      TEST_FAIL("write_fn not called properly for foo");
      return -1;
   }

   return 0;
}

/**
 * @brief Test reg_set/reg_get for field "bar".
 *
 * Verifies bitfield manipulation in register 0 for a field at bits 8–11.
 */
static int test_reg_set_get_bar(void)
{
   uint32_t data[4]      = {0};
   struct reg_dev dev = {.reg_width = 32,
                            .reg_num   = 4,
                            .field_map = test_fields,
                            .data      = data,
                            .write_fn  = mock_update_fn};

   if (reg_set(&dev, "bar", 0x5) != 0) {
      TEST_FAIL("reg_set(bar) failed");
      return -1;
   }

   if ((data[0] >> 8U & 0xfU) != 0x5) {
      TEST_FAIL("bar not correctly set in data[0]");
      return -1;
   }

   if (reg_get(&dev, "bar") != 0x5) {
      TEST_FAIL("reg_get(bar) wrong");
      return -1;
   }

   return 0;
}

/**
 * @brief Test reg_set/reg_get for field "wide".
 *
 * Verifies correct storage and retrieval of a full 32-bit field in register 1.
 */
static int test_reg_set_get_wide(void)
{
   uint32_t data[4]      = {0};
   struct reg_dev dev = {.reg_width = 32,
                            .reg_num   = 4,
                            .field_map = test_fields,
                            .data      = data,
                            .write_fn  = mock_update_fn};

   if (reg_set(&dev, "wide", 0xdeadbeef) != 0) {
      TEST_FAIL("reg_set(wide) failed");
      return -1;
   }

   if (data[1] != 0xdeadbeef) {
      TEST_FAIL("wide value not stored correctly");
      return -1;
   }

   if (reg_get(&dev, "wide") != 0xdeadbeef) {
      TEST_FAIL("reg_get(wide) wrong");
      return -1;
   }

   return 0;
}

/**
 * @brief Test reg_set/reg_get for field "across".
 *
 * Validates bit spanning across registers 2 and 3 for an 8-bit field split
 * across bits 28–31 and 0–3.
 */
static int test_reg_set_get_across(void)
{
   uint32_t data[4]      = {0};
   struct reg_dev dev = {.reg_width = 32,
                            .reg_num   = 4,
                            .field_map = test_fields,
                            .data      = data,
                            .write_fn  = mock_update_fn};

   if (reg_set(&dev, "across", 0xff) != 0) {
      TEST_FAIL("reg_set(across) failed");
      return -1;
   }

   if ((data[2] >> 28U) != 0xfU || (data[3] & 0xfU) != 0xfU) {
      TEST_FAIL("across bits not stored properly");
      return -1;
   }

   if (reg_get(&dev, "across") != 0xff) {
      TEST_FAIL("reg_get(across) wrong");
      return -1;
   }

   return 0;
}

static int test_reg_set_invalid_name(void)
{
   uint32_t data[2]      = {0};
   struct reg_dev dev = {.reg_width = 32,
                            .reg_num   = 2,
                            .field_map = test_fields,
                            .data      = data,
                            .write_fn  = mock_update_fn};

   if (reg_get(&dev, "nonexist") != 0) {
      TEST_FAIL("get should return 0 on nonexistent field");
      return -1;
   }

   if (reg_set(&dev, "nonexist", 1) != -1) {
      TEST_FAIL("set should fail on nonexistent field");
      return -1;
   }

   return 0;
}

static int test_reg_set_too_large(void)
{
   uint32_t data[1]      = {0};
   struct reg_dev dev = {.reg_width = 32,
                            .reg_num   = 1,
                            .field_map = test_fields,
                            .data      = data,
                            .write_fn  = mock_update_fn};

   if (reg_set(&dev, "foo", 0x1ff) != -1) {
      TEST_FAIL("should fail on out-of-range value for foo (8 bits)");
      return -1;
   }

   return 0;
}

static int test_update_fn_failure(void)
{
   uint32_t data[2]      = {0};
   struct reg_dev dev = {
       .reg_width = 32,
       .reg_num   = 2,
       .field_map = test_fields,
       .data      = data,
       .write_fn  = NULL // NULL simulates failure
   };

   if (reg_set(&dev, "foo", 0x12) != -1) {
      TEST_FAIL("should fail when write_fn is NULL");
      return -1;
   }

   return 0;
}

/*
 * Check if reg_get() and reg_set() correctly handle fields starting at register
 * boundary and crossing into the next register.
 */
static int test_field_spanning_regs_at_zero_offset(void)
{
   static const struct reg_field fields[] = {
       // 40 bits starting at bit 0 of reg 0, crosses reg 1
       {"cross_zero", 0, 0, 40, 0},
       {NULL,         0, 0, 0,  0}
   };
   uint32_t data[2]      = {0};
   struct reg_dev dev = {.reg_width = 32,
                            .reg_num   = 2,
                            .field_map = fields,
                            .data      = data,
                            .write_fn  = mock_update_fn};

   memset(update_log, 0, sizeof(update_log));
   update_fn_called = 0;

   uint64_t val = 0xFFFFFFFFFFULL; // 40-bit max value
   if (reg_set(&dev, "cross_zero", val) != 0) {
      TEST_FAIL("reg_set cross_zero failed");
      return -1;
   }

   uint64_t read_val = reg_get(&dev, "cross_zero");
   if (read_val != (val & ((1ULL << 40U) - 1))) {
      TEST_FAIL("reg_get cross_zero returned wrong value");
      return -1;
   }

   return 0;
}

/*
 * Verify that setting/getting works for 64-bit max values.
 */
static int test_field_max_width(void)
{
   static const struct reg_field fields[] = {
       {"max64", 0, 0, 64, 0},
       {NULL,    0, 0, 0,  0}
   };
   uint32_t data[2]      = {0}; // two 32-bit regs to hold 64 bits
   struct reg_dev dev = {.reg_width = 32,
                            .reg_num   = 2,
                            .field_map = fields,
                            .data      = data,
                            .write_fn  = mock_update_fn};

   memset(update_log, 0, sizeof(update_log));
   update_fn_called = 0;

   uint64_t val = UINT64_MAX;
   if (reg_set(&dev, "max64", val) != 0) {
      TEST_FAIL("reg_set max64 failed");
      return -1;
   }

   uint64_t read_val = reg_get(&dev, "max64");
   if (read_val != val) {
      TEST_FAIL("reg_get max64 returned wrong value");
      return -1;
   }

   return 0;
}

/*
 * Test how reg_set() and reg_get() behave if the field width is zero. They
 * should probably fail or return 0, not crash or corrupt data.
 */
static int test_zero_width_field(void)
{
   static const struct reg_field fields[] = {
       {"zero", 0, 0, 0, 0},
       {NULL,   0, 0, 0, 0}
   };
   uint32_t data[1]      = {0};
   struct reg_dev dev = {.reg_width = 32,
                            .reg_num   = 1,
                            .field_map = fields,
                            .data      = data,
                            .write_fn  = mock_update_fn};

   if (reg_set(&dev, "zero", 1) != -1) {
      TEST_FAIL("reg_set zero width field should fail");
      return -1;
   }

   if (reg_get(&dev, "zero") != 0) {
      TEST_FAIL("reg_get zero width field should return 0");
      return -1;
   }

   return 0;
}

/*
 * Test a field that claims to start or extend beyond the available registers
 * (e.g., reg_start or reg_end outside reg_num). Should fail gracefully.
 */
static int test_field_out_of_range(void)
{
   static const struct reg_field fields[] = {
       // reg 10 doesn't exist (only 2 regs below)
       {"out_of_range", 10, 0, 8, 0},
       {NULL,           0,  0, 0, 0}
   };
   uint32_t data[2]      = {0};
   struct reg_dev dev = {.reg_width = 32,
                            .reg_num   = 2,
                            .field_map = fields,
                            .data      = data,
                            .write_fn  = mock_update_fn};

   if (reg_set(&dev, "out_of_range", 1) != -1) {
      TEST_FAIL("reg_set out_of_range field should fail");
      return -1;
   }

   if (reg_get(&dev, "out_of_range") != 0) {
      TEST_FAIL("reg_get out_of_range field should return 0");
      return -1;
   }

   return 0;
}

/*
 * Test reg_get() and reg_set() with NULL pointers to verify they reject invalid
 * inputs gracefully.
 */
static int test_null_pointers(void)
{
   uint32_t data[1]      = {0};
   struct reg_dev dev = {.reg_width = 32,
                            .reg_num   = 1,
                            .field_map = test_fields,
                            .data      = data,
                            .write_fn  = mock_update_fn};

   if (reg_set(NULL, "foo", 1) != -1) {
      TEST_FAIL("reg_set NULL device should fail");
      return -1;
   }
   if (reg_set(&dev, NULL, 1) != -1) {
      TEST_FAIL("reg_set NULL name should fail");
      return -1;
   }
   if (reg_set(&dev, "foo", 1) != 0) { // sanity check, should succeed
      TEST_FAIL("reg_set valid call failed");
      return -1;
   }

   if (reg_get(NULL, "foo") != 0) {
      TEST_FAIL("reg_get NULL device should return 0");
      return -1;
   }
   if (reg_get(&dev, NULL) != 0) {
      TEST_FAIL("reg_get NULL name should return 0");
      return -1;
   }

   return 0;
}

/*
 * Checks if a field that ends on the last bit of a register (e.g. bits 24–31)
 * is correctly handled.
 */
static int test_field_at_register_end(void)
{
   static const struct reg_field fields[] = {
       {"tail", 0, 24, 8, 0}, // bits 24–31
       {NULL,   0, 0,  0, 0}
   };
   uint32_t data[1]      = {0};
   struct reg_dev dev = {.reg_width = 32,
                            .reg_num   = 1,
                            .field_map = fields,
                            .data      = data,
                            .write_fn  = mock_update_fn};

   memset(update_log, 0, sizeof(update_log));
   update_fn_called = 0;

   if (reg_set(&dev, "tail", 0xAB) != 0) {
      TEST_FAIL("reg_set tail failed");
      return -1;
   }

   if ((data[0] >> 24U) != 0xAB) {
      TEST_FAIL("reg_set tail didn't affect bits 24–31");
      return -1;
   }

   if (reg_get(&dev, "tail") != 0xAB) {
      TEST_FAIL("reg_get tail returned wrong value");
      return -1;
   }

   return 0;
}

/**
 * @brief Field in highest register bit (e.g. bit 31 of 32-bit reg).
 *
 * Tests sign-extension issues and correct shifting at the MSB.
 */
static int test_field_highest_bit(void)
{
   static const struct reg_field fields[] = {
       {"msb", 0, 31, 1, 0}, // just bit 31
       {NULL,  0, 0,  0, 0}
   };
   uint32_t data[1]      = {0};
   struct reg_dev dev = {.reg_width = 32,
                            .reg_num   = 1,
                            .field_map = fields,
                            .data      = data,
                            .write_fn  = mock_update_fn};

   if (reg_set(&dev, "msb", 1) != 0) {
      TEST_FAIL("reg_set msb failed");
      return -1;
   }

   if ((data[0] >> 31U) != 1) {
      TEST_FAIL("bit 31 not set properly");
      return -1;
   }

   if (reg_get(&dev, "msb") != 1) {
      TEST_FAIL("reg_get msb failed");
      return -1;
   }

   return 0;
}

/**
 * @brief reg_set() sets the same value as already in the register.
 *
 * Check that write_fn is called even if nothing changes — important for some
 * registers where the act of writing any value has side effects, even if it's
 * zero or the same value already written.
 */
static int test_set_no_change(void)
{
   static const struct reg_field fields[] = {
       {"fixed", 0, 0, 16, 0},
       {NULL,    0, 0, 0,  0}
   };
   uint32_t data[1]      = {0x00001234};
   struct reg_dev dev = {.reg_width = 32,
                            .reg_num   = 1,
                            .field_map = fields,
                            .data      = data,
                            .write_fn  = mock_update_fn};

   memset(update_log, 0, sizeof(update_log));
   update_fn_called = 0;

   if (reg_set(&dev, "fixed", 0x1234) != 0) {
      TEST_FAIL("reg_set fixed failed");
      return -1;
   }

   if (update_fn_called == 0) {
      TEST_FAIL("write_fn should be called even if nothing changes");
      return -1;
   }

   return 0;
}

/**
 * @brief Field starting after register 0.
 *
 * Tests whether fields can start at a later register, with earlier ones unused.
 */
static int test_field_starts_late(void)
{
   static const struct reg_field fields[] = {
       {"late", 2, 0, 32, 0},
       {NULL,   0, 0, 0,  0}
   };
   uint32_t data[4]      = {0};
   struct reg_dev dev = {.reg_width = 32,
                            .reg_num   = 4,
                            .field_map = fields,
                            .data      = data,
                            .write_fn  = mock_update_fn};

   if (reg_set(&dev, "late", 0x12345678) != 0) {
      TEST_FAIL("reg_set late failed");
      return -1;
   }

   if (data[2] != 0x12345678) {
      TEST_FAIL("data[2] not updated correctly");
      return -1;
   }

   if (reg_get(&dev, "late") != 0x12345678) {
      TEST_FAIL("reg_get late incorrect");
      return -1;
   }

   return 0;
}

/**
 * @brief Field ending before the final register.
 *
 * Ensures that unused trailing registers do not affect operation.
 */
static int test_field_ends_early(void)
{
   static const struct reg_field fields[] = {
       {"early", 0, 0, 32, 0},
       {NULL,    0, 0, 0,  0}
   };
   uint32_t data[3]      = {0};
   struct reg_dev dev = {.reg_width = 32,
                            .reg_num   = 3, // only reg 0 is used
                            .field_map = fields,
                            .data      = data,
                            .write_fn  = mock_update_fn};

   if (reg_set(&dev, "early", 0xCAFEBABE) != 0) {
      TEST_FAIL("reg_set early failed");
      return -1;
   }

   if (data[0] != 0xCAFEBABE) {
      TEST_FAIL("data[0] incorrect");
      return -1;
   }

   if (reg_get(&dev, "early") != 0xCAFEBABE) {
      TEST_FAIL("reg_get early incorrect");
      return -1;
   }

   return 0;
}

/**
 * @brief Field spanning multiple registers with a gap register.
 *
 * Verifies that unused middle registers (fully empty) are skipped.
 */
static int test_field_with_gap_registers(void)
{
   static const struct reg_field fields[] = {
       {"first",  0, 0, 32, 0},
       {"second", 2, 0, 32, 0},
       {NULL,     0, 0, 0,  0}
   };
   uint32_t data[3]      = {0};
   struct reg_dev dev = {.reg_width = 32,
                            .reg_num   = 3,
                            .field_map = fields,
                            .data      = data,
                            .write_fn  = mock_update_fn};

   if (reg_set(&dev, "first", 0xAAAA0000) != 0) {
      TEST_FAIL("reg_set first failed");
      return -1;
   }
   if (reg_set(&dev, "second", 0x0000BBBB) != 0) {
      TEST_FAIL("reg_set second failed");
      return -1;
   }

   if (data[0] != 0xAAAA0000) {
      TEST_FAIL("reg 0 incorrect");
      return -1;
   }
   if (data[1] != 0x00000000) {
      TEST_FAIL("gap register was incorrectly modified");
      return -1;
   }
   if (data[2] != 0x0000BBBB) {
      TEST_FAIL("reg 2 incorrect");
      return -1;
   }

   if (reg_get(&dev, "first") != 0xAAAA0000) {
      TEST_FAIL("reg_get first incorrect");
      return -1;
   }
   if (reg_get(&dev, "second") != 0x0000BBBB) {
      TEST_FAIL("reg_get second incorrect");
      return -1;
   }

   return 0;
}

/**
 * @brief 64-bit field with unaligned start but full register coverage.
 *
 * Ensures 64 bits is supported when field covers all bits of used registers.
 */
static int test_maxfield_unaligned_start(void)
{
   static const struct reg_field fields[] = {
       {"maxfield", 0, 0, 64, 0},
       {NULL,       0, 0, 0,  0}
   };
   uint32_t data[3]      = {0};
   struct reg_dev dev = {.reg_width = 32,
                            .reg_num   = 3,
                            .field_map = fields,
                            .data      = data,
                            .write_fn  = mock_update_fn};

   uint64_t val = 0x0123456789ABCDEFULL;
   if (reg_set(&dev, "maxfield", val) != 0) {
      TEST_FAIL("reg_set maxfield failed");
      return -1;
   }

   if (reg_get(&dev, "maxfield") != val) {
      TEST_FAIL("reg_get maxfield failed");
      return -1;
   }

   return 0;
}

/**
 * @brief test reg_get and reg_set for valid fields and values (expected
 * success)
 */
static int test_reg_get_set_valid(void)
{
   static const struct reg_field fields[] = {
       {"bit1",    0, 0,  1,  0},
       {"bit64",   0, 0,  64, 0},

       // powers of two sizes starting at bit 0 of reg 0
       {"bit2",    0, 0,  2,  0},
       {"bit4",    0, 0,  4,  0},
       {"bit8",    0, 0,  8,  0},
       {"bit16",   0, 0,  16, 0},
       {"bit32",   0, 0,  32, 0},

       // off-aligned 3-bit field at bit 29 (spans into reg1)
       {"off3",    0, 29, 3,  0},

       // crossing boundary 17-bit field starting at bit 31 of reg 0
       {"cross17", 0, 31, 17, 0},

       {NULL,      0, 0,  0,  0}
   };

   uint32_t data[3]      = {0};
   struct reg_dev dev = {
       .reg_width = 32,
       .reg_num   = 3,
       .field_map = fields,
       .data      = data,
       .write_fn  = mock_update_fn,
   };

   struct {
      const char *name;
      uint64_t val;
   } test_vals[] = {
       {"bit1",    1ULL         },
       {"bit64",   UINT64_MAX   },

       {"bit2",    3ULL         },
       {"bit4",    0xFULL       },
       {"bit8",    0xFFULL      },
       {"bit16",   0xFFFFULL    },
       {"bit32",   0xFFFFFFFFULL},

       {"off3",    0x7ULL       },
       {"cross17", 0x1FFFFULL   },

       {NULL,      0            }
   };

   for (size_t i = 0; test_vals[i].name != NULL; i++) {
      for (size_t j = 0; j < dev.reg_num; j++)
         dev.data[j] = 0;

      if (reg_set(&dev, test_vals[i].name, test_vals[i].val) != 0) {
         TEST_FAIL("reg_set failed for field %s", test_vals[i].name);
         return -1;
      }

      uint64_t got = reg_get(&dev, test_vals[i].name);
      if (got != test_vals[i].val) {
         TEST_FAIL("case %zu: reg_get failed for field %s: got 0x%llx expected "
                   "0x%llx",
                   i, test_vals[i].name, (unsigned long long)got,
                   (unsigned long long)test_vals[i].val);
         printout_buffer(data, sizeof(data) / sizeof(data[0]));
         return -1;
      }
   }

   return 0;
}

/**
 * @brief test reg_set for invalid or out-of-range values (expected failure)
 */
static int test_reg_set_invalid(void)
{
   static const struct reg_field fields[] = {
       {"bit1",  0, 0, 1,  0},
       {"bit4",  0, 0, 4,  0},
       {"bit64", 0, 0, 64, 0},
       {NULL,    0, 0, 0,  0}
   };

   uint32_t data[2]      = {0};
   struct reg_dev dev = {
       .reg_width = 32,
       .reg_num   = 2,
       .field_map = fields,
       .data      = data,
       .write_fn  = mock_update_fn,
   };

   struct {
      const char *name;
      uint64_t val;
   } bad_vals[] = {
       {"bit1", 2ULL   }, // exceeds 1-bit max (1)
       {"bit4", 0x20ULL}, // exceeds 4-bit max (0xF)

       {NULL,   0      }
   };

   for (size_t i = 0; bad_vals[i].name != NULL; i++) {
      for (size_t j = 0; j < dev.reg_num; j++)
         dev.data[j] = 0;

      if (reg_set(&dev, bad_vals[i].name, bad_vals[i].val) == 0) {
         TEST_FAIL("set unexpectedly succeeded for out-of-range value on "
                   "field %s",
                   bad_vals[i].name);
         return -1;
      }
   }

   return 0;
}

int test_reg_get_set(void)
{
   // test functions
   static int (*valid_fn[])(void) = {
       test_field_spanning_regs_at_zero_offset,
       test_field_max_width,
       test_field_at_register_end,
       test_field_highest_bit,
       test_set_no_change,
       test_field_starts_late,
       test_field_ends_early,
       test_field_with_gap_registers,
       test_maxfield_unaligned_start,
       test_reg_set_get_foo,
       test_reg_set_get_bar,
       test_reg_set_get_wide,
       test_reg_set_get_across,
       test_reg_get_set_valid,
       NULL,
   };

   static int (*invalid_fn[])(void) = {
       test_update_fn_failure,    test_reg_set_too_large,
       test_reg_set_invalid_name, test_zero_width_field,
       test_field_out_of_range,   test_null_pointers,
       test_reg_set_invalid,      NULL,
   };

   if (test_runner(valid_fn, invalid_fn) != 0)
      return -1;

   TEST_SUCCESS();
   return 0;
}

// end file test_reg.c
