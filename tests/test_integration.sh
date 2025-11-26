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
assert_contains "$output" "1.0.2" "Version output contains correct version number"

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
assert_contains "$output" "Kernel Tracer.*Configuring profiler" "Profiler configures successfully"
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
assert_contains "$output" "Kernel Tracer.*Configuring profiler" "C library profiler configures successfully"
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
# Vector kernels should have grid size [4096, 1, 1] and workgroup [256, 1, 1]
assert_contains "$output" "Grid Size: \[4096, 1, 1\]" "Vector kernel has correct grid size"
assert_contains "$output" "Workgroup Size: \[256, 1, 1\]" "Vector kernel has correct workgroup size"
# Matrix transpose should have 2D grid
assert_contains "$output" "Grid Size: \[512, 512, 1\]" "Matrix kernel has correct grid size"
assert_contains "$output" "Workgroup Size: \[16, 16, 1\]" "Matrix kernel has correct workgroup size"

# Test 10: Unknown option handling
print_info "Testing unknown option handling..."
output=$(RPV3_OPTIONS="--unknown-option" LD_PRELOAD="$BUILD_DIR/libkernel_tracer.so" "$BUILD_DIR/example_app" 2>&1)
assert_contains "$output" "Unknown option" "Unknown option produces warning"
assert_contains "$output" "Kernel Trace" "Profiler continues after unknown option"

print_summary
