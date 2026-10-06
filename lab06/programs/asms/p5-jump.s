# Lab 6: unconditional jumps.
#
# jal and jalr always redirect, so every one of them costs a flush -- there is
# no such thing as an untaken jump. jal also writes a return address, and it is
# worth checking in the trace that what lands in the link register is the
# address after the jump and not the address it jumped to.

    .section ".text.init"
    .globl _start
_start:
    li   t0, 0
    nop
    nop
    jal  ra, routine         # ra = the address of the next instruction
    nop
    nop
    nop

    li   t1, 2
    nop
    nop
    j    onward              # jal zero, ...: a jump that writes nothing
    nop
    nop
    nop
onward:
    li   t2, 3
    nop
    nop
    nop

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

routine:
    addi t3, zero, 7
    nop
    nop
    jalr zero, 0(ra)         # return: ra was written well before this
    nop
    nop
    nop

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
