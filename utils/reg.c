// SPDX-License-Identifier: MIT
/**
 * @file reg.h
 * @brief Register map representation and handling.
 * @author Jakob Kastelic
 * @copyright Copyright (c) 2025 Stanford Research Systems, Inc.
 */

#include "utils/reg.h"
#include "utils/debug.h"
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define WIDTH_OF(type) (sizeof(type) * CHAR_BIT)
#define MAX_REG        WIDTH_OF(uint32_t)
#define MAX_FIELD      WIDTH_OF(uint64_t)

/***********************************************************
 * BASIC MATH
 ***********************************************************/

/**
 * @brief Minimum of size_t inputs.
 *
 * @param a First value.
 * @param b Second value.
 * @return The smaller of the two inputs.
 */
static inline size_t reg_min(size_t a, size_t b)
{
   return (a < b) ? a : b;
}

/**
 * @brief Ceiling division.
 *
 * @param x Dividend.
 * @param y Divisor.
 * @return Quotient rounded up to nearest integer.
 */
static inline size_t reg_cdiv(size_t x, size_t y)
{
   return (x + y - 1) / y;
}

/**
 * @brief Create a bitmask of consecutive bits set within a 64-bit word.
 *
 * @param start Starting bit position (0-based).
 * @param len Number of bits to set.
 * @return Bitmask with bits set in [start, start+len-1], or 0 on error.
 */
uint64_t reg_mask64(size_t start, size_t len)
{
   if ((len == 0) || (len > MAX_FIELD)) {
      ERROR("invalid mask length");
      return 0;
   }

   if ((start >= MAX_FIELD) || ((start + len) > MAX_FIELD)) {
      ERROR("invalid mask start");
      return 0;
   }

   uint64_t mask = (len == MAX_FIELD) ? UINT64_MAX : (1ULL << len) - 1;

   return mask << start;
}

/**
 * @brief Create a bitmask of consecutive bits set within a 32-bit word.
 *
 * Creates a mask with `len` bits set to 1 starting from bit position
 * `start`. For example, start=3, len=4 yields 0b0000_1111_000 (bits 3..6
 * set).
 *
 * @param start Starting bit position (0-based).
 * @param len Number of bits to set.
 * @return Bitmask with bits set in [start, start+len-1].
 */
uint32_t reg_mask32(size_t start, size_t len)
{
   if ((len == 0) || (len > MAX_REG)) {
      ERROR("invalid mask len");
      return 0;
   }

   if ((start >= MAX_REG) || ((start + len) > MAX_REG)) {
      ERROR("invalid mask start");
      return 0;
   }

   return (uint32_t)reg_mask64(start, len);
}

/**
 * @brief Check if a value fits in a field of given width.
 *
 * @param val Value to check.
 * @param width Number of bits in the target register
 * @return true if it fits, false if not.
 */
static bool reg_fits(uint64_t val, size_t width)
{
   if (width < 64)
      return ((val >> width) == 0);

   // uint64_t will always fit into 64-bit registers (or wider)
   return true;
}

/***********************************************************
 * GENERAL HELPER FUNCTIONS
 ***********************************************************/

/**
 * @brief Syntactic sugar to check field and device flags.
 *
 * @param d Pointer to the device to check for flags, or NULL to skip.
 * @param f Field to check for flags, or NULL to skip.
 * @param flags Flags to check.
 * @return True if flags are set in either the field or the device, and false
 * if not. False if both d and f are NULL.
 */
static inline bool reg_flags(const struct reg_dev *const d,
                             const struct reg_field *f, const uint16_t flags)
{
   if (d && f)
      return ((d->flags & flags) || (f->flags & flags));

   if (d)
      return d->flags & flags;

   if (f)
      return f->flags & flags;

   return false;
}

/**
 * @brief Check that all the usual fields are filled out.
 *
 * @return 0 on success, or -1 on failure.
 */
static int reg_empty(const struct reg_dev *const d)
{
   if (!d) {
      ERROR("null device given");
      return -1;
   }

   if (!d->read_fn) {
      ERROR("missing read_fn");
      return -1;
   }

   if (!d->write_fn) {
      ERROR("missing write_fn");
      return -1;
   }

   if (d->reg_width == 0) {
      ERROR("register has zero width");
      return -1;
   }

   if (d->reg_width > MAX_REG) {
      ERROR("reg_width too large");
      return -1;
   }

   if (!d->data) {
      ERROR("d->data is NULL");
      return -1;
   }

   if (!d->field_map) {
      ERROR("missing field map");
      return -1;
   }

   return 0;
}

/***********************************************************
 * REGISTER MANIPULATION
 ***********************************************************/

/**
 * @brief Lock a mutex, if a mutex is provided.
 *
 * @return 0 on success, -1 on error.
 */
static int reg_lock(struct reg_dev *d)
{
   if (reg_empty(d)) {
      ERROR("invalid device");
      return -1;
   }

   if (d->mutex && d->lock_fn && (*d->lock_fn)(d->mutex)) {
      ERROR("lock failed");
      return -1;
   }

   if (d->lock_count != 0) {
      ERROR("mutex already locked");
      return -1;
   }

   d->lock_count++;
   return 0;
}

/**
 * @brief Unlock a mutex, if a mutex is provided.
 *
 * @return 0 on success, -1 on error.
 */
static int reg_unlock(struct reg_dev *d)
{
   if (reg_empty(d)) {
      ERROR("invalid device");
      return -1;
   }

   if (d->mutex && d->unlock_fn && (*d->unlock_fn)(d->mutex)) {
      ERROR("unlock failed");
      return -1;
   }

   if (d->lock_count != 1) {
      ERROR("invalid lock count");
      return -1;
   }

   d->lock_count--;
   return 0;
}

uint32_t reg_read(struct reg_dev *d, const size_t reg)
{
   if (reg_empty(d)) {
      ERROR("invalid device");
      return 0;
   }

   if (reg >= d->reg_num) {
      ERROR("register outside device bounds");
      return 0;
   }

   // read register from hardware, unless REG_NOCOMM is set
   if (!reg_flags(d, NULL, REG_NOCOMM)) {
      uint32_t val = d->read_fn(d->arg, reg);
      if (val & ~reg_mask32(0, d->reg_width)) {
         ERROR("read too many bits");
         return 0;
      }

      // set buffer
      d->data[reg] = val;
   }

   const uint32_t val = d->data[reg];

   return val;
}

int reg_write(struct reg_dev *d, size_t reg, const uint32_t val)
{
   if (reg_empty(d)) {
      ERROR("invalid device");
      return -1;
   }

   if (reg >= d->reg_num) {
      ERROR("register outside device bounds");
      return -1;
   }

   if (val & ~reg_mask32(0, d->reg_width)) {
      ERROR("value too large for register width");
      return -1;
   }

   if (!reg_flags(d, NULL, REG_NOCOMM))
      if (d->write_fn(d->arg, reg, val) != 0) {
         ERROR("write_fn callback failed");
         return -1;
      }

   d->data[reg] = val;

   return 0;
}

int reg_bulk(struct reg_dev *d, const uint32_t *data)
{
   if (reg_empty(d)) {
      ERROR("invalid device");
      return -1;
   }

   if (d->reg_num == 0) {
      // no-op: zero registers to copy
      return 0;
   }

   if (reg_lock(d)) {
      ERROR("cannot lock the mutex");
      return -1;
   }

   if (data == NULL)
      memset(d->data, 0, d->reg_num * sizeof(uint32_t));
   else
      memcpy(d->data, data, d->reg_num * sizeof(uint32_t));

   if (reg_unlock(d)) {
      ERROR("cannot unlock the mutex");
      return -1;
   }

   return 0;
}

/***********************************************************
 * FIELD MANIPULATION
 ***********************************************************/

/**
 * @brief Get mask of register bits occupied by field bits.
 *
 * @param n Field chunk number, starting from n=0 for the first, least
 * significant, chunk (the one located in register f->reg).
 * @param f_offs Field offset.
 * @param f_width Field width.
 * @param reg_width Width of device registers.
 */
static uint32_t reg_field_mask(const uint8_t n, const uint8_t f_offs,
                               const uint8_t f_width, const uint8_t reg_width)
{
   const size_t len0 = reg_min(f_offs + f_width, reg_width) - f_offs;
   size_t start      = 0;
   size_t len        = 0;

   if (n == 0) {
      start = f_offs;
      len   = len0;
   } else {
      start = 0;
      len   = f_width - len0 - ((size_t)(n - 1) * reg_width);
      len   = reg_min(len, reg_width);
   }

   return reg_mask32(start, len);
}

/**
 * @brief Get the part of a field in a given register.
 *
 * @param d Pointer to the device structure.
 * @param f Field to get data of.
 * @param n Chunk number, starting from n=0 for the first, least significant,
 * chunk (the one located in f->reg).
 * @return Result, shifted to its position in the field value, or 0 on error.
 * Note that 0 is also a valid return value.
 */
static uint64_t reg_get_chunk(struct reg_dev *const d,
                              const struct reg_field *const f, const uint8_t n)
{
   if (reg_empty(d)) {
      ERROR("invalid device");
      return 0;
   }

   if (!f) {
      ERROR("null field passed");
      return 0;
   }

   if (reg_flags(d, f, REG_DESCEND) && (f->reg < n)) {
      ERROR("descending chunk out of bounds");
      return 0;
   }

   const size_t len0 = reg_min(f->offs + f->width, d->reg_width) - f->offs;
   if ((n != 0) && (len0 + ((size_t)(n - 1) * d->reg_width) >= 64)) {
      ERROR("too many bits to obtain");
      return 0;
   }

   // volatile fields must be re-read from physical device
   // (except for REG_NOCOMM fields and/or devices)
   size_t r = (reg_flags(d, f, REG_DESCEND)) ? f->reg - n : f->reg + n;
   if (!reg_flags(d, f, REG_NOCOMM))
      if (reg_flags(d, f, REG_VOLATILE))
         reg_read(d, r);

   // fetch register contents
   uint64_t chunk = 0;
   if (reg_flags(d, f, REG_DESCEND))
      chunk = d->data[f->reg - n];
   else
      chunk = d->data[f->reg + n];

   // mask out irrelevant fields
   chunk &= reg_field_mask(n, f->offs, f->width, d->reg_width);

   // shift into position
   if (n == 0) {
      chunk >>= f->offs;
   } else {
      chunk <<= len0 + ((size_t)(n - 1) * d->reg_width);
   }

   return chunk;
}

/**
 * @brief Set the part of a field in a given register.
 *
 * @param d Pointer to the device structure.
 * @param f Field to set data in.
 * @param n Chunk number, starting from n=0 for the first, least significant,
 * chunk (the one located in register f->reg).
 * @param val Value to be written to the registers.
 * @return 0 on success, -1 on failure.
 */
static int reg_set_chunk(struct reg_dev *const d,
                         const struct reg_field *const f, const uint8_t n,
                         uint64_t val)
{
   if (reg_empty(d)) {
      ERROR("invalid device");
      return -1;
   }

   if (!f) {
      ERROR("null field passed");
      return -1;
   }

   if (reg_flags(d, f, REG_DESCEND) && (f->reg < n)) {
      ERROR("descending chunk out of bounds");
      return -1;
   }

   // shift value into position
   if (n == 0) {
      val <<= f->offs;
   } else {
      const size_t len0 = reg_min(f->offs + f->width, d->reg_width) - f->offs;
      val >>= len0 + ((size_t)(n - 1) * d->reg_width);
   }

   // mask out irrelevant fields
   const uint32_t mask = reg_field_mask(n, f->offs, f->width, d->reg_width);
   val &= mask;

   // store register contents
   size_t r   = (reg_flags(d, f, REG_DESCEND)) ? f->reg - n : f->reg + n;
   d->data[r] = (d->data[r] & ~mask) | val;

   // write to physical device (if no REG_NOCOMM flag)
   if (!reg_flags(d, f, REG_NOCOMM))
      if (d->write_fn(d->arg, r, d->data[r])) {
         ERROR("error writing to device");
         return -1;
      }

   return 0;
}

static int reg_check_field_width(const struct reg_dev *const d,
                                 const struct reg_field *const f)
{
   if (f->width == 0) {
      ERROR("zero-width field not allowed");
      return -1;
   }

   if (f->width > MAX_FIELD) {
      ERROR("field too wide");
      return -1;
   }

   if (f->reg >= d->reg_num) {
      ERROR("register outside the bounds of device");
      return -1;
   }

   const size_t num_regs = reg_cdiv(f->offs + f->width, d->reg_width);

   if (reg_flags(d, f, REG_DESCEND)) {
      if (f->reg + 1 < num_regs) {
         ERROR("too many descending registers");
         return -1;
      }
   }

   else { // ascending
      if (f->reg + num_regs > d->reg_num) {
         ERROR("too many ascending registers");
         return -1;
      }
   }

   return 0;
}

static uint64_t reg_get_field(struct reg_dev *const d,
                              const struct reg_field *const f)
{
   if (reg_empty(d)) {
      ERROR("invalid device");
      return 0;
   }

   if (!f) {
      ERROR("invalid field");
      return 0;
   }

   if (reg_check_field_width(d, f)) {
      ERROR("field width invalid");
      return 0;
   }

   // assemble chunks into a single number
   uint64_t val          = 0;
   const size_t num_regs = reg_cdiv(f->offs + f->width, d->reg_width);
   for (size_t n = 0; n < num_regs; n++)
      val |= reg_get_chunk(d, f, n);

   return val;
}

static int reg_set_field(struct reg_dev *const d,
                         const struct reg_field *const f, const uint64_t val)
{
   if (reg_empty(d)) {
      ERROR("invalid device");
      return -1;
   }

   if (!f) {
      ERROR("invalid field");
      return -1;
   }

   if (reg_check_field_width(d, f)) {
      ERROR("field width invalid");
      return -1;
   }

   if (!reg_fits(val, f->width)) {
      ERROR("value too large for field width");
      return -1;
   }

   const size_t num_regs = reg_cdiv(f->offs + f->width, d->reg_width);
   for (size_t n = 0; n < num_regs; n++) {
      // invert order of register writes if REG_MSR_FIRST is set
      size_t n_eff = n;
      if (reg_flags(d, f, REG_MSR_FIRST))
         n_eff = num_regs - n - 1;

      // write to buffer
      if (reg_set_chunk(d, f, n_eff, val)) {
         ERROR("error writing to buffer");
         if (d->unlock_fn)
            (*d->unlock_fn)(d->mutex);
         return -1;
      }
   }

   return 0;
}

/***********************************************************
 * CONSISTENCY CHECKS
 ***********************************************************/

static int reg_check_fields(const struct reg_dev *const d, const size_t i)
{
   if (reg_check_field_width(d, &d->field_map[i])) {
      ERROR("field width invalid");
      return -1;
   }

   for (size_t j = i + 1; d->field_map[j].name; j++) {
      if (d->field_map[i].name[0] == '_')
         continue;

      if (strcmp(d->field_map[i].name, d->field_map[j].name) == 0) {
         ERROR("detected duplicate strings");
         return -1;
      }
   }

   return 0;
}

static int reg_clear_buffer(struct reg_dev *d)
{
   if (reg_empty(d)) {
      ERROR("invalid device");
      return -1;
   }

   for (size_t i = 0; i < d->reg_num; i++)
      d->data[i] = 0;

   return 0;
}

/**
 * @brief Check no field overlaps with the given one.
 *
 * @param d Pointer to the device structure to query.
 * @param i Field number to check, from 0 to number of fields.
 * @return 0 on success, -1 on error.
 */
static int reg_check_field_overlaps(struct reg_dev *d, const size_t i)
{
   // write all 1's in field i
   const uint64_t mask = reg_mask64(0, d->field_map[i].width);
   if (reg_set_field(d, &d->field_map[i], mask)) {
      ERROR("cannot set field i");
      return -1;
   }

   // clear all other fields
   for (size_t j = 0; d->field_map[j].name; j++) {
      if (j != i) {
         if (d->field_map[j].name[0] == '_')
            continue;

         if (reg_set_field(d, &d->field_map[j], 0)) {
            ERROR("cannot set field j");
            return -1;
         }
      }
   }

   // read back field i
   if (reg_get_field(d, &d->field_map[i]) != mask) {
      ERROR("cannot read original value; overlap likely for field");
      ERROR(d->field_map[i].name);
      return -1;
   }

   // clear field i
   if (reg_set_field(d, &d->field_map[i], 0)) {
      ERROR("cannot clear field i");
      return -1;
   }

   // check all registers are now zero
   for (size_t j = 0; d->field_map[j].name; j++) {
      if (reg_get_field(d, &d->field_map[j]) != 0) {
         ERROR("registers failed to clear");
         return -1;
      }
   }

   return 0;
}

static int reg_check_field_partial_coverage(struct reg_dev *d)
{
   // write all 1's in all fields
   for (size_t i = 0; d->field_map[i].name; i++) {
      const uint64_t mask = reg_mask64(0, d->field_map[i].width);
      if (reg_set_field(d, &d->field_map[i], mask)) {
         ERROR("cannot set field i");
         return -1;
      }
   }

   // read back all fields
   for (size_t i = 0; d->field_map[i].name; i++) {
      const uint64_t mask = reg_mask64(0, d->field_map[i].width);
      if (reg_get_field(d, &d->field_map[i]) != mask) {
         ERROR("value not all ones");
         return -1;
      }
   }

   // check all registers are either completely full or empty
   for (size_t i = 0; i < d->reg_num; i++) {
      const uint32_t val = d->data[i];
      if ((val != 0) && (val != reg_mask32(0, d->reg_width))) {
         ERROR("register partially covered by fields");
         if (d->unlock_fn)
            (*d->unlock_fn)(d->mutex);
         return -1;
      }
   }

   return 0;
}

int reg_check(struct reg_dev *const d)
{
   if (reg_empty(d)) {
      ERROR("invalid device");
      return -1;
   }

   if ((d->lock_fn && !d->unlock_fn) || (!d->lock_fn && d->unlock_fn)) {
      ERROR("both or none of lock_fn, unlock_fn must be given");
      return -1;
   }

   if (reg_lock(d)) {
      ERROR("cannot lock the mutex");
      return -1;
   }

   // disable writing to physical device
   const uint16_t flags = d->flags;
   d->flags |= REG_NOCOMM;

   if (reg_clear_buffer(d))
      return -1;

   for (size_t i = 0; d->field_map[i].name; i++) {
      if (reg_check_fields(d, i))
         return -1;

      if (reg_check_field_overlaps(d, i))
         return -1;
   }

   if (reg_clear_buffer(d))
      return -1;

   if (reg_check_field_partial_coverage(d))
      return -1;

   if (reg_clear_buffer(d))
      return -1;

   // restore original flags
   d->flags = flags;

   if (reg_unlock(d)) {
      ERROR("cannot unlock the mutex");
      return -1;
   }

   return 0;
}

/***********************************************************
 * FIELD MAP MANIPULATION
 ***********************************************************/

/**
 * @brief Find a field by name.
 *
 * @param map Pointer to the field map to search in.
 * @param field Null-terminated name of the field to find.
 * @return The requested field, or NULL on error.
 */
static const struct reg_field *reg_find(const struct reg_field *const map,
                                        const char *const field)
{
   if (!map) {
      ERROR("no field map");
      return NULL;
   }

   if (!field) {
      ERROR("missing field");
      return NULL;
   }

   const struct reg_field *f = NULL;
   for (size_t i = 0; map[i].name; i++) {
      if (strcmp(map[i].name, field) == 0) {
         f = &map[i];
         break;
      }
   }

   return f;
}

uint64_t reg_get(struct reg_dev *const d, const char *const field)
{
   if (!field) {
      ERROR("missing field");
      return 0;
   }

   if (reg_lock(d)) {
      ERROR("cannot lock the mutex");
      return 0;
   }

   const struct reg_field *f = reg_find(d->field_map, field);
   if (!f) {
      ERROR("cannot find field");
      return 0;
   }

   const uint64_t val = reg_get_field(d, f);

   if (reg_unlock(d)) {
      ERROR("cannot unlock the mutex");
      return 0;
   }

   return val;
}

int reg_set(struct reg_dev *const d, const char *const field,
            const uint64_t val)
{
   if (!field) {
      ERROR("missing field");
      return -1;
   }

   if (reg_lock(d)) {
      ERROR("cannot lock the mutex");
      return -1;
   }

   const struct reg_field *f = reg_find(d->field_map, field);
   if (!f) {
      ERROR("cannot find field");
      return 0;
   }

   if (reg_set_field(d, f, val)) {
      ERROR("cannot set field");
      return -1;
   }

   if (reg_unlock(d)) {
      ERROR("cannot unlock the mutex");
      return -1;
   }

   return 0;
}

uint8_t reg_fwidth(const struct reg_dev *const d, const char *const field)
{
   if (!field) {
      ERROR("missing field");
      return -1;
   }

   const struct reg_field *f = reg_find(d->field_map, field);
   if (!f) {
      // not an error: can use this functio to check if a field is present
      return -1;
   }

   return f->width;
}

/***********************************************************
 * VIRTUAL DEVICES
 ***********************************************************/

/**
 * @brief Check that all the virtual device fields are filled out.
 *
 * @param `v` Virtual device data structure to verify.
 * @return 0 on success, or -1 on failure.
 */
static int reg_bad(const struct reg_virt *const v)
{
   if (!v) {
      ERROR("virtual device is NULL");
      return -1;
   }

   if (!v->fields || !v->fields[0]) {
      ERROR("virtual device has no fields");
      return -1;
   }

   if (!v->data) {
      ERROR("virtual device has no data");
      return -1;
   }

   if (!v->maps || !v->maps[0]) {
      ERROR("virtual device has no base maps");
      return -1;
   }

   if (!v->load_fn) {
      ERROR("missing load function");
      return -1;
   }

   return 0;
}

/**
 * @brief Test underlying physical device for all available maps.
 *
 * @param `v` Virtual device data structure to verify.
 * @return 0 on success, or -1 on failure.
 */
static int reg_verify_maps(struct reg_virt *const v)
{
   for (int i = 0; v->maps[i]; i++) {
      v->base.field_map = v->maps[i];
      if (reg_check(&v->base)) {
         ERROR("bad map or bad device");
         return -1;
      }
   }

   return 0;
}

int reg_verify(struct reg_virt *v)
{
   if (reg_bad(v)) {
      ERROR("malformed virtual device");
      return -1;
   }

   if (reg_verify_maps(v)) {
      ERROR("bad vdev maps");
      return -1;
   }

   if (reg_empty(&v->base)) {
      ERROR("invalid device");
      return -1;
   }

   // all fields must be present in at least one map (except non-physical)
   for (int i = 0; v->fields[i]; i++) {
      // skip non-physical virtual fields
      if (v->fields[i][0] == '_')
         continue;

      const struct reg_field *f = NULL;
      for (int j = 0; v->maps[j]; j++) {
         f = reg_find(v->maps[j], v->fields[i]);
         if (f)
            break;
      }

      if (!f) {
         ERROR("virtual field not mapped:");
         ERROR(v->fields[i]);
         return -1;
      }
   }

   // clear map, to be initialized on first reg_adjust
   v->base.field_map = NULL;

   return 0;
}

uint64_t reg_obtain(struct reg_virt *v, const char *field)
{
   if (reg_bad(v)) {
      ERROR("malformed virtual device");
      return -1;
   }

   for (int i = 0; v->fields[i]; i++)
      if (strcmp(v->fields[i], field) == 0)
         return v->data[i];

   ERROR("virtual field not found:");
   ERROR(field);
   return 0;
}

/**
 * @brief Re-set all physical device fields from the virtual device.
 *
 * @param v Virtual device affected.
 * @param except All fields will be re-set except this one.
 * @return 0 on success, -1 on failure.
 */
static int reg_reset(struct reg_virt *v, const struct reg_field *const except)
{
   // clear device data
   memset(v->base.data, 0, v->base.reg_num * sizeof(v->base.data[0]));

   // re-set all fields in the currently-loaded device map
   for (int i = 0; v->base.field_map[i].name; i++) {
      const struct reg_field *fi = &v->base.field_map[i];
      if (!fi) {
         ERROR("NULL field");
         return -1;
      }

      // skip re-setting REG_NORESET and underscore fields
      if ((fi != except) &&
          (reg_flags(&v->base, fi, REG_NORESET) || (fi->name[0] == '_')))
         continue;

      // skip re-setting fields that don't fit
      uint64_t fi_val = reg_obtain(v, fi->name);
      if (!reg_fits(fi_val, fi->width))
         continue;

      if (reg_set_field(&v->base, fi, fi_val)) {
         ERROR("could not set field:");
         ERROR(fi->name);
         return -1;
      }
   }

   return 0;
}

int reg_adjust(struct reg_virt *v, const char *const field, uint64_t val)
{
   if (reg_bad(v)) {
      ERROR("malformed virtual device");
      return -1;
   }

   if (!field) {
      ERROR("no field string given");
      return -1;
   }

   // locate virtual field
   bool found = false;
   for (int i = 0; v->fields[i]; i++)
      if (strcmp(v->fields[i], field) == 0) {
         v->data[i] = val;
         found      = true;
         break;
      }

   if (!found) {
      ERROR("did not find the virtual field");
      return -1;
   }

   // non-physical fields: that's it, we're done!
   if (field[0] == '_')
      return 0;

   // install default map, if missing (the first one, id = 0)
   if (!v->base.field_map) {
      if (v->load_fn(v->base.arg, 0)) {
         ERROR("cannot load new device configuration");
         return -1;
      }
      v->base.field_map = v->maps[0];
   }

   // look in the current map
   const struct reg_field *f = reg_find(v->base.field_map, field);
   if (f && reg_fits(val, f->width)) {
      return reg_set_field(&v->base, f, val);
   }

   // not found: check the other maps for match and fit
   int id = 0;
   for (id = 0; v->maps[id]; id++) {
      f = reg_find(v->maps[id], field);
      if (f && reg_fits(val, f->width))
         break;
      f = NULL; // if found but doesn't fit
   }

   if (!f) {
      ERROR("field not found in field_map (or value too big):");
      ERROR(field);
      return -1;
   }

   // load a new configuration
   if (v->load_fn(v->base.arg, id)) {
      ERROR("cannot load new device configuration");
      return -1;
   }

   // record the new map, if found
   if (v->maps[id]) {
      v->base.field_map = v->maps[id];
   } else {
      ERROR("new map is NULL");
      return -1;
   }

   if (reg_reset(v, f)) {
      ERROR("cannot re-set fields");
      return -1;
   }

   return 0;
}

// end file reg.c
