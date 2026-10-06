# mhartid, which reports which hardware thread is running.
#
# This machine has one, and it is number 0, and every read of mhartid must say
# so however the read is spelled.
#
# What a *write* to mhartid does is not tested here. On real hardware it raises
# an illegal-instruction trap, and this simulator has no traps -- it discards
# the write instead. That difference is real and is covered by a unit test,
# where no comparison against the hardware is involved.

    .section .text
    .globl main
main:
    la   s0, out

    csrrs t1, mhartid, zero     # a plain read: 0
    sw    t1, 0(s0)

    csrrc t1, mhartid, zero     # csrrc with x0 is a read as well
    sw    t1, 4(s0)             # 0

    csrr  t1, mhartid           # and so is the alias
    sw    t1, 8(s0)             # 0

    # A writable register, for contrast: the same three spellings do change it.
    li    t0, 0x00ABCDEF
    csrrw t1, mcause, t0
    sw    t1, 12(s0)            # 0, the old value
    csrrs t1, mcause, zero
    sw    t1, 16(s0)            # 0x00ABCDEF

    csrr  t1, mhartid           # ...and mhartid is still 0, untouched by it
    sw    t1, 20(s0)
    csrrs t1, mcause, zero
    sw    t1, 24(s0)            # 0x00ABCDEF, the two are separate storage

    li   a0, 0
    ret

    .section .data
    .align 2
    .globl out
out:
    .word 0, 0, 0, 0, 0, 0, 0
