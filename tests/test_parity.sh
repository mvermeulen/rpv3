#!/bin/bash
# Test script to verify parity between C and C++ implementations

set -e

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo "============================================================"
echo "Testing C vs C++ Parity"
echo "============================================================"

# Run with C++ tracer
echo "Running C++ tracer..."
RPV3_OPTIONS="--csv" LD_PRELOAD=./libkernel_tracer.so ./example_app > cpp_output.csv 2>&1

# Run with C tracer
echo "Running C tracer..."
RPV3_OPTIONS="--csv" LD_PRELOAD=./libkernel_tracer_c.so ./example_app > c_output.csv 2>&1

# Function to sanitize output for comparison
# We remove:
# 1. Timestamps and durations (last 5 columns in CSV)
# 2. Pointer addresses (if any, though CSV shouldn't have them in kernel names usually)
# 3. "Configuring RPV3" lines which might have different runtime versions or priorities if not fixed
# 4. Any stderr output that isn't CSV data
sanitize() {
    local input=$1
    local output=$2
    
    # Extract only CSV lines (starting with " or KernelName)
    grep -E '^(KernelName|")' "$input" > "$output.tmp"
    
    # Remove the last 5 columns (timestamps/durations) AND the first column (KernelName)
    # The C++ version demangles names, while C version uses mangled names.
    # We want to verify that all OTHER data (grid sizes, dispatch IDs, etc.) matches.
    # CSV format: KernelName,ThreadID,CorrelationID,KernelID,DispatchID,GridX,GridY,GridZ,WorkgroupX,WorkgroupY,WorkgroupZ,PrivateSeg,GroupSeg,...
    # We keep fields 2-13
    cut -d',' -f2-13 "$output.tmp" > "$output"
    
    rm "$output.tmp"
}

sanitize cpp_output.csv cpp_sanitized.csv
sanitize c_output.csv c_sanitized.csv

# Compare
echo "Comparing outputs..."
if diff -u cpp_sanitized.csv c_sanitized.csv; then
    echo -e "${GREEN}[PASS] C and C++ outputs match exactly (ignoring timestamps)${NC}"
else
    echo -e "${RED}[FAIL] C and C++ outputs differ${NC}"
    echo "Diff:"
    diff -u cpp_sanitized.csv c_sanitized.csv
    exit 1
fi

# Clean up
rm -f cpp_output.csv c_output.csv cpp_sanitized.csv c_sanitized.csv

echo -e "\n${GREEN}All tests passed!${NC}"
