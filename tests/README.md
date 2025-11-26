# RPV3 Testing Suite

This directory contains the test suite for the RPV3 Kernel Tracer project.

## Test Structure

### Unit Tests
- **`test_rpv3_options.c`** - Unit tests for the options parser
- **`run_unit_tests.sh`** - Compiles and runs unit tests

### Integration Tests
- **`test_integration.sh`** - End-to-end tests with the example application
  - Library loading verification
  - Kernel tracing validation
  - Output format checking
  - C vs C++ implementation comparison

### Regression Tests
- **`test_regression.sh`** - Backward compatibility and stability tests
  - Output format stability
  - Version string format
  - Environment variable compatibility
  - Memory leak detection (basic)

### Test Utilities
- **`test_utils.sh`** - Shared utility functions
  - Color output helpers
  - Assertion functions
  - Test result tracking

### Test Runner
- **`run_tests.sh`** - Master test runner that executes all test suites

## Running Tests

### Run All Tests
```bash
# From project root
make test

# Or directly
cd tests
./run_tests.sh
```

### Run Specific Test Suites
```bash
# Unit tests only
./tests/run_unit_tests.sh

# Integration tests only
./tests/test_integration.sh

# Regression tests only
./tests/test_regression.sh
```

### Using CMake/CTest
```bash
cd build
cmake ..
make
ctest --output-on-failure
```

## Test Coverage

### What's Tested

#### Options Parser (`rpv3_options.c`)
- ✅ Null/empty environment variable handling
- ✅ `--version` option
- ✅ `--help` and `-h` options
- ✅ `--timeline` option (not implemented message)
- ✅ Unknown option handling
- ✅ Multiple options parsing
- ✅ Whitespace and tab handling

#### Kernel Tracer Libraries
- ✅ Library loading without errors
- ✅ Kernel name extraction (demangled in C++, mangled in C)
- ✅ Grid and workgroup dimension extraction
- ✅ Memory segment size reporting
- ✅ Kernel counting accuracy
- ✅ C and C++ implementation equivalence
- ✅ Output format consistency

#### Integration
- ✅ End-to-end tracing with example app
- ✅ All three kernels traced correctly
- ✅ No crashes or memory errors
- ✅ Clean initialization and finalization

#### Backward Compatibility
- ✅ Output format stability
- ✅ Version string format
- ✅ Environment variable compatibility
- ✅ Help message completeness

## Adding New Tests

### Adding a Unit Test

Edit `test_rpv3_options.c`:

```c
TEST(my_new_test) {
    // Setup
    setenv("RPV3_OPTIONS", "my-option", 1);
    
    // Execute
    redirect_output();
    int result = rpv3_parse_options();
    restore_output();
    
    // Assert
    ASSERT_EQUALS(expected, result, "Test description");
}
```

Then add to `main()`:
```c
run_test_my_new_test();
```

### Adding an Integration Test

Edit `test_integration.sh`:

```bash
print_info "Testing my new feature..."
output=$(LD_PRELOAD="$BUILD_DIR/libkernel_tracer.so" "$BUILD_DIR/example_app" 2>&1)
assert_contains "$output" "expected string" "Test description"
```

### Adding a Regression Test

Edit `test_regression.sh` following the same pattern as integration tests.

## Troubleshooting

### Tests Fail to Find Libraries

Ensure the project is built:
```bash
mkdir -p build
cd build
cmake ..
make
```

### Permission Denied on Scripts

Make scripts executable:
```bash
chmod +x tests/*.sh
```

### ROCm Not Found

Ensure ROCm is installed and in your PATH:
```bash
export CMAKE_PREFIX_PATH=/opt/rocm
```

## Test Output

Tests use color-coded output:
- 🟢 **Green** - Test passed
- 🔴 **Red** - Test failed
- 🔵 **Blue** - Informational message
- 🟡 **Yellow** - Warning

Example output:
```
========================================
RPV3 Kernel Tracer - Integration Tests
========================================

ℹ INFO: Checking if libraries are built...
✓ PASS: C++ library exists
✓ PASS: C library exists
✓ PASS: Example app exists
...
========================================
Test Summary
========================================
Tests run:    25
Tests passed: 25
Tests failed: 0
========================================
All tests passed!
```

## Continuous Integration

Tests are designed to be CI/CD friendly:
- Return appropriate exit codes (0 for success, 1 for failure)
- Minimal dependencies (bash, gcc, cmake)
- Fast execution (< 30 seconds total)
- Clear, parseable output

## Future Enhancements

Potential test additions:
- [ ] Performance benchmarking
- [ ] Buffer tracing mode tests (when implemented)
- [ ] Timeline feature tests (when implemented)
- [ ] Stress testing with many kernels
- [ ] Multi-threaded application testing
- [ ] Code coverage reporting
