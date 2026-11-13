// The processor: architectural state and the five stages.
//
// Processor holds only the state of the machine itself: the PC, the registers,
// the memories it is connected to, and whether it has stopped.

#ifndef CS3160_PROCESSOR_H
#define CS3160_PROCESSOR_H

#include "config.h"
#include "decode.h"
#include "memory.h"
#include "stats.h"

struct Processor {
    const Config *cfg;
    Stats        *stats;
    MemoryLevel  *imem;  // what instruction fetch reads from
    MemoryLevel  *dmem;  // what loads and stores go to

    u32 pc;
    u32 regs[32];

    // Set when a store to the tohost address completes: the program has
    // finished, and only the pipeline remains to drain.
    bool halted;
    i32  exit_status;
};

void processor_init(Processor &cpu, const Config &cfg, Stats &st,
                    MemoryLevel &imem, MemoryLevel &dmem);

// Run until the program stores to tohost or the instruction budget runs out.
// Dispatches on cfg.proc to one of the three models below.
void processor_run(Processor &cpu);

// The three models, one per file, each a loop around the same stage functions.
void run_single(Processor &cpu);
void run_pipelined(Processor &cpu);
void run_forwarding(Processor &cpu);

// --- Register file ---------------------------------------------------------
//
// x0 is hardwired to zero: reads return 0 and writes are discarded. The second
// rule matters because the assembler encodes nop as `addi zero, zero, 0`.

u32  reg_read(const Processor &cpu, int n);
void reg_write(Processor &cpu, int n, u32 value);

// --- The five stages -------------------------------------------------------

struct Fetched {
    u32 raw;
    u32 pc;
    int latency;
};

// Read the instruction at `pc`. Counts one instruction-side memory access.
Fetched stage_fetch(Processor &cpu, u32 pc);

// Read the register operands of `d`. An operand the instruction does not read
// is returned as 0.
void stage_operands(const Processor &cpu, const Decoded &d, u32 *v_rs1, u32 *v_rs2);

struct Executed {
    u32  result;  // ALU result: value, address, or branch/jump target
    bool taken;   // for a branch, whether it is taken
};

Executed stage_execute(const Decoded &d, u32 v_rs1, u32 v_rs2);

// The address of the next instruction: pc + 4, except after a taken branch or a
// jump.
u32 next_pc_of(const Decoded &d, const Executed &e);

// Whether the instruction redirects the PC away from pc + 4: a jump, or a taken
// branch. next_pc_of() answers the same question with an address.
//
// This answer is *authoritative*, since it reads the branch outcome from `e`.
// stage_writeback()'s `taken_branches`, by contrast, has no `e` and infers the
// outcome from the address of the next instruction.
bool redirects_pc(const Decoded &d, const Executed &e);

// The result of the memory stage. `value` is, for a load, the value for the
// register file (already sign- or zero-extended) and, for a store, the value
// written, masked to the width of the access.
struct MemAccess {
    bool did_read;
    bool did_write;
    u32  addr;
    u32  value;
    int  latency;
};

// Perform the load or store, if the instruction is one. `addr` is the ALU
// result; `v_rs2` is the value a store writes.
//
// The program ends here: a store to the configured tohost address sets
// cpu.halted and records the exit status that crt.S encodes in it.
MemAccess stage_memory(Processor &cpu, const Decoded &d, u32 addr, u32 v_rs2);

// The value written to the register file: a load's data, a jump's return
// address, or the ALU result. Meaningful only when writes_rd(d.op).
//
// Writeback and every forwarding path must agree on this value, hence the
// single definition. For jal, the ALU result is the jump target, not the
// return address that rd receives.
u32 writeback_value(const Decoded &d, u32 result, const MemAccess &m);

// Write the result back, count the instruction as retired, and emit its [OUT]
// trace line. `next_pc` is the value next_pc_of() returned for it: the trace
// reports it, and a branch is counted as taken if it differs from pc + 4.
void stage_writeback(Processor &cpu, const Decoded &d, u32 result,
                     const MemAccess &m, u32 next_pc);

// --- Shared bits of the run loops -----------------------------------------

// Report an instruction that cannot be executed, and stop. Reaching one is a
// symptom of an earlier error, most often a jump through an overwritten return
// address, so the message gives the PC as well as the encoding.
void processor_illegal(const Processor &cpu, const Decoded &d) __attribute__((noreturn));

// The cost of a pipeline cycle whose instruction fetch took `fetch_latency` and
// whose data access took `mem_latency`, under the configured overlap rule. At
// least one cycle.
int cycle_cost(const Processor &cpu, int fetch_latency, int mem_latency);

// The same when the two accesses cannot overlap and are paid in sequence: the
// single-cycle machine has one datapath and no second stage in which a fetch
// could overlap, and cycle_cost() behaves so under `latency_overlap = sum`.
//
// A latency of n means the access occupies n cycles *including* the cycle the
// instruction takes in any case, not n further cycles; hence a floor of one
// rather than an added one.
int cycle_cost_serial(int fetch_latency, int mem_latency);

// Write the architectural state to the log at DEBUG level; returns at once when
// DEBUG is disabled.
void processor_dump(const Processor &cpu);

// --- Pipeline latches ------------------------------------------------------
//
// One type for all four pipeline registers. A latch holds everything known so
// far about its instruction: each stage fills in further fields, and later
// stages read those they need. Fields not yet filled in are not read.
//
// `valid` distinguishes an instruction from an empty stage (a bubble), which
// occurs in the first four cycles, on every stall and after every flush.

struct Latch {
    bool valid;

    Decoded d;

    u32 v_rs1;  // operand values, as read in ID (or forwarded into EX)
    u32 v_rs2;

    Executed  e;        // filled in by EX
    u32       next_pc;  // where this instruction says the next one comes from
    MemAccess m;        // filled in by MEM
};

// An empty latch: a bubble.
Latch latch_bubble(void);

// A latch holding a fetched instruction. Only the encoding and the PC are known:
// IF/ID holds bits, which ID decodes. `d.op` is therefore OP_INVALID, and only
// ID may read the latch.
Latch latch_from_fetch(const Fetched &f);

// A latch holding `d` and its operand values, as written by ID.
Latch latch_from_id(const Decoded &d, u32 v_rs1, u32 v_rs2);

// One DEBUG line giving the contents of each stage.
void pipeline_dump(long cycle, const Latch &if_id, const Latch &id_ex,
                   const Latch &ex_mem, const Latch &mem_wb);

#endif  // CS3160_PROCESSOR_H
