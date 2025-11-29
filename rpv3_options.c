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

/* Global flag for timeline mode */
int rpv3_timeline_enabled = 0;

/* Global flag for CSV output mode */
int rpv3_csv_enabled = 0;

/* Global counter mode */
rpv3_counter_mode_t rpv3_counter_mode = RPV3_COUNTER_MODE_NONE;

/* Global output file path */
char* rpv3_output_file = NULL;

/* Global output directory path */
char* rpv3_output_dir = NULL;

/* Global rocBLAS log pipe path */
char* rpv3_rocblas_pipe = NULL;

/* Global rocBLAS log file path */
char* rpv3_rocblas_log_file = NULL;

/* Global flag for backtrace mode */
int rpv3_backtrace_enabled = 0;

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
            printf("  --timeline   Enable timeline mode with GPU timestamps\n");
            printf("  --csv        Enable CSV output mode\n");
            printf("  --counter <group> Enable counter collection (compute, memory, mixed)\n");
            printf("  --output <file>   Redirect output to specified file\n");
            printf("  --outputdir <dir> Redirect output to directory with PID-based filename\n");
            printf("  --rocblas <pipe>  Read rocBLAS logs from named pipe\n");
            printf("  --rocblas-log <file> Redirect rocBLAS logs to file (requires --rocblas)\n");
            printf("  --backtrace  Enable function backtrace (incompatible with --timeline, --csv)\n");
            printf("\nExample:\n");
            printf("  RPV3_OPTIONS=\"--version\" LD_PRELOAD=./libkernel_tracer.so ./app\n");
            printf("  RPV3_OPTIONS=\"--timeline\" LD_PRELOAD=./libkernel_tracer.so ./app\n");
            printf("  RPV3_OPTIONS=\"--csv\" LD_PRELOAD=./libkernel_tracer.so ./app\n");
            printf("  RPV3_OPTIONS=\"--counter compute\" LD_PRELOAD=./libkernel_tracer.so ./app\n");
            should_exit = 1;
        }
        else if (strcmp(token, "--timeline") == 0) {
            rpv3_timeline_enabled = 1;
            printf("[RPV3] Timeline mode enabled\n");
        }
        else if (strcmp(token, "--csv") == 0) {
            rpv3_csv_enabled = 1;
            printf("[RPV3] CSV output mode enabled\n");
        }
        else if (strcmp(token, "--output") == 0) {
            token = strtok(NULL, " \t\n");
            if (token == NULL) {
                fprintf(stderr, "[RPV3] Error: --output requires a filename argument\n");
            } else {
                rpv3_output_file = strdup(token);
                printf("[RPV3] Output will be written to: %s\n", rpv3_output_file);
            }
        }
        else if (strcmp(token, "--outputdir") == 0) {
            token = strtok(NULL, " \t\n");
            if (token == NULL) {
                fprintf(stderr, "[RPV3] Error: --outputdir requires a directory argument\n");
            } else {
                rpv3_output_dir = strdup(token);
                printf("[RPV3] Output directory: %s\n", rpv3_output_dir);
            }
        }
        else if (strcmp(token, "--rocblas") == 0) {
            token = strtok(NULL, " \t\n");
            if (token == NULL) {
                fprintf(stderr, "[RPV3] Error: --rocblas requires a pipe name argument\n");
            } else {
                rpv3_rocblas_pipe = strdup(token);
                printf("[RPV3] RocBLAS log pipe: %s\n", rpv3_rocblas_pipe);
            }
        }
        else if (strcmp(token, "--rocblas-log") == 0) {
            token = strtok(NULL, " \t\n");
            if (token == NULL) {
                fprintf(stderr, "[RPV3] Error: --rocblas-log requires a filename argument\n");
            } else {
                rpv3_rocblas_log_file = strdup(token);
                printf("[RPV3] RocBLAS log file: %s\n", rpv3_rocblas_log_file);
            }
        }
        else if (strcmp(token, "--counter") == 0) {
            token = strtok(NULL, " \t\n");
            if (token == NULL) {
                fprintf(stderr, "[RPV3] Error: --counter requires an argument (compute, memory, mixed)\n");
            } else {
                if (strcmp(token, "compute") == 0) {
                    rpv3_counter_mode = RPV3_COUNTER_MODE_COMPUTE;
                    printf("[RPV3] Counter collection enabled: COMPUTE group\n");
                } else if (strcmp(token, "memory") == 0) {
                    rpv3_counter_mode = RPV3_COUNTER_MODE_MEMORY;
                    printf("[RPV3] Counter collection enabled: MEMORY group\n");
                } else if (strcmp(token, "mixed") == 0) {
                    rpv3_counter_mode = RPV3_COUNTER_MODE_MIXED;
                    printf("[RPV3] Counter collection enabled: MIXED group\n");
                } else {
                    fprintf(stderr, "[RPV3] Error: Unknown counter group '%s'. Supported: compute, memory, mixed\n", token);
                }
            }
        }
        else if (strcmp(token, "--backtrace") == 0) {
            rpv3_backtrace_enabled = 1;
            printf("[RPV3] Backtrace mode enabled\n");
        }
        else {
            fprintf(stderr, "[RPV3] Warning: Unknown option '%s' (ignored)\n", token);
        }
        
        token = strtok(NULL, " \t\n");
    }
    
    free(options_copy);
    
    /* Validate incompatible option combinations */
    if (rpv3_backtrace_enabled) {
        if (rpv3_timeline_enabled) {
            fprintf(stderr, "[RPV3] Error: --backtrace is incompatible with --timeline\n");
            fprintf(stderr, "[RPV3]        Backtrace overhead would distort timing measurements\n");
            return RPV3_OPTIONS_EXIT;
        }
        if (rpv3_csv_enabled) {
            fprintf(stderr, "[RPV3] Error: --backtrace is incompatible with --csv\n");
            fprintf(stderr, "[RPV3]        Variable-length backtraces don't fit CSV schema\n");
            return RPV3_OPTIONS_EXIT;
        }
    }
    
    return should_exit ? RPV3_OPTIONS_EXIT : RPV3_OPTIONS_CONTINUE;
}
