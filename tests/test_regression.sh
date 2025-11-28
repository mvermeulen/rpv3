#!/bin/bash
# Regression tests for RPV3 Kernel Tracer
# Ensures backward compatibility and catches breaking changes

set -e

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# Source test utilities
source "$SCRIPT_DIR/test_utils.sh"

BUILD_DIR="$PROJECT_DIR/build"

# Check if libraries exist in build dir, otherwise check project root
if [ ! -f "$BUILD_DIR/libkernel_tracer.so" ] && [ -f "$PROJECT_DIR/libkernel_tracer.so" ]; then
    print_info "Using libraries from project root instead of build directory"
    BUILD_DIR="$PROJECT_DIR"
fi

print_header "RPV3 Kernel Tracer - Regression Tests"

# Test 1: Output format stability
print_info "Testing output format stability..."
output=$(LD_PRELOAD="$BUILD_DIR/libkernel_tracer.so" "$BUILD_DIR/example_app" 2>&1)

# Verify expected output format hasn't changed
assert_contains "$output" "\[Kernel Tracer\]" "Output uses expected prefix format"
assert_contains "$output" "\[Kernel Trace #" "Kernel trace numbering format unchanged"
assert_contains "$output" "Kernel Name:" "Kernel name field present"
assert_contains "$output" "Thread ID:" "Thread ID field present"
assert_contains "$output" "Correlation ID:" "Correlation ID field present"
assert_contains "$output" "Kernel ID:" "Kernel ID field present"
assert_contains "$output" "Dispatch ID:" "Dispatch ID field present"
assert_contains "$output" "Grid Size:" "Grid size field present"
assert_contains "$output" "Workgroup Size:" "Workgroup size field present"
assert_contains "$output" "Private Segment Size:" "Private segment size field present"
assert_contains "$output" "Group Segment Size:" "Group segment size field present"

# Test 2: Version string format
print_info "Testing version string format..."
output=$(RPV3_OPTIONS="--version" LD_PRELOAD="$BUILD_DIR/libkernel_tracer.so" "$BUILD_DIR/example_app" 2>&1 || true)
assert_contains "$output" "RPV3 Kernel Tracer version [0-9]\+\.[0-9]\+\.[0-9]\+" "Version follows semver format"
assert_contains "$output" "ROCm Profiler SDK" "Version output mentions ROCm Profiler SDK"

# Test 3: Environment variable backward compatibility
print_info "Testing environment variable backward compatibility..."

# Old format should still work
output=$(RPV3_OPTIONS="--version" LD_PRELOAD="$BUILD_DIR/libkernel_tracer.so" "$BUILD_DIR/example_app" 2>&1 || true)
assert_contains "$output" "RPV3 Kernel Tracer version" "RPV3_OPTIONS env var still works"

# Test 4: C and C++ output equivalence (structure)
print_info "Testing C and C++ implementation output equivalence..."
cpp_output=$(LD_PRELOAD="$BUILD_DIR/libkernel_tracer.so" "$BUILD_DIR/example_app" 2>&1)
c_output=$(LD_PRELOAD="$BUILD_DIR/libkernel_tracer_c.so" "$BUILD_DIR/example_app" 2>&1)

# Both should have same number of kernel traces
cpp_trace_count=$(echo "$cpp_output" | grep -c "\[Kernel Trace #" || true)
c_trace_count=$(echo "$c_output" | grep -c "\[Kernel Trace #" || true)
assert_equals "$cpp_trace_count" "$c_trace_count" "C and C++ trace same number of kernels"

# Both should report same total
cpp_total=$(echo "$cpp_output" | grep "Total kernels traced:" | grep -o "[0-9]\+" || echo "0")
c_total=$(echo "$c_output" | grep "Total kernels traced:" | grep -o "[0-9]\+" || echo "0")
assert_equals "$cpp_total" "$c_total" "C and C++ report same total kernel count"

# Test 5: No regression in kernel name extraction
print_info "Testing kernel name extraction hasn't regressed..."
output=$(LD_PRELOAD="$BUILD_DIR/libkernel_tracer.so" "$BUILD_DIR/example_app" 2>&1)

# C++ version should have demangled names
assert_contains "$output" "vectorAdd(float const\*, float const\*, float\*, int)" "vectorAdd signature preserved"
assert_contains "$output" "vectorMul(float const\*, float const\*, float\*, int)" "vectorMul signature preserved"
assert_contains "$output" "matrixTranspose(float const\*, float\*, int, int)" "matrixTranspose signature preserved"

# Test 6: Grid size calculation correctness
print_info "Testing grid size calculations..."
output=$(LD_PRELOAD="$BUILD_DIR/libkernel_tracer.so" "$BUILD_DIR/example_app" 2>&1)

# Vector kernels: 1M elements / 256 threads = 4096 blocks
assert_contains "$output" "Grid Size: \[1048576, 1, 1\]" "Vector kernel grid size correct"

# Matrix: 512x512 with 16x16 blocks = 32x32 grid
assert_contains "$output" "Grid Size: \[512, 512, 1\]" "Matrix kernel grid size correct"

# Test 7: No memory leaks (basic check)
print_info "Testing for obvious memory issues..."
# Run multiple times to catch potential leaks
for i in {1..5}; do
    output=$(LD_PRELOAD="$BUILD_DIR/libkernel_tracer.so" "$BUILD_DIR/example_app" 2>&1)
    exit_code=$?
    if [ $exit_code -ne 0 ]; then
        print_fail "Run $i failed with exit code $exit_code"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        break
    fi
done
if [ $exit_code -eq 0 ]; then
    print_pass "Multiple runs complete without crashes"
    TESTS_PASSED=$((TESTS_PASSED + 1))
fi
TESTS_RUN=$((TESTS_RUN + 1))

# Test 8: Help message completeness
print_info "Testing help message hasn't lost information..."
output=$(RPV3_OPTIONS="--help" LD_PRELOAD="$BUILD_DIR/libkernel_tracer.so" "$BUILD_DIR/example_app" 2>&1 || true)
assert_contains "$output" "Usage:" "Help has usage section"
assert_contains "$output" "Options:" "Help has options section"
assert_contains "$output" "Example:" "Help has example section"
assert_contains "$output" "--version" "Help documents --version"
assert_contains "$output" "--help" "Help documents --help"
assert_contains "$output" "--timeline" "Help documents --timeline"

# Test 9: Profiler initialization sequence
print_info "Testing profiler initialization sequence..."
output=$(LD_PRELOAD="$BUILD_DIR/libkernel_tracer.so" "$BUILD_DIR/example_app" 2>&1)
assert_contains "$output" "Configuring RPV3.*Priority: 0" "Profiler configures with priority 0"
assert_contains "$output" "Initializing profiler tool" "Profiler initializes"
assert_contains "$output" "Profiler initialized successfully" "Profiler initialization succeeds"
assert_contains "$output" "Finalizing profiler tool" "Profiler finalizes cleanly"

# Test 10: No stderr pollution (except expected output)
print_info "Testing stderr cleanliness..."
stderr_output=$(LD_PRELOAD="$BUILD_DIR/libkernel_tracer.so" "$BUILD_DIR/example_app" 2>&1 >/dev/null)
# Should only contain profiler output, no errors
assert_not_contains "$stderr_output" "error:" "No error messages in stderr"
assert_not_contains "$stderr_output" "Error:" "No Error messages in stderr"
assert_not_contains "$stderr_output" "failed" "No failure messages in stderr"

print_summary
