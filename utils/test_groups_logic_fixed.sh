#!/bin/bash
check_groups_fixed() {
    # Pad with spaces
    USER_GROUPS=" $1 "
    echo "Testing groups: '$USER_GROUPS'"
    if [[ "$USER_GROUPS" == *" render "* ]] && [[ "$USER_GROUPS" == *" video "* ]]; then
        echo "  -> PASS"
    else
        echo "  -> FAIL"
    fi
}

echo "--- Fixed Logic Test ---"
check_groups_fixed "mev video render"       # Expected: PASS
check_groups_fixed "mev video renderer"     # Expected: FAIL
check_groups_fixed "mev videography render" # Expected: FAIL
check_groups_fixed "mev video"              # Expected: FAIL
