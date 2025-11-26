/* Simple test to verify rpv3_timeline_enabled flag is set correctly */
#include <stdio.h>
#include <stdlib.h>
#include "../rpv3_options.h"

int main() {
    printf("Testing timeline option flag...\n\n");
    
    // Test 1: No option set
    printf("Test 1: No RPV3_OPTIONS set\n");
    unsetenv("RPV3_OPTIONS");
    rpv3_timeline_enabled = 0;  // Reset
    rpv3_parse_options();
    printf("  rpv3_timeline_enabled = %d (expected: 0)\n", rpv3_timeline_enabled);
    printf("  %s\n\n", rpv3_timeline_enabled == 0 ? "PASS" : "FAIL");
    
    // Test 2: Timeline option set
    printf("Test 2: RPV3_OPTIONS=\"--timeline\"\n");
    setenv("RPV3_OPTIONS", "--timeline", 1);
    rpv3_timeline_enabled = 0;  // Reset
    rpv3_parse_options();
    printf("  rpv3_timeline_enabled = %d (expected: 1)\n", rpv3_timeline_enabled);
    printf("  %s\n\n", rpv3_timeline_enabled == 1 ? "PASS" : "FAIL");
    
    // Test 3: Multiple options with timeline
    printf("Test 3: RPV3_OPTIONS=\"--timeline --version\"\n");
    setenv("RPV3_OPTIONS", "--timeline --version", 1);
    rpv3_timeline_enabled = 0;  // Reset
    rpv3_parse_options();
    printf("  rpv3_timeline_enabled = %d (expected: 1)\n", rpv3_timeline_enabled);
    printf("  %s\n\n", rpv3_timeline_enabled == 1 ? "PASS" : "FAIL");
    
    printf("All tests completed!\n");
    return 0;
}
