#!/usr/bin/env python3
"""Lab 2 test runner.

    python3 tests/run_tests.py              test everything
    python3 tests/run_tests.py 1-sum        test one problem

Unlike Lab 1, these tests do not run your main. They link your file against a
driver of ours which becomes main, calls your function with arguments of its
choosing, and checks both what you returned and whether you left the
callee-saved registers alone. Your own main is kept (renamed) so `make run`
still works while you develop.

Each case reports two results:
    value  your function computed the right answer
    abi    your function preserved s0-s11 and sp

Writes result.json for the grading script. It records which tests passed; it
does NOT assign marks. This file is replaced with a fresh copy before grading.
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

HERE = os.path.dirname(os.path.abspath(__file__))
LAB = os.path.dirname(HERE)
PROGRAMS = os.path.join(LAB, "programs")
DRIVERS = os.path.join(HERE, "drivers")

GREEN, RED, YELLOW, DIM, RESET = (
    ("\033[32m", "\033[31m", "\033[33m", "\033[2m", "\033[0m")
    if sys.stdout.isatty() else ("", "", "", "", ""))


def check_case(elf_path, problem, case, workdir, idx):
    """Returns (value_ok, abi_ok, detail)."""
    elf = Elf(elf_path)

    for name, capacity in problem["arrays"].items():
        elf.write(elf.addr_of(name), words(case[name]))
    for name in problem["scalars"]:
        elf.write(elf.addr_of(name), words([case[name]]))

    patched = os.path.join(workdir, f"case{idx}.r5o")
    elf.save(patched)

    trace, overran = harness.run_spike(patched)
    if overran:
        return False, False, ("did not finish -- ran past the instruction "
                              "limit. Infinite recursion, or a base case that "
                              "never triggers?")
    if not trace.strip():
        return False, False, "spike produced no trace"

    status = harness.exit_status(elf, trace)
    if status == harness.TRAP_STATUS:
        return False, False, ("crashed -- ended up executing something that is "
                              "not code. Usually a lost return address: did "
                              "you save ra before calling, and restore it "
                              "after?")
    if status is None or status != 0:
        return False, False, f"did not finish cleanly (status {status})"

    regions = [(elf.addr_of(s), n * 4) for s, n in problem["outputs"].items()]
    regions.append((elf.addr_of("out_abi"), 4))
    regions.append((elf.addr_of("out_ran"), 4))
    mem = harness.final_memory(elf, trace, regions)

    def read(sym, nwords):
        return unwords(harness.read_region(mem, elf.addr_of(sym), nwords * 4))

    if read("out_ran", 1)[0] != 1:
        return False, False, "the driver never finished -- your function did not return"

    abi_ok = read("out_abi", 1)[0] == 1
    want = problem["expect"](case)

    problems = []
    for sym, nwords in problem["outputs"].items():
        got = read(sym, nwords)
        if got != want[sym]:
            label = "returned" if sym == "out_ret" else sym
            problems.append(f"{label}: got {got if nwords > 1 else got[0]}, "
                            f"expected {want[sym] if nwords > 1 else want[sym][0]}")

    detail = "; ".join(problems)
    if not abi_ok:
        note = ("clobbered a callee-saved register (s0-s11) or left sp moved: "
                "save what you use, restore it before you return")
        detail = f"{detail}. {note}" if detail else note
    return (not problems), abi_ok, detail


def describe(problem, case):
    bits = [f"{n.removeprefix('t_')}={case[n]}" for n in problem["scalars"]]
    for name in problem["arrays"]:
        vals = case[name]
        n = case.get("t_n", len(vals))
        shown = vals[:min(n, 6)] if len(vals) > 6 else vals
        more = "..." if len(shown) < min(n, len(vals)) else ""
        bits.append(f"{name.removeprefix('t_')}=[{', '.join(map(str, shown))}{more}]")
    return "  ".join(bits) or "(no arguments)"


def source_of(problem):
    override = os.environ.get("CS3160_SRC")
    if override:
        return os.path.join(override, os.path.basename(problem["source"]))
    return os.path.join(LAB, problem["source"])


def run_problem(problem, workdir):
    src = source_of(problem)
    record = {"id": problem["id"], "title": problem["title"],
              "built": False, "linked": False, "cases": []}

    print(f"\n{problem['id']}  {DIM}{problem['title']}{RESET}")

    if not os.path.exists(src):
        print(f"  {RED}MISSING{RESET}  {problem['source']} does not exist")
        record["error"] = "source file missing"
        return record

    elf_path = os.path.join(workdir, problem["id"] + ".r5o")
    driver = os.path.join(DRIVERS, problem["driver"])
    try:
        harness.link_with_driver(src, driver, elf_path, PROGRAMS, DRIVERS)
    except harness.BuildError as e:
        msg = str(e)
        record["error"] = msg[:2000]
        if f"undefined reference to `{problem['function']}'" in msg:
            print(f"  {RED}LINK FAILED{RESET}  no global function named "
                  f"'{problem['function']}'. Did you write "
                  f"`.globl {problem['function']}`?")
        else:
            print(f"  {RED}BUILD FAILED{RESET}  "
                  f"{msg.splitlines()[0] if msg else 'assembly failed'}")
        return record
    record["built"] = record["linked"] = True

    for i, case in enumerate(problem["cases"]):
        value_ok, abi_ok, detail = check_case(elf_path, problem, case, workdir, i)
        record["cases"].append({"case": i, "value_ok": value_ok,
                                "abi_ok": abi_ok, "passed": value_ok and abi_ok,
                                "input": describe(problem, case),
                                "detail": detail})
        v = (f"{GREEN}value pass{RESET}" if value_ok
             else f"{RED}value FAIL{RESET}")
        a = f"{GREEN}abi ok{RESET}" if abi_ok else f"{RED}abi FAIL{RESET}"
        print(f"  [{v}  {a}] case {i}: {describe(problem, case)}")
        if detail:
            print(f"           {YELLOW}{detail}{RESET}")

    return record


def main():
    wanted = sys.argv[1:]
    problems = [p for p in PROBLEMS if not wanted or p["id"] in wanted]
    if not problems:
        print(f"no such problem. known: {', '.join(p['id'] for p in PROBLEMS)}")
        return 2

    for tool in ("riscv-none-elf-gcc", "riscv-none-elf-objcopy", "spike"):
        if shutil.which(tool) is None:
            print(f"{RED}{tool} not found on PATH{RESET}")
            return 2

    workdir = tempfile.mkdtemp(prefix="cs3160-lab02-")
    try:
        results = [run_problem(p, workdir) for p in problems]
    finally:
        shutil.rmtree(workdir, ignore_errors=True)

    total = sum(len(r["cases"]) for r in results)
    values = sum(1 for r in results for c in r["cases"] if c["value_ok"])
    abis = sum(1 for r in results for c in r["cases"] if c["abi_ok"])

    print(f"\n{'-' * 62}")
    for r in results:
        of = len(r["cases"])
        v = sum(1 for c in r["cases"] if c["value_ok"])
        a = sum(1 for c in r["cases"] if c["abi_ok"])
        if not r["linked"]:
            print(f"  {r['id']:<12} {RED}did not build{RESET}")
        else:
            cv = GREEN if v == of else (RED if v == 0 else YELLOW)
            ca = GREEN if a == of else (RED if a == 0 else YELLOW)
            print(f"  {r['id']:<12} value {cv}{v}/{of}{RESET}   "
                  f"abi {ca}{a}/{of}{RESET}")
    print(f"  {'total':<12} value {values}/{total}   abi {abis}/{total}")

    with open(os.path.join(HERE, "result.json"), "w") as f:
        json.dump({"lab": "lab02", "problems": results,
                   "values_passed": values, "abi_passed": abis,
                   "cases_total": total}, f, indent=2)

    return 0 if values == total and abis == total else 1


if __name__ == "__main__":
    sys.exit(main())
