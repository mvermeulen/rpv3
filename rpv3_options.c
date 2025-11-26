/* MIT License
 * RPV3 Options Parser - Implementation
 * Provides environment variable-based option parsing for the kernel tracer
 */

/* Enable POSIX features for strdup */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "rpv3_options.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Parse options from the RPV3_OPTIONS environment variable */
int rpv3_parse_options(void) {
    const char* options_env = getenv("RPV3_OPTIONS");
    
    /* No options specified - continue normally */
    if (!options_env || options_env[0] == '\0') {
        return RPV3_OPTIONS_CONTINUE;
    }
    
    /* Make a copy of the environment variable for tokenization */
    char* options_copy = strdup(options_env);
    if (!options_copy) {
        fprintf(stderr, "[RPV3] Warning: Failed to allocate memory for options parsing\n");
        return RPV3_OPTIONS_CONTINUE;
    }
    
    /* Parse space-separated options */
    char* token = strtok(options_copy, " \t\n");
    int should_exit = 0;
    
    while (token != NULL) {
        if (strcmp(token, "--version") == 0) {
            printf("RPV3 Kernel Tracer version %s\n", RPV3_VERSION);
            printf("ROCm Profiler SDK kernel tracing library\n");
            should_exit = 1;
        }
        else if (strcmp(token, "--help") == 0 || strcmp(token, "-h") == 0) {
            printf("RPV3 Kernel Tracer - ROCm Profiler SDK kernel tracing library\n");
            printf("Version: %s\n\n", RPV3_VERSION);
            printf("Usage: Set RPV3_OPTIONS environment variable with space-separated options\n");
            printf("Options:\n");
            printf("  --version    Print version information and exit\n");
            printf("  --help, -h   Print this help message and exit\n");
            printf("  --timeline   Enable timeline information (NOT YET IMPLEMENTED)\n");
            printf("\nExample:\n");
            printf("  RPV3_OPTIONS=\"--version\" LD_PRELOAD=./libkernel_tracer.so ./app\n");
            should_exit = 1;
        }
        else if (strcmp(token, "--timeline") == 0) {
            fprintf(stderr, "[RPV3] Error: --timeline option is not yet implemented\n");
            fprintf(stderr, "[RPV3] This feature will enable timeline information in a future release\n");
            should_exit = 1;
        }
        else {
            fprintf(stderr, "[RPV3] Warning: Unknown option '%s' (ignored)\n", token);
        }
        
        token = strtok(NULL, " \t\n");
    }
    
    free(options_copy);
    
    return should_exit ? RPV3_OPTIONS_EXIT : RPV3_OPTIONS_CONTINUE;
}
