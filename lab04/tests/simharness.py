"""Shared test-harness machinery for the simulator labs (weeks 3 onwards).

One implementation for every simulator week, because the weeks differ only in
*what* they compare, not in how. Three tiers:

    build     does it compile at all?
    unit      does each function do the right thing called on its own?
    programs  does the whole simulator produce what ours produces?

What the third tier runs and what it looks at comes from `tests/golden/spec.json`,
which tools/make-golden.py writes at the same time as the golden files
themselves. That is deliberate: the arguments used to record an answer and the
arguments used to check against it are then the same string, and cannot drift
into disagreeing.

Three kinds of comparison, named in that spec:

    stdout    what the simulator printed -- weeks 3 and 4, where the whole
              output is a memory dump or a disassembly
    trace     the [OUT] line per retired instruction -- week 5 onwards, where
              the strong check is that every instruction did the same thing
    stats     the counters in stats.json, restricted to the keys the week is
              responsible for

Nothing here assigns marks. It records what passed into result.json and
tools/grade.py applies the weights afterwards.

This file is the canonical copy; tools/sync-testlib.sh puts it in each lab.
"""

import json
import os
import re
import subprocess
import sys

GREEN, RED, YELLOW, DIM, RESET = (
    ("\033[32m", "\033[31m", "\033[33m", "\033[2m", "\033[0m")
    if sys.stdout.isatty() else ("", "", "", "", ""))

TEST_LINE = re.compile(r"^(PASS|FAIL)\s+(\S+)")

# The simulator's log goes to stdout as well as to the log file, and its lines
# are not output: one of them contains the absolute path of the image, which
# differs on every machine. Comparing them would mean a student failed for
# running the tests from a different directory than the one the answer key was
# recorded in.
LOG_LINE = re.compile(r"^\[(INFO|DEBUG|WARN|ERROR|OUT)\]")


def program_output(text):
    """What the simulator actually printed, without its log."""
    return [l for l in text.splitlines() if not LOG_LINE.match(l)]


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


class Harness:
    def __init__(self, tests_dir):
        self.tests = tests_dir
        self.lab_dir = os.path.dirname(tests_dir)
        self.build = os.path.join(self.lab_dir, "build")
        self.golden = os.path.join(tests_dir, "golden")
        self.images = os.path.join(tests_dir, "images")
        with open(os.path.join(self.golden, "spec.json")) as f:
            self.spec = json.load(f)
        self.record = {"lab": self.spec["lab"], "programs": []}

    # ------------------------------------------------------------ tier: build

    def tier_build(self):
        print(f"{DIM}building{RESET}")
        r = run(["make", "-C", self.lab_dir, "all"])
        ok = r.returncode == 0 and os.path.exists(os.path.join(self.build, "sim"))
        text = r.stdout + r.stderr
        warnings = [l for l in text.splitlines() if "warning:" in l]
        self.record["build"] = {"passed": ok, "warnings": len(warnings),
                                "output": text[-4000:]}
        if ok:
            print(f"  {GREEN}build ok{RESET}" +
                  (f"  {YELLOW}({len(warnings)} warnings){RESET}"
                   if warnings else ""))
        else:
            print(f"  {RED}BUILD FAILED{RESET}")
            for line in text.splitlines():
                if "error:" in line:
                    print(f"    {line.strip()}")
        return ok

    # ------------------------------------------------------------- tier: unit

    def tier_unit(self):
        target = os.path.join("build", "unit_tests")
        r = run(["make", "-C", self.lab_dir, target])
        if r.returncode != 0:
            print(f"  {RED}unit tests did not build{RESET}")
            for line in (r.stdout + r.stderr).splitlines():
                if "error:" in line:
                    print(f"    {line.strip()}")
            self.record["unit"] = {"built": False, "cases": []}
            return

        env = dict(os.environ, CS3160_TEST_LINES="1")
        r = run([os.path.join(self.build, "unit_tests")], env=env,
                cwd=self.lab_dir)
        cases = [{"name": m.group(2), "passed": m.group(1) == "PASS"}
                 for m in (TEST_LINE.match(l.strip()) for l in r.stdout.splitlines())
                 if m]

        self.record["unit"] = {"built": True, "cases": cases,
                               "output": r.stdout[-8000:]}
        ok = sum(1 for c in cases if c["passed"])
        colour = GREEN if ok == len(cases) else (RED if ok == 0 else YELLOW)
        print(f"  unit tests {colour}{ok}/{len(cases)}{RESET}")
        for c in cases:
            if not c["passed"]:
                print(f"    {RED}FAIL{RESET} {c['name']}")

    # --------------------------------------------------------- tier: programs

    @staticmethod
    def _first_difference(got, want, unit):
        """Where two line-lists part company, and what was there."""
        for i, (a, b) in enumerate(zip(got, want)):
            if a != b:
                return (f"differs at {unit} {i}\n"
                        f"      yours: {a}\n"
                        f"      ours:  {b}")
        if len(got) != len(want):
            if not got:
                return "your simulator printed nothing"
            return (f"the two agree for {min(len(got), len(want))} {unit}s and "
                    f"then one stops: yours has {len(got)}, ours {len(want)}")
        return None

    def _compare_stdout(self, name, stdout, entry):
        with open(os.path.join(self.golden, name + ".out")) as f:
            want = f.read().splitlines()
        got = program_output(stdout)
        diff = self._first_difference(got, want, "line")
        entry["stdout_ok"] = diff is None
        if diff:
            entry["detail"].append(diff)

    def _compare_trace(self, name, log_path, entry):
        with open(os.path.join(self.golden, name + ".out")) as f:
            want = [l for l in f.read().splitlines() if l.startswith("[OUT]")]
        got = []
        if os.path.exists(log_path):
            with open(log_path) as f:
                got = [l for l in f.read().splitlines() if l.startswith("[OUT]")]
        diff = self._first_difference(got, want, "retired instruction")
        entry["trace_ok"] = diff is None
        if diff:
            entry["detail"].append(diff)

    def _compare_stats(self, name, stats_path, entry):
        with open(os.path.join(self.golden, name + ".stats.json")) as f:
            want = json.load(f)
        got = json.load(open(stats_path)) if os.path.exists(stats_path) else {}
        wrong = [(k, got.get(k), want[k]) for k in self.spec["stat_keys"]
                 if got.get(k) != want[k]]
        entry["stats_ok"] = not wrong
        entry["stats_wrong"] = [{"key": k, "got": g, "want": w}
                                for k, g, w in wrong]

    def tier_program(self, name):
        image = os.path.join(self.images, name + ".r5ob")
        stats_path = os.path.join(self.build, name + ".stats.json")
        log_path = os.path.join(self.build, name + ".log")

        cmd = ([os.path.join(self.build, "sim")] + self.spec["args"]
               + [f"--stats={stats_path}", f"--log={log_path}", image])
        try:
            r = run(cmd, timeout=120)
        except subprocess.TimeoutExpired:
            r = None

        entry = {"program": name, "ran": False, "detail": []}
        for kind in self.spec["compare"]:
            entry[kind + "_ok"] = False

        if r is None:
            entry["detail"] = ["did not finish within two minutes -- an "
                               "endless loop, or a halt condition that never "
                               "triggers"]
        elif r.returncode != 0:
            last = (r.stderr.strip().splitlines() or ["it exited non-zero"])[-1]
            entry["detail"] = [last]
        else:
            entry["ran"] = True
            for kind in self.spec["compare"]:
                getattr(self, "_compare_" + kind)(
                    name,
                    {"stdout": r.stdout, "trace": log_path,
                     "stats": stats_path}[kind],
                    entry)

        marks = "  ".join(
            (f"{GREEN}{k} ok{RESET}" if entry.get(k + "_ok")
             else f"{RED}{k} FAIL{RESET}")
            for k in self.spec["compare"])
        if entry["ran"]:
            print(f"  [{marks}] {name}")
        else:
            print(f"  {RED}{name}: did not run{RESET}")
        for d in entry["detail"]:
            print(f"      {YELLOW}{d}{RESET}")
        for w in entry.get("stats_wrong", []):
            print(f"      {YELLOW}{w['key']}: got {w['got']}, "
                  f"expected {w['want']}{RESET}")

        self.record["programs"].append(entry)

    # ------------------------------------------------------------------- main

    def programs(self):
        return sorted(n[:-len(".out")] for n in os.listdir(self.golden)
                      if n.endswith(".out"))

    def main(self, argv):
        wanted = argv[1:]

        if not self.tier_build():
            self.save()
            print(f"\n{RED}Fix the build first: nothing else can run.{RESET}")
            return 1

        if not wanted or "unit" in wanted:
            print(f"\n{DIM}unit tests{RESET}")
            self.tier_unit()

        todo = [p for p in self.programs() if not wanted or p in wanted]
        if todo:
            print(f"\n{DIM}programs{RESET}")
        for name in todo:
            self.tier_program(name)

        return self.summary()

    def summary(self):
        unit = self.record.get("unit", {}).get("cases", [])
        unit_ok = sum(1 for c in unit if c["passed"])
        n = len(self.record["programs"])

        print(f"\n{'-' * 62}")
        print(f"  build        {GREEN}ok{RESET}")
        if unit:
            print(f"  unit         {unit_ok}/{len(unit)}")
        everything = unit_ok == len(unit)
        for kind in self.spec["compare"]:
            ok = sum(1 for p in self.record["programs"] if p.get(kind + "_ok"))
            print(f"  {kind:<12} {ok}/{n}")
            everything = everything and ok == n
        self.save()
        return 0 if (everything and n > 0) else 1

    def save(self):
        with open(os.path.join(self.tests, "result.json"), "w") as f:
            json.dump(self.record, f, indent=2)


def main(tests_dir, argv):
    return Harness(tests_dir).main(argv)
