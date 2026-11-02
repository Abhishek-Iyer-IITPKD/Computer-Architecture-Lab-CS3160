# CS3160 -- the assembly programs the simulator runs.
#
#   make          assemble every asms/*.s into bins/ and dumps/
#   make run      run every program in bins/ on Spike, logs into runs/
#   make bins/1-even.r5o      build just one program
#   make runs/1-even.iss      build and run just one program
#   make clean    remove bins/ dumps/ runs/
#
# The programs against which the simulator is tested, provided for reading and
# for adding further programs. `make` produces a .r5ob image of each, the format
# build/sim loads:
#
#     make
#     ../build/sim --proc=single bins/1-even.r5ob
#
# The test images are prebuilt in tests/images/, so the tests need neither this
# Makefile nor the RISC-V toolchain; it is provided for experimentation.
#
# This file should not need editing.

RISCV = riscv-none-elf-
ARCH  = rv32ima_zicsr
ABI   = ilp32

# -nostdlib/-nostartfiles: the start-up code is custom/crt.S, and no C library
# is linked. custom/test.ld fixes the addresses of code and data.
ASFLAGS = -T./custom/test.ld -static -mcmodel=medany \
          -fvisibility=hidden -nostdlib -nostartfiles -g \
          ./custom/crt.S -I./custom \
          -march=$(ARCH) -mabi=$(ABI)

# --log-commits prints one line per retired instruction: the execution trace
# that the tests inspect. MAX_STEPS bounds a non-terminating program; a correct
# program exits through tohost first.
MAX_STEPS = 100000
SPIKE_FLAGS = --isa=$(ARCH) --priv=msu --steps=$(MAX_STEPS) --log-commits -l

ASMS  := $(wildcard asms/*.s)
BINS  := $(patsubst asms/%.s,bins/%.r5o,$(ASMS))
DUMPS := $(patsubst asms/%.s,dumps/%.dump,$(ASMS))
RUNS  := $(patsubst asms/%.s,runs/%.iss,$(ASMS))

.PHONY: all run clean
all: $(BINS) $(DUMPS)
run: $(RUNS)

# .r5o is the ELF; .r5ob is the same program as a flat binary image, the format
# the course's simulator loads.
bins/%.r5o: asms/%.s custom/test.ld custom/crt.S
	@mkdir -p bins
	$(RISCV)gcc $< $(ASFLAGS) -o $@
	$(RISCV)objcopy -O binary $@ $@b

dumps/%.dump: bins/%.r5o
	@mkdir -p dumps
	$(RISCV)objdump -d $< > $@

runs/%.iss: bins/%.r5o
	@mkdir -p runs
	-spike $(SPIKE_FLAGS) $< 2> $@

clean:
	rm -rf bins dumps runs
