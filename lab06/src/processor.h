// The processor: architectural state, and the five stages.
//
// The Processor struct holds only state that genuinely belongs to the
// machine: the PC, the registers, what it is connected to, and whether it has
// stopped.

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

    // Set when a store to the tohost address completes. The program has
    // finished; what remains is for the pipeline to drain.
    bool halted;
    i32  exit_status;
};

void processor_init(Processor &cpu, const Config &cfg, Stats &st,
                    MemoryLevel &imem, MemoryLevel &dmem);

// Run until the program stores to tohost or the instruction budget runs out.
// Dispatches on cfg.proc to one of the three models below.
void processor_run(Processor &cpu);

// The three models. Each is one file, and each is a loop around the same five
// stage functions.
void run_single(Processor &cpu);
void run_pipelined(Processor &cpu);
void run_forwarding(Processor &cpu);

// --- Register file ---------------------------------------------------------
//
// x0 is hardwired to zero: reads give 0 and writes are discarded. Both halves
// of that matter -- `addi zero, zero, 0` is how the assembler writes nop, and a
// register file that let it through would quietly corrupt x0.

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

// Read the register operands this instruction needs. Registers it does not read
// come back as 0, so a hazard check never sees a stale value for an operand the
// instruction has no interest in.
void stage_operands(const Processor &cpu, const Decoded &d, u32 *v_rs1, u32 *v_rs2);

struct Executed {
    u32  result;  // ALU result: value, address, or branch/jump target
    bool taken;   // for a branch, whether it is taken
};

Executed stage_execute(const Decoded &d, u32 v_rs1, u32 v_rs2);

// Where the instruction after this one comes from. For everything except a
// taken branch or a jump, that is pc + 4.
u32 next_pc_of(const Decoded &d, const Executed &e);

// Does this instruction send the PC somewhere other than the next instruction
// -- a jump, or a branch that turned out to be taken? The same question
// next_pc_of() answers with an address, asked as a yes or no.
//
// This is the *authoritative* answer, because it reads the branch's own verdict
// out of `e`. Compare stage_writeback()'s `taken_branches`, which has no `e` to
// consult and infers the same thing from where the next instruction came from.
bool redirects_pc(const Decoded &d, const Executed &e);

// What the memory stage did. `value` is the value the register file will be
// given for a load -- already sign- or zero-extended -- or the value written
// for a store, masked to the width of the access.
struct MemAccess {
    bool did_read;
    bool did_write;
    u32  addr;
    u32  value;
    int  latency;
};

// Perform the load or the store, if this instruction is one. `addr` is the ALU
// result; `v_rs2` is the value a store writes.
//
// This is also where the program ends: a store to the configured tohost address
// sets cpu.halted and records the exit status crt.S encoded in it.
MemAccess stage_memory(Processor &cpu, const Decoded &d, u32 addr, u32 v_rs2);

// The value this instruction gives the register file: a load's data, a jump's
// return address, or the ALU result. Only meaningful when writes_rd(d.op).
//
// One definition, because the writeback stage and every forwarding path have to
// agree about it. They did not once: forwarding out of EX/MEM handed over a
// jal's ALU result, which is the target it jumped to and not the return address
// it writes, so the first instruction at a call's target could read a link
// register that had never held that value.
u32 writeback_value(const Decoded &d, u32 result, const MemAccess &m);

// Write the result back, count the instruction as retired, and emit its line of
// the [OUT] trace. `next_pc` is what next_pc_of() returned for this
// instruction -- the trace reports it, and a branch is counted as taken or not
// by comparing it against pc + 4.
void stage_writeback(Processor &cpu, const Decoded &d, u32 result,
                     const MemAccess &m, u32 next_pc);

// --- Shared bits of the run loops -----------------------------------------

// Report an instruction the simulator cannot execute and stop. Fetching one is
// always a symptom of something earlier -- a jump through a clobbered return
// address, most often -- so the message names the PC as well as the encoding.
void processor_illegal(const Processor &cpu, const Decoded &d) __attribute__((noreturn));

// The cycle cost of a pipeline cycle in which an instruction fetch cost
// `fetch_latency` and a data access cost `mem_latency`, per the configured
// overlap rule. Never less than one cycle.
int cycle_cost(const Processor &cpu, int fetch_latency, int mem_latency);

// The same when the two accesses cannot overlap and must be paid one after the
// other. This is what a single-cycle machine does -- it has one datapath and no
// second stage for a fetch to hide in -- and it is also what cycle_cost() does
// under `latency_overlap = sum`, so both go through here.
//
// Note what a latency means, because the two models used to disagree about it:
// a latency of n means the access occupies n cycles *including* the one the
// instruction would have taken anyway, not n cycles on top of it. Hence the
// floor at one rather than an added one.
int cycle_cost_serial(int fetch_latency, int mem_latency);

// Write the architectural state to the log at DEBUG level. Cheap to call: it
// checks whether DEBUG is enabled first.
void processor_dump(const Processor &cpu);

// --- Pipeline latches ------------------------------------------------------
//
// One struct for all four pipeline registers. A latch holds everything known
// about the instruction in it so far: the stages ahead of it fill in more
// fields, and the stages behind it read the ones they need. Fields an
// instruction has not reached yet are simply not looked at.
//
// `valid` is the important field. A pipeline stage is either working on an
// instruction or it is empty, and an empty stage is a first-class thing -- it
// happens on the first four cycles, on every stall, and after every flush. The
// Python version instead used "is this field None?" as the emptiness test,
// which meant a bubble and an instruction whose first operand happened to be
// absent were the same thing.

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

// Put a freshly fetched instruction into a latch. Only the encoding and the PC
// are known at this point -- IF/ID holds bits, and decoding them is the next
// stage's work -- so `d.op` is left as OP_INVALID and nothing but the ID stage
// should look at it.
Latch latch_from_fetch(const Fetched &f);

// Put `d` into a latch as the ID stage would, having read its operands.
Latch latch_from_id(const Decoded &d, u32 v_rs1, u32 v_rs2);

// One line describing what is in each stage, at DEBUG level.
void pipeline_dump(long cycle, const Latch &if_id, const Latch &id_ex,
                   const Latch &ex_mem, const Latch &mem_wb);

#endif  // CS3160_PROCESSOR_H
