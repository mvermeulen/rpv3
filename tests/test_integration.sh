#!/bin/bash
# Integration tests for RPV3 Kernel Tracer
# Tests end-to-end functionality with the example application

set -e

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# Source test utilities
source "$SCRIPT_DIR/test_utils.sh"

# Check if build exists
BUILD_DIR="$PROJECT_DIR/build"
if [ ! -d "$BUILD_DIR" ]; then
    print_warn "Build directory not found. Attempting to build project..."
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    cmake ..
    make
    cd "$PROJECT_DIR"
fi

# Check if libraries exist in build dir, otherwise check project root
if [ ! -f "$BUILD_DIR/libkernel_tracer.so" ] && [ -f "$PROJECT_DIR/libkernel_tracer.so" ]; then
    print_info "Using libraries from project root instead of build directory"
    BUILD_DIR="$PROJECT_DIR"
fi

print_header "RPV3 Kernel Tracer - Integration Tests"

# Test 1: Check if libraries exist
print_info "Checking if libraries are built..."
assert_file_exists "$BUILD_DIR/libkernel_tracer.so" "C++ library exists"
assert_file_exists "$BUILD_DIR/libkernel_tracer_c.so" "C library exists"
assert_file_exists "$BUILD_DIR/example_app" "Example app exists"

# Test 2: Version option (C++ library)
print_info "Testing --version option with C++ library..."
output=$(RPV3_OPTIONS="--version" LD_PRELOAD="$BUILD_DIR/libkernel_tracer.so" "$BUILD_DIR/example_app" 2>&1 || true)
assert_contains "$output" "RPV3 Kernel Tracer version" "Version output contains version string"
assert_contains "$output" "1.4.2" "Version output contains correct version number"


# Test 3: Version option (C library)
print_info "Testing --version option with C library..."
output=$(RPV3_OPTIONS="--version" LD_PRELOAD="$BUILD_DIR/libkernel_tracer_c.so" "$BUILD_DIR/example_app" 2>&1 || true)
assert_contains "$output" "RPV3 Kernel Tracer version" "C library version output contains version string"

# Test 4: Help option (C++ library)
print_info "Testing --help option with C++ library..."
output=$(RPV3_OPTIONS="--help" LD_PRELOAD="$BUILD_DIR/libkernel_tracer.so" "$BUILD_DIR/example_app" 2>&1 || true)
assert_contains "$output" "Usage:" "Help output contains usage information"
assert_contains "$output" "--version" "Help output mentions --version option"
assert_contains "$output" "--help" "Help output mentions --help option"

# Test 5: Kernel tracing (C++ library)
print_info "Testing kernel tracing with C++ library..."
output=$(LD_PRELOAD="$BUILD_DIR/libkernel_tracer.so" "$BUILD_DIR/example_app" 2>&1)
assert_contains "$output" "Kernel Tracer.*Configuring RPV3" "Profiler configures successfully"
assert_contains "$output" "Kernel Trace #1" "First kernel is traced"
assert_contains "$output" "Kernel Trace #2" "Second kernel is traced"
assert_contains "$output" "Kernel Trace #3" "Third kernel is traced"
assert_contains "$output" "vectorAdd" "C++ library demangles kernel names (vectorAdd)"
assert_contains "$output" "vectorMul" "C++ library demangles kernel names (vectorMul)"
assert_contains "$output" "matrixTranspose" "C++ library demangles kernel names (matrixTranspose)"
assert_contains "$output" "Grid Size:" "Output includes grid size"
assert_contains "$output" "Workgroup Size:" "Output includes workgroup size"
assert_contains "$output" "Total kernels traced: 3" "Correct kernel count in summary"

# Test 6: Kernel tracing (C library)
print_info "Testing kernel tracing with C library..."
output=$(LD_PRELOAD="$BUILD_DIR/libkernel_tracer_c.so" "$BUILD_DIR/example_app" 2>&1)
assert_contains "$output" "Kernel Tracer.*Configuring RPV3" "C library profiler configures successfully"
assert_contains "$output" "Kernel Trace #1" "C library traces first kernel"
assert_contains "$output" "Kernel Trace #2" "C library traces second kernel"
assert_contains "$output" "Kernel Trace #3" "C library traces third kernel"
assert_contains "$output" "Grid Size:" "C library output includes grid size"
assert_contains "$output" "Total kernels traced: 3" "C library correct kernel count"

# Test 7: No profiler (baseline)
print_info "Testing example app without profiler..."
output=$("$BUILD_DIR/example_app" 2>&1)
assert_not_contains "$output" "Kernel Tracer" "No profiler output when not loaded"
assert_contains "$output" "All kernels completed successfully" "App runs successfully without profiler"

# Test 8: Library loading without errors
print_info "Testing library loads without errors..."
output=$(LD_PRELOAD="$BUILD_DIR/libkernel_tracer.so" "$BUILD_DIR/example_app" 2>&1)
exit_code=$?
assert_exit_code 0 $exit_code "Example app exits successfully with C++ profiler"

output=$(LD_PRELOAD="$BUILD_DIR/libkernel_tracer_c.so" "$BUILD_DIR/example_app" 2>&1)
exit_code=$?
assert_exit_code 0 $exit_code "Example app exits successfully with C profiler"

# Test 9: Grid and workgroup dimensions
print_info "Testing grid and workgroup dimension extraction..."
output=$(LD_PRELOAD="$BUILD_DIR/libkernel_tracer.so" "$BUILD_DIR/example_app" 2>&1)
# Vector kernels should have grid size [1048576, 1, 1] and workgroup [256, 1, 1]
assert_contains "$output" "Grid Size: \[1048576, 1, 1\]" "Vector kernel has correct grid size"
assert_contains "$output" "Workgroup Size: \[256, 1, 1\]" "Vector kernel has correct workgroup size"
# Matrix transpose should have 2D grid
assert_contains "$output" "Grid Size: \[512, 512, 1\]" "Matrix kernel has correct grid size"
assert_contains "$output" "Workgroup Size: \[16, 16, 1\]" "Matrix kernel has correct workgroup size"

# Test 10: Unknown option handling
print_info "Testing unknown option handling..."
output=$(RPV3_OPTIONS="--unknown-option" LD_PRELOAD="$BUILD_DIR/libkernel_tracer.so" "$BUILD_DIR/example_app" 2>&1)
assert_contains "$output" "Unknown option" "Unknown option produces warning"
assert_contains "$output" "Kernel Trace" "Profiler continues after unknown option"

# Test 11: Timeline mode (C++ library)
print_info "Testing --timeline option with C++ library..."
output=$(RPV3_OPTIONS="--timeline" LD_PRELOAD="$BUILD_DIR/libkernel_tracer.so" "$BUILD_DIR/example_app" 2>&1)
assert_contains "$output" "Timeline mode enabled" "Timeline mode is enabled"
assert_contains "$output" "Setting up buffer tracing" "Buffer tracing is configured"
assert_contains "$output" "Start Timestamp:" "Timeline output includes start timestamp"
assert_contains "$output" "End Timestamp:" "Timeline output includes end timestamp"
assert_contains "$output" "Duration:.*μs" "Timeline output includes duration"
assert_contains "$output" "Time Since Start:.*ms" "Timeline output includes time since start"
assert_contains "$output" "Kernel Trace #1" "First kernel is traced in timeline mode"
assert_contains "$output" "Kernel Trace #2" "Second kernel is traced in timeline mode"
assert_contains "$output" "Kernel Trace #3" "Third kernel is traced in timeline mode"

# Test 12: Timeline mode (C library)
print_info "Testing --timeline option with C library..."
output=$(RPV3_OPTIONS="--timeline" LD_PRELOAD="$BUILD_DIR/libkernel_tracer_c.so" "$BUILD_DIR/example_app" 2>&1)
assert_contains "$output" "Timeline mode enabled" "C library timeline mode is enabled"
assert_contains "$output" "Start Timestamp:" "C library timeline output includes start timestamp"
assert_contains "$output" "Duration:.*μs" "C library timeline output includes duration"
assert_contains "$output" "Time Since Start:.*ms" "C library timeline output includes time since start"

# Test 13: Timeline timestamps are non-zero
print_info "Testing timeline timestamps are non-zero..."
output=$(RPV3_OPTIONS="--timeline" LD_PRELOAD="$BUILD_DIR/libkernel_tracer.so" "$BUILD_DIR/example_app" 2>&1)
# Extract a timestamp value and verify it's not zero
timestamp=$(echo "$output" | grep "Start Timestamp:" | head -1 | grep -o "[0-9]\+" | head -1)
if [ -n "$timestamp" ] && [ "$timestamp" -gt 0 ]; then
    print_pass "Timeline timestamps are non-zero (found: $timestamp ns)"
    TESTS_PASSED=$((TESTS_PASSED + 1))
else
    print_fail "Timeline timestamps are non-zero"
    TESTS_FAILED=$((TESTS_FAILED + 1))
fi
TESTS_RUN=$((TESTS_RUN + 1))

# Test 14: Timeline duration is reasonable
print_info "Testing timeline duration calculations..."
output=$(RPV3_OPTIONS="--timeline" LD_PRELOAD="$BUILD_DIR/libkernel_tracer.so" "$BUILD_DIR/example_app" 2>&1)
# Extract duration and verify it's in a reasonable range (1-1000 μs for these simple kernels)
duration=$(echo "$output" | grep "Duration:" | head -1 | grep -o "[0-9.]\+" | head -1)
if [ -n "$duration" ]; then
    # Use bc for floating point comparison if available, otherwise use awk
    if command -v bc &> /dev/null; then
        is_valid=$(echo "$duration > 0 && $duration < 1000" | bc)
    else
        is_valid=$(awk -v d="$duration" 'BEGIN { print (d > 0 && d < 1000) ? 1 : 0 }')
    fi
    
    if [ "$is_valid" = "1" ]; then
        print_pass "Timeline duration is reasonable (found: $duration μs)"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        print_fail "Timeline duration is reasonable (found: $duration μs, expected 0-1000)"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
else
    print_fail "Timeline duration is reasonable (no duration found)"
    TESTS_FAILED=$((TESTS_FAILED + 1))
fi
TESTS_RUN=$((TESTS_RUN + 1))


print_summary
