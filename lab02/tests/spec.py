"""Lab 2 problem definitions and test cases.

Lab 2 tests functions, not whole programs. Each problem is checked by linking
the student's file against a driver of ours that supplies main, chooses the
arguments, and inspects what came back. So each case checks two independent
things:

  value  -- did the function compute the right answer?
  abi    -- did it leave every callee-saved register and sp as it found them?

They are reported separately on purpose. A function that returns the right
number but tramples s0 is a specific, nameable mistake, and this is the lab
where that mistake is the lesson.
"""

ARR_CAP = 16


def expect_sum(case):
    return {"out_ret": [sum(case["t_arr"][:case["t_n"]])]}


def expect_fact(case):
    n = case["t_n"]
    f = 1
    for i in range(2, n + 1):
        f *= i
    # 32-bit wraparound, interpreted as signed
    f &= 0xFFFFFFFF
    if f > 0x7FFFFFFF:
        f -= 1 << 32
    return {"out_ret": [f]}


def _mat(flat):
    return [flat[0:3], flat[3:6], flat[6:9]]


def expect_matmul(case):
    A, B = _mat(case["t_A"]), _mat(case["t_B"])
    C = [sum(A[i][k] * B[k][j] for k in range(3)) for i in range(3) for j in range(3)]
    all_nonzero = 1 if all(v != 0 for v in C) else 0
    return {"t_C": C, "out_ret": [all_nonzero]}


IDENTITY = [1, 0, 0, 0, 1, 0, 0, 0, 1]
COUNTUP = [1, 2, 3, 4, 5, 6, 7, 8, 9]

PROBLEMS = [
    {
        "id": "1-sum",
        "title": "int sum(int *arr, int n) -- a leaf function",
        "source": "programs/asms/1-sum.s",
        "function": "sum",
        "driver": "1-sum.s",
        "arrays": {"t_arr": ARR_CAP},
        "scalars": ["t_n"],
        "outputs": {"out_ret": 1},
        "expect": expect_sum,
        "cases": [
            {"t_n": 6, "t_arr": [4, 8, 15, 16, 23, 42] + [0] * (ARR_CAP - 6)},
            {"t_n": 1, "t_arr": [7] + [0] * (ARR_CAP - 1)},
            {"t_n": 0, "t_arr": [99] * ARR_CAP},        # empty: must return 0
            {"t_n": 5, "t_arr": [-1, -2, -3, -4, -5] + [0] * (ARR_CAP - 5)},
            {"t_n": 4, "t_arr": [10, -10, 20, -20] + [0] * (ARR_CAP - 4)},
            # n smaller than the array: the tail must not be added in
            {"t_n": 3, "t_arr": [1, 1, 1, 1000, 1000] + [0] * (ARR_CAP - 5)},
            {"t_n": 16, "t_arr": list(range(1, ARR_CAP + 1))},
        ],
    },
    {
        "id": "2-fact",
        "title": "int fact(int n) -- recursion, so a real stack frame",
        "source": "programs/asms/2-fact.s",
        "function": "fact",
        "driver": "2-fact.s",
        "arrays": {},
        "scalars": ["t_n"],
        "outputs": {"out_ret": 1},
        "expect": expect_fact,
        "cases": [
            {"t_n": 0},      # 0! is 1 by definition
            {"t_n": 1},
            {"t_n": 2},
            {"t_n": 3},
            {"t_n": 6},
            {"t_n": 10},     # 3628800
            {"t_n": 12},     # 479001600, the largest that still fits
        ],
    },
    {
        "id": "3-matmul",
        "title": "int matmul(int *A, int *B, int *C) -- three arguments",
        "source": "programs/asms/3-matmul.s",
        "function": "matmul",
        "driver": "3-matmul.s",
        "arrays": {"t_A": 9, "t_B": 9, "t_C": 9},
        "scalars": [],
        "outputs": {"t_C": 9, "out_ret": 1},
        "expect": expect_matmul,
        "cases": [
            # Multiplying by the identity must give A back unchanged.
            {"t_A": COUNTUP, "t_B": IDENTITY, "t_C": [0] * 9},
            {"t_A": IDENTITY, "t_B": COUNTUP, "t_C": [0] * 9},
            # A zero row makes a zero row in C, so the return value is 0.
            {"t_A": [1, 2, 3, 0, 0, 0, 7, 8, 9], "t_B": COUNTUP, "t_C": [0] * 9},
            # Identity squared: off-diagonal zeros, so also 0.
            {"t_A": IDENTITY, "t_B": IDENTITY, "t_C": [0] * 9},
            # Everything non-zero: return value 1.
            {"t_A": [1] * 9, "t_B": [1] * 9, "t_C": [0] * 9},
            # Negatives, and a general product.
            {"t_A": [1, -2, 3, -4, 5, -6, 7, -8, 9],
             "t_B": [2, 1, 1, 1, 2, 1, 1, 1, 2], "t_C": [0] * 9},
            # C must be fully overwritten, not accumulated into.
            {"t_A": COUNTUP, "t_B": IDENTITY, "t_C": [999] * 9},
        ],
    },
]
