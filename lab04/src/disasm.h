// Disassembly: an encoding, or a Decoded, rendered as text.
//
// This exists for three reasons. It makes the DEBUG log readable, and it is how
// the decoder is checked: the same instruction disassembled by this file and by
// riscv-none-elf-objdump must come out the same, which catches an immediate
// assembled from the wrong bits far more reliably than running a program does.
//
// The text follows objdump's --no-aliases output: real mnemonics rather than
// pseudo-instructions (so `addi zero, zero, 0` rather than `nop`), ABI register
// names, and branch and jump targets as absolute addresses.

#ifndef CS3160_DISASM_H
#define CS3160_DISASM_H

#include <string>

#include "decode.h"

// The ABI name of register `n` -- "zero", "ra", "sp", "a0", "s2", ... Lab 1
// works in register numbers, but everything from lab 2 on uses these.
const char *reg_name(int n);

// Render a decoded instruction. Needs no extra arguments: the PC a branch or
// jump target is relative to is already in the Decoded.
std::string disasm(const Decoded &d);

// Convenience for the common case of having only the bits.
std::string disasm(u32 raw, u32 pc);

#endif  // CS3160_DISASM_H
