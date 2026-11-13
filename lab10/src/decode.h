// Instruction decode.
//
// decode() translates a 32-bit encoding into a Decoded structure: the
// operation, the registers it reads and writes, and its immediate, already
// sign-extended and scaled as its format requires. All later consumers (the
// ALU, the memory stage, the hazard unit, the disassembler) read these fields
// and never the raw bits.
//
// The operation is an Op enumerator, so the stages that act on it use a switch
// and the compiler reports a missing case.
//
// The subset is RV32IM plus Zicsr. programs/Makefile builds for
// -march=rv32ima_zicsr, but the A extension's atomics are not decoded, since
// no program here uses them.

#ifndef CS3160_DECODE_H
#define CS3160_DECODE_H

#include "common.h"

enum Op {
    OP_INVALID = 0,  // not an encoding this simulator knows

    // Register-register arithmetic and logic (RV32I).
    OP_ADD, OP_SUB, OP_SLL, OP_SLT, OP_SLTU, OP_XOR, OP_SRL, OP_SRA, OP_OR, OP_AND,

    // Multiply and divide (RV32M).
    OP_MUL, OP_MULH, OP_MULHSU, OP_MULHU, OP_DIV, OP_DIVU, OP_REM, OP_REMU,

    // Register-immediate arithmetic and logic.
    OP_ADDI, OP_SLTI, OP_SLTIU, OP_XORI, OP_ORI, OP_ANDI, OP_SLLI, OP_SRLI, OP_SRAI,

    // Upper immediates.
    OP_LUI, OP_AUIPC,

    // Control flow.
    OP_JAL, OP_JALR,
    OP_BEQ, OP_BNE, OP_BLT, OP_BGE, OP_BLTU, OP_BGEU,

    // Memory.
    OP_LB, OP_LH, OP_LW, OP_LBU, OP_LHU,
    OP_SB, OP_SH, OP_SW,

    // System and ordering.
    OP_FENCE, OP_FENCE_I,
    OP_ECALL, OP_EBREAK, OP_MRET,
    OP_CSRRW, OP_CSRRS, OP_CSRRC, OP_CSRRWI, OP_CSRRSI, OP_CSRRCI,

    OP_COUNT
};

// The six RISC-V instruction formats: the layouts of the register numbers and
// the immediate within the encoding.
enum Format { FMT_NONE, FMT_R, FMT_I, FMT_S, FMT_B, FMT_U, FMT_J };

struct Decoded {
    u32 raw;  // the encoding, kept for the disassembler and for error messages
    u32 pc;   // where it was fetched from

    Op     op;
    Format fmt;

    int rs1;  // register numbers; 0 when the instruction has no such operand
    int rs2;
    int rd;

    // The immediate, sign-extended to 32 bits and scaled per format:
    //   I, S      the byte offset or constant, sign-extended from 12 bits
    //   B, J      the *byte* branch/jump offset, sign-extended (bit 0 always 0)
    //   U         the constant already shifted left by 12
    //   shifts    the shift amount, 0..31
    u32 imm;

    u32 csr;  // CSR number, for the Zicsr instructions only
};

// Decode `raw`, fetched from `pc`. Never fails: an unknown encoding yields
// op == OP_INVALID, and the stage that would execute it decides how to respond.
Decoded decode(u32 raw, u32 pc);

// A Decoded that carries no instruction, for filling in an empty pipeline latch.
Decoded decode_none(void);

// --- Properties of an operation ----------------------------------------------
//
// Functions rather than bit tests, so that each call site states its meaning
// and a new instruction needs one case in each switch.

bool is_branch(Op op);         // conditional: may or may not redirect the PC
bool is_jump(Op op);           // unconditional: always redirects the PC
bool is_load(Op op);
bool is_store(Op op);
bool is_csr(Op op);
bool writes_rd(Op op);         // does it write a register? (x0 writes still count)
bool reads_rs1(Op op);
bool reads_rs2(Op op);

// Bytes touched by a load or a store: 1, 2 or 4. Zero for anything else.
int mem_size(Op op);

// Whether the load sign-extends its value (lb and lh do; lbu, lhu and lw do not).
bool mem_signed(Op op);

// Mnemonic, e.g. "addi". "?" for OP_INVALID.
const char *op_name(Op op);

#endif  // CS3160_DECODE_H
