#!/bin/bash
# Test script for --backtrace option

set -e

# Detect build directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# Source test utilities
source "$SCRIPT_DIR/test_utils.sh"

print_header "Testing --backtrace Option"

# Test 1: Basic backtrace with C++ library
print_info "Test 1: Basic backtrace with C++ library"
output=$(RPV3_OPTIONS="--backtrace" LD_PRELOAD="$BUILD_DIR/libkernel_tracer.so" "$BUILD_DIR/example_app" 2>&1)
assert_contains "$output" "Backtrace mode enabled" "Backtrace mode is enabled"
assert_contains "$output" "Call Stack" "Call stack is printed"
assert_contains "$output" "libamdhip64.so" "HIP library is shown in backtrace"
assert_contains "$output" "example_app" "Application is shown in backtrace"
assert_contains "$output" "vectorAdd" "Kernel name is shown"
assert_contains "$output" "Dispatch ID:" "Dispatch ID is shown"
assert_contains "$output" "Grid Size:" "Grid size is shown"

# Test 2: Basic backtrace with C library
print_info "Test 2: Basic backtrace with C library"
output=$(RPV3_OPTIONS="--backtrace" LD_PRELOAD="$BUILD_DIR/libkernel_tracer_c.so" "$BUILD_DIR/example_app" 2>&1)
assert_contains "$output" "Backtrace mode enabled" "C library: Backtrace mode is enabled"
assert_contains "$output" "Call Stack" "C library: Call stack is printed"
assert_contains "$output" "libamdhip64.so" "C library: HIP library is shown"
assert_contains "$output" "example_app" "C library: Application is shown"

# Test 3: Incompatibility with --timeline
print_info "Test 3: Incompatibility with --timeline"
output=$(RPV3_OPTIONS="--backtrace --timeline" LD_PRELOAD="$BUILD_DIR/libkernel_tracer.so" "$BUILD_DIR/example_app" 2>&1 || true)
assert_contains "$output" "Error.*backtrace.*incompatible.*timeline" "Error message for timeline incompatibility"
assert_contains "$output" "Backtrace overhead would distort timing measurements" "Explanation for timeline incompatibility"

# Test 4: Incompatibility with --csv
print_info "Test 4: Incompatibility with --csv"
output=$(RPV3_OPTIONS="--backtrace --csv" LD_PRELOAD="$BUILD_DIR/libkernel_tracer.so" "$BUILD_DIR/example_app" 2>&1 || true)
assert_contains "$output" "Error.*backtrace.*incompatible.*csv" "Error message for CSV incompatibility"
assert_contains "$output" "Variable-length backtraces don't fit CSV schema" "Explanation for CSV incompatibility"

# Test 5: RocBLAS backtrace (if available)
if [ -f "$BUILD_DIR/example_rocblas" ]; then
    print_info "Test 5: RocBLAS backtrace"
    output=$(RPV3_OPTIONS="--backtrace" LD_PRELOAD="$BUILD_DIR/libkernel_tracer.so" "$BUILD_DIR/example_rocblas" 2>&1)
    assert_contains "$output" "Call Stack" "RocBLAS: Call stack is printed"
    assert_contains "$output" "librocblas.so" "RocBLAS library is shown in backtrace"
    assert_contains "$output" "rocblas_sgemm" "RocBLAS function is identified"
    assert_contains "$output" "Cijk_" "Tensile kernel is traced"
else
    print_warning "Skipping RocBLAS test (example_rocblas not found)"
fi

# Test 6: Verify no normal trace output in backtrace mode
print_info "Test 6: Verify backtrace-only output"
output=$(RPV3_OPTIONS="--backtrace" LD_PRELOAD="$BUILD_DIR/libkernel_tracer.so" "$BUILD_DIR/example_app" 2>&1)
assert_not_contains "$output" "Thread ID:" "Thread ID not shown in backtrace mode"
assert_not_contains "$output" "Correlation ID:" "Correlation ID not shown in backtrace mode"
assert_not_contains "$output" "Private Segment Size:" "Private segment not shown in backtrace mode"

# Test 7: Multiple kernels traced
print_info "Test 7: Multiple kernels traced"
output=$(RPV3_OPTIONS="--backtrace" LD_PRELOAD="$BUILD_DIR/libkernel_tracer.so" "$BUILD_DIR/example_app" 2>&1)
assert_contains "$output" "Kernel Trace #1" "First kernel traced"
assert_contains "$output" "Kernel Trace #2" "Second kernel traced"
assert_contains "$output" "Kernel Trace #3" "Third kernel traced"
assert_contains "$output" "Total kernels traced: 3" "Correct total count"

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}All backtrace tests passed!${NC}"
echo -e "${GREEN}========================================${NC}"
