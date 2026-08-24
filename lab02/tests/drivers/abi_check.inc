# Shared driver scaffolding for the Lab 2 tests. Not editable by students.
#
# A driver built on this replaces the student's main. It loads a distinct
# sentinel into every callee-saved register, calls the function under test,
# and then checks that all of them -- and the stack pointer -- came back
# unchanged. That is the whole content of "callee-saved": a function may use
# s0-s11, but only if it puts them back.
#
# Results land in three globals the harness reads:
#   out_ret   what the function returned in a0
#   out_abi   1 if every callee-saved register and sp survived, else 0
#   out_ran   1 if the driver reached the end at all

	.macro ABI_PROLOGUE
	.section .text
	.globl main
main:
	addi sp, sp, -64
	sw   ra, 60(sp)
	sw   s0, 56(sp)
	sw   s1, 52(sp)
	sw   s2, 48(sp)
	sw   s3, 44(sp)
	sw   s4, 40(sp)
	sw   s5, 36(sp)
	sw   s6, 32(sp)
	sw   s7, 28(sp)
	sw   s8, 24(sp)
	sw   s9, 20(sp)
	sw   s10, 16(sp)
	sw   s11, 12(sp)

	# Remember sp in memory, not in a register: every register we could
	# hide it in is one we are about to hand to the student's code.
	la   t0, sp_save
	sw   sp, 0(t0)

	# A different sentinel per register, so a failure is traceable.
	li   s0,  0x5A5A0000
	li   s1,  0x5A5A0001
	li   s2,  0x5A5A0002
	li   s3,  0x5A5A0003
	li   s4,  0x5A5A0004
	li   s5,  0x5A5A0005
	li   s6,  0x5A5A0006
	li   s7,  0x5A5A0007
	li   s8,  0x5A5A0008
	li   s9,  0x5A5A0009
	li   s10, 0x5A5A000a
	li   s11, 0x5A5A000b
	.endm


	# Check one callee-saved register against its sentinel.
	# t3 accumulates the verdict; t4 holds the expected value.
	.macro CHECK_S reg, want
	li   t4, \want
	beq  \reg, t4, 8f
	li   t3, 0
8:
	.endm


	.macro ABI_EPILOGUE
	# The return value, before anything else can disturb a0.
	la   t0, out_ret
	sw   a0, 0(t0)

	li   t3, 1			# assume all preserved
	CHECK_S s0,  0x5A5A0000
	CHECK_S s1,  0x5A5A0001
	CHECK_S s2,  0x5A5A0002
	CHECK_S s3,  0x5A5A0003
	CHECK_S s4,  0x5A5A0004
	CHECK_S s5,  0x5A5A0005
	CHECK_S s6,  0x5A5A0006
	CHECK_S s7,  0x5A5A0007
	CHECK_S s8,  0x5A5A0008
	CHECK_S s9,  0x5A5A0009
	CHECK_S s10, 0x5A5A000a
	CHECK_S s11, 0x5A5A000b

	# sp must be back where it started.
	la   t0, sp_save
	lw   t1, 0(t0)
	beq  sp, t1, 9f
	li   t3, 0
9:
	mv   sp, t1			# restore sp ourselves, so that a student
					# who corrupted it still gets a clean
					# report rather than a crash in here
	la   t0, out_abi
	sw   t3, 0(t0)

	li   t0, 1
	la   t1, out_ran
	sw   t0, 0(t1)

	lw   s11, 12(sp)
	lw   s10, 16(sp)
	lw   s9, 20(sp)
	lw   s8, 24(sp)
	lw   s7, 28(sp)
	lw   s6, 32(sp)
	lw   s5, 36(sp)
	lw   s4, 40(sp)
	lw   s3, 44(sp)
	lw   s2, 48(sp)
	lw   s1, 52(sp)
	lw   s0, 56(sp)
	lw   ra, 60(sp)
	addi sp, sp, 64
	li   a0, 0
	ret
	.endm


	# Globals every driver needs.
	.macro ABI_DATA
	.section .data
	.align 2
sp_save:	.word 0
out_ret:	.word 0
out_abi:	.word 0
out_ran:	.word 0
	.endm
