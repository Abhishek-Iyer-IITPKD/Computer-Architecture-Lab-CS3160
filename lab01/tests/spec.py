def expect_even(case):
    """Count of non-negative even values among the first n."""
    return [sum(1 for v in case["l"][:case["n"]] if v >= 0 and v % 2 == 0)]


def expect_prime(case):
    a = case["a"]
    if a < 2:
        return [-1]
    d = 2
    while d * d <= a:
        if a % d == 0:
            return [-1]
        d += 1
    return [1]


def expect_descending(case):
    n = case["n"]
    head = sorted(case["a"][:n], reverse=True)
    return head + case["a"][n:]


def expect_histogram(case):
    counts = [0] * 11
    for v in case["marks"][:case["n"]]:
        counts[v] += 1
    return counts


EVEN_CAP = 16
SORT_CAP = 16
MARKS_CAP = 32

PROBLEMS = [
    {
        "id": "1-even",
        "title": "Count non-negative even numbers",
        "source": "programs/asms/1-even.s",
        "arrays": {"l": EVEN_CAP},
        "scalars": ["n"],
        "output": ("result", 1),
        "expect": expect_even,
        "cases": [
            # The case shipped in the handout, so students can check by hand.
            {"n": 5, "l": [2, -1, 7, 5, 3] + [0] * (EVEN_CAP - 5)},
            # All even, all positive.
            {"n": 6, "l": [2, 4, 6, 8, 10, 12] + [0] * (EVEN_CAP - 6)},
            # None qualify: odd or negative.
            {"n": 5, "l": [1, 3, -2, -4, 7] + [0] * (EVEN_CAP - 5)},
            # Zero counts as non-negative and even.
            {"n": 4, "l": [0, -6, 5, 8] + [0] * (EVEN_CAP - 4)},
            # n smaller than the array: trailing elements must be ignored.
            {"n": 3, "l": [2, 4, 6, 8, 10] + [0] * (EVEN_CAP - 5)},
            # Empty range.
            {"n": 0, "l": [2, 4, 6] + [0] * (EVEN_CAP - 3)},
            {"n": 16, "l": [i - 8 for i in range(EVEN_CAP)]},
        ],
    },
    {
        "id": "2-prime",
        "title": "Primality test",
        "source": "programs/asms/2-prime.s",
        "arrays": {},
        "scalars": ["a"],
        "output": ("result", 1),
        "expect": expect_prime,
        "cases": [
            {"a": 10},   # the handout's case
            {"a": 7},
            {"a": 2},    # smallest prime, and the only even one
            {"a": 3},
            {"a": 4},
            {"a": 9},    # composite, odd, square
            {"a": 1},    # not prime by definition
            {"a": 0},
            {"a": 97},
            {"a": 91},   # 7 * 13, catches "no factor below 10" shortcuts
        ],
    },
    {
        "id": "3-descending",
        "title": "Sort an array in place, descending",
        "source": "programs/asms/3-descending.s",
        "arrays": {"a": SORT_CAP},
        "scalars": ["n"],
        "output": ("a", SORT_CAP),
        "expect": expect_descending,
        "cases": [
            {"n": 8, "a": [70, 80, 40, 20, 10, 30, 50, 60] + [0] * (SORT_CAP - 8)},
            # Already sorted.
            {"n": 5, "a": [50, 40, 30, 20, 10] + [0] * (SORT_CAP - 5)},
            # Reverse of the answer: worst case for most algorithms.
            {"n": 5, "a": [10, 20, 30, 40, 50] + [0] * (SORT_CAP - 5)},
            # Duplicates.
            {"n": 6, "a": [5, 5, 3, 9, 3, 9] + [0] * (SORT_CAP - 6)},
            # Negatives.
            {"n": 6, "a": [-3, 7, -10, 0, 4, -1] + [0] * (SORT_CAP - 6)},
            # Single element, and a zero-length range: must not corrupt memory.
            {"n": 1, "a": [42] + [7] * (SORT_CAP - 1)},
            {"n": 0, "a": [9, 8, 7] + [0] * (SORT_CAP - 3)},
            # n smaller than the array: the tail must be left alone.
            {"n": 4, "a": [1, 2, 3, 4, 99, 98] + [0] * (SORT_CAP - 6)},
        ],
    },
    {
        "id": "4-histogram",
        "title": "Histogram of marks in 0..10",
        "source": "programs/asms/4-histogram.s",
        "arrays": {"marks": MARKS_CAP, "count": 11},
        "scalars": ["n"],
        "output": ("count", 11),
        "expect": expect_histogram,
        "cases": [
            {"n": 20,
             "marks": [2, 3, 0, 5, 10, 7, 1, 10, 10, 8,
                       8, 9, 6, 7, 8, 2, 4, 5, 0, 9] + [0] * (MARKS_CAP - 20),
             "count": [0] * 11},
            # Every bucket exactly once.
            {"n": 11, "marks": list(range(11)) + [0] * (MARKS_CAP - 11),
             "count": [0] * 11},
            # All the same mark.
            {"n": 8, "marks": [6] * 8 + [0] * (MARKS_CAP - 8),
             "count": [0] * 11},
            # Boundary buckets only.
            {"n": 6, "marks": [0, 10, 0, 10, 0, 10] + [0] * (MARKS_CAP - 6),
             "count": [0] * 11},
            # n smaller than the array: the tail must not be counted.
            {"n": 3, "marks": [1, 1, 1, 4, 4, 4] + [0] * (MARKS_CAP - 6),
             "count": [0] * 11},
            {"n": 0, "marks": [5] * 4 + [0] * (MARKS_CAP - 4),
             "count": [0] * 11},
        ],
    },
]
