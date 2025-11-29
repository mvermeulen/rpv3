/* MIT License
 * RPV3 Options Parser - Header for C and C++ implementations
 * Provides environment variable-based option parsing for the kernel tracer
 */

#ifndef RPV3_OPTIONS_H
#define RPV3_OPTIONS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Version information */
#define RPV3_VERSION_MAJOR 1
#define RPV3_VERSION_MINOR 5
#define RPV3_VERSION_PATCH 1
#define RPV3_VERSION_STRING "1.5.1"
#define RPV3_VERSION "1.5.1"  /* For backward compatibility */


/* Return codes */
#define RPV3_OPTIONS_CONTINUE 0  /* Continue with normal initialization */
#define RPV3_OPTIONS_EXIT 1      /* Exit early (e.g., --version was handled) */

/* Global flag for timeline mode (set by --timeline option) */
extern int rpv3_timeline_enabled;

/* Global flag for CSV output mode (set by --csv option) */
extern int rpv3_csv_enabled;

/* Counter collection modes */
typedef enum {
    RPV3_COUNTER_MODE_NONE = 0,
    RPV3_COUNTER_MODE_COMPUTE,
    RPV3_COUNTER_MODE_MEMORY,
    RPV3_COUNTER_MODE_MIXED
} rpv3_counter_mode_t;

/* Global counter mode (set by --counter option) */
extern rpv3_counter_mode_t rpv3_counter_mode;

/* Global output file path (set by --output option) */
extern char* rpv3_output_file;

/* Global output directory path (set by --outputdir option) */
extern char* rpv3_output_dir;

/* Global rocBLAS log pipe path (set by --rocblas option) */
extern char* rpv3_rocblas_pipe;

/* Global rocBLAS log file path (set by --rocblas-log option) */
extern char* rpv3_rocblas_log_file;

/* Global flag for backtrace mode (set by --backtrace option) */
extern int rpv3_backtrace_enabled;

/**
 * Parse options from the RPV3_OPTIONS environment variable
 * 
 * Reads the RPV3_OPTIONS environment variable and processes space-separated options.
 * Currently supports:
 *   --version : Print version information and return RPV3_OPTIONS_EXIT
 *   --help, -h : Print help message and return RPV3_OPTIONS_EXIT
 *   --timeline : Enable timeline mode with GPU timestamps (sets rpv3_timeline_enabled)
 *   --csv : Enable CSV output mode (sets rpv3_csv_enabled)
 *   --counter <group> : Enable counter collection (compute, memory, mixed)
 *   --output <filename> : Redirect output to specified file (sets rpv3_output_file)
 *   --outputdir <directory> : Redirect output to directory with PID-based filename (sets rpv3_output_dir)
 *   --backtrace : Enable function backtrace at kernel dispatch (incompatible with --timeline and --csv)
 * 
 * @return RPV3_OPTIONS_CONTINUE (0) to continue normal operation
 *         RPV3_OPTIONS_EXIT (1) to exit early without initializing profiler
 */
int rpv3_parse_options(void);

#ifdef __cplusplus
}
#endif

#endif /* RPV3_OPTIONS_H */
