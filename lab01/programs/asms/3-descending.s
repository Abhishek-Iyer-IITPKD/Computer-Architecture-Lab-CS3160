# Problem 3 -- sort an array into descending order.
#
# Sort the first `n` entries of `a` so that they run from largest to smallest.
# Sort IN PLACE: the answer is the array itself, not a copy somewhere else.
# Entries beyond the first `n` must be left exactly as they were.
#
# Any sorting algorithm is acceptable. Watch the easy-to-miss cases: n = 0,
# n = 1, values that repeat, and negative values.
#
# DO NOT rename, reorder or resize the globals below. The tests replace the
# contents of `n` and `a` with their own data and then read `a` back, so `a`
# must keep room for all 16 words.

.section .data
	.align 2
a:	.word 70, 80, 40, 20, 10, 30, 50, 60
	.fill 8, 4, 0		# a holds 16 words in total
n:	.word 8

.section .text
	.globl main
main:
	# your code here
	la x5, a			# x5 = address of a
	lw x6, n			# x6 = n
	li x7, 1			# x7 = index of element we are sorting (I am using Insertion Sort)
	li x11, 4			# x11 = byte offset main
	li x12, 0			# x12 = byte offset secondary
loop:
	bge x7, x6, done
	add x17, x11, x5
	lw x28, 0(x17)
	addi x12, x11, -4
comploop:
	blt x12, x0, next
	add x17, x12, x5
	lw x29, 0(x17)
	bge x29, x28, next
	addi x12, x12, 4
	add x17, x12, x5
	sw x29, 0(x17)
	addi x12, x12, -8
	j comploop
next:
	addi x12, x12, 4
	add x17, x12, x5
	sw x28, 0(x17)
	addi x7, x7, 1
	addi x11, x11, 4
	j loop
done:
	li   x10, 0		# exit status 0
	ret			# main must return -- do not loop forever
