#!/bin/bash
# Test script to verify RocBLAS log filtering

set -e

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo "============================================================"
echo "Testing RocBLAS Log Filtering"
echo "============================================================"

# Create a dummy log file with mixed content
LOG_FILE="rocblas_filter.log"
cat <<EOF > $LOG_FILE
rocblas_create_handle
rocblas_set_stream
valid_log_entry_1
rocblas_destroy_handle
valid_log_entry_2
EOF

# Define tracer libraries
TRACER_CPP="./libkernel_tracer.so"
TRACER_C="./libkernel_tracer_c.so"

# Function to run test
run_test() {
    local lib=$1
    local lang=$2
    
    echo -e "\nTesting $lang Tracer ($lib)..."
    
    # Run with --rocblas <file>
    # We use RPV3_OPTIONS to set the option
    # We use example_rocblas because it runs GEMM kernels which pass is_tensile_kernel check
    OUTPUT=$(RPV3_OPTIONS="--rocblas $LOG_FILE" LD_PRELOAD=$lib ./example_rocblas 2>&1 || true)
    
    # Check for filtered lines (should NOT be present)
    if echo "$OUTPUT" | grep -q "rocblas_create_handle"; then
        echo -e "${RED}[FAIL] Found 'rocblas_create_handle' in output${NC}"
        return 1
    fi
    if echo "$OUTPUT" | grep -q "rocblas_set_stream"; then
        echo -e "${RED}[FAIL] Found 'rocblas_set_stream' in output${NC}"
        return 1
    fi
    if echo "$OUTPUT" | grep -q "rocblas_destroy_handle"; then
        echo -e "${RED}[FAIL] Found 'rocblas_destroy_handle' in output${NC}"
        return 1
    fi
    
    # Check for valid lines (should be present)
    if echo "$OUTPUT" | grep -q "valid_log_entry_1"; then
        echo -e "${GREEN}[PASS] Found 'valid_log_entry_1' in output${NC}"
    else
        echo -e "${RED}[FAIL] Did not find 'valid_log_entry_1' in output${NC}"
        echo "Output:"
        echo "$OUTPUT"
        return 1
    fi
}

# Run tests
run_test $TRACER_CPP "C++"
run_test $TRACER_C "C"

# Clean up
rm -f $LOG_FILE

echo -e "\n${GREEN}All tests passed!${NC}"
