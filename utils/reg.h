// SPDX-License-Identifier: MIT
/**
 * @file reg.h: Register representation and handling
 *
 * This file introduces a flexible notation to represent fields in a device
 * register, and the methods to operate on such register maps.
 *
 * @author Jakob Kastelic
 * @copyright Copyright (c) 2025 Stanford Research Systems, Inc.
 */

#ifndef REG_H
#define REG_H

#include <stddef.h>
#include <stdint.h>

#define REG_READONLY  (1U << 0U)
#define REG_WRITEONLY (1U << 1U)
#define REG_VOLATILE  (1U << 2U)
#define REG_NOCOMM    (1U << 3U)
#define REG_ALIAS     (1U << 4U)
#define REG_DESCEND   (1U << 5U)
#define REG_MSR_FIRST (1U << 6U)

struct reg_field {
   const char *name;
   const size_t reg;
   const uint8_t offs;
   const uint8_t width;
   const uint8_t flags;
};

struct reg_device {
   uint8_t reg_width;
   size_t reg_num;
   uint32_t *data;
   const struct reg_field *field_map;
   uint32_t (*read_fn)(size_t reg);
   int (*write_fn)(size_t reg, uint32_t val);
   uint8_t flags;
};

struct reg_meta {
   const char **fields;
   uint64_t *data;
   struct reg_field **base_maps;
   int (*activate)(int id);
   struct reg_device base;
};

/**
 * \newpage
 * *Method summary:*
 * @allfunc
 * Please refer to the detailed function documentation in the following pages.
 */

/**
 * @subsection A Simple Example
 *
 * To start, define the register map with only the fields that are needed in
 * the application (even if the underlying device has more):
 *
 *     const struct reg_field dev_map[] = {
 *        // name  reg  offs width flags
 *        {"EN_X",  0,   0,   1,    0},
 *        {"FTW",   0,   1,   36,   0},
 *        {"MODE",  1,   5,   27,   0},
 *        // registers 2--4 unused
 *        {"SETP",  5,   0,   32,   0},
 *        { NULL,   0,   0,   0,    0} // mandatory termination!
 *     };
 *
 * Next, allocate `reg_num` words (32 bits each) of storage for the register
 * data and define a device data structure:
 *
 *     #define NUM_REGS 6
 *     uint32_t dev_data[NUM_REGS];
 *
 *     struct reg_device dev = {
 *        .reg_width = 32,
 *        .reg_num   = NUM_REGS,
 *        .field_map = dev_map,
 *        .data      = dev_data,
 *        .read_fn   = dev_read_fn,
 *        .write_fn  = dev_write_fn,
 *     };
 *
 * To set the value of a field, update the buffer, and write the register(s)
 * whose data has been changed to the underlying physical device, just call
 * `reg_set()`:
 *
 *     if (reg_set(&dev, "MODE", 0x03U))
 *        ;// handle the error
 *
 * The data has now been transferred to the physical device and is also stored
 * in the buffer. To retrieve the value from the buffer:
 *
 *     uint64_t val = reg_get(&dev, "MODE");
 *
 * To force re-reading the field from the physical device, set the
 * `REG_VOLATILE` field or device flag. Additional flags are documented in the
 * following sections.
 */

/**
 * @subsection ``Raw'' Register Access
 *
 * These functions are not intended to be used with explicit (literal)
 * arguments, as in `reg_read(0x12D)` or `reg_write(REG_XYZ)`. That would
 * either introduce magic numbers or, likely, duplicate information already
 * present in the register map. Instead, they are intended for programmatic use,
 * such as in a loop to write default values to all registers at once.
 *
 * To prevent reading or writing from the underlying physical device, set the
 * `REG_NOCOMM` device flag before calling `reg_read()` and `reg_write()`. In
 * that case, the values will be read and written to the data buffer only.
 *
 * Note that multi-byte registers are stored native-endian and no conversions
 * are done when retrieving the data. This should not pose problems so long as
 * storing and retrieving are done by devices having the same endianness.
 */

/**
 * @api
 */

/// @func Read register from physical device and update buffer.
uint32_t reg_read(const struct reg_device *d, size_t reg);
/// @param `d` Device to read from.
/// @param `reg` Sequential register number.
/// @return Register value. On failure, return 0.
/// @endfunc

/// @func Write register to physical device and update buffer.
int reg_write(struct reg_device *d, size_t reg, uint32_t val);
/// @param `d` Device data structure.
/// @param `reg` Sequential register number.
/// @param `val` Value to write into the register.
/// @return 0 on success, $-1$ on error.
/// @endfunc

/// @func Bulk import of register data into the device data structure.
int reg_bulk(struct reg_device *d, const uint32_t *data);
/// @param `d` Device data structure.
/// @param `data` Data to read from, at least `d->reg_num` words (32 bits each).
/// If pointer is `NULL`, all the data will be cleared to 0.
/// @return 0 on success, $-1$ on error.
/// @endfunc

/**
 * After import, all the fields are assumed to be "clean", i.e. up-to-date with
 * the physical device. Thus, `reg_bulk()` does not call `d->write_fn()`.
 */

/**
 * @subsection Field Maps and Fields
 *
 * Registers are at most 32 bits wide, fields 64 bits. Thus a field can span
 * several registers, depending on its width and offset in the starting
 * register, and the register width. All field names within a single register
 * map of a physical device must be unique; however, the same name may be reused
 * between several different register maps belonging to the same meta device
 * (see next section).
 *
 * Partially defined registers are not allowed; each register must be either
 * fully covered by fields, or not at all. However, gaps spanning entire
 * registers are allowed in the map, in case none of the fields in these ``gap''
 * registers are needed in the application.
 *
 * Omitting unneeded registers from the map speeds up the field search, as does
 * sorting the field map to place more frequently used registers at the top.
 * However, with realistically sized register maps (up to a couple hundred
 * fields), the field lookup is unlikely to be a performance bottleneck.
 *
 * Register maps must be terminated with the sentinel field `{NULL, 0, 0, 0,
 * 0}`.
 */

/**
 * @subsubsection Field and Device Flags
 *
 * Each field can carry zero or more flags. In addition, the same flags can be
 * applied to the device as whole. Device flags apply to all the fields in the
 * device and thus override field flags. Field flags are fixed, while device
 * flags may be turned on and off at runtime. The available flags are as
 * follows:
 *
 * @begin itemize
 *
 * @item `REG_VOLATILE` means that each time we get a field value, the register
 * in which the field resides will be read anew from the physical device.
 *
 * @item `REG_NOCOMM` will disable, for the field or device on which it is set,
 * all reading and writing of data to the physical device. This flag overrides
 * any `REG_VOLATILE` flags set for the same field or device.
 *
 * @item `REG_MSR_FIRST` specifies that the most-significant register (MSR)
 * should be written to the underlying device first when setting a
 * multi-register field. Unlike `REG_DESCEND`, only the order of writes is
 * affected, not the register layout.
 *
 * @item `REG_DESCEND` reverses the register order in the layout for
 * multi-register fields. (See detailed discussion below.)
 *
 * @end itemize
 *
 * Other flags are currently not implemented.
 */

/**
 * @subsubsection Ascending and Descending Fields
 *
 * By default, fields spanning multiple registers have lower bits (LSBs) in
 * lower-numbered registers. If the field has the `REG_DESCEND` flag set, then
 * the behavior is reversed: LSBs are set in *higher* registers.
 * Nevertheless, the order of bits and bytes within a single register is the
 * same either way (native endian).
 *
 * Registers are indexed from `reg = 0` upwards. Each register contains
 * `reg_width` bits (up to 32). A field in a register reg is packed
 * contiguously across registers, starting at `data[reg]`.
 *
 * For example, with `reg_width = 8`, consider the register map
 *
 *     const struct reg_field dev_map[] = {
 *        // name     reg offs width flags
 *        {"FIELD_UP", 0,  0,   12,  0},
 *        {"X",        1,  4,   4,   0},
 *        {"Y",        2,  4,   4,   0},
 *        {"FIELD_DN", 3,  0,   12,  REG_DESCEND},
 *        { NULL,      0,  0,   0,   0}
 *     };
 *
 * Here, the ascending field `FIELD_UP` will occupy register bits as follows:
 *
 *     data[0]: bits 0 through 7 (total 8 bits) <-- LSB
 *     data[1]: bits 0 through 3 (total 4 bits)
 *
 * The descending field `FIELD_DN` will have the register layout:
 *
 *     data[2]: bits 0 through 3 (total 4 bits)
 *     data[3]: bits 0 through 7 (total 8 bits) <-- LSB
 *
 * The difference is that the least significant bits of `FIELD_UP` are placed in
 * `data[0]`, while for `FIELD_DN`, they are placed in `data[3]`. In any case,
 * the least significant bits are always placed in the register `f->reg`, while
 * the most significant bits are aligned according to the platform endianness.
 *
 * Unless `REG_MSR_FIRST` is specified, the registers are always written to the
 * physical device starting with the least significant register. Thus for
 * ascending fields, lower registers will be written first, while for descending
 * fields, higher registers will be written first. Set the `REG_MSR_FIRST` flag
 * to invert this behavior.
 */

/**
 * @api
 */

/// @func Check map of register fields for consistency.
int reg_check(struct reg_device *d);
/// @param `d` Device data structure to check.
/// @return 0 on success, $-1$ on failure.
/// @endfunc

/**
 * Note that `reg_check()` clears the device buffer after running the checks.
 * The checks involve repeated reading and writing to the buffer, but not the
 * underlying device.
 *
 * It is recommended to call `reg_check()` once for each new or modified
 * register map to ensure map consistency. The behavior of functions that make
 * use of the register map is undefined when a map does not pass the
 * `reg_check()`. On the other hand, `reg_check()` is expected to catch any
 * malformed maps and return an error.
 */

/// @func Get the value of a given field from the device buffer.
uint64_t reg_get(struct reg_device *d, const char *field);
/// @param `d` Device data structure to read from.
/// @param `field` Null-terminated field name.
/// @return Register value. On failure, return 0.
/// @endfunc

/**
 * If a field has the `REG_VOLATILE` flag, then each call to `reg_get()` will
 * re-read the register in which this field is stored from the physical device.
 * Otherwise, the value is obtained from the device buffer.
 */

/// @func Set the value of a given field on the physical device.
int reg_set(struct reg_device *d, const char *field, uint64_t val);
/// @param `d` Device data structure to modify.
/// @param `val` Value to set in the field.
/// @param `field` Null-terminated field name.
/// @return 0 on success, $-1$ on failure.
/// @endfunc

/**
 * @subsection Meta Devices
 *
 * Meta devices extend a physical device, as represented by `struct reg_device`,
 * by allowing a virtually unlimited number of registers to be written and read
 * back. When a meta device detects that the requested field is not found in the
 * currently active register map, it will automatically call `m->activate()` to
 * reload a different register map on the device, including the field values
 * previously set.
 *
 * For example, assume we have a meta device with these fields:
 *
 *     const char *meta_fields[] = {
 *        "VAL_A", "VAL_B", "VAL_C",
 *        "VAL_P", "VAL_Q", NULL
 *     };
 *
 * However, if the underlying physical device (perhaps due to space limitations)
 * supports only three fields at a time, the we have to break up the ``meta
 * map'' into two physical maps, such as
 *
 *     const struct reg_field map1[] = {
 *        // name   reg  offs width flags
 *        {"VAL_A", 0,   0,   8,    0},
 *        {"VAL_B", 0,   8,   8,    0},
 *        {"VAL_C", 1,  16,   16,   0},
 *        { NULL,   0,  0,    0,    0}
 *     };
 *
 *     const struct reg_field map2[] = {
 *        // name   reg  offs width flags
 *        {"VAL_P", 0,   0,   8,    0},
 *        {"VAL_Q", 0,   8,   8,    0},
 *        { NULL,   0,   0,   0,    0}
 *     };
 *
 * To be able to manipulate all fields in an equal way, we define a virtual
 * device:
 *
 *     uint64_t data[5];
 *
 *     const struct reg_field *bm[] = {map1, map2};
 *
 *     struct reg_meta mdev = {
 *        .fields    = meta_fields,
 *        .data      = data,
 *        .base_maps = bm,
 *        .activate  = reconfigure_fn,
 *        .base = {
 *           .reg_width = 32,
 *           .reg_num   = NUM_REGS,
 *           .field_map = dev_map,
 *           .data      = dev_data,
 *           .read_fn   = dev_read_fn,
 *           .write_fn  = dev_write_fn,
 *        },
 *     };
 *
 * Now data access is as simple as before:
 *
 *     reg_adjust(&mdev, "VAL_A", 0x12);
 *     reg_adjust(&mdev, "VAL_P", 0x7F);
 *     reg_obtain(&mdev, "VAL_A");
 *
 * Behind the scenes, the code will load `map1` to set the value of field
 * `"VAL_A"`, then reload the configuration to `map2` to allow setting
 * `"VAL_P"`. When the we demand to read the value of `"VAL_A"` again, the
 * configuration is again changed to `map1`.
 */

/**
 * @api
 */

/// @func Get the value of a given meta field.
int reg_obtain(struct reg_meta *m, const char *field);
/// @param `m` Meta device data structure to read from.
/// @param `field` Null-terminated field name.
/// @return Register value. On failure, return 0.
/// @endfunc

/// @func Set the value of a given meta field.
int reg_adjust(struct reg_meta *m, const char *field, uint64_t val);
/// @param `m` Meta device data structure to modify.
/// @param `val` Value to set in the field.
/// @param `field` Null-terminated field name.
/// @return 0 on success, $-1$ on failure.
/// @endfunc

#endif // REG_H

// end file reg.h
