// Disassembly: an encoding, or a Decoded, rendered as text.
//
// Disassembly makes the DEBUG log readable, and it verifies the decoder: an
// instruction disassembled here and by riscv-none-elf-objdump must give the
// same text, which detects an immediate taken from the wrong bits far more
// reliably than running a program does.
//
// The text follows objdump's no-aliases output: machine mnemonics rather than
// pseudo-instructions (`addi zero, zero, 0`, not `nop`), ABI register names,
// and branch and jump targets as absolute addresses.

#ifndef CS3160_DISASM_H
#define CS3160_DISASM_H

#include <string>

#include "decode.h"

// The ABI name of register `n`: "zero", "ra", "sp", "a0", "s2", ...
const char *reg_name(int n);

// Render a decoded instruction. The PC from which a branch or jump target is
// computed is part of the Decoded.
std::string disasm(const Decoded &d);

// Decode `raw`, fetched from `pc`, and render it.
std::string disasm(u32 raw, u32 pc);

#endif  // CS3160_DISASM_H
