// SPDX-License-Identifier: MIT
/**
 * @file test_common.h
 * @brief Routines for error handling etc.
 *
 * @author Jakob Kastelic
 * @copyright Copyright (c) 2025 Stanford Research Systems, Inc.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h> // NOLINT(misc-include-cleaner)

#define TEST_FAIL(...)                                                         \
   do {                                                                        \
      printf("\033[1;31mFAIL:\033[0m %s in %s (line %d): ", __func__,          \
             __FILE__, __LINE__);                                              \
      printf(__VA_ARGS__);                                                     \
      printf("\n");                                                            \
   } while (0)

#define TEST_SUCCESS()                                                         \
   do {                                                                        \
      printf("\033[32mSUCCESS:\033[0m %s\n", __func__);                        \
   } while (0)

/**
 * @brief Run test cases.
 *
 * @param valid_fn Test cases that return 0 (success).
 * @param invalid_fn Test cases that return -1 (error).
 */
int test_runner(int (*valid_fn[])(void), int (*invalid_fn[])(void));

/**
 * @brief Print the contents of a data buffer.
 *
 * @param data Pointes to the data buffer to read.
 * @param len Number of elements to read from the buffer.
 */
void printout_buffer(const uint32_t *data, size_t len);

// end file test_common.h
