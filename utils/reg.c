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
 * @return Bitmask with bits set in [start, start+len-1].
 */
static inline uint64_t reg_mask64(size_t start, size_t len)
{
   if ((len == 0) || (len > MAX_FIELD))
      return 0;

   if ((start >= MAX_FIELD) || ((start + len) > MAX_FIELD))
      return 0;

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
static inline uint32_t reg_mask32(size_t start, size_t len)
{
   if ((len == 0) || (len > MAX_REG))
      return 0;

   if ((start >= MAX_REG) || ((start + len) > MAX_REG))
      return 0;

   return (uint32_t)reg_mask64(start, len);
}

/***********************************************************
 * REGISTER MANIPULATION
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
static inline bool reg_flags(const struct reg_dev *d, const struct reg_field *f,
                             const uint16_t flags)
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
 * @brief Lock a mutex, if a mutex is provided.
 * @return 0 on success, -1 on error.
 */
static int reg_lock(struct reg_dev *d)
{
   if (d->lock_fn && (*d->lock_fn)(d->mutex)) {
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
 * @return 0 on success, -1 on error.
 */
static int reg_unlock(struct reg_dev *d)
{
   if (d->unlock_fn && (*d->unlock_fn)(d->mutex)) {
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
   if (!d || !d->read_fn) {
      ERROR("invalid argument: null pointer or missing read_fn");
      return 0;
   }

   if (d->reg_width == 0) {
      ERROR("register has zero width");
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
   if (!d || !d->data || !d->write_fn) {
      ERROR("null pointers in device struct");
      return -1;
   }

   if (d->reg_width == 0) {
      ERROR("zero register width");
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
   if (!d) {
      ERROR("no device given");
      return -1;
   }

   if (d->reg_num == 0) {
      // no-op: zero registers to copy
      return 0;
   }

   if (!d->data) {
      ERROR("d->data is NULL");
      return -1;
   }

   if (d->reg_width == 0) {
      ERROR("zero register width");
      return -1;
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
      len   = f_width - len0 - ((n - 1) * reg_width);
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
   if (!d || !d->data || !f) {
      ERROR("null pointer(s) passed");
      return 0;
   }

   if (reg_flags(d, f, REG_DESCEND) && (f->reg < n)) {
      ERROR("descending chunk out of bounds");
      return 0;
   }

   const size_t len0 = reg_min(f->offs + f->width, d->reg_width) - f->offs;
   if ((n != 0) && (len0 + ((n - 1) * d->reg_width) >= 64)) {
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
      chunk <<= len0 + ((n - 1) * d->reg_width);
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
   if (!d || !d->data || !f) {
      ERROR("null pointer(s) passed");
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
      val >>= len0 + ((n - 1) * d->reg_width);
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

/***********************************************************
 * CONSISTENCY CHECKS
 ***********************************************************/

static int reg_check_field_duplicate_names(const struct reg_field *map)
{
   for (size_t i = 0; map[i].name; i++) {
      for (size_t j = i + 1; map[j].name; j++) {
         if (strcmp(map[i].name, map[j].name) == 0) {
            ERROR("detected duplicate strings");
            return -1;
         }
      }
   }
   return 0;
}

static int reg_clear_buffer(struct reg_dev *d)
{
   if (!d) {
      ERROR("null device");
      return -1;
   }

   if (reg_lock(d)) {
      ERROR("cannot lock the mutex");
      return -1;
   }

   for (size_t i = 0; i < d->reg_num; i++)
      d->data[i] = 0;

   if (reg_unlock(d)) {
      ERROR("cannot unlock the mutex");
      return -1;
   }

   return 0;
}

/**
 * @brief Check no field overlaps with the given one.
 * @param d Pointer to the device structure to query.
 * @param i Field number to check, from 0 to number of fields.
 * @return 0 on success, -1 on error.
 */
static int reg_check_field_overlaps(struct reg_dev *d, const size_t i)
{
   // write all 1's in field i
   const uint64_t mask = reg_mask64(0, d->field_map[i].width);
   if (reg_set(d, d->field_map[i].name, mask)) {
      ERROR("cannot set field i");
      return -1;
   }

   // clear all other fields
   for (size_t j = 0; d->field_map[j].name; j++) {
      if (j != i) {
         if (reg_set(d, d->field_map[j].name, 0)) {
            ERROR("cannot set field j");
            return -1;
         }
      }
   }

   // read back field i
   if (reg_get(d, d->field_map[i].name) != mask) {
      ERROR("cannot read original value; overlap likely");
      return -1;
   }

   // clear field i
   if (reg_set(d, d->field_map[i].name, 0)) {
      ERROR("cannot clear field i");
      return -1;
   }

   // check all registers are now zero
   for (size_t j = 0; d->field_map[j].name; j++) {
      if (reg_get(d, d->field_map[j].name) != 0) {
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
      if (reg_set(d, d->field_map[i].name, mask)) {
         ERROR("cannot set field i");
         return -1;
      }
   }

   // read back all fields
   for (size_t i = 0; d->field_map[i].name; i++) {
      const uint64_t mask = reg_mask64(0, d->field_map[i].width);
      if (reg_get(d, d->field_map[i].name) != mask) {
         ERROR("value not all ones");
         return -1;
      }
   }

   if (reg_lock(d)) {
      ERROR("cannot lock the mutex");
      return -1;
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

   if (reg_unlock(d)) {
      ERROR("cannot unlock the mutex");
      return -1;
   }

   return 0;
}

int reg_check(struct reg_dev *const d)
{
   if (!d || !d->reg_num || !d->field_map) {
      ERROR("invalid device pointer or zero reg_num or null field_map");
      return -1;
   }

   if (d->reg_width > MAX_REG) {
      ERROR("reg_width too large");
      return -1;
   }

   if ((d->lock_fn && !d->unlock_fn) || (!d->lock_fn && d->unlock_fn)) {
      ERROR("both or none of lock_fn, unlock_fn must be given");
      return -1;
   }

   // disable writing to physical device
   const uint16_t flags = d->flags;
   d->flags |= REG_NOCOMM;

   if (reg_check_field_duplicate_names(d->field_map))
      return -1;

   if (reg_clear_buffer(d))
      return -1;

   for (size_t i = 0; d->field_map[i].name; i++) {
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

   return 0;
}

/***********************************************************
 * FIELD MAP MANIPULATION
 ***********************************************************/

/**
 * @brief Find a field by name.
 *
 * @param d Pointer to the device structure to query.
 * @param field Null-terminated name of the field to find.
 * @return The requested field, or NULL on error.
 */
static const struct reg_field *reg_find(const struct reg_dev *const d,
                                        const char *const field)
{
   if (!d || !d->data || !field || !d->field_map) {
      ERROR("invalid argument: null pointer(s) passed");
      return NULL;
   }

   // search for field by name
   const struct reg_field *f = NULL;
   for (size_t i = 0; d->field_map[i].name != NULL; i++) {
      if (strcmp(d->field_map[i].name, field) == 0) {
         f = &d->field_map[i];
         break;
      }
   }

   // not found
   if (!f) {
      ERROR("field name not found in field_map:");
      ERROR(field);
      return NULL;
   }

   // field invalid
   if (f->width == 0 || f->width > MAX_FIELD) {
      ERROR("field width invalid");
      return NULL;
   }

   // check field does not extend outside the device
   const size_t num_regs = reg_cdiv(f->offs + f->width, d->reg_width);

   if (reg_flags(d, f, REG_DESCEND)) {
      if (f->reg + 1 < num_regs) {
         ERROR("too many descending registers");
         return NULL;
      }
   }

   else { // ascending
      if (f->reg + num_regs > d->reg_num) {
         ERROR("too many ascending registers");
         return NULL;
      }
   }

   return f;
}

uint64_t reg_get(struct reg_dev *const d, const char *const field)
{
   if (!d || !d->data || !d->field_map || !field) {
      ERROR("invalid argument: null pointers in dev");
      return 0;
   }

   const struct reg_field *f = reg_find(d, field);
   if (!f) {
      ERROR("invalid field");
      return 0;
   }

   if (reg_lock(d)) {
      ERROR("cannot lock the mutex");
      return 0;
   }

   // assemble chunks into a single number
   uint64_t val          = 0;
   const size_t num_regs = reg_cdiv(f->offs + f->width, d->reg_width);
   for (size_t n = 0; n < num_regs; n++)
      val |= reg_get_chunk(d, f, n);

   if (reg_unlock(d)) {
      ERROR("cannot unlock the mutex");
      return 0;
   }

   return val;
}

int reg_set(struct reg_dev *const d, const char *const field,
            const uint64_t val)
{
   if (!d || !d->data || !field || !d->write_fn) {
      ERROR("invalid argument: null pointer(s) or missing write_fn");
      return -1;
   }

   const struct reg_field *f = reg_find(d, field);
   if (!f) {
      ERROR("invalid field");
      return -1;
   }

   if (f->width < 64 && (val >> f->width) != 0) {
      ERROR("value too large for field width");
      return -1;
   }

   if (reg_lock(d)) {
      ERROR("cannot lock the mutex");
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

   if (reg_unlock(d)) {
      ERROR("cannot unlock the mutex");
      return -1;
   }

   return 0;
}

// end file reg.c
