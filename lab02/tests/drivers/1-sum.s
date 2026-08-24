# Driver for problem 1: int sum(int *arr, int n)
.include "abi_check.inc"

	ABI_DATA
	.align 2
t_arr:	.fill 16, 4, 0
t_n:	.word 0

	ABI_PROLOGUE
	la   a0, t_arr			# first argument
	lw   a1, t_n			# second argument
	call sum
	ABI_EPILOGUE
