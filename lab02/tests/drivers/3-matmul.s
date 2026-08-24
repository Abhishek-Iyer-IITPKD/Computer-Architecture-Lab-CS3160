# Driver for problem 3: int matmul(int *A, int *B, int *C)
.include "abi_check.inc"

	ABI_DATA
	.align 2
t_A:	.fill 9, 4, 0
t_B:	.fill 9, 4, 0
t_C:	.fill 9, 4, 0

	ABI_PROLOGUE
	la   a0, t_A
	la   a1, t_B
	la   a2, t_C
	call matmul
	ABI_EPILOGUE
