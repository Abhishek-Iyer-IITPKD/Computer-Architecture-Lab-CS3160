# Lab 6: loads and stores, hazard-free.
#
# Drives the MEM stage: a word written and read back, then the narrower widths
# and their sign extension. Note the spacing after every load -- the value is
# not in the register file until that instruction reaches WB, and this week's
# pipeline has nothing that would notice if you used it sooner.

    .section ".text.init"
    .globl _start
_start:
    lui  t0, %hi(buf)
    lui  t1, 0x12345         # `li t1, 0x12345678` would expand to lui+addi
    nop                      # back to back, which is exactly what this week
    nop                      # cannot do -- so it is written out by hand
    addi t0, t0, %lo(buf)
    nop
    nop
    addi t1, t1, 0x678
    nop
    nop
    sw   t1, 0(t0)           # both operands well clear
    nop
    nop
    lw   t2, 0(t0)           # read the word back
    nop
    nop
    lbu  t3, 0(t0)           # 0x78, zero-extended
    nop
    nop
    lb   t4, 3(t0)           # 0x12, sign-extended (positive here)
    nop
    nop
    lhu  t6, 0(t0)           # 0x5678
    nop
    nop
    sb   t3, 4(t0)
    nop
    nop
    sh   t6, 6(t0)
    nop
    nop
    lw   a1, 4(t0)           # what the two narrow stores left

# --- exit with status 0 ----------------------------------------------------
    li   a0, 1
    nop
    nop
    lui  t5, %hi(tohost)
    nop
    nop
    sw   a0, %lo(tohost)(t5)
    nop
1:  j    1b

    .section ".data"
    .align 4
    .globl buf
buf:
    .word 0, 0, 0, 0

# The host interface. Both symbols have to exist and be page-aligned this way:
# a store to `tohost` is what ends the program, and Spike -- which the reference
# answers are checked against -- only watches for it when `fromhost` is there
# too. Defining just the one leaves Spike running until it is killed.
    .section ".tohost","aw",@progbits
    .align 6
    .globl tohost
tohost:   .dword 0
    .align 6
    .globl fromhost
fromhost: .dword 0
