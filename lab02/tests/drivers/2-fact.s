# Driver for problem 2: int fact(int n)
.include "abi_check.inc"

	ABI_DATA
	.align 2
t_n:	.word 0

	ABI_PROLOGUE
	lw   a0, t_n			# only argument
	call fact
	ABI_EPILOGUE
