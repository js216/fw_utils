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
#define REG_NORESET   (1U << 7U)

struct reg_field {
   const char *name;
   const size_t reg;
   const uint8_t offs;
   const uint8_t width;
   const uint16_t flags;
};

struct reg_dev {
   uint16_t flags;

   // register map
   uint8_t reg_width;
   size_t reg_num;
   const struct reg_field *field_map;

   // physical read/write
   int arg;
   uint32_t (*read_fn)(int arg, size_t reg);
   int (*write_fn)(int arg, size_t reg, uint32_t val);

   // data buffer
   uint32_t *data;
   void *mutex;
   int (*lock_fn)(void *mutex);
   int (*unlock_fn)(void *mutex);
   int lock_count;
};

struct reg_virt {
   const char **fields;
   uint64_t *data;
   const struct reg_field **maps;
   int (*load_fn)(int arg, int id);
   struct reg_dev base;
};

/**
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
 *     struct reg_dev dev = {
 *        .reg_width = 32,
 *        .reg_num   = NUM_REGS,
 *        .field_map = dev_map,
 *        .read_fn   = dev_read_fn,
 *        .write_fn  = dev_write_fn,
 *        .data      = dev_data,
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
 * To prevent reading or writing from the underlying physical device, the device
 * may set the `REG_NOCOMM` flag before calling `reg_read()` and `reg_write()`.
 * In that case, the values will be read and written to the data buffer only.
 *
 * Note that multi-byte registers are stored native-endian and no conversions
 * are done when retrieving the data. This should not pose problems so long as
 * storing and retrieving the data from the buffer are done by processors having
 * the same endianness. Typically, the same processor will execute both
 * operations. At any rate, the conversion from the buffer representation to the
 * format required by the target device is implemented in device-specific
 * `read_fn` and `write_fn`; these are responsible for physical data transfer
 * and thus determine the final endianness or even the bit order. See the
 * section below for more details about the Device Data Structure, which defines
 * the hardware-specific register access methods.
 */

/**
 * @api
 */

/// @func Read register from physical device and update buffer.
uint32_t reg_read(struct reg_dev *d, size_t reg);
/// @param `d` Device to read from.
/// @param `reg` Sequential register number.
/// @return Register value. On failure, return 0.
/// @endfunc

/// @func Write register to physical device and update buffer.
int reg_write(struct reg_dev *d, size_t reg, uint32_t val);
/// @param `d` Device data structure.
/// @param `reg` Sequential register number.
/// @param `val` Value to write into the register.
/// @return 0 on success, $-1$ on error.
/// @endfunc

/// @func Bulk import of register data into the device data structure.
int reg_bulk(struct reg_dev *d, const uint32_t *data);
/// @param `d` Device data structure.
/// @param `data` Data to read from, at least `d->reg_num` words (32 bits each).
/// If pointer is `NULL`, all the data will be cleared to 0.
/// @return 0 on success, $-1$ on error.
/// @endfunc

/**
 * After import, all the fields are assumed to be "clean", i.e.\ up-to-date with
 * the physical device. Thus, `reg_bulk()` does not call `d->write_fn()`.
 */

/**
 * @subsection Device Data Structure
 *
 * A device is configured by filling the members of `struct reg_dev`. All
 * members must be set to appropriate values.
 *
 * @subsubsection Data Buffer
 *
 * The register data is stored in an internal data buffer as per the `data`
 * pointer in `struct reg_dev`. The buffer must be a contiguous array of
 * `uint32_t` values, in length at least `reg_num`. The code cannot detect a
 * mismatch between the size of the allocated buffer and `reg_num` and will
 * cause a buffer overrun if the buffer is too small for the given `reg_num`.
 *
 * In a multi-threaded program, some form of synchronization will be required to
 * prevent interleaved write calls from corrupting the data buffer. To this end,
 * the data structure provides space to store a pointer to a `mutex`, which will
 * be passed as argument to the `lock_fn` and `unlock_fn` functions. These
 * functions will be called for each field (but not register!) access, either
 * get or set. They shall return 0 if the locking/unlocking succeeded, and $-1$
 * otherwise.
 *
 * Beware that register access functions `reg_read()` and `reg_write()` do not
 * make use of locking. Locking needs to be done at the level where atomic
 * access is desired. Since field access is implemented on top of register
 * access, the field access is protected so fields spanning across several
 * registers do not get corrupted.
 *
 * If either one of the `lock_fn` and `unlock_fn` is provided, the other one
 * should be as well to prevent a situation where a mutex is locked and never
 * released, or the other way around. This requirement is also enforced by
 * `reg_check()`. However, if locking is not needed, both function pointers can
 * be `NULL`. Also, if the mutex itself is `NULL`, no locking is done whether or
 * not the functions are defined.
 */

/**
 * @subsubsection Physical Read and Write
 *
 * To connect the map of register fields with a physical device, two functions
 * need to be defined, with pointers to them stored in `struct reg_dev`:
 *
 *     uint32_t read_fn(int arg, size_t reg);
 *     int write_fn(int arg, size_t reg, uint32_t val);
 *
 * These handle the hardware specific procedures to get the data in and out of
 * the devices. Their behavior is arbitrary. For example, if no hardware I/O is
 * needed, returning zero is sufficient:
 *
 *     uint32_t test_read_fn(int arg, size_t reg) {return 0;}
 *     int test_write_fn(int arg, size_t reg, uint32_t val) {return 0;}
 *
 * Typically, the functions will reformat the register value in some way and
 * write it to a processor-specific address, where a hardware communication
 * peripheral can pick the data up. For example, the function may reorder the
 * bits to be MSB first, then write to the relevant SPI register for transfer.
 *
 * In case several similar or devices are needed, they can be distinguished by
 * means of the `arg` member in `struct reg_dev`. The code will pass `arg` to
 * `read_fn` and `write_fn` without any processing or checking. The `arg` can
 * have any value, such as the sequential number of an output channel.
 *
 * The `write_fn` shall return 0 on success and $-1$ on error. There is no
 * requirement for the `read_fn` to signal errors, but typically a 0 value
 * should be returned on errors.
 */

/**
 * @subsection Field Maps and Fields
 *
 * Registers are at most 32 bits wide, fields 64 bits. Thus a field can span
 * several registers, depending on its width and offset in the starting
 * register, and the register width. All field names within a single register
 * map of a physical device must be unique, except those starting with the
 * underscore character (e.g., `_RESERVED`); however, the same name may be
 * reused between several different register maps belonging to the same virtual
 * device (see next section).
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
 * Register maps must be terminated with `{NULL, 0, 0, 0, 0}`.
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
 * @item `REG_NORESET` prevents virtual devices from automatically
 * writing this field each time a field map is reloaded. (See the section on
 * virtual devices for more details.)
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
int reg_check(struct reg_dev *d);
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
uint64_t reg_get(struct reg_dev *d, const char *field);
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
int reg_set(struct reg_dev *d, const char *field, uint64_t val);
/// @param `d` Device data structure to modify.
/// @param `val` Value to set in the field.
/// @param `field` Null-terminated field name.
/// @return 0 on success, $-1$ on failure.
/// @endfunc

/**
 * @subsection Virtual Devices
 *
 * Virtual devices extend a physical device, as represented by `struct reg_dev`,
 * by allowing a virtually unlimited number of registers to be written and read
 * back. When a virtual device detects that the requested field is not found in
 * the currently loaded register map, it will try to find it in another map.
 *
 * For example, assume we have a virtual device with these fields:
 *
 *     const char *virt_fields[] = {
 *        "A", "B", "C", "P", "Q",
 *        NULL // mandatory termination!
 *     };
 *
 * However, if the underlying physical device (perhaps due to space limitations)
 * supports only three fields at a time, then we have to break up the ``virtual
 * map'' into two physical maps, such as
 *
 *     const struct reg_field map1[] = {
 *        // name  reg  offs width flags
 *        {"A",     0,   0,   8,    0},
 *        {"B",     0,   8,   8,    0},
 *        {"C",     1,   0,   16,   0},
 *        { NULL,   0,   0,   0,    0}
 *     };
 *
 *     const struct reg_field map2[] = {
 *        // name reg  offs width flags
 *        {"P",    0,   0,   8,    0},
 *        {"Q",    0,   8,   8,    0},
 *        {"A",    1,   0,  16,    0},
 *        { NULL,  0,   0,   0,    0}
 *     };
 *
 * To be able to manipulate the fields across both maps, we define a virtual
 * device:
 *
 *     #define NUM_REGS 2
 *     #define NUM_FIELDS 5
 *
 *     uint32_t dev_data[NUM_REGS];
 *     uint64_t virt_data[NUM_FIELDS];
 *
 *     struct reg_virt vdev = {
 *        .fields   = virt_fields,
 *        .data     = virt_data,
 *        .maps     = (const struct reg_field *[]){map1, map2, NULL},
 *        .load_fn  = dev_load_fn,
 *        .base = {
 *           .reg_width = 16,
 *           .reg_num   = NUM_REGS,
 *           .read_fn   = dev_read_fn,
 *           .write_fn  = dev_write_fn,
 *           .data      = dev_data,
 *        },
 *     };
 *
 * Now data access is as simple as before:
 *
 *     if (reg_verify(&vdev))
 *        ;// handle the error
 *
 *     reg_adjust(&vdev, "A", 0x12);
 *     reg_adjust(&vdev, "P", 0x7F);
 *     reg_obtain(&vdev, "A");
 *
 * Behind the scenes, the code use the `dev_load_fn()` to load `map1` to set
 * the value of field `"A"`, then change the configuration to `map2` to allow
 * setting `"P"`. Since the field `"A"` is shared between the two maps, the code
 * re-loads its value after changing the configuration.
 */

/**
 * @subsubsection Virtual Fields: Unique and Shared
 *
 * A virtual device defines two matched arrays: `fields` is a null-terminated
 * array of strings giving names to the available fields, while `data` contains
 * their value. The two arrays must be allocated to have the same number of
 * elements (though not necessarily bytes, in case `uint64_t` takes up a
 * different amount of space than a string pointer). Thus, the last element of
 * the data array is kept empty, as it corresponds to the `NULL` termination of
 * the `fields` array.
 *
 * All virtual fields must be present in at least one of the maps.
 *
 * If a virtual field appears in only one of the possible maps corresponding to
 * that virtual device, it is considered *unique*. For a unique field, each
 * `adjust` call will check that the map it belongs is loaded. If it is not,
 * the code will automatically load it.
 *
 * On the other hand, *shared* fields---those that appear in more than one
 * map---may or may not require re-loading a new map on field access. Note that
 * unlike in physical field maps, the virtual fields only have a name and a
 * value; no flags, width, or offset are specified. When a field name is
 * shared, it is possible that different maps will assign a different width to
 * it. This complicates the access rules:
 *
 * @begin itemize
 *
 * @item If the field is present in the currently loaded map,
 * and the value fits in the size of the field, then it is set without changing
 * the map. If the value is too large to fit, then we look for the next map
 * containing the same field name and try if it fits, and so on.
 *
 * @item An error is reported if the field name cannot be found in any of the
 * maps belonging to the given virtual device, or if the name can be found,
 * but the value is too large to fit into any of them.
 *
 * @end itemize
 *
 * Unlike adjusting the field value, obtaining it can return the value directly
 * from the data buffer and will not reload the map. In fact, there is no need
 * to consult the individual physical devices at all in the process. In other
 * words, the flag `REG_VOLATILE` has no effect for virtual fields.
 */

/**
 * @subsubsection Load Function
 *
 * When a virtual field cannot be found in the currently loaded map, the code
 * will call a virtual device's `load_fn(int arg, int id)` to reconfigure the
 * hardware as appropriate. The function is given two arguments: `arg` is
 * whatever is set in the underlying device structure (i.e., `vdev->base->arg`),
 * while the `id` is the sequential number of the requested new field map. The
 * numbering starts with 0 and matches the order the maps are listed in
 * `vdev->maps`.
 *
 * Assuming the load function succeeded (i.e., returned 0), we then re-set all
 * the values of all the fields that are present in the new map, both unique
 * and shared. To prevent re-setting a field value, set the `REG_NORESET` flag
 * for the relevant field or device. However, in that case the virtual field
 * data and device data can become unsynchronized. In other words, for a
 * `REG_NORESET` field, `reg_obtain` and `reg_get` may return different values:
 * `reg_get` will report a 0, since the field is cleared and not re-set after
 * loading the new configuration, while `reg_obtain` will still return the
 * previously-set value. This allows a `REG_NORESET` field to be manually set to
 * its old value whenever the application requires it, rather than always after
 * re-loading configurations.
 *
 * If loading a map requires re-setting a field whose value is too large to fit
 * in to new map, the value of the field will be retained in the virtual device
 * buffer, but will not be transferred to the physical device, nor will it be
 * recorded in the physical device buffer. For example, if the old map defines a
 * 16-bit field `"F"`, while the new map defines `"F"` to have only 8 bits, then
 * on re-loading, this field will not be re-set automatically. However, if the
 * configuration is reloaded yet again, such that the field again has
 * sufficient width, then it will be transferred.
 *
 * Set the `REG_NORESET` flag on the field in the new map to prevent setting
 * values automatically on reload. Fields starting with underscore behave as if
 * they had `REG_NORESET` set.
 */

/**
 * @api
 */

/// @func Check virtual device for consistency.
int reg_verify(struct reg_virt *v);
/// @param `v` Virtual device data structure to check.
/// @return 0 on success, $-1$ on failure.
/// @endfunc

/// @func Get the value of a given virtual field.
uint64_t reg_obtain(struct reg_virt *v, const char *field);
/// @param `v` Virtual device data structure to read from.
/// @param `field` Null-terminated field name.
/// @return Register value. On failure, return 0.
/// @endfunc

/// @func Set the value of a given virtual field.
int reg_adjust(struct reg_virt *v, const char *field, uint64_t val);
/// @param `v` Virtual device data structure to modify.
/// @param `val` Value to set in the field.
/// @param `field` Null-terminated field name.
/// @return 0 on success, $-1$ on failure.
/// @endfunc

#endif // REG_H

// end file reg.h
