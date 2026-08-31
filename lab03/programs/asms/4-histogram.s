# REFERENCE SOLUTION -- not distributed to students.
# count[m] = how many of the first n marks equal m, for m in 0..10.

.section .data
	.align 2
count:	.fill 11, 4, 0		# one bucket per mark 0..10
marks:	.word 2, 3, 0, 5, 10, 7, 1, 10, 10, 8
	.word 8, 9, 6, 7, 8, 2, 4, 5, 0, 9
	.fill 12, 4, 0		# marks must hold 32 words in total
n:	.word 20

.section .text
	.globl main
main:
	la   x5, marks		# x5 = cursor into marks
	la   x6, count
	lw   x7, n		# x7 = marks left to tally

loop:
	beqz x7, done
	lw   x28, 0(x5)		# x28 = this mark
	slli x29, x28, 2	# scale to a word offset
	add  x29, x6, x29	# x29 = &count[mark]
	lw   x30, 0(x29)
	addi x30, x30, 1
	sw   x30, 0(x29)
	addi x5, x5, 4
	addi x7, x7, -1
	j    loop

done:
	li   x10, 0
	ret
