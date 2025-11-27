#!/bin/bash
# Integration test for CSV output mode

set -e

echo "=== CSV Output Integration Test ==="

# Build the project
echo "Building project..."
make -s clean > /dev/null 2>&1
make -s > /dev/null 2>&1

# Test 1: CSV output with C++ library
echo ""
echo "Test 1: CSV output (C++ library)"
OUTPUT=$(RPV3_OPTIONS="--csv" LD_PRELOAD=./libkernel_tracer.so ./example_app 2>&1)

# Check for CSV header
if echo "$OUTPUT" | grep -q "^KernelName,ThreadID,CorrelationID,KernelID,DispatchID,GridX,GridY,GridZ,WorkgroupX,WorkgroupY,WorkgroupZ,PrivateSeg,GroupSeg,StartTimestamp,EndTimestamp,DurationNs,DurationUs,TimeSinceStartMs"; then
    echo "  ✓ CSV header found"
else
    echo "  ✗ CSV header NOT found"
    exit 1
fi

# Count CSV data rows (exclude header and any stderr messages)
CSV_ROWS=$(echo "$OUTPUT" | grep -v "^\[" | grep -v "^$" | tail -n +2 | wc -l)
if [ "$CSV_ROWS" -gt 0 ]; then
    echo "  ✓ Found $CSV_ROWS CSV data rows"
else
    echo "  ✗ No CSV data rows found"
    exit 1
fi

# Verify CSV format (kernel names are now quoted to handle commas in function signatures)
# Look for lines that start with a quote (quoted kernel name)
FIRST_DATA_ROW=$(echo "$OUTPUT" | grep '^"' | head -n 1)
if [ -z "$FIRST_DATA_ROW" ]; then
    echo "  ✗ No CSV data rows found with quoted kernel name"
    exit 1
fi

# Extract the last 17 fields (after quoted kernel name)
# These should be: ThreadID,CorrelationID,KernelID,DispatchID,GridX,GridY,GridZ,WorkgroupX,WorkgroupY,WorkgroupZ,PrivateSeg,GroupSeg,StartTimestamp,EndTimestamp,DurationNs,DurationUs,TimeSinceStartMs
LAST_17_FIELDS=$(echo "$FIRST_DATA_ROW" | rev | cut -d',' -f1-17 | rev)
FIELD_COUNT=$(echo "$LAST_17_FIELDS" | awk -F',' '{print NF}')
if [ "$FIELD_COUNT" -eq 17 ]; then
    echo "  ✓ Correct CSV format (17 data fields after quoted kernel name)"
else
    echo "  ✗ Incorrect CSV format: found $FIELD_COUNT data fields (expected 17)"
    exit 1
fi

# Verify no human-readable output in CSV mode
if echo "$OUTPUT" | grep -q "Kernel Trace #"; then
    echo "  ✗ Found human-readable output in CSV mode"
    exit 1
else
    echo "  ✓ No human-readable output (CSV only)"
fi

# Test 2: CSV output with C library
echo ""
echo "Test 2: CSV output (C library)"
OUTPUT_C=$(RPV3_OPTIONS="--csv" LD_PRELOAD=./libkernel_tracer_c.so ./example_app 2>&1)

if echo "$OUTPUT_C" | grep -q "^KernelName,ThreadID"; then
    echo "  ✓ CSV header found (C library)"
else
    echo "  ✗ CSV header NOT found (C library)"
    exit 1
fi

CSV_ROWS_C=$(echo "$OUTPUT_C" | grep -v "^\[" | grep -v "^$" | tail -n +2 | wc -l)
if [ "$CSV_ROWS_C" -gt 0 ]; then
    echo "  ✓ Found $CSV_ROWS_C CSV data rows (C library)"
else
    echo "  ✗ No CSV data rows found (C library)"
    exit 1
fi

# Test 3: CSV + Timeline mode
echo ""
echo "Test 3: CSV + Timeline mode"
OUTPUT_TIMELINE=$(RPV3_OPTIONS="--csv --timeline" LD_PRELOAD=./libkernel_tracer.so ./example_app 2>&1)

if echo "$OUTPUT_TIMELINE" | grep -q "^KernelName,ThreadID"; then
    echo "  ✓ CSV header found (timeline mode)"
else
    echo "  ✗ CSV header NOT found (timeline mode)"
    exit 1
fi

# Test 4: Verify CSV can be parsed
echo ""
echo "Test 4: CSV parsing verification"
CSV_DATA=$(echo "$OUTPUT" | grep -v "^\[" | grep -v "^$")
if echo "$CSV_DATA" | column -t -s, > /dev/null 2>&1; then
    echo "  ✓ CSV data is parseable"
else
    echo "  ✗ CSV data is NOT parseable"
    exit 1
fi

# Test 5: Verify numeric fields contain valid numbers
echo ""
echo "Test 5: Numeric field validation"
FIRST_DATA=$(echo "$OUTPUT" | grep '^"' | head -n 1)
THREAD_ID=$(echo "$FIRST_DATA" | rev | cut -d',' -f17 | rev)
if [[ "$THREAD_ID" =~ ^[0-9]+$ ]]; then
    echo "  ✓ ThreadID is numeric"
else
    echo "  ✗ ThreadID is not numeric: $THREAD_ID"
    exit 1
fi

echo ""
echo "=== All CSV tests passed! ==="
