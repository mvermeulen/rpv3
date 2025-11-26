#!/bin/bash
# Unit test runner for RPV3 options parser

set -e

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# Source test utilities
source "$SCRIPT_DIR/test_utils.sh"

print_info "Compiling unit tests..."

# Compile the unit test executable
gcc -std=c11 -I"$PROJECT_DIR" \
    -o "$SCRIPT_DIR/test_rpv3_options" \
    "$SCRIPT_DIR/test_rpv3_options.c" \
    "$PROJECT_DIR/rpv3_options.c"

print_info "Running unit tests..."
echo ""

# Run the tests
"$SCRIPT_DIR/test_rpv3_options"
exit_code=$?

# Cleanup
rm -f "$SCRIPT_DIR/test_rpv3_options"

exit $exit_code
