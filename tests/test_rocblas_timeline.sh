#!/bin/bash
# Test script to verify RocBLAS timeline support

set -e

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo "============================================================"
echo "Testing RocBLAS Timeline Support"
echo "============================================================"

# Create a dummy log file
LOG_FILE="rocblas_timeline.log"
echo "dummy log content" > $LOG_FILE

# Define tracer libraries
TRACER_CPP="./libkernel_tracer.so"
TRACER_C="./libkernel_tracer_c.so"

# Function to run test
run_test_file() {
    local lib=$1
    local lang=$2
    
    echo -e "\nTesting $lang Tracer with File ($lib)..."
    
    # Run with --timeline and --rocblas <file>
    OUTPUT=$(RPV3_OPTIONS="--timeline --rocblas $LOG_FILE" LD_PRELOAD=$lib ./example_app 2>&1 || true)
    
    if echo "$OUTPUT" | grep -q "Detected RocBLAS log file/pipe: $LOG_FILE"; then
        echo -e "${GREEN}[PASS] Tracer accepted regular file in timeline mode${NC}"
    else
        echo -e "${RED}[FAIL] Tracer did not accept regular file in timeline mode${NC}"
        echo "Output:"
        echo "$OUTPUT"
        return 1
    fi
}

run_test_pipe() {
    local lib=$1
    local lang=$2
    
    echo -e "\nTesting $lang Tracer with Pipe ($lib)..."
    
    PIPE_NAME="test_pipe_timeline"
    rm -f $PIPE_NAME
    mkfifo $PIPE_NAME
    
    # Run with --timeline and --rocblas <pipe>
    # We expect a warning/error and NO detection message
    # We must set ROCBLAS_LOG_TRACE to match, otherwise it fails earlier with "Logging disabled"
    OUTPUT=$(RPV3_OPTIONS="--timeline --rocblas $PIPE_NAME" ROCBLAS_LOG_TRACE=$PIPE_NAME LD_PRELOAD=$lib ./example_app 2>&1 || true)
    
    if echo "$OUTPUT" | grep -q "Warning: RocBLAS logging with named pipes is not supported in timeline mode"; then
        echo -e "${GREEN}[PASS] Tracer correctly warned about pipe in timeline mode${NC}"
    else
        echo -e "${RED}[FAIL] Tracer did not warn about pipe in timeline mode${NC}"
        echo "Output:"
        echo "$OUTPUT"
        rm -f $PIPE_NAME
        return 1
    fi
    
    rm -f $PIPE_NAME
}

# Run tests
run_test_file $TRACER_CPP "C++"
run_test_file $TRACER_C "C"
run_test_pipe $TRACER_CPP "C++"
run_test_pipe $TRACER_C "C"

# Clean up
rm -f $LOG_FILE

echo -e "\n${GREEN}All tests passed!${NC}"
