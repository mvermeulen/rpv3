#!/bin/bash
check_groups() {
    USER_GROUPS="$1"
    echo "Testing groups: '$USER_GROUPS'"
    if [[ "$USER_GROUPS" == *"render"* ]] && [[ "$USER_GROUPS" == *"video"* ]]; then
        echo "  -> PASS"
    else
        echo "  -> FAIL"
    fi
}

echo "--- Current Logic Test ---"
check_groups "mev video render"       # Expected: PASS
check_groups "mev video renderer"     # Expected: FAIL (False Positive check)
check_groups "mev videography render" # Expected: FAIL (False Positive check)
check_groups "mev video"              # Expected: FAIL
