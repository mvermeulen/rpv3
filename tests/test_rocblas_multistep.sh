#!/bin/bash
# Test script to verify RocBLAS multi-step workflow (generate log -> trace log)

set -e

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo "============================================================"
echo "Testing RocBLAS Multi-Step Workflow"
echo "============================================================"

LOG_FILE="rocblas_multistep.log"
rm -f $LOG_FILE

# Step 1: Generate RocBLAS log (no tracer)
echo -e "\nStep 1: Generating RocBLAS log..."
export ROCBLAS_LAYER=1
export ROCBLAS_LOG_TRACE_PATH=$LOG_FILE

# Run example_rocblas without tracer
./example_rocblas > /dev/null 2>&1

# Verify log file exists and has content
if [ -s "$LOG_FILE" ]; then
    echo -e "${GREEN}[PASS] Log file created and not empty${NC}"
else
    echo -e "${RED}[FAIL] Log file not created or empty${NC}"
    exit 1
fi

# Step 2: Run tracer reading from the log file
echo -e "\nStep 2: Running tracer with generated log..."
unset ROCBLAS_LAYER
unset ROCBLAS_LOG_TRACE_PATH

# Run with tracer
OUTPUT=$(RPV3_OPTIONS="--csv --rocblas $LOG_FILE" LD_PRELOAD=./libkernel_tracer.so ./example_rocblas 2>&1)

# Verify output contains RocBLAS logs
# We look for a known RocBLAS function that should be in the log (e.g., rocblas_sgemm)
# Note: The tracer filters out create_handle, etc., so we look for the compute kernels
if echo "$OUTPUT" | grep -q "# rocblas_sgemm"; then
    echo -e "${GREEN}[PASS] Found 'rocblas_sgemm' in trace output${NC}"
else
    echo -e "${RED}[FAIL] Did not find 'rocblas_sgemm' in trace output${NC}"
    echo "Output snippet:"
    echo "$OUTPUT" | head -n 20
    exit 1
fi

# Clean up
rm -f $LOG_FILE

echo -e "\n${GREEN}All tests passed!${NC}"
