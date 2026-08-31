#include "disasm.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

static const char *REG_NAMES[32] = {
    "zero", "ra", "sp", "gp", "tp",  "t0",  "t1", "t2",
    "s0",   "s1", "a0", "a1", "a2",  "a3",  "a4", "a5",
    "a6",   "a7", "s2", "s3", "s4",  "s5",  "s6", "s7",
    "s8",   "s9", "s10", "s11", "t3", "t4", "t5", "t6",
};

const char *reg_name(int n) {
    if (n < 0 || n > 31) {
        return "x?";
    }
    return REG_NAMES[n];
}

// The control and status registers the shipped runtime touches. crt.S sets
// mstatus and mtvec, reads mhartid, and its trap handler reads mcause and mepc.
// Anything else is printed as a number.
static const char *csr_name(u32 csr) {
    switch (csr) {
        case 0x001: return "fflags";
        case 0x002: return "frm";
        case 0x003: return "fcsr";
        case 0x300: return "mstatus";
        case 0x304: return "mie";
        case 0x305: return "mtvec";
        case 0x340: return "mscratch";
        case 0x341: return "mepc";
        case 0x342: return "mcause";
        case 0x343: return "mtval";
        case 0x344: return "mip";
        case 0xf11: return "mvendorid";
        case 0xf12: return "marchid";
        case 0xf13: return "mimpid";
        case 0xf14: return "mhartid";
        default:    return NULL;
    }
}

// The four bits of a fence's predecessor or successor set, in the order the
// manual lists them.
static std::string fence_set(u32 v) {
    std::string s;
    if (v & 0x8) s += "i";
    if (v & 0x4) s += "o";
    if (v & 0x2) s += "r";
    if (v & 0x1) s += "w";
    if (s.empty()) {
        s = "0";
    }
    return s;
}

static std::string format(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

static std::string format(const char *fmt, ...) {
    char    buf[160];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    return std::string(buf);
}

std::string disasm(const Decoded &d) {
// TODO(week 4): render a decoded instruction as text.
//
// The format is the one objdump prints with -M no-aliases, because that
// is what your output is compared against:
//
//     addi t0, zero, 1
//     lw t0, 8(t1)
//     beq t0, t1, 0x80000010
//
// Three things to get right: register names come from reg_name() above
// (ABI names, not x5); a load or store puts its offset in the
// offset(base) form; and a branch or jump prints its **target address**,
// not its offset -- which you can work out because d.pc is right there.
//
// format() above is a printf that returns a std::string, which is
// usually the shortest way to write each case.
(void)csr_name; (void)fence_set; (void)format;

std::string inst = op_name(d.op);
inst += " ";
if(d.fmt == FMT_R || d.fmt == FMT_I || d.fmt == FMT_U || d.fmt == FMT_J){
    inst += REG_NAMES[d.rd];
    inst += ", ";
}
if(d.op == OP_JALR || d.op == OP_LB || d.op == OP_LH || d.op == OP_LW || d.op == OP_LBU || d.op == OP_LHU || d.op == OP_SB || d.op == OP_SH || d.op == OP_SW){
    inst += std::to_string(d.imm);
    inst += "(";
    inst += REG_NAMES[d.rs1];
    inst += ")";
}
else{
    if(d.fmt == FMT_R || d.fmt == FMT_I || d.fmt == FMT_S || d.fmt == FMT_B){
        inst += REG_NAMES[d.rs1];
        inst += ", ";
    }
    if(d.fmt == FMT_R || d.fmt == FMT_S || d.fmt == FMT_B){
        inst += REG_NAMES[d.rs2];
    } else if(d.fmt == FMT_I){
        inst += std::to_string(d.imm);
    } else if(d.fmt == FMT_U){
        inst += "0x";
        inst += std::to_string(d.imm);
    }
}
if(d.fmt == FMT_B || d.fmt == FMT_J){
    inst += ", ";
    inst += "0x";
    inst += std::to_string(d.pc + d.imm);
}

return inst;
}

std::string disasm(u32 raw, u32 pc) {
    return disasm(decode(raw, pc));
}
