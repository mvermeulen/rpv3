#!/bin/bash
# Test script to verify relaxed RocBLAS environment variable checks for regular files

set -e

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo "============================================================"
echo "Testing Relaxed RocBLAS Environment Variable Checks"
echo "============================================================"

# Create a dummy log file
LOG_FILE="rocblas_dummy.log"
echo "dummy log content" > $LOG_FILE

# Define tracer libraries
TRACER_CPP="./libkernel_tracer.so"
TRACER_C="./libkernel_tracer_c.so"

# Function to run test
run_test() {
    local lib=$1
    local lang=$2
    
    echo -e "\nTesting $lang Tracer ($lib)..."
    
    # Run with --rocblas pointing to the file, but NO matching env var
    # We expect this to SUCCEED now (it would have failed before)
    # We use a simple command like 'ls' just to trigger the tool_init
    
    OUTPUT=$(RPV3_OPTIONS="--rocblas $LOG_FILE" LD_PRELOAD=$lib ./example_app 2>&1 || true)
    
    if echo "$OUTPUT" | grep -q "Detected RocBLAS log file/pipe: $LOG_FILE"; then
        echo -e "${GREEN}[PASS] Tracer accepted regular file without env var match${NC}"
    else
        echo -e "${RED}[FAIL] Tracer did not accept regular file${NC}"
        echo "Output:"
        echo "$OUTPUT"
        return 1
    fi
    
    # Verify it didn't print the error message
    if echo "$OUTPUT" | grep -q "Error: --rocblas .* does not match"; then
        echo -e "${RED}[FAIL] Tracer printed mismatch error${NC}"
        return 1
    fi
}

# Run tests
run_test $TRACER_CPP "C++"
run_test $TRACER_C "C"

# Clean up
rm -f $LOG_FILE

echo -e "\n${GREEN}All tests passed!${NC}"
