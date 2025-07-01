// SPDX-License-Identifier: MIT
/**
 * @file test_reg.h
 * @brief Tests for register map representation and handling.
 * @author Jakob Kastelic
 * @copyright Copyright (c) 2025 Stanford Research Systems, Inc.
 */

#ifndef TEST_REG_H
#define TEST_REG_H

int test_reg_check(void);
int test_reg_read(void);
int test_reg_get_phy(void);
int test_reg_get_set(void);
int test_reg_bulk(void);
int test_reg_write(void);
int test_reg_desc(void);
int test_reg_multi(void);
int test_reg_virt(void);

#endif // TEST_REG_H

// end file test_reg.h
