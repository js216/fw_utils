// SPDX-License-Identifier: MIT
/**
 * @file test_reg_virt.c
 * @brief Tests for register map representation and handling.
 * @author Jakob Kastelic
 * @copyright Copyright (c) 2025 Stanford Research Systems, Inc.
 */

#include "tests/test_common.h"
#include "tests/test_reg.h"
#include "utils/debug.h"
#include "utils/reg.h"
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define TEST_REG_VIRT_REGS   4
#define TEST_REG_VIRT_FIELDS 6

static const struct reg_field map1[] = {
    //    reg of wd flags
    {"A",  0, 0, 8,  0},
    {"B",  0, 8, 8,  0},
    {"C",  1, 0, 16, 0},
    {NULL, 0, 0, 0,  0}
};

static const struct reg_field map2[] = {
    //    reg of wd flags
    {"P",  0, 0, 8,  0          },
    {"Q",  0, 8, 8,  REG_NORESET},
    {"A",  1, 0, 16, 0          },
    {NULL, 0, 0, 0,  0          }
};

static const struct reg_field map3[] = {
    //    reg of wd flags
    {"R",  0, 0, 64, 0},
    {NULL, 0, 0, 0,  0}
};

static const char *virt_fields[]           = {"A", "B", "C", "P", "Q", NULL};
static const struct reg_field *virt_maps[] = {map1, map2, map3, NULL};

struct test_cases {
   const char *field;
   const uint64_t val;
   uint64_t v_data[TEST_REG_VIRT_FIELDS]; // virtual
   uint32_t d_data[TEST_REG_VIRT_REGS];   // device buffer
   const int correct_map;
};

static const struct test_cases tc_good[] = {
    {"A",  0xff,   {0xff, 0, 0, 0, 0, 0},            {0x00ff, 0x0000, 0, 0}, 0},
    {"P",  0xff,   {0xff, 0, 0, 0xff, 0, 0},         {0x00ff, 0x00ff, 0, 0}, 1},
    {"Q",  0x67,   {0xff, 0, 0, 0xff, 0x67, 0},      {0x67ff, 0x00ff, 0, 0}, 1},
    {"B",  0xff,   {0xff, 0xff, 0, 0xff, 0x67, 0},   {0xffff, 0x0000, 0, 0}, 0},
    {"B",  0xff,   {0xff, 0xff, 0, 0xff, 0x67, 0},   {0xffff, 0x0000, 0, 0}, 0},
    {"A",  0x00,   {0, 0xff, 0, 0xff, 0x67, 0},      {0xff00, 0x0000, 0, 0}, 0},
    {"C",  0xffff, {0, 0xff, 0xffff, 0xff, 0x67, 0}, {0xff00, 0xffff, 0, 0}, 0},
    {"C",  0x98,   {0, 0xff, 0x98, 0xff, 0x67, 0},   {0xff00, 0x0098, 0, 0}, 0},
    {"P",  0xff,   {0, 0xff, 0x98, 0xff, 0x67, 0},   {0x00ff, 0x0000, 0, 0}, 1},
    {"Q",  0x00,   {0, 0xff, 0x98, 0xff, 0x00, 0},   {0x00ff, 0x0000, 0, 0}, 1},
    {"A",  0xffff, {0xffff, 0xff, 0x98, 0xff, 0, 0}, {0x00ff, 0xffff, 0, 0}, 1},
    {"A",  0x73,   {0x73, 0xff, 0x98, 0xff, 0, 0},   {0x00ff, 0x0073, 0, 0}, 1},
    {"B",  0x67,   {0x73, 0x67, 0x98, 0xff, 0, 0},   {0x6773, 0x0098, 0, 0}, 0},
    {NULL, 0,      {0},                              {0},                    0},
};

// map2:A (16 bits) does not fit into map1:A (8 bits)
static const struct test_cases tc_bad[] = {
    {"A",  0xff,  {0xff, 0, 0, 0, 0, 0},        {0x00ff, 0x0000, 0, 0}, 0},
    {"P",  0xff,  {0xff, 0, 0, 0xff, 0, 0},     {0x00ff, 0x00ff, 0, 0}, 1},
    {"A",  0xaaa, {0xaaa, 0, 0, 0xff, 0, 0},    {0x00ff, 0x0aaa, 0, 0}, 1},
    {"B",  0xbb,  {0xaaa, 0xbb, 0, 0xff, 0, 0}, {0xbbaa, 0x0000, 0, 0}, 0},
    {NULL, 0,     {0},                          {0},                    0},
};

static uint32_t dev_data[TEST_REG_VIRT_REGS];
static uint64_t virt_data[TEST_REG_VIRT_FIELDS];
static struct reg_virt vdev;

static uint32_t mock_data[TEST_REG_VIRT_REGS];
static int mock_map_id;

static int dev_load_fn(int arg, int id)
{
   (void)arg;
   mock_map_id = id;
   return 0;
}

static uint32_t dev_read_fn(int arg, size_t reg)
{
   (void)arg;
   return mock_data[reg];
}

static int dev_write_fn(int arg, size_t reg, uint32_t val)
{
   (void)arg;
   mock_data[reg] = val;
   return 0;
}

static int compare64(const uint64_t *d1, const uint64_t *d2, const size_t len)
{
   for (size_t i = 0; i < len; i++)
      if (d1[i] != d2[i])
         goto mismatch;

   return 0;

mismatch:
   printf("d1\td2\n");
   printf("=============\n");
   for (size_t i = 0; i < len; i++)
      printf("0x%" PRIx64 "\t0x%" PRIx64 "\n", d1[i], d2[i]);
   return -1;
}

static int compare32(const uint32_t *d1, const uint32_t *d2, const size_t len)
{
   for (size_t i = 0; i < len; i++)
      if (d1[i] != d2[i])
         goto mismatch;

   return 0;

mismatch:
   printf("d1\td2\n");
   printf("=============\n");
   for (size_t i = 0; i < len; i++)
      printf("0x%" PRIx32 "\t0x%" PRIx32 "\n", d1[i], d2[i]);
   return -1;
}

static int test_setup(void)
{
   vdev.base.reg_width = 16;
   vdev.base.reg_num   = TEST_REG_VIRT_REGS;
   vdev.base.read_fn   = dev_read_fn;
   vdev.base.write_fn  = dev_write_fn;
   vdev.base.data      = dev_data;
   vdev.load_fn        = dev_load_fn;
   vdev.data           = virt_data;
   vdev.fields         = virt_fields;
   vdev.maps           = virt_maps;

   if (reg_verify(&vdev)) {
      ERROR("cannot verify virtual device");
      return -1;
   }

   return 0;
}

static int check_cases(const struct test_cases *tc)
{
   for (size_t i = 0; tc[i].field; i++) {
      if (reg_adjust(&vdev, tc[i].field, tc[i].val)) {
         ERROR("cannot adjust reg");
         ERROR(tc[i].field);
         return -1;
      }

      if (vdev.base.field_map != virt_maps[tc[i].correct_map]) {
         ERROR("using the wrong map");
         return -1;
      }

      if (mock_map_id != tc[i].correct_map) {
         ERROR("loaded the wrong map");
         return -1;
      }

      for (int j = 0; virt_fields[j]; j++) {
         if (reg_obtain(&vdev, virt_fields[j]) != tc[i].v_data[j]) {
            ERROR("wrong data for field");
            ERROR(virt_fields[j]);
            return -1;
         }
      }

      if (compare64(vdev.data, tc[i].v_data, TEST_REG_VIRT_FIELDS)) {
         ERROR("v_data does not match after setting");
         ERROR(tc[i].field);
         return -1;
      }

      if (compare32(vdev.base.data, tc[i].d_data, TEST_REG_VIRT_REGS)) {
         ERROR("d_data does not match after setting");
         ERROR(tc[i].field);
         return -1;
      }

      if (compare32(mock_data, tc[i].d_data, TEST_REG_VIRT_REGS)) {
         ERROR("m_data does not match after setting");
         ERROR(tc[i].field);
         return -1;
      }
   }

   return 0;
}

static int test_good(void)
{
   return check_cases(tc_good);
}

static int test_bad(void)
{
   return !check_cases(tc_bad);
}

int test_reg_virt(void)
{
   static int (*valid_fn[])(void) = {test_setup, test_good, NULL};

   static int (*invalid_fn[])(void) = {test_bad, NULL};

   if (test_runner(valid_fn, invalid_fn)) {
      TEST_FAIL("all tests did not pass");
      return -1;
   }

   TEST_SUCCESS();
   return 0;
}

// end file test_reg_virt.c
