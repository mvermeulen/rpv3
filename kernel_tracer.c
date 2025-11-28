/* MIT License
 * Sample ROCm Profiler SDK kernel tracer - C implementation (Enhanced)
 * Demonstrates using rocprofiler-sdk to trace kernel dispatches with detailed information
 */

#define _GNU_SOURCE

/* Enable POSIX features for strdup */
#define _POSIX_C_SOURCE 200809L

/* Include HIP headers first to satisfy RCCL dependencies in rocprofiler headers */
#define __HIP_PLATFORM_AMD__
#include <hip/hip_runtime_api.h>

#include <rocprofiler-sdk/registration.h>
#include <rocprofiler-sdk/rocprofiler.h>
#include <rocprofiler-sdk/callback_tracing.h>
#include <rocprofiler-sdk/buffer.h>
#include <rocprofiler-sdk/buffer_tracing.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <poll.h>
#include <dlfcn.h>

#include "rpv3_options.h"

/* Simple kernel name storage (array-based for C compatibility) */
#define MAX_KERNELS 256

typedef struct {
    rocprofiler_kernel_id_t kernel_id;
    char kernel_name[256];
    int valid;
} kernel_info_t;

/* Global state */
static atomic_uint_fast64_t kernel_count = ATOMIC_VAR_INIT(0);
static rocprofiler_context_id_t client_ctx = {0};
static rocprofiler_client_id_t* client_id = NULL;
static kernel_info_t kernel_table[MAX_KERNELS];
static atomic_int kernel_table_size = ATOMIC_VAR_INIT(0);

/* Timeline mode state */
static int timeline_enabled = 0;
static uint64_t tracer_start_timestamp = 0;  /* Baseline timestamp when tracer starts */
static rocprofiler_buffer_id_t trace_buffer = {0};

/* CSV output mode state */
static int csv_enabled = 0;

/* Counter collection state */
static rpv3_counter_mode_t counter_mode = RPV3_COUNTER_MODE_NONE;
static rocprofiler_buffer_id_t counter_buffer = {0};

/* Output file state */
static FILE* output_file = NULL;
static char output_filename[512];

/* RocBLAS log pipe path */
static char rocblas_pipe_path[256] = {0};

/* RocBLAS log pipe file descriptor */
static int rocblas_pipe_fd = -1;

/* RocBLAS log file handle */
static FILE* rocblas_log_file = NULL;

/* Output macro for trace data (CSV or human-readable kernel details) */
#define TRACE_PRINTF(...) fprintf(output_file ? output_file : stdout, __VA_ARGS__)

/* Output macro for status messages (init, summary, errors) */
/* If CSV output is enabled AND we are writing to a file, status messages go to stdout */
/* Otherwise, they follow the trace output (to file if set, else stdout) */
#define STATUS_PRINTF(...) fprintf((output_file && csv_enabled) ? stdout : (output_file ? output_file : stdout), __VA_ARGS__)

/* Agent profile storage */
#define MAX_AGENTS 16
typedef struct {
    rocprofiler_agent_id_t agent_id;
    rocprofiler_profile_config_id_t profile_id;
    int valid;
} agent_profile_t;

static agent_profile_t agent_profiles[MAX_AGENTS];
static atomic_int agent_profiles_count = ATOMIC_VAR_INIT(0);

/* Temporary storage for counter discovery */
#define MAX_COUNTERS 1024
typedef struct {
    char name[256];
    rocprofiler_counter_id_t id;
} counter_entry_t;

typedef struct {
    counter_entry_t counters[MAX_COUNTERS];
    size_t count;
} counter_list_t;

/* Interceptors for RocBLAS logging */
typedef FILE* (*fopen_t)(const char*, const char*);
typedef FILE* (*fdopen_t)(int, const char*);
static fopen_t real_fopen = NULL;
static fopen_t real_fopen64 = NULL;
static fdopen_t real_fdopen = NULL;

FILE* fopen(const char* path, const char* mode) {
    if (!real_fopen) {
        real_fopen = (fopen_t)dlsym(RTLD_NEXT, "fopen");
    }
    FILE* fp = real_fopen(path, mode);
    
    const char* trace_path = getenv("ROCBLAS_LOG_TRACE_PATH");
    const char* bench_path = getenv("ROCBLAS_LOG_BENCH_PATH");
    const char* profile_path = getenv("ROCBLAS_LOG_PROFILE_PATH");
    
    int match = 0;
    if (path) {
        if (trace_path && strcmp(path, trace_path) == 0) match = 1;
        else if (bench_path && strcmp(path, bench_path) == 0) match = 1;
        else if (profile_path && strcmp(path, profile_path) == 0) match = 1;
        else if (rpv3_rocblas_pipe && strcmp(path, rpv3_rocblas_pipe) == 0) match = 1;
    }

    if (fp && match) {
         setvbuf(fp, NULL, _IONBF, 0);
    }
    return fp;
}

FILE* fopen64(const char* path, const char* mode) {
    if (!real_fopen64) {
        real_fopen64 = (fopen_t)dlsym(RTLD_NEXT, "fopen64");
        if (!real_fopen64) real_fopen64 = (fopen_t)dlsym(RTLD_NEXT, "fopen");
    }
    FILE* fp = real_fopen64(path, mode);
    
    const char* trace_path = getenv("ROCBLAS_LOG_TRACE_PATH");
    const char* bench_path = getenv("ROCBLAS_LOG_BENCH_PATH");
    const char* profile_path = getenv("ROCBLAS_LOG_PROFILE_PATH");
    
    int match = 0;
    if (path) {
        if (trace_path && strcmp(path, trace_path) == 0) match = 1;
        else if (bench_path && strcmp(path, bench_path) == 0) match = 1;
        else if (profile_path && strcmp(path, profile_path) == 0) match = 1;
        else if (rpv3_rocblas_pipe && strcmp(path, rpv3_rocblas_pipe) == 0) match = 1;
    }
    
    if (fp && match) {
         setvbuf(fp, NULL, _IONBF, 0);
    }
    return fp;
}

FILE* fdopen(int fd, const char* mode) {
    if (!real_fdopen) {
        real_fdopen = (fdopen_t)dlsym(RTLD_NEXT, "fdopen");
    }
    FILE* fp = real_fdopen(fd, mode);
    
    if (fp) {
        char path[1024];
        char proc_path[64];
        snprintf(proc_path, sizeof(proc_path), "/proc/self/fd/%d", fd);
        ssize_t len = readlink(proc_path, path, sizeof(path) - 1);
        if (len != -1) {
            path[len] = '\0';
            
            const char* trace_path = getenv("ROCBLAS_LOG_TRACE_PATH");
            const char* bench_path = getenv("ROCBLAS_LOG_BENCH_PATH");
            const char* profile_path = getenv("ROCBLAS_LOG_PROFILE_PATH");
            
            int match = 0;
            if (trace_path && strcmp(path, trace_path) == 0) match = 1;
            else if (bench_path && strcmp(path, bench_path) == 0) match = 1;
            else if (profile_path && strcmp(path, profile_path) == 0) match = 1;
            else if (rpv3_rocblas_pipe && strcmp(path, rpv3_rocblas_pipe) == 0) match = 1;
            
            if (match) {
                 setvbuf(fp, NULL, _IONBF, 0);
            }
        }
    }
    return fp;
}

/* Helper function to store kernel name */
void store_kernel_name(rocprofiler_kernel_id_t kernel_id, const char* name) {
    if (!name) return;
    
    int size = atomic_load(&kernel_table_size);
    
    /* Check if kernel already exists */
    for (int i = 0; i < size && i < MAX_KERNELS; i++) {
        if (kernel_table[i].valid && kernel_table[i].kernel_id == kernel_id) {
            return; /* Already stored */
        }
    }
    
    /* Add new kernel */
    if (size < MAX_KERNELS) {
        int idx = atomic_fetch_add(&kernel_table_size, 1);
        if (idx < MAX_KERNELS) {
            kernel_table[idx].kernel_id = kernel_id;
            strncpy(kernel_table[idx].kernel_name, name, sizeof(kernel_table[idx].kernel_name) - 1);
            kernel_table[idx].kernel_name[sizeof(kernel_table[idx].kernel_name) - 1] = '\0';
            kernel_table[idx].valid = 1;
        }
    }
}

/* Helper function to lookup kernel name */
const char* lookup_kernel_name(rocprofiler_kernel_id_t kernel_id) {
    int size = atomic_load(&kernel_table_size);
    
    for (int i = 0; i < size && i < MAX_KERNELS; i++) {
        if (kernel_table[i].valid && kernel_table[i].kernel_id == kernel_id) {
            return kernel_table[i].kernel_name;
        }
    }
    
    return "<unknown>";
}

/* Helper function to check if a kernel is a Tensile routine */
int is_tensile_kernel(const char* name) {
    if (!name) return 0;
    return (strstr(name, "Cijk") != NULL || 
            strstr(name, "assembly") != NULL || 
            strstr(name, "Tensile") != NULL);
}

/* Callback function for kernel symbol registration */
void kernel_symbol_callback(rocprofiler_callback_tracing_record_t record,
                           rocprofiler_user_data_t* user_data,
                           void* callback_data) {
    (void) user_data;
    (void) callback_data;
    
    if (record.kind == ROCPROFILER_CALLBACK_TRACING_CODE_OBJECT &&
        record.operation == ROCPROFILER_CODE_OBJECT_DEVICE_KERNEL_SYMBOL_REGISTER) {
        
        rocprofiler_callback_tracing_code_object_kernel_symbol_register_data_t* data = 
            (rocprofiler_callback_tracing_code_object_kernel_symbol_register_data_t*)record.payload;
        
        if (record.phase == ROCPROFILER_CALLBACK_PHASE_LOAD && data && data->kernel_name) {
            /* Store the kernel name */
            store_kernel_name(data->kernel_id, data->kernel_name);
        }
    }
}

/* Buffer callback function for timeline mode (batch processing) */
void timeline_buffer_callback(
    rocprofiler_context_id_t context,
    rocprofiler_buffer_id_t buffer_id,
    rocprofiler_record_header_t** headers,
    size_t num_headers,
    void* user_data,
    uint64_t drop_count
) {
    (void) context;
    (void) buffer_id;
    (void) user_data;
    
    if (drop_count > 0) {
        fprintf(stderr, "[Kernel Tracer] Warning: Dropped %lu records\n", (unsigned long)drop_count);
    }
    
    /* CSV header output (once per process) */
    if (csv_enabled) {
        static int csv_header_printed = 0;
        if (!csv_header_printed) {
            TRACE_PRINTF("KernelName,ThreadID,CorrelationID,KernelID,DispatchID,GridX,GridY,GridZ,WorkgroupX,WorkgroupY,WorkgroupZ,PrivateSeg,GroupSeg,StartTimestamp,EndTimestamp,DurationNs,DurationUs,TimeSinceStartMs\n");
            csv_header_printed = 1;
        }
    }
    
    /* Process batch of records */
    for (size_t i = 0; i < num_headers; i++) {
        rocprofiler_record_header_t* header = headers[i];
        
        /* Only process kernel dispatch records */
        if (header->category == ROCPROFILER_BUFFER_CATEGORY_TRACING &&
            header->kind == ROCPROFILER_BUFFER_TRACING_KERNEL_DISPATCH) {
            
            rocprofiler_buffer_tracing_kernel_dispatch_record_t* record =
                (rocprofiler_buffer_tracing_kernel_dispatch_record_t*)header->payload;
            
            uint64_t count = atomic_fetch_add(&kernel_count, 1) + 1;
            
            /* Extract timestamps (guaranteed non-zero in buffer mode) */
            uint64_t start_ns = record->start_timestamp;
            uint64_t end_ns = record->end_timestamp;
            uint64_t duration_ns = end_ns - start_ns;
            double duration_us = duration_ns / 1000.0;
            
            /* Calculate time since tracer started */
            double time_since_start_ms = (start_ns - tracer_start_timestamp) / 1000000.0;
            
            /* Look up kernel name */
            const char* kernel_name = lookup_kernel_name(record->dispatch_info.kernel_id);
            
            if (csv_enabled) {
                /* CSV output */
                TRACE_PRINTF("\"%s\",%lu,%lu,%lu,%lu,%u,%u,%u,%u,%u,%u,%u,%u,%lu,%lu,%lu,%.3f,%.3f\n",
                       kernel_name,
                       (unsigned long)record->thread_id,
                       (unsigned long)record->correlation_id.internal,
                       (unsigned long)record->dispatch_info.kernel_id,
                       (unsigned long)record->dispatch_info.dispatch_id,
                       record->dispatch_info.grid_size.x,
                       record->dispatch_info.grid_size.y,
                       record->dispatch_info.grid_size.z,
                       record->dispatch_info.workgroup_size.x,
                       record->dispatch_info.workgroup_size.y,
                       record->dispatch_info.workgroup_size.z,
                       record->dispatch_info.private_segment_size,
                       record->dispatch_info.group_segment_size,
                       (unsigned long)start_ns,
                       (unsigned long)end_ns,
                       (unsigned long)(end_ns - start_ns),
                       duration_us,
                       time_since_start_ms);
            } else {
                /* Human-readable output */
                TRACE_PRINTF("\n[Kernel Trace #%lu]\n", (unsigned long)count);
                TRACE_PRINTF("  Kernel Name: %s\n", kernel_name);
                TRACE_PRINTF("  Thread ID: %lu\n", (unsigned long)record->thread_id);
                TRACE_PRINTF("  Correlation ID: %lu\n", (unsigned long)record->correlation_id.internal);
                TRACE_PRINTF("  Kernel ID: %lu\n", (unsigned long)record->dispatch_info.kernel_id);
                TRACE_PRINTF("  Dispatch ID: %lu\n", (unsigned long)record->dispatch_info.dispatch_id);
                TRACE_PRINTF("  Grid Size: [%u, %u, %u]\n",
                       record->dispatch_info.grid_size.x,
                       record->dispatch_info.grid_size.y,
                       record->dispatch_info.grid_size.z);
                TRACE_PRINTF("  Workgroup Size: [%u, %u, %u]\n",
                       record->dispatch_info.workgroup_size.x,
                       record->dispatch_info.workgroup_size.y,
                       record->dispatch_info.workgroup_size.z);
                TRACE_PRINTF("  Private Segment Size: %u bytes (scratch memory per work-item)\n",
                       record->dispatch_info.private_segment_size);
                TRACE_PRINTF("  Group Segment Size: %u bytes (LDS memory per work-group)\n",
                       record->dispatch_info.group_segment_size);
                
                /* Timeline information (only in buffer mode) */
                TRACE_PRINTF("  Start Timestamp: %lu ns\n", (unsigned long)start_ns);
                TRACE_PRINTF("  End Timestamp: %lu ns\n", (unsigned long)end_ns);
                TRACE_PRINTF("  Duration: %.3f μs\n", duration_us);
                TRACE_PRINTF("  Time Since Start: %.3f ms\n", time_since_start_ms);
            }

            /* Read from RocBLAS log if available and kernel matches pattern */
            /* In timeline mode, we only support reading from files, not pipes */
            if (rocblas_pipe_fd != -1 && is_tensile_kernel(kernel_name)) {
                char line_buffer[4096];
                int line_pos = 0;
                int valid_line_found = 0;
                
                /* Read char-by-char to handle filtering and stop after one valid line */
                while (!valid_line_found) {
                    char c;
                    ssize_t bytes_read = read(rocblas_pipe_fd, &c, 1);
                    
                    if (bytes_read > 0) {
                        /* Write to log file if enabled (raw stream) */
                        if (rocblas_log_file) {
                            fputc(c, rocblas_log_file);
                        }
                        
                        if (c == '\n') {
                            line_buffer[line_pos] = '\0';
                            
                            /* Clean up carriage return if present */
                            if (line_pos > 0 && line_buffer[line_pos-1] == '\r') {
                                line_buffer[line_pos-1] = '\0';
                            }
                            
                            /* Check filters */
                            if (strstr(line_buffer, "rocblas_create_handle") != NULL || 
                                strstr(line_buffer, "rocblas_destroy_handle") != NULL ||
                                strstr(line_buffer, "rocblas_set_stream") != NULL) {
                                /* Skip this line, reset buffer and continue reading */
                                line_pos = 0;
                            } else {
                                /* Valid line found */
                                if (strlen(line_buffer) > 0) {
                                    TRACE_PRINTF("# %s\n", line_buffer);
                                    valid_line_found = 1; /* Stop reading */
                                } else {
                                    /* Empty line, just reset */
                                    line_pos = 0;
                                }
                            }
                        } else {
                            if (line_pos < sizeof(line_buffer) - 1) {
                                line_buffer[line_pos++] = c;
                            }
                            /* Else: line too long, truncate */
                        }
                    } else {
                        /* EOF or error */
                        break;
                    }
                }
                
                if (rocblas_log_file) {
                    fflush(rocblas_log_file);
                }
            }
        }
    }
}


/* Callback function for kernel dispatch events */
void kernel_dispatch_callback(rocprofiler_callback_tracing_record_t record,
                              rocprofiler_user_data_t* user_data,
                              void* callback_data) {
    (void) user_data;
    (void) callback_data;
    
    /* CSV header output (once per process) */
    if (csv_enabled) {
        static int csv_header_printed = 0;
        if (!csv_header_printed) {
            TRACE_PRINTF("KernelName,ThreadID,CorrelationID,KernelID,DispatchID,GridX,GridY,GridZ,WorkgroupX,WorkgroupY,WorkgroupZ,PrivateSeg,GroupSeg,StartTimestamp,EndTimestamp,DurationNs,DurationUs,TimeSinceStartMs\n");
            csv_header_printed = 1;
        }
    }
    
    if (record.kind != ROCPROFILER_CALLBACK_TRACING_KERNEL_DISPATCH) return;
        
    if (record.phase == ROCPROFILER_CALLBACK_PHASE_ENTER) {
        // Do nothing on ENTER for now
    }
    else if (record.phase == ROCPROFILER_CALLBACK_PHASE_EXIT) {
        rocprofiler_callback_tracing_kernel_dispatch_data_t* dispatch_data = 
            (rocprofiler_callback_tracing_kernel_dispatch_data_t*)record.payload;
        
        if (!dispatch_data) {
            STATUS_PRINTF("[Kernel Trace] <no dispatch data on exit>\n");
            return;
        }
        
        uint64_t count = atomic_fetch_add(&kernel_count, 1) + 1;
        (void)count;  /* Suppress unused variable warning */
        
        rocprofiler_kernel_dispatch_info_t info = dispatch_data->dispatch_info;
        const char* kernel_name = lookup_kernel_name(info.kernel_id);
        
        uint64_t start_ns = dispatch_data->start_timestamp;
        uint64_t end_ns = dispatch_data->end_timestamp;
        uint64_t duration_ns = (end_ns > start_ns) ? (end_ns - start_ns) : 0;
        double duration_us = duration_ns / 1000.0;
        double time_since_start_ms = (start_ns > tracer_start_timestamp) ? 
                                     ((start_ns - tracer_start_timestamp) / 1000000.0) : 0.0;
        
        if (csv_enabled) {
            /* CSV mode: output complete line on EXIT */
            TRACE_PRINTF("\"%s\",%lu,%lu,%lu,%lu,%u,%u,%u,%u,%u,%u,%u,%u,%lu,%lu,%lu,%.3f,%.3f\n",
                   kernel_name,
                   (unsigned long)record.thread_id,
                   (unsigned long)record.correlation_id.internal,
                   (unsigned long)info.kernel_id,
                   (unsigned long)info.dispatch_id,
                   info.grid_size.x,
                   info.grid_size.y,
                   info.grid_size.z,
                   info.workgroup_size.x,
                   info.workgroup_size.y,
                   info.workgroup_size.z,
                   info.private_segment_size,
                   info.group_segment_size,
                   (unsigned long)start_ns,
                   (unsigned long)end_ns,
                   (unsigned long)duration_ns,
                   duration_us,
                   time_since_start_ms);
        } else {
            /* Standard mode: display full details on exit */
            TRACE_PRINTF("\n[Kernel Trace #%lu]\n", (unsigned long)count);
            TRACE_PRINTF("  Kernel Name: %s\n", kernel_name);
            TRACE_PRINTF("  Thread ID: %lu\n", (unsigned long)record.thread_id);
            TRACE_PRINTF("  Correlation ID: %lu\n", (unsigned long)record.correlation_id.internal);
            TRACE_PRINTF("  Kernel ID: %lu\n", (unsigned long)info.kernel_id);
            TRACE_PRINTF("  Dispatch ID: %lu\n", (unsigned long)info.dispatch_id);
            TRACE_PRINTF("  Grid Size: [%u, %u, %u]\n", 
                   info.grid_size.x, 
                   info.grid_size.y, 
                   info.grid_size.z);
            TRACE_PRINTF("  Workgroup Size: [%u, %u, %u]\n",
                   info.workgroup_size.x,
                   info.workgroup_size.y,
                   info.workgroup_size.z);
            TRACE_PRINTF("  Private Segment Size: %u bytes (scratch memory per work-item)\n",
                   info.private_segment_size);
            TRACE_PRINTF("  Group Segment Size: %u bytes (LDS memory per work-group)\n",
                   info.group_segment_size);
            TRACE_PRINTF("  Group Segment Size: %u bytes (LDS memory per work-group)\n",
                   info.group_segment_size);
            
            if (end_ns > 0) {
                TRACE_PRINTF("  Start Timestamp: %lu ns\n", (unsigned long)start_ns);
                TRACE_PRINTF("  End Timestamp: %lu ns\n", (unsigned long)end_ns);
                TRACE_PRINTF("  Duration: %.3f μs\n", duration_us);
                TRACE_PRINTF("  Time Since Start: %.3f ms\n", time_since_start_ms);
            }
        }
            
        /* Read from RocBLAS pipe if available and kernel matches pattern */
        if (rocblas_pipe_fd != -1 && is_tensile_kernel(kernel_name)) {
            struct pollfd pfd;
            pfd.fd = rocblas_pipe_fd;
            pfd.events = POLLIN;
            
            /* Wait up to 500ms for data */
            int ret = poll(&pfd, 1, 500);
            
            if (ret > 0 && (pfd.revents & POLLIN)) {
                char line_buffer[4096];
                int line_pos = 0;
                int valid_line_found = 0;
                
                /* Read char-by-char to handle pipe correctly and stop after one valid line */
                while (!valid_line_found) {
                    char c;
                    ssize_t bytes_read = read(rocblas_pipe_fd, &c, 1);
                    
                    if (bytes_read > 0) {
                        /* Write to log file if enabled (raw stream) */
                        if (rocblas_log_file) {
                            fputc(c, rocblas_log_file);
                        }
                        
                        if (c == '\n') {
                            line_buffer[line_pos] = '\0';
                            
                            /* Clean up carriage return if present */
                            if (line_pos > 0 && line_buffer[line_pos-1] == '\r') {
                                line_buffer[line_pos-1] = '\0';
                            }
                            
                            /* Check filters */
                            if (strstr(line_buffer, "rocblas_create_handle") != NULL || 
                                strstr(line_buffer, "rocblas_destroy_handle") != NULL ||
                                strstr(line_buffer, "rocblas_set_stream") != NULL) {
                                /* Skip this line, reset buffer and continue reading */
                                line_pos = 0;
                            } else {
                                /* Valid line found */
                                if (strlen(line_buffer) > 0) {
                                    TRACE_PRINTF("# %s\n", line_buffer);
                                    valid_line_found = 1; /* Stop reading */
                                } else {
                                    /* Empty line, just reset */
                                    line_pos = 0;
                                }
                            }
                        } else {
                            if (line_pos < sizeof(line_buffer) - 1) {
                                line_buffer[line_pos++] = c;
                            }
                            /* Else: line too long, truncate (ignore extra chars until newline) */
                        }
                    } else {
                        /* EAGAIN or error or EOF */
                        break;
                    }
                }
                
                if (rocblas_log_file) {
                    fflush(rocblas_log_file);
                }
            }
        }
    }
}

/* Setup buffer tracing (for timeline mode) */
int setup_buffer_tracing() {
    STATUS_PRINTF("[Kernel Tracer] Setting up buffer tracing for timeline mode...\n");
    
    /* Create buffer */
    const size_t buffer_size = 8192;      /* 8 KB */
    const size_t buffer_watermark = 7168; /* Flush at 87.5% full */
    
    rocprofiler_status_t status = rocprofiler_create_buffer(
        client_ctx,
        buffer_size,
        buffer_watermark,
        ROCPROFILER_BUFFER_POLICY_LOSSLESS,  /* Don't drop records */
        timeline_buffer_callback,
        NULL,  /* callback data */
        &trace_buffer
    );
    
    if (status != ROCPROFILER_STATUS_SUCCESS) {
        fprintf(stderr, "[Kernel Tracer] Failed to create buffer\n");
        return -1;
    }
    
    /* Configure buffer tracing for kernel dispatches */
    status = rocprofiler_configure_buffer_tracing_service(
        client_ctx,
        ROCPROFILER_BUFFER_TRACING_KERNEL_DISPATCH,
        NULL,  /* operations (NULL = all) */
        0,     /* operations count */
        trace_buffer
    );
    
    if (status != ROCPROFILER_STATUS_SUCCESS) {
        fprintf(stderr, "[Kernel Tracer] Failed to configure buffer tracing\n");
        return -1;
    }
    
    /* Still need code object callback for kernel names */
    status = rocprofiler_configure_callback_tracing_service(
        client_ctx,
        ROCPROFILER_CALLBACK_TRACING_CODE_OBJECT,
        NULL,
        0,
        kernel_symbol_callback,
        NULL
    );
    
    if (status != ROCPROFILER_STATUS_SUCCESS) {
        fprintf(stderr, "[Kernel Tracer] Failed to configure code object callback\n");
        return -1;
    }
    
    return 0;
}

/* Setup callback tracing (for non-timeline mode) */
int setup_callback_tracing() {
    STATUS_PRINTF("[Kernel Tracer] Setting up callback tracing...\n");
    
    /* Configure callback tracing for code object/kernel symbol registration */
    if (rocprofiler_configure_callback_tracing_service(
            client_ctx,
            ROCPROFILER_CALLBACK_TRACING_CODE_OBJECT,
            NULL,
            0,
            kernel_symbol_callback,
            NULL
        ) != ROCPROFILER_STATUS_SUCCESS) {
        fprintf(stderr, "[Kernel Tracer] Failed to configure code object callback tracing\n");
        return -1;
    }
    
    /* Configure callback tracing for kernel dispatches */
    if (rocprofiler_configure_callback_tracing_service(
            client_ctx,
            ROCPROFILER_CALLBACK_TRACING_KERNEL_DISPATCH,
            NULL,
            0,
            kernel_dispatch_callback,
            NULL
        ) != ROCPROFILER_STATUS_SUCCESS) {
        fprintf(stderr, "[Kernel Tracer] Failed to configure kernel dispatch callback tracing\n");
        return -1;
    }
    
    return 0;
}

/* Helper to store agent profile */
void store_agent_profile(rocprofiler_agent_id_t agent_id, rocprofiler_profile_config_id_t profile_id) {
    int idx = atomic_fetch_add(&agent_profiles_count, 1);
    if (idx < MAX_AGENTS) {
        agent_profiles[idx].agent_id = agent_id;
        agent_profiles[idx].profile_id = profile_id;
        agent_profiles[idx].valid = 1;
    } else {
        fprintf(stderr, "[Kernel Tracer] Warning: Max agents reached, cannot store profile\n");
    }
}

/* Helper to find agent profile */
int find_agent_profile(rocprofiler_agent_id_t agent_id, rocprofiler_profile_config_id_t* profile_id) {
    int count = atomic_load(&agent_profiles_count);
    for (int i = 0; i < count && i < MAX_AGENTS; i++) {
        if (agent_profiles[i].valid && agent_profiles[i].agent_id.handle == agent_id.handle) {
            *profile_id = agent_profiles[i].profile_id;
            return 1;
        }
    }
    return 0;
}

/* Get target counters for the selected mode */
/* Returns number of counters, fills names array */
int get_target_counters(rpv3_counter_mode_t mode, const char** names, int max_names) {
    int count = 0;
    
    if (mode == RPV3_COUNTER_MODE_COMPUTE || mode == RPV3_COUNTER_MODE_MIXED) {
        if (count < max_names) names[count++] = "SQ_INSTS_VALU";
        if (count < max_names) names[count++] = "SQ_WAVES";
        if (count < max_names) names[count++] = "SQ_INSTS_SALU";
    }
    
    if (mode == RPV3_COUNTER_MODE_MEMORY || mode == RPV3_COUNTER_MODE_MIXED) {
        if (count < max_names) names[count++] = "TCC_EA_RDREQ_sum";
        if (count < max_names) names[count++] = "TCC_EA_WRREQ_sum";
        if (count < max_names) names[count++] = "TCC_EA_RDREQ_32B_sum";
        if (count < max_names) names[count++] = "TCC_EA_RDREQ_64B_sum";
        if (count < max_names) names[count++] = "TCP_TCC_WRITE_REQ_sum";
    }
    
    return count;
}

/* Callback for dispatch counting service */
void dispatch_counting_callback(
    rocprofiler_dispatch_counting_service_data_t dispatch_data,
    rocprofiler_profile_config_id_t* config,
    rocprofiler_user_data_t* user_data,
    void* callback_data_args
) {
    (void) user_data;
    (void) callback_data_args;
    
    /* Find the profile for this agent */
    rocprofiler_profile_config_id_t profile_id;
    if (find_agent_profile(dispatch_data.dispatch_info.agent_id, &profile_id)) {
        *config = profile_id;
    }
}

/* Callback for processing collected counter records */
void counter_record_callback(
    rocprofiler_context_id_t context,
    rocprofiler_buffer_id_t buffer_id,
    rocprofiler_record_header_t** headers,
    size_t num_headers,
    void* user_data,
    uint64_t drop_count
) {
    (void) context;
    (void) buffer_id;
    (void) user_data;
    
    if (drop_count > 0) {
        fprintf(stderr, "[Kernel Tracer] Warning: Dropped %lu counter records\n", (unsigned long)drop_count);
    }
    
    for (size_t i = 0; i < num_headers; i++) {
        rocprofiler_record_header_t* header = headers[i];
        
        if (header->category == ROCPROFILER_BUFFER_CATEGORY_COUNTERS &&
            header->kind == ROCPROFILER_COUNTER_RECORD_VALUE) {
            
            rocprofiler_counter_record_t* record = (rocprofiler_counter_record_t*)header->payload;
            
             TRACE_PRINTF("[Counters] Dispatch ID: %lu, Value: %f\n",
                   (unsigned long)record->dispatch_id, record->counter_value);
        }
    }
}

/* Callback to find supported counters */
rocprofiler_status_t counter_info_callback(
    rocprofiler_agent_id_t agent,
    rocprofiler_counter_id_t* counters,
    size_t num_counters,
    void* user_data
) {
    (void) agent;
    counter_list_t* list = (counter_list_t*)user_data;
    
    for (size_t i = 0; i < num_counters; i++) {
        rocprofiler_counter_info_v0_t info;
        rocprofiler_status_t status = rocprofiler_query_counter_info(
            counters[i],
            ROCPROFILER_COUNTER_INFO_VERSION_0,
            &info
        );
        
        if (status == ROCPROFILER_STATUS_SUCCESS && info.name) {
            if (list->count < MAX_COUNTERS) {
                strncpy(list->counters[list->count].name, info.name, 255);
                list->counters[list->count].name[255] = '\0';
                list->counters[list->count].id = counters[i];
                list->count++;
            }
        }
    }
    return ROCPROFILER_STATUS_SUCCESS;
}

/* Callback for agent query */
rocprofiler_status_t agent_query_callback(
    rocprofiler_agent_version_t version,
    const void** agents,
    size_t num_agents,
    void* user_data
) {
    (void) version;
    /* We passed a struct pointer, but need to cast appropriately */
    /* Let's define the struct here to match */
    struct agent_data_t {
        rocprofiler_agent_id_t agents[MAX_AGENTS];
        size_t count;
    };
    struct agent_data_t* data = (struct agent_data_t*)user_data;
    
    for (size_t i = 0; i < num_agents; i++) {
        const rocprofiler_agent_v0_t* info = (const rocprofiler_agent_v0_t*)agents[i];
        if (info->type == ROCPROFILER_AGENT_TYPE_GPU) {
            if (data->count < MAX_AGENTS) {
                data->agents[data->count] = info->id;
                data->count++;
            }
        }
    }
    return ROCPROFILER_STATUS_SUCCESS;
}

/* Create profile for agent */
void create_profile_for_agent(rocprofiler_agent_id_t agent_id) {
    /* 1. Get all supported counters */
    counter_list_t supported_counters;
    supported_counters.count = 0;
    
    rocprofiler_iterate_agent_supported_counters(
        agent_id,
        counter_info_callback,
        &supported_counters
    );
    
    /* 2. Select matching counters */
    const char* target_names[32];
    int num_targets = get_target_counters(counter_mode, target_names, 32);
    
    rocprofiler_counter_id_t selected_counters[32];
    size_t selected_count = 0;
    
    STATUS_PRINTF("[Kernel Tracer] Creating profile for agent. Targets: %d, Supported: %zu\n", 
           num_targets, supported_counters.count);
           
    for (int i = 0; i < num_targets; i++) {
        int found = 0;
        for (size_t j = 0; j < supported_counters.count; j++) {
            if (strcmp(target_names[i], supported_counters.counters[j].name) == 0) {
                selected_counters[selected_count++] = supported_counters.counters[j].id;
                STATUS_PRINTF("  + Added counter: %s\n", target_names[i]);
                found = 1;
                break;
            }
        }
        if (!found) {
            STATUS_PRINTF("  - Counter not found: %s\n", target_names[i]);
        }
    }
    
    if (selected_count == 0) {
        STATUS_PRINTF("[Kernel Tracer] Warning: No matching counters found for this agent\n");
        return;
    }
    
    /* 3. Create profile */
    rocprofiler_profile_config_id_t profile_id = {0};
    rocprofiler_status_t status = rocprofiler_create_profile_config(
        agent_id,
        selected_counters,
        selected_count,
        &profile_id
    );
    
    if (status == ROCPROFILER_STATUS_SUCCESS) {
        store_agent_profile(agent_id, profile_id);
        STATUS_PRINTF("[Kernel Tracer] Profile created successfully with %zu counters\n", selected_count);
    } else {
        fprintf(stderr, "[Kernel Tracer] Failed to create profile config: %d\n", status);
    }
}

/* Setup counter collection */
int setup_counter_collection() {
    STATUS_PRINTF("[Kernel Tracer] Setting up counter collection...\n");
    
    /* IMPORTANT: Counter collection also needs code object callback for kernel symbols */
    /* Configure callback tracing for code object/kernel symbol registration */
    rocprofiler_status_t status = rocprofiler_configure_callback_tracing_service(
        client_ctx,
        ROCPROFILER_CALLBACK_TRACING_CODE_OBJECT,
        NULL,
        0,
        kernel_symbol_callback,
        NULL
    );
    
    if (status != ROCPROFILER_STATUS_SUCCESS) {
        fprintf(stderr, "[Kernel Tracer] Failed to configure code object callback tracing\n");
        return -1;
    }
    
    /* 1. Query agents */
    struct agent_data_t {
        rocprofiler_agent_id_t agents[MAX_AGENTS];
        size_t count;
    } agent_data;
    agent_data.count = 0;
    
    rocprofiler_query_available_agents(
        ROCPROFILER_AGENT_INFO_VERSION_0,
        agent_query_callback,
        sizeof(rocprofiler_agent_v0_t),
        &agent_data
    );
    
    if (agent_data.count == 0) {
        STATUS_PRINTF("[Kernel Tracer] No GPU agents found for counter collection\n");
        return 0;
    }
    
    /* 2. Check support and create profiles */
    int any_agent_supported = 0;
    for (size_t i = 0; i < agent_data.count; i++) {
        create_profile_for_agent(agent_data.agents[i]);
        /* Check if profile was created */
        rocprofiler_profile_config_id_t dummy;
        if (find_agent_profile(agent_data.agents[i], &dummy)) {
            any_agent_supported = 1;
        }
    }
    
    if (!any_agent_supported) {
        STATUS_PRINTF("[Kernel Tracer] Warning: No agents support counter collection or no counters found. Counter collection disabled.\n");
        STATUS_PRINTF("[Kernel Tracer] Falling back to callback tracing mode...\n");
        /* Fall back to regular callback tracing since counters aren't available */
        /* Code object callback is already configured above */
        /* Just need to add kernel dispatch callback */
        status = rocprofiler_configure_callback_tracing_service(
            client_ctx,
            ROCPROFILER_CALLBACK_TRACING_KERNEL_DISPATCH,
            NULL,
            0,
            kernel_dispatch_callback,
            NULL
        );
        
        if (status != ROCPROFILER_STATUS_SUCCESS) {
            fprintf(stderr, "[Kernel Tracer] Failed to configure kernel dispatch callback tracing\n");
            return -1;
        }
        
        return 0;
    }
    
    /* 3. Create buffer */
    const size_t buffer_size = 64 * 1024;
    const size_t buffer_watermark = 56 * 1024;
    
    status = rocprofiler_create_buffer(
        client_ctx,
        buffer_size,
        buffer_watermark,
        ROCPROFILER_BUFFER_POLICY_LOSSLESS,
        counter_record_callback,
        NULL,
        &counter_buffer
    );
    
    if (status != ROCPROFILER_STATUS_SUCCESS) {
        fprintf(stderr, "[Kernel Tracer] Failed to create counter buffer\n");
        return -1;
    }
    
    /* 4. Configure service */
    status = rocprofiler_configure_buffer_dispatch_counting_service(
        client_ctx,
        counter_buffer,
        dispatch_counting_callback,
        NULL
    );
    
    if (status != ROCPROFILER_STATUS_SUCCESS) {
        fprintf(stderr, "[Kernel Tracer] Warning: Failed to configure dispatch counting service (status: %d)\n", status);
        fprintf(stderr, "[Kernel Tracer] This hardware/ROCm version may not support counter collection.\n");
        fprintf(stderr, "[Kernel Tracer] Falling back to callback tracing mode...\n");
        
        /* Destroy the counter buffer since we won't use it */
        if (counter_buffer.handle != 0) {
            rocprofiler_destroy_buffer(counter_buffer);
            counter_buffer.handle = 0;
        }
        
        /* Fall back to callback tracing */
        /* Code object callback is already configured */
        /* Just need to add kernel dispatch callback */
        status = rocprofiler_configure_callback_tracing_service(
            client_ctx,
            ROCPROFILER_CALLBACK_TRACING_KERNEL_DISPATCH,
            NULL,
            0,
            kernel_dispatch_callback,
            NULL
        );
        
        if (status != ROCPROFILER_STATUS_SUCCESS) {
            fprintf(stderr, "[Kernel Tracer] Failed to configure kernel dispatch callback tracing\n");
            return -1;
        }
        
        return 0;
    }
    
    STATUS_PRINTF("[Kernel Tracer] Counter collection configured successfully\n");
    return 0;
}

/* Tool initialization callback */

int tool_init(rocprofiler_client_finalize_t fini_func,
              void* tool_data) {
    (void) fini_func;
    (void) tool_data;
    
    STATUS_PRINTF("[Kernel Tracer] Initializing profiler tool...\n");
    
    /* Check if timeline mode is enabled (from rpv3_options) */
    timeline_enabled = (rpv3_timeline_enabled != 0);
    
    /* Check if CSV mode is enabled (from rpv3_options) */
    csv_enabled = (rpv3_csv_enabled != 0);

    /* Handle output redirection */
    if (rpv3_output_file) {
        output_file = fopen(rpv3_output_file, "w");
        if (!output_file) {
            fprintf(stderr, "[Kernel Tracer] Warning: Could not open output file '%s': %s\n", 
                    rpv3_output_file, strerror(errno));
            fprintf(stderr, "[Kernel Tracer] Falling back to stdout\n");
        } else {
            fprintf(stdout, "[Kernel Tracer] Output redirected to: %s\n", rpv3_output_file);
        }
    } else if (rpv3_output_dir) {
        pid_t pid = getpid();
        const char* ext = csv_enabled ? ".csv" : ".txt";
        snprintf(output_filename, sizeof(output_filename), "%s/rpv3_%d%s", 
                 rpv3_output_dir, pid, ext);
        
        output_file = fopen(output_filename, "w");
        if (!output_file) {
            fprintf(stderr, "[Kernel Tracer] Warning: Could not open output file '%s': %s\n", 
                    output_filename, strerror(errno));
            fprintf(stderr, "[Kernel Tracer] Falling back to stdout\n");
        } else {
            fprintf(stdout, "[Kernel Tracer] Output redirected to: %s\n", output_filename);
        }
    }

    /* Handle RocBLAS Log Pipe */
    if (rpv3_rocblas_pipe) {
        /* Check if it's a FIFO or regular file first */
        struct stat st;
        int is_reg_file = 0;
        int is_fifo = 0;
        
        if (stat(rpv3_rocblas_pipe, &st) == 0) {
            if (S_ISREG(st.st_mode)) is_reg_file = 1;
            if (S_ISFIFO(st.st_mode)) is_fifo = 1;
        }

        /* Verify against ROCBLAS_LOG_TRACE or ROCBLAS_LOG_TRACE_PATH environment variable */
        /* Only enforce this check if it is NOT a regular file (i.e. it is a pipe or we don't know yet) */
        if (!is_reg_file) {
            const char* env_pipe = getenv("ROCBLAS_LOG_TRACE");
            if (!env_pipe) {
                env_pipe = getenv("ROCBLAS_LOG_TRACE_PATH");
            }
            
            if (!env_pipe) {
                fprintf(stderr, "[Kernel Tracer] Warning: --rocblas specified '%s' but ROCBLAS_LOG_TRACE/ROCBLAS_LOG_TRACE_PATH is not set.\n", rpv3_rocblas_pipe);
                fprintf(stderr, "[Kernel Tracer] RocBLAS will not write to the pipe. Logging disabled.\n");
                rpv3_rocblas_pipe = NULL; /* Disable logging */
            } else if (strcmp(rpv3_rocblas_pipe, env_pipe) != 0) {
                fprintf(stderr, "[Kernel Tracer] Error: --rocblas '%s' does not match ROCBLAS_LOG_TRACE/PATH '%s'.\n", 
                        rpv3_rocblas_pipe, env_pipe);
                fprintf(stderr, "[Kernel Tracer] Logging disabled to prevent mismatch.\n");
                rpv3_rocblas_pipe = NULL; /* Disable logging */
            }
        }
        
        if (rpv3_rocblas_pipe) {
            if (is_fifo || is_reg_file) {
                /* Check for timeline mode + pipe incompatibility */
                if (timeline_enabled && is_fifo) {
                    fprintf(stderr, "[Kernel Tracer] Warning: RocBLAS logging with named pipes is not supported in timeline mode.\n");
                    fprintf(stderr, "[Kernel Tracer] Please use a regular file for --rocblas with --timeline.\n");
                    rpv3_rocblas_pipe = NULL; /* Disable logging */
                } else {
                    STATUS_PRINTF("[Kernel Tracer] Detected RocBLAS log file/pipe: %s\n", rpv3_rocblas_pipe);
                    
                    /* Open non-blocking */
                    rocblas_pipe_fd = open(rpv3_rocblas_pipe, O_RDONLY | O_NONBLOCK);
                    if (rocblas_pipe_fd != -1) {
                        strncpy(rocblas_pipe_path, rpv3_rocblas_pipe, sizeof(rocblas_pipe_path) - 1);
                        STATUS_PRINTF("[Kernel Tracer] Successfully opened RocBLAS log pipe\n");
                    } else {
                        fprintf(stderr, "[Kernel Tracer] Failed to open RocBLAS log pipe: %s\n", strerror(errno));
                    }
                }
            } else {
                STATUS_PRINTF("[Kernel Tracer] Pipe '%s' is not a FIFO or not found.\n", rpv3_rocblas_pipe);
            }
        }
    }

    /* Handle RocBLAS Log File */
    if (rpv3_rocblas_log_file) {
        if (!rpv3_rocblas_pipe) {
            fprintf(stderr, "[Kernel Tracer] Warning: --rocblas-log specified but --rocblas is missing. Ignoring.\n");
        } else {
            rocblas_log_file = fopen(rpv3_rocblas_log_file, "w");
            if (!rocblas_log_file) {
                fprintf(stderr, "[Kernel Tracer] Warning: Could not open rocBLAS log file '%s': %s\n", 
                        rpv3_rocblas_log_file, strerror(errno));
            } else {
                STATUS_PRINTF("[Kernel Tracer] Redirecting RocBLAS logs to: %s\n", rpv3_rocblas_log_file);
            }
        }
    }
    
    /* Check if counter mode is enabled */
    counter_mode = rpv3_counter_mode;
    
    if (timeline_enabled) {
        STATUS_PRINTF("[Kernel Tracer] Timeline mode enabled\n");
        /* Capture baseline timestamp when tracer starts */
        rocprofiler_get_timestamp(&tracer_start_timestamp);
    } else if (csv_enabled) {
        /* CSV mode needs start timestamp even without timeline */
        rocprofiler_get_timestamp(&tracer_start_timestamp);
    }
    
    if (counter_mode != RPV3_COUNTER_MODE_NONE) {
        STATUS_PRINTF("[Kernel Tracer] Counter collection enabled (mode: %d)\n", counter_mode);
    }
    
    /* Initialize kernel table */
    memset(kernel_table, 0, sizeof(kernel_table));
    
    /* Create a context for profiling */
    if (rocprofiler_create_context(&client_ctx) != ROCPROFILER_STATUS_SUCCESS) {
        fprintf(stderr, "[Kernel Tracer] Failed to create context\n");
        return -1;
    }
    
    /* Setup tracing based on mode */
    int result;
    if (timeline_enabled) {
        result = setup_buffer_tracing();
    } else if (counter_mode != RPV3_COUNTER_MODE_NONE) {
        result = setup_counter_collection();
    } else {
        result = setup_callback_tracing();
    }
    
    if (result != 0) {
        return -1;
    }
    
    /* Verify context is valid */
    int valid_ctx = 0;
    if (rocprofiler_context_is_valid(client_ctx, &valid_ctx) != ROCPROFILER_STATUS_SUCCESS ||
        valid_ctx == 0) {
        fprintf(stderr, "[Kernel Tracer] Context is not valid\n");
        return -1;
    }
    
    /* Start the context */
    if (rocprofiler_start_context(client_ctx) != ROCPROFILER_STATUS_SUCCESS) {
        fprintf(stderr, "[Kernel Tracer] Failed to start context\n");
        return -1;
    }
    
    STATUS_PRINTF("[Kernel Tracer] Profiler initialized successfully\n");
    
    return 0;
}

/* Tool finalization callback */
void tool_fini(void* tool_data) {
    (void) tool_data;
    
    STATUS_PRINTF("\n[Kernel Tracer] Finalizing profiler tool...\n");
    
    if (rocblas_pipe_fd != -1) {
        close(rocblas_pipe_fd);
        rocblas_pipe_fd = -1;
    }

    if (rocblas_log_file) {
        fclose(rocblas_log_file);
        rocblas_log_file = NULL;
    }

    /* Flush buffer if in timeline mode */
    if (timeline_enabled && trace_buffer.handle != 0) {
        rocprofiler_flush_buffer(trace_buffer);
    }
    
    STATUS_PRINTF("[Kernel Tracer] Total kernels traced: %lu\n", 
           (unsigned long)atomic_load(&kernel_count));
    STATUS_PRINTF("[Kernel Tracer] Unique kernel symbols tracked: %d\n",
           atomic_load(&kernel_table_size));
    
    /* Stop context if still active */
    if (client_ctx.handle != 0) {
        rocprofiler_stop_context(client_ctx);
    }
    
    /* Destroy buffer if created */
    if (timeline_enabled && trace_buffer.handle != 0) {
        rocprofiler_destroy_buffer(trace_buffer);
    }
    
    if (counter_buffer.handle != 0) {
        rocprofiler_destroy_buffer(counter_buffer);
    }

    /* Close output file if open */
    if (output_file) {
        fprintf(stderr, "[Kernel Tracer] Output saved to: %s\n", 
                rpv3_output_file ? rpv3_output_file : output_filename);
        fclose(output_file);
        output_file = NULL;
    }
}

/* Main entry point for the profiler tool */
rocprofiler_tool_configure_result_t*
rocprofiler_configure(uint32_t version,
                     const char* runtime_version,
                     uint32_t priority,
                     rocprofiler_client_id_t* id) {
    
    /* Parse options from environment variable */
    if (rpv3_parse_options() == RPV3_OPTIONS_EXIT) {
        return NULL;  /* Exit early without initializing profiler */
    }
    
    /* Compute version components */
    uint32_t major = version / 10000;
    uint32_t minor = (version % 10000) / 100;
    uint32_t patch = version % 100;
    
    STATUS_PRINTF("[Kernel Tracer] Configuring RPV3 v%s (Runtime: v%u.%u.%u, Priority: %u)\n",
           RPV3_VERSION, major, minor, patch, priority);
    
    /* Store client ID */
    client_id = id;
    
    /* Set client name */
    id->name = "KernelTracer";
    
    /* Create and return configure result */
    static rocprofiler_tool_configure_result_t result;
    result.size = sizeof(rocprofiler_tool_configure_result_t);
    result.initialize = tool_init;
    result.finalize = tool_fini;
    result.tool_data = NULL;
    
    return &result;
}

