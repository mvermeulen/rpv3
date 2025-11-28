#!/bin/bash
# Master test runner for RPV3 Kernel Tracer
# Runs all test suites and reports overall status

set -e

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# Source test utilities
source "$SCRIPT_DIR/test_utils.sh"

# Overall test tracking
TOTAL_SUITES=0
PASSED_SUITES=0
FAILED_SUITES=0

print_header "RPV3 Kernel Tracer - Test Suite Runner"

# Ensure project is built
print_info "Ensuring project is built..."
cd "$PROJECT_DIR"
make clean && make

echo ""

# Run unit tests
print_header "Running Unit Tests"
TOTAL_SUITES=$((TOTAL_SUITES + 1))
if "$SCRIPT_DIR/run_unit_tests.sh"; then
    PASSED_SUITES=$((PASSED_SUITES + 1))
    print_pass "Unit tests passed"
else
    FAILED_SUITES=$((FAILED_SUITES + 1))
    print_fail "Unit tests failed"
fi

echo ""

# Run integration tests
print_header "Running Integration Tests"
TOTAL_SUITES=$((TOTAL_SUITES + 1))
if bash "$SCRIPT_DIR/test_integration.sh"; then
    PASSED_SUITES=$((PASSED_SUITES + 1))
    print_pass "Integration tests passed"
else
    FAILED_SUITES=$((FAILED_SUITES + 1))
    print_fail "Integration tests failed"
fi

echo ""

# Run regression tests
print_header "Running Regression Tests"
TOTAL_SUITES=$((TOTAL_SUITES + 1))
if bash "$SCRIPT_DIR/test_regression.sh"; then
    PASSED_SUITES=$((PASSED_SUITES + 1))
    print_pass "Regression tests passed"
else
    FAILED_SUITES=$((FAILED_SUITES + 1))
    print_fail "Regression tests failed"
fi

echo ""

# Run counter tests
print_header "Running Counter Tests"
TOTAL_SUITES=$((TOTAL_SUITES + 1))
if bash "$SCRIPT_DIR/test_counters.sh"; then
    PASSED_SUITES=$((PASSED_SUITES + 1))
    print_pass "Counter tests passed"
else
    FAILED_SUITES=$((FAILED_SUITES + 1))
    print_fail "Counter tests failed"
fi

echo ""

# Run README example tests
print_header "Running README Example Tests"
TOTAL_SUITES=$((TOTAL_SUITES + 1))
if "$SCRIPT_DIR/test_readme_examples.py"; then
    PASSED_SUITES=$((PASSED_SUITES + 1))
    print_pass "README example tests passed"
else
    FAILED_SUITES=$((FAILED_SUITES + 1))
    print_fail "README example tests failed"
fi

echo ""

# Print overall summary
print_header "Overall Test Summary"
echo "Test suites run:    $TOTAL_SUITES"
echo -e "Test suites passed: ${GREEN}$PASSED_SUITES${NC}"
echo -e "Test suites failed: ${RED}$FAILED_SUITES${NC}"
echo "========================================"

if [ $FAILED_SUITES -eq 0 ]; then
    echo -e "${GREEN}✓ All test suites passed!${NC}"
    exit 0
else
    echo -e "${RED}✗ Some test suites failed!${NC}"
    exit 1
fi
