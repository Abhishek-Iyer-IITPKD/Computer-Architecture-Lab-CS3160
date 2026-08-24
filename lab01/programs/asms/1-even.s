# Problem 1 -- count the non-negative even numbers.
#
# Count how many of the first `n` entries of the array `l` are both
# non-negative (>= 0) and even. Store that count in `result`.
#
# Remember: 0 is non-negative, and 0 is even.
#
# DO NOT rename, reorder or resize the globals below. The tests replace the
# contents of `n` and `l` with their own data and then read `result`, so `l`
# must keep room for all 16 words.

.section .data
	.align 2
n:	.word 5
l:	.word 2, -1, 7, 5, 3
	.fill 11, 4, 0		# l holds 16 words in total
result:	.word 0

.section .text
	.globl main
main:
	# your code here
	la x5, l			# x5 = address of l
	lw x6, n			# x6 = n
	li x7, 0			# x7 = count = 0
loop:
	beqz x6, done
	lw x28, 0(x5)			# x28 = current num
	blt x28, x0, next
	li x29, 2
	rem x29, x28, x29
	bne x29, x0, next
	addi x7, x7, 1
next:
	addi x5, x5, 4
    addi x6, x6, -1
    j loop
done:
	la x28, result
	sw x7, 0(x28)
	li   x10, 0		# exit status 0
	ret			# main must return -- do not loop forever
