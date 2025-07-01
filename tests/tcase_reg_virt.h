// SPDX-License-Identifier: MIT
/**
 * @file tcase_reg_virt.h
 * @brief Tests for register map representation and handling.
 * @author Jakob Kastelic
 * @copyright Copyright (c) 2025 Stanford Research Systems, Inc.
 */

#include "utils/reg.h"
#include <stdbool.h>
#include <stddef.h>

#define TCASE_VIRT_MAX_REGS   2
#define TCASE_VIRT_MAX_MAPS   5
#define TCASE_VIRT_MAX_FIELDS 10

struct map_virt_test {
   bool good;
   const char *desc;
   struct reg_virt vdev;
};

static const struct reg_field map1[] = {
    //    reg of wd flags
    {"A",  0, 0, 8,  0},
    {"B",  0, 8, 8,  0},
    {"C",  1, 0, 16, 0},
    {NULL, 0, 0, 0,  0}
};

static const struct reg_field map2[] = {
    //    reg of wd flags
    {"P",  0, 0, 8,  0},
    {"Q",  0, 8, 8,  0},
    {"A",  1, 0, 16, 0},
    {NULL, 0, 0, 0,  0}
};

static const struct map_virt_test mvt[] = {
    {.good = true,
     .desc = "example from header file",
     .vdev =
         {
             .fields = (const char *[]){"A", "B", "C", "P", "Q", NULL},
             .maps   = (const struct reg_field *[]){map1, map2, NULL},
             .base   = {.reg_width = 16, .reg_num = 2},
         }},
};

// end file tcase_reg_virt.h
