#include <stdio.h>
#include <string.h>
#include "lib.c"

/* ============================================================
    Minimalist test harness helpers
    Purpose: provide a lightweight framework for writing and
    running small unit tests without external dependencies.
    Conventions:
        - TEST(name):   defines a test function.
        - RUN_TEST(fn): runs the test and prints its name in bold.
        - ASSERT(expr): checks a boolean expression; prints PASS/FAIL.
        - ASSERT_STR_EQ(expected, actual): string comparison helper
            using strcmp; prints PASS/FAIL with the values.

    Notes:
        - All macros set global `failed = 1` on first failure.
        - Colors are ANSI escape codes (green = pass, red = fail).
        - Intended for quick, educational experiments — not
            production-grade unit testing.
============================================================ */
int failed = 0;

#define TEST(name) \
    void name()

#define RUN_TEST(name) \
    printf("\n\033[1m%s\n\033[0m", #name); \
    name()

#define ASSERT(expr) \
    if (!(expr)) { \
        failed = 1; \
        printf("\033[0;31mFAIL: %s\n\033[0m", #expr); \
    } else { \
        printf("\033[0;32mPASS: %s\n\033[0m", #expr); \
    }

#define ASSERT_STR_EQ(expected, actual) \
    if (!(strcmp(expected, actual) == 0)) { \
        failed = 1; \
        printf("\033[0;31mFAIL: %s != %s\n\033[0m", expected, actual); \
    } else { \
        printf("\033[0;32mPASS: %s == %s\n\033[0m", expected, actual); \
    }
/* ========================== End helpers ===================== */

TEST(test_to_path_with_slash) {
    char req[] = "GET /blog/ HTTP/1.1\nHost: example.com";
    char * path = to_path(req);

    ASSERT_STR_EQ("blog/index.html", path);
}

TEST(test_to_path_without_slash) {
    char req[] = "GET /blog HTTP/1.1\nHost: example.com";
    char * path = to_path(req);

    ASSERT_STR_EQ("blog/index.html", path);
}

TEST(test_to_path_root) {
    char req[] = "GET / HTTP/1.1\nHost: example.com";
    char * path = to_path(req);

    ASSERT_STR_EQ("index.html", path);
}

TEST(test_to_path_malformed) {
    char req[] = "GET ";
    char * path = to_path(req);

    ASSERT(path == nullptr);
}

int main() {
    // Run all registered tests in sequence.
    // Each test prints its name,and the PASS/FAIL result.
    // The program returns 0 if all tests pass, or 1 if any test fails.
    RUN_TEST(test_to_path_with_slash);
    RUN_TEST(test_to_path_without_slash);
    RUN_TEST(test_to_path_root);
    RUN_TEST(test_to_path_malformed);

    return failed;
}
