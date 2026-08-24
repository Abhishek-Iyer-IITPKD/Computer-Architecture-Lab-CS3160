# Problem 2 -- int fact(int n)
#
# Write a RECURSIVE function `fact` that returns n factorial.
#
#   argument 1 (a0) : n
#   returns    (a0) : n!   (fact(0) and fact(1) are both 1)
#
# `fact` calls itself, so it is NOT a leaf function. Two things must survive
# the recursive call:
#
#   ra  -- `call` overwrites it. If you do not save it, the inner return will
#          send you somewhere you have already been, and the program crashes.
#   n   -- you need it again after the call returns, to multiply by.
#
# That means a stack frame. Keep sp a multiple of 16.
#
# The tests call `fact` directly. The main below is for your own use.

.section .data
	.align 2
n:	.word 6
answer:	.word 0

.section .text

	.globl fact
fact:
	addi sp, sp, -16
	sw ra, 12(sp)
	sw s0, 8(sp)
	sw s1, 4(sp)
	mv s0, a0
	li s1, 1
	beqz a0, base
	addi a0, a0, -1
	call fact
	mul s1, s0, a0
base:
	mv a0, s1
	lw s1, 4(sp)
	lw s0, 8(sp)
	lw ra, 12(sp)
	addi sp, sp, 16
	ret

	.globl main
main:
	addi sp, sp, -16
	sw   ra, 12(sp)

	lw   a0, n
	call fact
	la   t0, answer
	sw   a0, 0(t0)

	lw   ra, 12(sp)
	addi sp, sp, 16
	li   a0, 0
	ret
