#!/bin/bash
# Integration tests for RPV3 Kernel Tracer - Counter Collection
# Tests --counter option with compute, memory, and mixed modes

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

print_header "RPV3 Kernel Tracer - Counter Tests"

# Helper function to run test
run_counter_test() {
    local lib="$1"
    local mode="$2"
    local lib_name="$3"
    
    print_info "Testing --counter $mode with $lib_name..."
    output=$(RPV3_OPTIONS="--counter $mode" LD_PRELOAD="$lib" "$BUILD_DIR/example_app" 2>&1)
    exit_code=$?
    
    # Check exit code
    assert_exit_code 0 $exit_code "App exits successfully ($lib_name, $mode)"
    
    # Check if counter collection was enabled
    # Note: The mode number might vary, so just check the text
    assert_contains "$output" "Counter collection enabled" "Counter collection enabled ($lib_name, $mode)"
    
    # Check for graceful handling of unsupported hardware (common in CI/dev environments)
    if echo "$output" | grep -q "No agents support counter collection"; then
        print_warn "Hardware does not support counters (expected in some envs)"
        assert_contains "$output" "Counter collection disabled" "Gracefully disabled on unsupported HW"
    else
        # If supported, check for profile creation
        assert_contains "$output" "Setting up counter collection" "Counter setup started"
        # We might not find counters if none match, but we shouldn't crash
    fi
    
    # Check that kernels still ran
    assert_contains "$output" "All kernels completed successfully" "Kernels completed ($lib_name, $mode)"
}

# Test C++ Library
run_counter_test "$BUILD_DIR/libkernel_tracer.so" "compute" "C++ Library"
run_counter_test "$BUILD_DIR/libkernel_tracer.so" "memory" "C++ Library"
run_counter_test "$BUILD_DIR/libkernel_tracer.so" "mixed" "C++ Library"

# Test C Library
run_counter_test "$BUILD_DIR/libkernel_tracer_c.so" "compute" "C Library"
run_counter_test "$BUILD_DIR/libkernel_tracer_c.so" "memory" "C Library"
run_counter_test "$BUILD_DIR/libkernel_tracer_c.so" "mixed" "C Library"

print_summary
