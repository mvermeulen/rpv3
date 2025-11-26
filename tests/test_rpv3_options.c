/* MIT License
 * Unit tests for rpv3_options.c
 * Tests the options parsing functionality
 */

#include "../rpv3_options.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Test counter */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* Color codes */
#define RED "\033[0;31m"
#define GREEN "\033[0;32m"
#define BLUE "\033[0;34m"
#define NC "\033[0m"

/* Test macros */
#define TEST(name) \
    void test_##name(); \
    void run_test_##name() { \
        tests_run++; \
        printf(BLUE "Running: " NC "%s\n", #name); \
        test_##name(); \
    } \
    void test_##name()

#define ASSERT_EQUALS(expected, actual, msg) \
    do { \
        if ((expected) == (actual)) { \
            tests_passed++; \
            printf(GREEN "  ✓ PASS" NC ": %s\n", msg); \
        } else { \
            tests_failed++; \
            printf(RED "  ✗ FAIL" NC ": %s\n", msg); \
            printf("    Expected: %d, Got: %d\n", (expected), (actual)); \
        } \
    } while(0)

/* Redirect stdout/stderr for testing */
static FILE* original_stdout;
static FILE* original_stderr;

void redirect_output() {
    original_stdout = stdout;
    original_stderr = stderr;
    stdout = fopen("/dev/null", "w");
    stderr = fopen("/dev/null", "w");
}

void restore_output() {
    if (stdout != original_stdout) {
        fclose(stdout);
        stdout = original_stdout;
    }
    if (stderr != original_stderr) {
        fclose(stderr);
        stderr = original_stderr;
    }
}

/* Test cases */

TEST(null_environment_variable) {
    unsetenv("RPV3_OPTIONS");
    redirect_output();
    int result = rpv3_parse_options();
    restore_output();
    ASSERT_EQUALS(RPV3_OPTIONS_CONTINUE, result, "Null env var should return CONTINUE");
}

TEST(empty_environment_variable) {
    setenv("RPV3_OPTIONS", "", 1);
    redirect_output();
    int result = rpv3_parse_options();
    restore_output();
    ASSERT_EQUALS(RPV3_OPTIONS_CONTINUE, result, "Empty env var should return CONTINUE");
}

TEST(version_option) {
    setenv("RPV3_OPTIONS", "--version", 1);
    redirect_output();
    int result = rpv3_parse_options();
    restore_output();
    ASSERT_EQUALS(RPV3_OPTIONS_EXIT, result, "--version should return EXIT");
}

TEST(help_option) {
    setenv("RPV3_OPTIONS", "--help", 1);
    redirect_output();
    int result = rpv3_parse_options();
    restore_output();
    ASSERT_EQUALS(RPV3_OPTIONS_EXIT, result, "--help should return EXIT");
}

TEST(help_short_option) {
    setenv("RPV3_OPTIONS", "-h", 1);
    redirect_output();
    int result = rpv3_parse_options();
    restore_output();
    ASSERT_EQUALS(RPV3_OPTIONS_EXIT, result, "-h should return EXIT");
}

TEST(timeline_option) {
    setenv("RPV3_OPTIONS", "--timeline", 1);
    rpv3_timeline_enabled = 0;
    int result = rpv3_parse_options();
    ASSERT_EQUALS(RPV3_OPTIONS_CONTINUE, result, "--timeline should return CONTINUE");
    ASSERT_EQUALS(1, rpv3_timeline_enabled, "rpv3_timeline_enabled should be set to 1");
    return 0;
}

TEST(unknown_option) {
    setenv("RPV3_OPTIONS", "--unknown", 1);
    redirect_output();
    int result = rpv3_parse_options();
    restore_output();
    ASSERT_EQUALS(RPV3_OPTIONS_CONTINUE, result, "Unknown option should return CONTINUE (with warning)");
}

TEST(multiple_options) {
    setenv("RPV3_OPTIONS", "--version --help", 1);
    redirect_output();
    int result = rpv3_parse_options();
    restore_output();
    ASSERT_EQUALS(RPV3_OPTIONS_EXIT, result, "Multiple options should return EXIT if any trigger exit");
}

TEST(mixed_valid_invalid_options) {
    setenv("RPV3_OPTIONS", "--unknown --version", 1);
    redirect_output();
    int result = rpv3_parse_options();
    restore_output();
    ASSERT_EQUALS(RPV3_OPTIONS_EXIT, result, "Mixed options should return EXIT if any valid option triggers exit");
}

TEST(whitespace_handling) {
    setenv("RPV3_OPTIONS", "  --version  ", 1);
    redirect_output();
    int result = rpv3_parse_options();
    restore_output();
    ASSERT_EQUALS(RPV3_OPTIONS_EXIT, result, "Should handle leading/trailing whitespace");
}

TEST(tab_separated_options) {
    setenv("RPV3_OPTIONS", "--version\t--help", 1);
    redirect_output();
    int result = rpv3_parse_options();
    restore_output();
    ASSERT_EQUALS(RPV3_OPTIONS_EXIT, result, "Should handle tab-separated options");
}

/* Main test runner */
int main() {
    printf("\n");
    printf(BLUE "========================================\n" NC);
    printf(BLUE "RPV3 Options Parser Unit Tests\n" NC);
    printf(BLUE "========================================\n" NC);
    printf("\n");

    /* Run all tests */
    run_test_null_environment_variable();
    run_test_empty_environment_variable();
    run_test_version_option();
    run_test_help_option();
    run_test_help_short_option();
    run_test_timeline_option();
    run_test_unknown_option();
    run_test_multiple_options();
    run_test_mixed_valid_invalid_options();
    run_test_whitespace_handling();
    run_test_tab_separated_options();

    /* Print summary */
    printf("\n");
    printf("========================================\n");
    printf("Test Summary\n");
    printf("========================================\n");
    printf("Tests run:    %d\n", tests_run);
    printf(GREEN "Tests passed: %d\n" NC, tests_passed);
    printf(RED "Tests failed: %d\n" NC, tests_failed);
    printf("========================================\n");

    if (tests_failed == 0) {
        printf(GREEN "All tests passed!\n" NC);
        return 0;
    } else {
        printf(RED "Some tests failed!\n" NC);
        return 1;
    }
}
