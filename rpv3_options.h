/* MIT License
 * RPV3 Options Parser - Shared header for C and C++ implementations
 * Provides environment variable-based option parsing for the kernel tracer
 */

#ifndef RPV3_OPTIONS_H
#define RPV3_OPTIONS_H

/* Enable POSIX features for strdup */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Version information */
#define RPV3_VERSION "1.0.0"
#define RPV3_VERSION_MAJOR 1
#define RPV3_VERSION_MINOR 0
#define RPV3_VERSION_PATCH 1

/* Return codes */
#define RPV3_OPTIONS_CONTINUE 0  /* Continue with normal initialization */
#define RPV3_OPTIONS_EXIT 1      /* Exit early (e.g., --version was handled) */

/**
 * Parse options from the RPV3_OPTIONS environment variable
 * 
 * Reads the RPV3_OPTIONS environment variable and processes space-separated options.
 * Currently supports:
 *   --version : Print version information and return RPV3_OPTIONS_EXIT
 * 
 * @return RPV3_OPTIONS_CONTINUE (0) to continue normal operation
 *         RPV3_OPTIONS_EXIT (1) to exit early without initializing profiler
 */
static inline int rpv3_parse_options(void) {
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
            printf("\nExample:\n");
            printf("  RPV3_OPTIONS=\"--version\" LD_PRELOAD=./libkernel_tracer.so ./app\n");
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

#ifdef __cplusplus
}
#endif

#endif /* RPV3_OPTIONS_H */
