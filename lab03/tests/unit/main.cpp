// The unit-test runner: run every registered TEST, report, exit non-zero if
// any of them failed.

#include "check.h"

#include <cstdarg>
#include <cstdlib>

#include "log.h"

std::vector<TestCase> &test_registry(void) {
    static std::vector<TestCase> registry;
    return registry;
}

static int g_failures      = 0;
static int g_case_failures = 0;

int test_failures(void) { return g_failures; }

void test_fail(const char *file, int line, const char *fmt, ...) {
    g_failures++;
    g_case_failures++;
    fprintf(stderr, "    %s:%d: ", file, line);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

int main(void) {
    // The simulator logs while it works; send that to a file rather than the
    // test output, and keep only what the tests themselves print.
    log_open("build/unit_tests.log", LOG_DEBUG);

    // The lab harness sets this so it can record a result per named test rather
    // than a single count -- which is what lets a mark be attached to a stage
    // instead of to "the unit tests". Off by default: sixty lines of PASS is
    // not what someone running `make test` wants to read.
    const bool per_test = getenv("CS3160_TEST_LINES") != NULL;

    std::vector<TestCase> &tests = test_registry();
    int passed = 0;

    for (size_t i = 0; i < tests.size(); i++) {
        g_case_failures = 0;
        tests[i].fn();
        if (g_case_failures == 0) {
            passed++;
            if (per_test) printf("PASS %s\n", tests[i].name);
        } else {
            if (per_test) printf("FAIL %s\n", tests[i].name);
            fprintf(stderr, "  FAILED %s\n", tests[i].name);
        }
    }

    printf("%d of %zu unit tests passed", passed, tests.size());
    if (g_failures != 0) {
        printf(" (%d check%s failed)", g_failures, g_failures == 1 ? "" : "s");
    }
    printf("\n");

    log_close();
    return (g_failures == 0) ? 0 : 1;
}
