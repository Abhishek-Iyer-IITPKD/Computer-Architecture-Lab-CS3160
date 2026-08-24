# CS3160 Lab 1 -- build and run RISC-V assembly programs.
#
#   make          assemble every asms/*.s into bins/ and dumps/
#   make run      run every program in bins/ on Spike, logs into runs/
#   make bins/1-even.r5o      build just one program
#   make runs/1-even.iss      build and run just one program
#   make clean    remove bins/ dumps/ runs/
#
# You should not need to edit this file.

RISCV = riscv-none-elf-
ARCH  = rv32ima_zicsr
ABI   = ilp32

# -nostdlib/-nostartfiles: we supply our own start-up code (custom/crt.S) and
# link against no C library at all. custom/test.ld fixes where code and data land.
ASFLAGS = -T./custom/test.ld -static -mcmodel=medany \
          -fvisibility=hidden -nostdlib -nostartfiles -g \
          ./custom/crt.S -I./custom \
          -march=$(ARCH) -mabi=$(ABI)

# --log-commits prints one line per retired instruction: this is the execution
# trace the test harness inspects.
# MAX_STEPS only bounds a runaway program; a correct one exits via tohost first.
MAX_STEPS = 100000
SPIKE_FLAGS = --isa=$(ARCH) --priv=msu --steps=$(MAX_STEPS) --log-commits -l

ASMS  := $(wildcard asms/*.s)
BINS  := $(patsubst asms/%.s,bins/%.r5o,$(ASMS))
DUMPS := $(patsubst asms/%.s,dumps/%.dump,$(ASMS))
RUNS  := $(patsubst asms/%.s,runs/%.iss,$(ASMS))

.PHONY: all run clean
all: $(BINS) $(DUMPS)
run: $(RUNS)

# .r5o is the ELF. .r5ob is the same program as a flat binary image, which is
# what the simulator you write later in the course will load.
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
