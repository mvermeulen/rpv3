#!/bin/bash
# Test script for RPV3 output options (--output and --outputdir)

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Path to libraries and app
LIB_CPP="./build/libkernel_tracer.so"
LIB_C="./build/libkernel_tracer_c.so"
APP="./build/example_app"

# Output directory for tests
TEST_DIR="/tmp/rpv3_output_test"
mkdir -p $TEST_DIR

echo "=== Testing RPV3 Output Options ==="

# Function to run test
run_test() {
    local test_name=$1
    local lib=$2
    local options=$3
    local expected_file=$4
    local check_content=$5
    
    echo -n "Testing $test_name... "
    
    # Clear previous output
    rm -f $expected_file
    rm -rf $TEST_DIR/*
    
    # Run application
    RPV3_OPTIONS="$options" LD_PRELOAD=$lib $APP > /dev/null 2>&1
    
    # Check if file exists
    if [ -f "$expected_file" ]; then
        # Check content if requested
        if [ ! -z "$check_content" ]; then
            if grep -q "$check_content" "$expected_file"; then
                echo -e "${GREEN}PASS${NC}"
                return 0
            else
                echo -e "${RED}FAIL${NC} (Content mismatch)"
                echo "Expected to find: $check_content"
                return 1
            fi
        else
            echo -e "${GREEN}PASS${NC}"
            return 0
        fi
    else
        # For PID-based tests, check if any file matches pattern
        if [[ "$expected_file" == *"<pid>"* ]]; then
            # Find file in directory
            local pattern=$(echo "$expected_file" | sed 's/<pid>/*/g')
            local found_files=$(ls $pattern 2>/dev/null)
            
            if [ ! -z "$found_files" ]; then
                if [ ! -z "$check_content" ]; then
                    if grep -q "$check_content" $found_files; then
                        echo -e "${GREEN}PASS${NC}"
                        return 0
                    else
                        echo -e "${RED}FAIL${NC} (Content mismatch in $found_files)"
                        return 1
                    fi
                else
                    echo -e "${GREEN}PASS${NC}"
                    return 0
                fi
            else
                echo -e "${RED}FAIL${NC} (File not found matching $pattern)"
                return 1
            fi
        else
            echo -e "${RED}FAIL${NC} (File not found: $expected_file)"
            return 1
        fi
    fi
}

# 1. Test --output with specific filename (C++)
run_test "C++ --output" "$LIB_CPP" "--output $TEST_DIR/custom.txt" "$TEST_DIR/custom.txt" "Kernel Name:"

# 2. Test --output with specific filename (C)
run_test "C --output" "$LIB_C" "--output $TEST_DIR/custom_c.txt" "$TEST_DIR/custom_c.txt" "Kernel Name:"

# 3. Test --outputdir with PID naming (C++)
run_test "C++ --outputdir" "$LIB_CPP" "--outputdir $TEST_DIR" "$TEST_DIR/rpv3_<pid>.txt" "Kernel Name:"

# 4. Test --outputdir with CSV mode (C++) - should have .csv extension
run_test "C++ --outputdir + CSV" "$LIB_CPP" "--outputdir $TEST_DIR --csv" "$TEST_DIR/rpv3_<pid>.csv" "KernelName,ThreadID"

# 5. Test precedence: --output overrides --outputdir
run_test "Precedence check" "$LIB_CPP" "--output $TEST_DIR/override.txt --outputdir $TEST_DIR" "$TEST_DIR/override.txt" "Kernel Name:"

# Clean up
rm -rf $TEST_DIR

echo "=== All Output Option Tests Completed ==="
