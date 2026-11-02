# CS3160 Lab 9 -- the cache: mapping and replacement
#
#   make          build the simulator into build/sim
#   make test     run the tests against it
#   make clean    remove all generated files
#
# `make test` is the checker used for grading. It awards no marks; it reports
# which tests pass.
#
# -Wall -Wextra is enabled deliberately. The starter compiles without warnings,
# so every warning comes from new code, and in C++ a warning frequently
# indicates a bug.

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -g
BUILD    := build

SRCS := $(wildcard src/*.cpp)
OBJS := $(patsubst src/%.cpp,$(BUILD)/%.o,$(SRCS))

# Everything except the entry point, so that the unit tests can supply main.
LIB_OBJS := $(filter-out $(BUILD)/sim.o,$(OBJS))

TEST_SRCS := $(wildcard tests/unit/*.cpp)
TEST_OBJS := $(patsubst tests/unit/%.cpp,$(BUILD)/tests/%.o,$(TEST_SRCS))

.PHONY: all test unit clean compile_commands.json

# Every build runs through tests/telemetry.py, which shows the compiler output
# unchanged, exits with the compiler's status, and records the build, whether
# it succeeded or failed.
#
# Without tests/ the build still works; only the record is lost.
all: compile_commands.json
	@if [ -f tests/telemetry.py ]; then \
	  python3 tests/telemetry.py build -- $(MAKE) --no-print-directory $(BUILD)/sim; \
	else \
	  $(MAKE) --no-print-directory $(BUILD)/sim; \
	fi

$(BUILD)/sim: $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/%.o: src/%.cpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/tests/%.o: tests/unit/%.cpp
	@mkdir -p $(BUILD)/tests
	$(CXX) $(CXXFLAGS) -Isrc -Itests -MMD -MP -c $< -o $@

$(BUILD)/unit_tests: $(TEST_OBJS) $(LIB_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

# The unit tests alone, without the program tests.
unit: compile_commands.json
	@if [ -f tests/telemetry.py ]; then \
	  python3 tests/telemetry.py build -- $(MAKE) --no-print-directory $(BUILD)/unit_tests; \
	else \
	  $(MAKE) --no-print-directory $(BUILD)/unit_tests; \
	fi
	./$(BUILD)/unit_tests

test:
	@python3 tests/run_tests.py || true

clean:
	rm -rf $(BUILD)
	rm -f tests/result.json
	rm -rf tests/__pycache__

# compile_commands.json tells clangd (the C++ extension configured in .vscode/)
# how each file is compiled, which enables Go to Definition from a call to a
# function body in another file. It requires absolute paths, so it is written by
# each build on this machine rather than supplied, and only when it changes. If
# it cannot be written, only editor navigation is affected.
compile_commands.json:
	@[ ! -f tests/compdb.py ] || python3 tests/compdb.py $(BUILD)/sim $(BUILD)/unit_tests 2>/dev/null || true

-include $(wildcard $(BUILD)/*.d $(BUILD)/tests/*.d)
