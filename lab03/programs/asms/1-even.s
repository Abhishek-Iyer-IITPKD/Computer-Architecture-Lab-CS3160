# REFERENCE SOLUTION -- not distributed to students.
# Count the non-negative even values among the first n entries of l.
#
# Lab 1 uses register numbers only. x5-x7 and x28-x31 are the registers you
# may freely overwrite; ABI names for them arrive in Lab 2.

.section .data
	.align 2
n:	.word 5
l:	.word 2, -1, 7, 5, 3
	.fill 11, 4, 0		# l must hold 16 words in total
result:	.word 0

.section .text
	.globl main
main:
	la   x5, l		# x5 = cursor into l
	lw   x6, n		# x6 = how many entries are left
	li   x7, 0		# x7 = running count
	li   x28, 2

loop:
	beqz x6, done
	lw   x29, 0(x5)
	bltz x29, next		# negative: not counted
	rem  x30, x29, x28
	bnez x30, next		# odd: not counted
	addi x7, x7, 1
next:
	addi x5, x5, 4
	addi x6, x6, -1
	j    loop

done:
	la   x5, result
	sw   x7, 0(x5)
	li   x10, 0
	ret
