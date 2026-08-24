# Problem 1 -- int sum(int *arr, int n)
#
# Write a function `sum` that adds up the first n entries of the array arr and
# returns the total.
#
#   argument 1 (a0) : the address of the array
#   argument 2 (a1) : how many entries to add
#   returns    (a0) : the total; 0 if n is 0
#
# `sum` calls nothing, so it is a LEAF function: it needs no stack frame at
# all, provided you only use t- and a-registers. If you use any of s0-s11 you
# must save and restore them.
#
# The tests call `sum` directly with their own arguments. They ignore the main
# below, which is there so that `make run` does something while you develop.

.section .data
	.align 2
arr:	.word 4, 8, 15, 16, 23, 42
	.fill 10, 4, 0
n:	.word 6
answer:	.word 0

.section .text

	.globl sum
sum:
	li t0, 0
	mv t1, a0
	add t1, t1, a1
	add t1, t1, a1
	add t1, t1, a1
	add t1, t1, a1
	addi t1, t1, -4
loop:
	blt t1, a0, end
	lw t2, 0(t1)
	add t0, t0, t2
	addi t1, t1, -4
	j loop
end:
	mv a0, t0
	ret

	.globl main
main:
	addi sp, sp, -16
	sw   ra, 12(sp)

	la   a0, arr
	lw   a1, n
	call sum
	la   t0, answer
	sw   a0, 0(t0)

	lw   ra, 12(sp)
	addi sp, sp, 16
	li   a0, 0
	ret
