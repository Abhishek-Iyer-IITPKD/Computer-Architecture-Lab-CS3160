# CS3160 Lab 3 -- see the handout
#
#   make          build your simulator into build/sim
#   make test     run the tests against it
#   make clean    remove everything that was generated
#
# `make test` is the same checker used to grade your work, so a stage that
# passes here is a stage that passes then. It does not award marks; it only
# reports what your simulator gets right.
#
# -Wall -Wextra is on deliberately. The starter you were given compiles without
# a single warning, so any warning you see is one you introduced, and in C++ a
# warning is very often the bug you were about to spend an hour on.

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -g
BUILD    := build

SRCS := $(wildcard src/*.cpp)
OBJS := $(patsubst src/%.cpp,$(BUILD)/%.o,$(SRCS))

# Everything except the entry point, so the unit tests can bring their own main.
LIB_OBJS := $(filter-out $(BUILD)/sim.o,$(OBJS))

TEST_SRCS := $(wildcard tests/unit/*.cpp)
TEST_OBJS := $(patsubst tests/unit/%.cpp,$(BUILD)/tests/%.o,$(TEST_SRCS))

.PHONY: all test unit clean

all: $(BUILD)/sim

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

# The unit tests on their own, for when you are working on one stage and do not
# want to read past the program-level failures that follow from it.
unit: $(BUILD)/unit_tests
	./$(BUILD)/unit_tests

test:
	@python3 tests/run_tests.py || true

clean:
	rm -rf $(BUILD)
	rm -f tests/result.json
	rm -rf tests/__pycache__

-include $(wildcard $(BUILD)/*.d $(BUILD)/tests/*.d)
