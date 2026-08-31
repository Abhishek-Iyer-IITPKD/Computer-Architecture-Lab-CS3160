#!/usr/bin/env python3
"""Lab 3 test runner.

    make test                          everything
    python3 tests/run_tests.py unit    just the unit tests
    python3 tests/run_tests.py 1-even  just one program

Three tiers, in the order a failure is worth reading:

    build     does your simulator compile at all?
    unit      does each stage do the right thing when called on its own?
    programs  does the whole simulator produce the same trace and the same
              statistics as ours, over a real program?

A stage that fails its unit test will fail every program too, and the unit
failure names the stage while the program failure names an address three
hundred instructions in. Fix upwards.

The machinery is in simharness.py beside this file, shared by every simulator
week. What gets run and what gets compared is in golden/spec.json, written when
the answer key was recorded.

Writes result.json for the grading script: it records what passed and assigns
no marks. Replaced with a fresh copy before grading, so editing it changes
nothing except what you see while you work.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import simharness

if __name__ == "__main__":
    sys.exit(simharness.main(os.path.dirname(os.path.abspath(__file__)), sys.argv))
