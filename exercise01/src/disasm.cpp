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
    const char *m   = op_name(d.op);
    const char *rd  = reg_name(d.rd);
    const char *rs1 = reg_name(d.rs1);
    const char *rs2 = reg_name(d.rs2);
    i32         imm = as_signed(d.imm);

    if (d.op == OP_INVALID) {
        return format(".word 0x%08x", d.raw);
    }

    if (is_load(d.op)) {
        return format("%s %s, %d(%s)", m, rd, imm, rs1);
    }
    if (is_store(d.op)) {
        return format("%s %s, %d(%s)", m, rs2, imm, rs1);
    }
    if (is_branch(d.op)) {
        return format("%s %s, %s, 0x%08x", m, rs1, rs2, d.pc + d.imm);
    }
    if (is_csr(d.op)) {
        const char *csr  = csr_name(d.csr);
        std::string cstr = (csr != NULL) ? std::string(csr) : format("0x%x", d.csr);
        // The immediate forms carry a 5-bit constant where the others have rs1.
        if (d.op == OP_CSRRWI || d.op == OP_CSRRSI || d.op == OP_CSRRCI) {
            return format("%s %s, %s, %u", m, rd, cstr.c_str(), d.imm);
        }
        return format("%s %s, %s, %s", m, rd, cstr.c_str(), rs1);
    }

    switch (d.op) {
        case OP_LUI:
        case OP_AUIPC:
            // objdump prints the constant as it was written, before the shift.
            return format("%s %s, 0x%x", m, rd, d.imm >> 12);

        case OP_JAL:
            return format("%s %s, 0x%08x", m, rd, d.pc + d.imm);

        case OP_JALR:
            return format("%s %s, %d(%s)", m, rd, imm, rs1);

        case OP_SLLI:
        case OP_SRLI:
        case OP_SRAI:
            // Shift amounts in hex, which is how objdump prints them.
            return format("%s %s, %s, 0x%x", m, rd, rs1, d.imm);

        case OP_FENCE:
            return format("fence %s,%s",
                          fence_set(bits(d.raw, 27, 24)).c_str(),
                          fence_set(bits(d.raw, 23, 20)).c_str());

        case OP_FENCE_I:
        case OP_ECALL:
        case OP_EBREAK:
        case OP_MRET:
            return std::string(m);

        default:
            break;
    }

    switch (d.fmt) {
        case FMT_R: return format("%s %s, %s, %s", m, rd, rs1, rs2);
        case FMT_I: return format("%s %s, %s, %d", m, rd, rs1, imm);
        default:    break;
    }
    return format("%s ?", m);
}

std::string disasm(u32 raw, u32 pc) {
    return disasm(decode(raw, pc));
}
