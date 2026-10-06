// Types and small helpers used everywhere in the simulator.
//
// The simulated machine is RV32: every architectural value is 32 bits wide.
// We keep two views of the same bits:
//
//   u32  - the bit pattern, used for addresses, encodings and unsigned compares
//   i32  - the same bits read as a signed number, used for signed compares
//
// Converting between them is always just a reinterpretation, never a change of
// value, which is why the helpers below are casts and not arithmetic.

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

// Is `v` a power of two? Cache sizes and block sizes must be.
inline bool is_power_of_two(u32 v) { return v != 0 && (v & (v - 1)) == 0; }

// log2 of a power of two. Undefined for anything else, so callers check first.
inline int log2_exact(u32 v) {
    int n = 0;
    while ((1u << n) < v) {
        n++;
    }
    return n;
}

// Report a condition the simulator cannot continue past and stop.
//
// Everything reached through this function is a bug, either in the simulated program,
// or in the configuration, or in the simulator itself. Stop rather than fail silently.
void fatal(const char *fmt, ...) __attribute__((format(printf, 1, 2), noreturn));

#endif  // CS3160_COMMON_H
