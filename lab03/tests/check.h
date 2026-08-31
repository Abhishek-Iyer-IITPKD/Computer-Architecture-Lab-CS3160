// A test harness small enough to read in one sitting.
//
// The reference simulator's own tests exist to catch the cases that are easy to
// get subtly wrong -- sign extension, division by zero, replacement order --
// where an end-to-end run would still produce a plausible number. They are not
// the lab harness; that lives in each labNN/tests/.
//
// Usage:
//
//     TEST(decode_addi) {
//         Decoded d = decode(0x00108093, 0x80000000);
//         CHECK_EQ(d.op, OP_ADDI);
//     }
//
// Every TEST registers itself, main() runs them all and reports what failed.

#ifndef CS3160_TESTS_CHECK_H
#define CS3160_TESTS_CHECK_H

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

struct TestCase {
    const char *name;
    void (*fn)(void);
};

// The registry. A function-local static keeps it initialised before any test
// registers itself, whatever order the linker chose.
std::vector<TestCase> &test_registry(void);

int  test_failures(void);
void test_fail(const char *file, int line, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

struct TestRegistrar {
    TestRegistrar(const char *name, void (*fn)(void)) {
        TestCase tc = {name, fn};
        test_registry().push_back(tc);
    }
};

#define TEST(name)                                                       \
    static void test_##name(void);                                       \
    static TestRegistrar registrar_##name(#name, test_##name);           \
    static void test_##name(void)

#define CHECK(cond)                                                      \
    do {                                                                 \
        if (!(cond)) {                                                   \
            test_fail(__FILE__, __LINE__, "CHECK(%s) failed", #cond);    \
        }                                                                \
    } while (0)

// Compared as long long so that the same macro serves ints, u32s and enums.
#define CHECK_EQ(got, want)                                              \
    do {                                                                 \
        long long g_ = (long long)(got);                                 \
        long long w_ = (long long)(want);                                 \
        if (g_ != w_) {                                                  \
            test_fail(__FILE__, __LINE__,                                \
                      "%s: got %lld (0x%llx), want %lld (0x%llx)",       \
                      #got, g_, (unsigned long long)g_,                  \
                      w_, (unsigned long long)w_);                       \
        }                                                                \
    } while (0)

#define CHECK_STR_EQ(got, want)                                          \
    do {                                                                 \
        std::string g_ = (got);                                          \
        std::string w_ = (want);                                         \
        if (g_ != w_) {                                                  \
            test_fail(__FILE__, __LINE__, "%s: got \"%s\", want \"%s\"", \
                      #got, g_.c_str(), w_.c_str());                     \
        }                                                                \
    } while (0)

#endif  // CS3160_TESTS_CHECK_H
