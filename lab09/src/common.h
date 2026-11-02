// Types and small helpers used everywhere in the simulator.
//
// The simulated machine is RV32: every architectural value is 32 bits wide,
// viewed in two ways:
//
//   u32  the bit pattern: addresses, encodings, unsigned comparisons
//   i32  the same bits as a signed number: signed comparisons
//
// Conversion between them reinterprets the bits without changing them, so the
// helpers below are casts, not arithmetic.

#ifndef CS3160_COMMON_H
#define CS3160_COMMON_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t  i32;
typedef int64_t  i64;

// Read the bits of `v` as a signed 32-bit number.
inline i32 as_signed(u32 v) { return (i32)v; }

// Read the bits of `v` as an unsigned 32-bit number.
inline u32 as_unsigned(i32 v) { return (u32)v; }

// Sign-extend the low `bits` bits of `v` to a full 32-bit value.
// Example: sign_extend(0xFFF, 12) == 0xFFFFFFFF (that is, -1).
inline u32 sign_extend(u32 v, int bits) {
    u32 mask = (bits >= 32) ? 0xFFFFFFFFu : ((1u << bits) - 1u);
    v &= mask;
    u32 sign = 1u << (bits - 1);
    if (v & sign) {
        v |= ~mask;
    }
    return v;
}

// Extract bits [hi:lo] of `v`, shifted down to bit 0.
inline u32 bits(u32 v, int hi, int lo) {
    return (v >> lo) & ((hi - lo + 1 >= 32) ? 0xFFFFFFFFu : ((1u << (hi - lo + 1)) - 1u));
}

// Whether `v` is a power of two, as cache and block sizes must be.
inline bool is_power_of_two(u32 v) { return v != 0 && (v & (v - 1)) == 0; }

// log2 of a power of two. Undefined otherwise; callers check first.
inline int log2_exact(u32 v) {
    int n = 0;
    while ((1u << n) < v) {
        n++;
    }
    return n;
}

// Report an unrecoverable condition and stop.
//
// Every call indicates an error in the simulated program, the configuration or
// the simulator itself; stopping is preferable to continuing incorrectly.
void fatal(const char *fmt, ...) __attribute__((format(printf, 1, 2), noreturn));

#endif  // CS3160_COMMON_H
