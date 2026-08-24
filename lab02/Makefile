# CS3160 Lab 2
#
#   make          build every program in programs/asms/
#   make test     run the tests against your programs
#   make clean    remove everything that was generated
#
# `make test` is the same checker used to grade your work, so a program that
# passes here is a program that passes then. It does not award marks; it only
# reports which cases your programs get right.

.PHONY: all test clean

all:
	$(MAKE) -C programs

test:
	@python3 tests/run_tests.py || true

clean:
	$(MAKE) -C programs clean
	rm -f tests/result.json
	rm -rf tests/__pycache__
