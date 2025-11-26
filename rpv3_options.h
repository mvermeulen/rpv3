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
#define RPV3_VERSION "1.1.1"
#define RPV3_VERSION_MAJOR 1
#define RPV3_VERSION_MINOR 1
#define RPV3_VERSION_PATCH 1


/* Return codes */
#define RPV3_OPTIONS_CONTINUE 0  /* Continue with normal initialization */
#define RPV3_OPTIONS_EXIT 1      /* Exit early (e.g., --version was handled) */

/* Global flag for timeline mode (set by --timeline option) */
extern int rpv3_timeline_enabled;

/**
 * Parse options from the RPV3_OPTIONS environment variable
 * 
 * Reads the RPV3_OPTIONS environment variable and processes space-separated options.
 * Currently supports:
 *   --version : Print version information and return RPV3_OPTIONS_EXIT
 *   --help, -h : Print help message and return RPV3_OPTIONS_EXIT
 *   --timeline : Enable timeline mode with GPU timestamps (sets rpv3_timeline_enabled)
 * 
 * @return RPV3_OPTIONS_CONTINUE (0) to continue normal operation
 *         RPV3_OPTIONS_EXIT (1) to exit early without initializing profiler
 */
int rpv3_parse_options(void);

#ifdef __cplusplus
}
#endif

#endif /* RPV3_OPTIONS_H */
