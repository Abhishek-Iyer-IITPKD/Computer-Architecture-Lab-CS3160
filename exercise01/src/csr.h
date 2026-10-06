// The control and status registers.
//
// A RISC-V hart has a second register file beside x0..x31, addressed by a
// 12-bit number rather than a 5-bit one, and reached only by the Zicsr
// instructions. It holds the machine's *state about itself* -- what mode it is
// in, where a trap should jump to, which core this is -- rather than the
// program's data.
//
// It is a register file, and not memory. Nothing about a CSR access leaves the
// core: there is no address, no bus, no cache and no RAM involved. That is why
// this file holds only the storage and the lookup, and why the access itself
// lives in processor.cpp beside reg_read() and reg_write() -- a CSR is read in
// the same stage as x[rs1] and written in the same stage as x[rd].
//
// This simulator models five of them, which is exactly the set crt.S touches:
//
//     0x300  mstatus   machine status: interrupt enables, previous mode
//     0x305  mtvec     trap vector: where control goes when a trap fires
//     0x341  mepc      the PC a trap interrupted, so it can be resumed
//     0x342  mcause    why the trap fired
//     0xF14  mhartid   which hardware thread this is. Read-only, and 0 here:
//                      the simulator has one core.
//
// Everything else is an error rather than a zero. A program reading a CSR this
// simulator does not have is either using a feature that is not modelled or
// has jumped somewhere it should not have, and both are worth being told about
// -- silently returning 0 makes the first look like it worked.
//
// The file is small and flat: five words and a lookup.

#ifndef CS3160_CSR_H
#define CS3160_CSR_H

#include "common.h"

enum CsrNum {
    CSR_MSTATUS = 0x300,
    CSR_MTVEC   = 0x305,
    CSR_MEPC    = 0x341,
    CSR_MCAUSE  = 0x342,
    CSR_MHARTID = 0xF14,
};

// How many the file holds. Five, in the order above.
enum { CSR_COUNT = 5 };

struct Csrs {
    u32 word[CSR_COUNT];
};

// All five to zero, which is the state after reset. mhartid stays zero for the
// life of the machine: this simulator has one hart and it is number 0.
void csr_init(Csrs &c);

// Which slot in the file a CSR number names, or -1 if this simulator does not
// have that register.
//
// This is the CSR file's decoder, and it is the counterpart of using `n` as an
// index into regs[] for a general-purpose register. The mapping is deliberately
// a function rather than `word[num]`: CSR numbers are spread across the 12-bit
// space (0x300 to 0xF14 here), so an array indexed by the number itself would
// be four thousand words to hold five.
int csr_index(u32 num);

// Is this register writable? mhartid is not: it reports which core is running
// and no program may change that. A write to it is discarded rather than
// refused -- a real machine traps, and this simulator has no traps.
bool csr_writable(u32 num);

// The name for the trace and the log, or "?" for one this simulator has not
// got.
const char *csr_mnemonic(u32 num);

#endif  // CS3160_CSR_H
