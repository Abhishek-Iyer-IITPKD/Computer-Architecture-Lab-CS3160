#!/usr/bin/env python3
"""Lab 1 test runner.

    python3 tests/run_tests.py              test everything
    python3 tests/run_tests.py 1-even       test one problem

Writes result.json next to itself for the grading script. It records which
tests passed.
"""

import json
import os
import shutil
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import harness
from harness import Elf, words, unwords
from spec import PROBLEMS

POISON = -559038737

HERE = os.path.dirname(os.path.abspath(__file__))
LAB = os.path.dirname(HERE)
PROGRAMS = os.path.join(LAB, "programs")

GREEN, RED, YELLOW, DIM, RESET = (
    ("\033[32m", "\033[31m", "\033[33m", "\033[2m", "\033[0m")
    if sys.stdout.isatty() else ("", "", "", "", ""))


def check_case(elf_path, problem, case, workdir, idx):
    elf = Elf(elf_path)

    missing = [s for s in list(problem["arrays"]) + problem["scalars"] +
               [problem["output"][0]] if s not in elf.symbols]
    if missing:
        return False, f"missing global(s): {', '.join(sorted(missing))}"

    tracked = {s: elf.addr_of(s) for s in
               list(problem["arrays"]) + problem["scalars"] + [problem["output"][0]]}
    for name, capacity in problem["arrays"].items():
        start = tracked[name]
        end = start + 4 * capacity
        for other, addr in tracked.items():
            if other != name and start < addr < end:
                return False, (f"{name} must be declared with {capacity} words; "
                               f"'{other}' starts only "
                               f"{(addr - start) // 4} words into it")

    for name, capacity in problem["arrays"].items():
        values = case[name]
        if len(values) != capacity:
            return False, f"internal: case {name} is {len(values)}, want {capacity}"
        elf.write(elf.addr_of(name), words(values))
    for name in problem["scalars"]:
        elf.write(elf.addr_of(name), words([case[name]]))

    out_name, out_words = problem["output"]
    if out_name not in problem["arrays"] and out_name not in problem["scalars"]:
        elf.write(elf.addr_of(out_name), words([POISON] * out_words))

    patched = os.path.join(workdir, f"case{idx}.r5o")
    elf.save(patched)

    trace, overran = harness.run_spike(patched)
    if overran:
        return False, ("program did not finish -- it ran past the instruction "
                       "limit. Does main return?")
    if not trace.strip():
        return False, "spike produced no trace"

    status = harness.exit_status(elf, trace)
    if status is None:
        return False, "program never reached the end of main"
    if status == harness.TRAP_STATUS:
        return False, ("program crashed -- it ended up executing something "
                       "that is not code. The usual cause is overwriting x1, "
                       "which holds the address `ret` returns to.")
    if status != 0:
        return False, f"program exited with status {status}, expected 0"

    out_addr = elf.addr_of(out_name)
    regions = [(out_addr, out_words * 4)]
    mem = harness.final_memory(elf, trace, regions)
    got = unwords(harness.read_region(mem, out_addr, out_words * 4))
    want = problem["expect"](case)

    if got != want:
        return False, f"{out_name}: got {got}, expected {want}"
    return True, ""


def describe(problem, case):
    bits = []
    for name in problem["scalars"]:
        bits.append(f"{name}={case[name]}")
    for name in problem["arrays"]:
        vals = case[name]
        n = case.get("n", len(vals))
        shown = vals[:min(n, 8)] if problem["arrays"][name] > 4 else vals
        more = "..." if n > len(shown) else ""
        bits.append(f"{name}=[{', '.join(map(str, shown))}{more}]")
    return "  ".join(bits)


def source_of(problem):
    override = os.environ.get("CS3160_SRC")
    if override:
        return os.path.join(override, os.path.basename(problem["source"]))
    return os.path.join(LAB, problem["source"])


def run_problem(problem, workdir):
    src = source_of(problem)
    record = {"id": problem["id"], "title": problem["title"],
              "built": False, "cases": []}

    print(f"\n{problem['id']}  {DIM}{problem['title']}{RESET}")

    if not os.path.exists(src):
        print(f"  {RED}MISSING{RESET}  {problem['source']} does not exist")
        record["error"] = "source file missing"
        return record

    elf_path = os.path.join(workdir, problem["id"] + ".r5o")
    try:
        harness.assemble(src, elf_path, PROGRAMS)
    except harness.BuildError as e:
        first = e.args[0].splitlines()[0] if e.args[0] else "assembly failed"
        print(f"  {RED}BUILD FAILED{RESET}  {first}")
        record["error"] = str(e)[:2000]
        return record
    record["built"] = True

    for i, case in enumerate(problem["cases"]):
        ok, detail = check_case(elf_path, problem, case, workdir, i)
        record["cases"].append({"case": i, "passed": ok,
                                "input": describe(problem, case),
                                "detail": detail})
        mark = f"{GREEN}pass{RESET}" if ok else f"{RED}FAIL{RESET}"
        print(f"  [{mark}] case {i}: {describe(problem, case)}")
        if not ok:
            print(f"         {YELLOW}{detail}{RESET}")

    return record


def main():
    wanted = sys.argv[1:]
    problems = [p for p in PROBLEMS if not wanted or p["id"] in wanted]
    if not problems:
        print(f"no such problem. known: {', '.join(p['id'] for p in PROBLEMS)}")
        return 2

    for tool in ("riscv-none-elf-gcc", "spike"):
        if shutil.which(tool) is None:
            print(f"{RED}{tool} not found on PATH{RESET}")
            return 2

    workdir = tempfile.mkdtemp(prefix="cs3160-lab01-")
    try:
        results = [run_problem(p, workdir) for p in problems]
    finally:
        shutil.rmtree(workdir, ignore_errors=True)

    total = sum(len(r["cases"]) for r in results)
    passed = sum(1 for r in results for c in r["cases"] if c["passed"])

    print(f"\n{'-' * 56}")
    for r in results:
        n = sum(1 for c in r["cases"] if c["passed"])
        of = len(r["cases"])
        colour = GREEN if of and n == of else (RED if n == 0 else YELLOW)
        status = "did not build" if not r["built"] else f"{n}/{of} cases"
        print(f"  {r['id']:<16} {colour}{status}{RESET}")
    print(f"  {'total':<16} {passed}/{total} cases")

    with open(os.path.join(HERE, "result.json"), "w") as f:
        json.dump({"lab": "lab01", "problems": results,
                   "cases_passed": passed, "cases_total": total}, f, indent=2)

    return 0 if passed == total else 1


if __name__ == "__main__":
    sys.exit(main())
