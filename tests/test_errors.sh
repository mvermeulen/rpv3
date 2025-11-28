#!/bin/bash
# Test script to verify error handling and edge cases

set -e

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo "============================================================"
echo "Testing Error Handling"
echo "============================================================"

# Test 1: Invalid --rocblas file (non-existent)
echo -e "\nTest 1: Invalid --rocblas file (non-existent)"
OUTPUT=$(RPV3_OPTIONS="--rocblas non_existent_file.log" LD_PRELOAD=./libkernel_tracer.so ./example_app 2>&1 || true)

# We expect the app to run but maybe print a warning (or just ignore it silently depending on implementation)
# The key is that it shouldn't crash.
if echo "$OUTPUT" | grep -q "All kernels completed successfully"; then
    echo -e "${GREEN}[PASS] App ran successfully despite invalid rocblas file${NC}"
else
    echo -e "${RED}[FAIL] App failed or crashed${NC}"
    echo "$OUTPUT"
    exit 1
fi

# Check if it printed a warning (optional, but good practice)
if echo "$OUTPUT" | grep -q "Failed to open RocBLAS log"; then
    echo -e "${GREEN}[PASS] Warning message found${NC}"
else
    echo -e "${RED}[WARN] No warning message found for invalid file${NC}"
fi


# Test 2: Invalid --outputdir (non-existent)
echo -e "\nTest 2: Invalid --outputdir (non-existent)"
OUTPUT=$(RPV3_OPTIONS="--outputdir /non/existent/dir" LD_PRELOAD=./libkernel_tracer.so ./example_app 2>&1 || true)

if echo "$OUTPUT" | grep -q "All kernels completed successfully"; then
    echo -e "${GREEN}[PASS] App ran successfully despite invalid outputdir${NC}"
else
    echo -e "${RED}[FAIL] App failed or crashed${NC}"
    echo "$OUTPUT"
    exit 1
fi

if echo "$OUTPUT" | grep -q "Failed to open output file"; then
    echo -e "${GREEN}[PASS] Error message found for invalid outputdir${NC}"
else
    echo -e "${RED}[WARN] No error message found for invalid outputdir${NC}"
fi


# Test 3: Permission denied on output file
echo -e "\nTest 3: Permission denied on output file"
touch readonly.txt
chmod 444 readonly.txt

OUTPUT=$(RPV3_OPTIONS="--output readonly.txt" LD_PRELOAD=./libkernel_tracer.so ./example_app 2>&1 || true)

if echo "$OUTPUT" | grep -q "All kernels completed successfully"; then
    echo -e "${GREEN}[PASS] App ran successfully despite permission error${NC}"
else
    echo -e "${RED}[FAIL] App failed or crashed${NC}"
    echo "$OUTPUT"
    exit 1
fi

if echo "$OUTPUT" | grep -q "Failed to open output file"; then
    echo -e "${GREEN}[PASS] Error message found for permission denied${NC}"
else
    echo -e "${RED}[WARN] No error message found for permission denied${NC}"
fi

rm -f readonly.txt


# Test 4: Malformed options
echo -e "\nTest 4: Malformed options (--output with no arg)"
# This might cause the parser to pick up the next token or just fail.
# In our implementation, if --output is last, it might crash or warn.
OUTPUT=$(RPV3_OPTIONS="--output" LD_PRELOAD=./libkernel_tracer.so ./example_app 2>&1 || true)

if echo "$OUTPUT" | grep -q "All kernels completed successfully"; then
    echo -e "${GREEN}[PASS] App ran successfully despite malformed options${NC}"
else
    echo -e "${RED}[FAIL] App failed or crashed${NC}"
    echo "$OUTPUT"
    exit 1
fi

echo -e "\n${GREEN}All tests passed!${NC}"
