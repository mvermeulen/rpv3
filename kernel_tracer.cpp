// MIT License
// Sample ROCm Profiler SDK kernel tracer - Enhanced version
// Demonstrates using rocprofiler-sdk to trace kernel dispatches with detailed information

// Include HIP headers first to satisfy RCCL dependencies in rocprofiler headers
#define __HIP_PLATFORM_AMD__
#include <hip/hip_runtime_api.h>

#include <rocprofiler-sdk/registration.h>
#include <rocprofiler-sdk/rocprofiler.h>
#include <rocprofiler-sdk/callback_tracing.h>
#include <rocprofiler-sdk/buffer.h>
#include <rocprofiler-sdk/buffer_tracing.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <unordered_map>
#include <string>
#include <vector>
#include <map>
#include <regex>
#include <regex>
#include <cxxabi.h>
#include <unistd.h>
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <poll.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>

#include "rpv3_options.h"
#include <dlfcn.h>
#include <execinfo.h>

extern "C" {
    // Intercept fopen to force line buffering on pipes
    typedef FILE* (*fopen_t)(const char*, const char*);
    static fopen_t real_fopen = nullptr;
    static fopen_t real_fopen64 = nullptr;

    FILE* fopen(const char* path, const char* mode) {
        if (!real_fopen) {
            real_fopen = (fopen_t)dlsym(RTLD_NEXT, "fopen");
        }
        FILE* fp = real_fopen(path, mode);
        
        // Check against environment variables directly to handle early initialization
        const char* trace_path = getenv("ROCBLAS_LOG_TRACE_PATH");
        const char* bench_path = getenv("ROCBLAS_LOG_BENCH_PATH");
        const char* profile_path = getenv("ROCBLAS_LOG_PROFILE_PATH");
        
        bool match = false;
        if (path) {
            if (trace_path && strcmp(path, trace_path) == 0) match = true;
            else if (bench_path && strcmp(path, bench_path) == 0) match = true;
            else if (profile_path && strcmp(path, profile_path) == 0) match = true;
            else if (rpv3_rocblas_pipe && strcmp(path, rpv3_rocblas_pipe) == 0) match = true;
        }

        if (fp && match) {
             setvbuf(fp, nullptr, _IONBF, 0);
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
        
        bool match = false;
        if (path) {
            if (trace_path && strcmp(path, trace_path) == 0) match = true;
            else if (bench_path && strcmp(path, bench_path) == 0) match = true;
            else if (profile_path && strcmp(path, profile_path) == 0) match = true;
            else if (rpv3_rocblas_pipe && strcmp(path, rpv3_rocblas_pipe) == 0) match = true;
        }
        
        if (fp && match) {
             setvbuf(fp, nullptr, _IONBF, 0);
        }
        return fp;
    }

    typedef FILE* (*fdopen_t)(int, const char*);
    static fdopen_t real_fdopen = nullptr;

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
                
                bool match = false;
                if (trace_path && strcmp(path, trace_path) == 0) match = true;
                else if (bench_path && strcmp(path, bench_path) == 0) match = true;
                else if (profile_path && strcmp(path, profile_path) == 0) match = true;
                else if (rpv3_rocblas_pipe && strcmp(path, rpv3_rocblas_pipe) == 0) match = true;
                
        if (fp && match) {
             setvbuf(fp, nullptr, _IONBF, 0);
        }
        }
            }
        return fp;
    }
}

namespace {
    std::atomic<uint64_t> kernel_count{0};
    rocprofiler_context_id_t client_ctx = {};
    rocprofiler_client_id_t* client_id = nullptr;
    std::unordered_map<rocprofiler_kernel_id_t, std::string> kernel_names;
    
    // Timeline mode state
    bool timeline_enabled = false;
    uint64_t tracer_start_timestamp = 0;  // Baseline timestamp when tracer starts
    rocprofiler_buffer_id_t trace_buffer = {};

    // CSV output mode state
    bool csv_enabled = false;

    // Backtrace mode state
    bool backtrace_enabled = false;

    // Counter collection state
    rpv3_counter_mode_t counter_mode = RPV3_COUNTER_MODE_NONE;
    
    struct AgentIdComparator {
        bool operator()(const rocprofiler_agent_id_t& lhs, const rocprofiler_agent_id_t& rhs) const {
            return lhs.handle < rhs.handle;
        }
    };
    
    std::map<rocprofiler_agent_id_t, rocprofiler_profile_config_id_t, AgentIdComparator> agent_profiles;
    rocprofiler_buffer_id_t counter_buffer = {};

    // Output file state
    FILE* output_file = nullptr;
    char output_filename[512];

    // RocBLAS Log Pipe state
    int rocblas_pipe_fd = -1;
    // RocBLAS log pipe path
    static char rocblas_pipe_path[256] = {0};

    // RocBLAS log file handle
    static FILE* rocblas_log_file = NULL;

    // Output macro for trace data (CSV or human-readable kernel details)
    // Protected by a mutex to ensure atomic lines
    std::mutex output_mutex;
    #define TRACE_PRINTF(...) do { \
        std::lock_guard<std::mutex> lock(output_mutex); \
        fprintf(output_file ? output_file : stdout, __VA_ARGS__); \
    } while(0)

    // Output macro for status messages (init, summary, errors)
    // If CSV output is enabled AND we are writing to a file, status messages go to stdout
    // Otherwise, they follow the trace output (to file if set, else stdout)
    #define STATUS_PRINTF(...) fprintf((output_file && csv_enabled) ? stdout : (output_file ? output_file : stdout), __VA_ARGS__)
}

// Helper function to demangle C++ kernel names
std::string demangle_kernel_name(const char* mangled_name) {
    if (!mangled_name) return "<unknown>";
    
    // Remove .kd suffix if present
    std::string name = std::regex_replace(mangled_name, std::regex{R"((\.kd)$)"}, "");
    
    // Try to demangle C++ names
    int status = 0;
    char* demangled = abi::__cxa_demangle(name.c_str(), nullptr, nullptr, &status);
    
    if (status == 0 && demangled) {
        std::string result(demangled);
        free(demangled);
        return result;
    }
    
    return name;
}

// Helper function to check if a kernel is a Tensile routine
bool is_tensile_kernel(const std::string& name) {
    return (name.find("Cijk") != std::string::npos || 
            name.find("assembly") != std::string::npos || 
            name.find("Tensile") != std::string::npos);
}

// Helper function to print backtrace
void print_backtrace() {
    const int max_frames = 64;
    void* buffer[max_frames];
    
    // Capture the backtrace
    int nptrs = backtrace(buffer, max_frames);
    
    if (nptrs <= 0) {
        TRACE_PRINTF("  (backtrace unavailable)\n");
        return;
    }
    
    TRACE_PRINTF("\nCall Stack (%d frames):\n", nptrs);
    
    // Process each frame
    for (int i = 0; i < nptrs; i++) {
        Dl_info info;
        
        if (dladdr(buffer[i], &info)) {
            // Extract library name (basename only)
            const char* lib_name = "???";
            if (info.dli_fname) {
                const char* slash = strrchr(info.dli_fname, '/');
                lib_name = slash ? (slash + 1) : info.dli_fname;
            }
            
            // Get function name
            if (info.dli_sname) {
                // Try to demangle C++ names
                std::string demangled = demangle_kernel_name(info.dli_sname);
                
                // Skip internal profiler frames
                if (strstr(lib_name, "libkernel_tracer") != nullptr ||
                    strstr(lib_name, "librocprofiler") != nullptr) {
                    continue;
                }
                
                // Calculate offset
                void* offset = (void*)((char*)buffer[i] - (char*)info.dli_saddr);
                
                TRACE_PRINTF("  #%-2d %s: %s + %p\n", i, lib_name, demangled.c_str(), offset);
            } else {
                // No symbol name available
                TRACE_PRINTF("  #%-2d %s: [0x%lx]\n", i, lib_name, (unsigned long)buffer[i]);
            }
        } else {
            // dladdr failed
            TRACE_PRINTF("  #%-2d [0x%lx]\n", i, (unsigned long)buffer[i]);
        }
    }
    TRACE_PRINTF("\n");
}

// Callback function for kernel symbol registration
void kernel_symbol_callback(rocprofiler_callback_tracing_record_t record,
                           rocprofiler_user_data_t* user_data,
                           void* callback_data) {
    (void) user_data;
    (void) callback_data;
    
    if (record.kind == ROCPROFILER_CALLBACK_TRACING_CODE_OBJECT &&
        record.operation == ROCPROFILER_CODE_OBJECT_DEVICE_KERNEL_SYMBOL_REGISTER) {
        
        auto* data = static_cast<rocprofiler_callback_tracing_code_object_kernel_symbol_register_data_t*>(record.payload);
        
        if (record.phase == ROCPROFILER_CALLBACK_PHASE_LOAD && data && data->kernel_name) {
            // Store the kernel name with demangling
            kernel_names[data->kernel_id] = demangle_kernel_name(data->kernel_name);
        }
        else if (record.phase == ROCPROFILER_CALLBACK_PHASE_UNLOAD && data) {
            // Don't remove kernel names in timeline mode - buffer callback needs them
            if (!timeline_enabled) {
                kernel_names.erase(data->kernel_id);
            }
        }
    }
}

// Buffer callback function for timeline mode (batch processing)
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
        fprintf(stderr, "[Kernel Tracer] Warning: Dropped %lu records\n", drop_count);
    }
    
    // CSV header output (once per process)
    if (csv_enabled) {
        static bool csv_header_printed = false;
        if (!csv_header_printed) {
            TRACE_PRINTF("KernelName,ThreadID,CorrelationID,KernelID,DispatchID,GridX,GridY,GridZ,WorkgroupX,WorkgroupY,WorkgroupZ,PrivateSeg,GroupSeg,StartTimestamp,EndTimestamp,DurationNs,DurationUs,TimeSinceStartMs\n");
            csv_header_printed = true;
        }
    }
    
    // Process batch of records
    for (size_t i = 0; i < num_headers; i++) {
        rocprofiler_record_header_t* header = headers[i];
        
        // Only process kernel dispatch records
        if (header->category == ROCPROFILER_BUFFER_CATEGORY_TRACING &&
            header->kind == ROCPROFILER_BUFFER_TRACING_KERNEL_DISPATCH) {
            
            auto* record = static_cast<rocprofiler_buffer_tracing_kernel_dispatch_record_t*>(header->payload);
            
            uint64_t count = kernel_count.fetch_add(1) + 1;
            
            // Extract timestamps (guaranteed non-zero in buffer mode)
            uint64_t start_ns = record->start_timestamp;
            uint64_t end_ns = record->end_timestamp;
            uint64_t duration_ns = end_ns - start_ns;
            double duration_us = duration_ns / 1000.0;
            
            // Calculate time since tracer started
            double time_since_start_ms = (start_ns - tracer_start_timestamp) / 1000000.0;
            
            // Look up kernel name
            std::string kernel_name = "<unknown>";
            auto it = kernel_names.find(record->dispatch_info.kernel_id);
            if (it != kernel_names.end()) {
                kernel_name = it->second;
            }
            
            if (csv_enabled) {
                TRACE_PRINTF("\"%s\",%lu,%lu,%lu,%lu,%u,%u,%u,%u,%u,%u,%u,%u,%lu,%lu,%lu,%.3f,%.3f\n",
                       kernel_name.c_str(),
                       (unsigned long)record->thread_id,
                       (unsigned long)record->correlation_id.internal,
                       (unsigned long)record->dispatch_info.kernel_id,
                       (unsigned long)record->dispatch_info.dispatch_id,
                       record->dispatch_info.grid_size.x, record->dispatch_info.grid_size.y, record->dispatch_info.grid_size.z,
                       record->dispatch_info.workgroup_size.x, record->dispatch_info.workgroup_size.y, record->dispatch_info.workgroup_size.z,
                       record->dispatch_info.private_segment_size,
                       record->dispatch_info.group_segment_size,
                       (unsigned long)start_ns,
                       (unsigned long)end_ns,
                       (unsigned long)(end_ns - start_ns),
                       duration_us,
                       time_since_start_ms);
            } else {
                // Human-readable output
                TRACE_PRINTF("\n[Kernel Trace #%lu]\n", (unsigned long)count);
                TRACE_PRINTF("  Kernel Name: %s\n", kernel_name.c_str());
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
                
                // Timeline information (only in buffer mode)
                TRACE_PRINTF("  Start Timestamp: %lu ns\n", (unsigned long)start_ns);
                TRACE_PRINTF("  End Timestamp: %lu ns\n", (unsigned long)end_ns);
                TRACE_PRINTF("  Duration: %.3f μs\n", duration_us);
                TRACE_PRINTF("  Time Since Start: %.3f ms\n", time_since_start_ms);
            }

            // Read from RocBLAS log if available and kernel matches pattern
            // In timeline mode, we only support reading from files, not pipes
            if (rocblas_pipe_fd != -1 && is_tensile_kernel(kernel_name)) {
                char line_buffer[4096];
                int line_pos = 0;
                bool valid_line_found = false;
                
                // Read char-by-char to handle filtering and stop after one valid line
                while (!valid_line_found) {
                    char c;
                    ssize_t bytes_read = read(rocblas_pipe_fd, &c, 1);
                    
                    if (bytes_read > 0) {
                        // Write to log file if enabled (raw stream)
                        if (rocblas_log_file) {
                            fputc(c, rocblas_log_file);
                        }
                        
                        if (c == '\n') {
                            line_buffer[line_pos] = '\0';
                            
                            // Clean up carriage return if present
                            if (line_pos > 0 && line_buffer[line_pos-1] == '\r') {
                                line_buffer[line_pos-1] = '\0';
                            }
                            
                            // Check filters
                            if (strstr(line_buffer, "rocblas_create_handle") != nullptr || 
                                strstr(line_buffer, "rocblas_destroy_handle") != nullptr ||
                                strstr(line_buffer, "rocblas_set_stream") != nullptr) {
                                // Skip this line, reset buffer and continue reading
                                line_pos = 0;
                            } else {
                                // Valid line found
                                if (strlen(line_buffer) > 0) {
                                    TRACE_PRINTF("# %s\n", line_buffer);
                                    valid_line_found = true; // Stop reading
                                } else {
                                    // Empty line, just reset
                                    line_pos = 0;
                                }
                            }
                        } else {
                            if (line_pos < (int)sizeof(line_buffer) - 1) {
                                line_buffer[line_pos++] = c;
                            }
                            // Else: line too long, truncate
                        }
                    } else {
                        // EOF or error
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


// Callback function for kernel dispatch events
void kernel_dispatch_callback(rocprofiler_callback_tracing_record_t record,
                              rocprofiler_user_data_t* user_data,
                              void* callback_data) {
    (void) user_data;
    (void) callback_data;
    
    // CSV header output (once per process)
    if (csv_enabled) {
        static bool csv_header_printed = false;
        if (!csv_header_printed) {
            TRACE_PRINTF("KernelName,ThreadID,CorrelationID,KernelID,DispatchID,GridX,GridY,GridZ,WorkgroupX,WorkgroupY,WorkgroupZ,PrivateSeg,GroupSeg,StartTimestamp,EndTimestamp,DurationNs,DurationUs,TimeSinceStartMs\n");
            csv_header_printed = true;
        }
    }
    
    // Only process kernel dispatch events on entry
    if (record.kind == ROCPROFILER_CALLBACK_TRACING_KERNEL_DISPATCH &&
        record.phase == ROCPROFILER_CALLBACK_PHASE_ENTER) {
        
        // In CSV mode, suppress ENTER phase output
        if (csv_enabled) {
            return;
        }
        
        uint64_t count = kernel_count.fetch_add(1) + 1;
        
        // Cast payload to kernel dispatch data
        auto* dispatch_data = static_cast<rocprofiler_callback_tracing_kernel_dispatch_data_t*>(record.payload);
        
        if (!dispatch_data) {
            TRACE_PRINTF("[Kernel Trace #%lu] <no dispatch data>\n", (unsigned long)count);
            return;
        }
        
        const auto& info = dispatch_data->dispatch_info;
        
        // Look up kernel name
        std::string kernel_name = "<unknown>";
        auto it = kernel_names.find(info.kernel_id);
        if (it != kernel_names.end()) {
            kernel_name = it->second;
        }
        
        // Backtrace mode: print kernel info and call stack
        if (backtrace_enabled) {
            TRACE_PRINTF("\n[Kernel Trace #%lu]\n", (unsigned long)count);
            TRACE_PRINTF("  Kernel Name: %s\n", kernel_name.c_str());
            TRACE_PRINTF("  Dispatch ID: %lu\n", (unsigned long)info.dispatch_id);
            TRACE_PRINTF("  Grid Size: [%u, %u, %u]\n", 
                   info.grid_size.x, info.grid_size.y, info.grid_size.z);
            
            // Print the backtrace
            print_backtrace();
            
            TRACE_PRINTF("----------------------------------------\n");
            return;
        }
        
        // Normal mode: print full kernel details
        TRACE_PRINTF("\n[Kernel Trace #%lu]\n", (unsigned long)count);
        TRACE_PRINTF("  Kernel Name: %s\n", kernel_name.c_str());
        TRACE_PRINTF("  Thread ID: %lu\n", (unsigned long)record.thread_id);
        TRACE_PRINTF("  Correlation ID: %lu\n", (unsigned long)record.correlation_id.internal);
        TRACE_PRINTF("  Kernel ID: %lu\n", (unsigned long)info.kernel_id);
        TRACE_PRINTF("  Dispatch ID: %lu\n", (unsigned long)info.dispatch_id);
        TRACE_PRINTF("  Grid Size: [%u, %u, %u]\n", 
               info.grid_size.x, info.grid_size.y, info.grid_size.z);
        TRACE_PRINTF("  Workgroup Size: [%u, %u, %u]\n",
               info.workgroup_size.x, info.workgroup_size.y, info.workgroup_size.z);
        TRACE_PRINTF("  Private Segment Size: %u bytes (scratch memory per work-item)\n",
               info.private_segment_size);
        TRACE_PRINTF("  Group Segment Size: %u bytes (LDS memory per work-group)\n",
               info.group_segment_size);
    }
    else if (record.kind == ROCPROFILER_CALLBACK_TRACING_KERNEL_DISPATCH &&
             record.phase == ROCPROFILER_CALLBACK_PHASE_EXIT) {
        
        // Cast payload to kernel dispatch data
        auto* dispatch_data = static_cast<rocprofiler_callback_tracing_kernel_dispatch_data_t*>(record.payload);
        
        if (!dispatch_data) {
            return;
        }
        
        // Common data retrieval
        const auto& info = dispatch_data->dispatch_info;
        std::string kernel_name = "<unknown>";
        auto it = kernel_names.find(info.kernel_id);
        if (it != kernel_names.end()) {
            kernel_name = it->second;
        } else {
            // Only print debug if not in CSV mode to avoid breaking CSV format? 
            // Or print to stderr?
            // STATUS_PRINTF("[Kernel Tracer] Debug: Kernel name lookup failed for ID %lu\n", (unsigned long)info.kernel_id);
        }

        if (csv_enabled) {
            // CSV mode: output complete line on EXIT
            uint64_t count = kernel_count.fetch_add(1) + 1;
            (void)count;  // Suppress unused variable warning
            
            uint64_t start_ns = dispatch_data->start_timestamp;
            uint64_t end_ns = dispatch_data->end_timestamp;
            uint64_t duration_ns = (end_ns > start_ns) ? (end_ns - start_ns) : 0;
            double duration_us = duration_ns / 1000.0;
            double time_since_start_ms = (start_ns > tracer_start_timestamp) ? 
                                         ((start_ns - tracer_start_timestamp) / 1000000.0) : 0.0;
            
            TRACE_PRINTF("\"%s\",%lu,%lu,%lu,%lu,%u,%u,%u,%u,%u,%u,%u,%u,%lu,%lu,%lu,%.3f,%.3f\n",
                   kernel_name.c_str(),
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
            // Standard mode: display timestamps on exit
            if (dispatch_data->end_timestamp > 0) {
                uint64_t duration_ns = dispatch_data->end_timestamp - dispatch_data->start_timestamp;
                double duration_us = duration_ns / 1000.0;
                
                TRACE_PRINTF("  Start Timestamp: %lu ns\n", (unsigned long)dispatch_data->start_timestamp);
                TRACE_PRINTF("  End Timestamp: %lu ns\n", (unsigned long)dispatch_data->end_timestamp);
                TRACE_PRINTF("  Duration: %.3f μs\n", duration_us);
            }
        }
        
        // Read from RocBLAS pipe if available and kernel matches pattern
        // This is now outside the if/else block so it runs for both CSV and Standard modes
        if (rocblas_pipe_fd != -1 && is_tensile_kernel(kernel_name)) {
            // Use poll to wait for data with timeout (robust against timing issues)
            struct pollfd pfd;
            pfd.fd = rocblas_pipe_fd;
            pfd.events = POLLIN;
            
            // Wait up to 500ms for data
            int ret = poll(&pfd, 1, 500);
            
            if (ret > 0 && (pfd.revents & POLLIN)) {
                char line_buffer[4096];
                int line_pos = 0;
                bool valid_line_found = false;
                
                // Read char-by-char to handle pipe correctly and stop after one valid line
                while (!valid_line_found) {
                    char c;
                    ssize_t bytes_read = read(rocblas_pipe_fd, &c, 1);
                    
                    if (bytes_read > 0) {
                        // Write to log file if enabled (raw stream)
                        if (rocblas_log_file) {
                            fputc(c, rocblas_log_file);
                        }
                        
                        if (c == '\n') {
                            line_buffer[line_pos] = '\0';
                            
                            // Clean up carriage return if present
                            if (line_pos > 0 && line_buffer[line_pos-1] == '\r') {
                                line_buffer[line_pos-1] = '\0';
                            }
                            
                            // Check filters
                            if (strstr(line_buffer, "rocblas_create_handle") != nullptr || 
                                strstr(line_buffer, "rocblas_destroy_handle") != nullptr ||
                                strstr(line_buffer, "rocblas_set_stream") != nullptr) {
                                // Skip this line, reset buffer and continue reading
                                line_pos = 0;
                            } else {
                                // Valid line found
                                if (strlen(line_buffer) > 0) {
                                    TRACE_PRINTF("# %s\n", line_buffer);
                                    valid_line_found = true; // Stop reading
                                } else {
                                    // Empty line, just reset
                                    line_pos = 0;
                                }
                            }
                        } else {
                            if (line_pos < (int)sizeof(line_buffer) - 1) {
                                line_buffer[line_pos++] = c;
                            }
                            // Else: line too long, truncate (ignore extra chars until newline)
                        }
                    } else {
                        // EAGAIN or error or EOF
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

// Setup buffer tracing (for timeline mode)
int setup_buffer_tracing() {
    STATUS_PRINTF("[Kernel Tracer] Setting up buffer tracing for timeline mode...\n");
    
    // Create buffer
    const size_t buffer_size = 8192;      // 8 KB
    const size_t buffer_watermark = 7168; // Flush at 87.5% full
    
    rocprofiler_status_t status = rocprofiler_create_buffer(
        client_ctx,
        buffer_size,
        buffer_watermark,
        ROCPROFILER_BUFFER_POLICY_LOSSLESS,  // Don't drop records
        timeline_buffer_callback,
        nullptr,  // callback data
        &trace_buffer
    );
    
    if (status != ROCPROFILER_STATUS_SUCCESS) {
        fprintf(stderr, "[Kernel Tracer] Failed to create buffer\n");
        return -1;
    }
    
    // Configure buffer tracing for kernel dispatches
    status = rocprofiler_configure_buffer_tracing_service(
        client_ctx,
        ROCPROFILER_BUFFER_TRACING_KERNEL_DISPATCH,
        nullptr,  // operations (nullptr = all)
        0,        // operations count
        trace_buffer
    );
    
    if (status != ROCPROFILER_STATUS_SUCCESS) {
        fprintf(stderr, "[Kernel Tracer] Failed to configure buffer tracing\n");
        return -1;
    }
    
    // Still need code object callback for kernel names
    status = rocprofiler_configure_callback_tracing_service(
        client_ctx,
        ROCPROFILER_CALLBACK_TRACING_CODE_OBJECT,
        nullptr,
        0,
        kernel_symbol_callback,
        nullptr
    );
    
    if (status != ROCPROFILER_STATUS_SUCCESS) {
        fprintf(stderr, "[Kernel Tracer] Failed to configure code object callback\n");
        return -1;
    }
    
    return 0;
}

// Setup callback tracing (for non-timeline mode)
int setup_callback_tracing() {
    STATUS_PRINTF("[Kernel Tracer] Setting up callback tracing...\n");
    
    // Configure callback tracing for code object/kernel symbol registration
    if (rocprofiler_configure_callback_tracing_service(
            client_ctx,
            ROCPROFILER_CALLBACK_TRACING_CODE_OBJECT,
            nullptr,
            0,
            kernel_symbol_callback,
            nullptr
        ) != ROCPROFILER_STATUS_SUCCESS) {
        fprintf(stderr, "[Kernel Tracer] Failed to configure code object callback tracing\n");
        return -1;
    }
    
    // Configure callback tracing for kernel dispatches
    if (rocprofiler_configure_callback_tracing_service(
            client_ctx,
            ROCPROFILER_CALLBACK_TRACING_KERNEL_DISPATCH,
            nullptr,
            0,
            kernel_dispatch_callback,
            nullptr
        ) != ROCPROFILER_STATUS_SUCCESS) {
        fprintf(stderr, "[Kernel Tracer] Failed to configure kernel dispatch callback tracing\n");
        return -1;
    }
    
    return 0;
}

// Get target counters for the selected mode
std::vector<std::string> get_target_counters(rpv3_counter_mode_t mode) {
    std::vector<std::string> counters;
    
    if (mode == RPV3_COUNTER_MODE_COMPUTE || mode == RPV3_COUNTER_MODE_MIXED) {
        counters.push_back("SQ_INSTS_VALU");
        counters.push_back("SQ_WAVES");
        counters.push_back("SQ_INSTS_SALU");
    }
    
    if (mode == RPV3_COUNTER_MODE_MEMORY || mode == RPV3_COUNTER_MODE_MIXED) {
        counters.push_back("TCC_EA_RDREQ_sum"); // HBM Read
        counters.push_back("TCC_EA_WRREQ_sum"); // HBM Write
        counters.push_back("TCC_EA_RDREQ_32B_sum");
        counters.push_back("TCC_EA_RDREQ_64B_sum");
        counters.push_back("TCP_TCC_WRITE_REQ_sum");
    }
    
    return counters;
}

// Create a profile for a specific agent
void create_profile_for_agent(rocprofiler_agent_id_t agent_id) {
    // 1. Get all supported counters for this agent
    std::map<std::string, rocprofiler_counter_id_t> supported_counters;
    
    STATUS_PRINTF("[Kernel Tracer] Debug: Querying counters for agent handle %lu\n", agent_id.handle);
    
    rocprofiler_status_t status = rocprofiler_iterate_agent_supported_counters(
        agent_id,
        [](rocprofiler_agent_id_t agent, rocprofiler_counter_id_t* counters, size_t num_counters, void* user_data) {
            (void) agent;
            auto* available_counters = static_cast<std::map<std::string, rocprofiler_counter_id_t>*>(user_data);
            
            STATUS_PRINTF("[Kernel Tracer] Debug: Callback received %zu counters\n", num_counters);
            
            for (size_t i = 0; i < num_counters; i++) {
                rocprofiler_counter_info_v0_t info;
                rocprofiler_status_t status = rocprofiler_query_counter_info(
                    counters[i],
                    ROCPROFILER_COUNTER_INFO_VERSION_0,
                    &info
                );
                
                if (status == ROCPROFILER_STATUS_SUCCESS && info.name) {
                    (*available_counters)[std::string(info.name)] = counters[i];
                    // Print first few counters for debug
                    if (i < 5) STATUS_PRINTF("[Kernel Tracer] Debug: Found counter %s\n", info.name);
                } else {
                    STATUS_PRINTF("[Kernel Tracer] Debug: Failed to query info for counter %zu (status: %d)\n", i, status);
                }
            }
            return ROCPROFILER_STATUS_SUCCESS;
        },
        &supported_counters
    );
    
    if (status != ROCPROFILER_STATUS_SUCCESS) {
        STATUS_PRINTF("[Kernel Tracer] Debug: rocprofiler_iterate_agent_supported_counters failed with status %d\n", status);
    }
    
    // 2. Select counters that match our target list
    std::vector<std::string> target_names = get_target_counters(counter_mode);
    std::vector<rocprofiler_counter_id_t> selected_counters;
    
    STATUS_PRINTF("[Kernel Tracer] Creating profile for agent. Targets: %zu, Supported: %zu\n", 
           target_names.size(), supported_counters.size());
           
    for (const auto& name : target_names) {
        auto it = supported_counters.find(name);
        if (it != supported_counters.end()) {
            selected_counters.push_back(it->second);
            STATUS_PRINTF("  + Added counter: %s\n", name.c_str());
        } else {
            STATUS_PRINTF("  - Counter not found: %s\n", name.c_str());
        }
    }
    
    if (selected_counters.empty()) {
        STATUS_PRINTF("[Kernel Tracer] Warning: No matching counters found for this agent\n");
        return;
    }
    
    // 3. Create the profile
    rocprofiler_profile_config_id_t profile_id = {};
    status = rocprofiler_create_profile_config(
        agent_id,
        selected_counters.data(),
        selected_counters.size(),
        &profile_id
    );
    
    if (status == ROCPROFILER_STATUS_SUCCESS) {
        agent_profiles[agent_id] = profile_id;
        STATUS_PRINTF("[Kernel Tracer] Profile created successfully with %zu counters\n", selected_counters.size());
    } else {
        fprintf(stderr, "[Kernel Tracer] Failed to create profile config: %d\n", status);
    }
}

// Callback for dispatch counting service (called before kernel launch)
void dispatch_counting_callback(
    rocprofiler_dispatch_counting_service_data_t dispatch_data,
    rocprofiler_profile_config_id_t* config,
    rocprofiler_user_data_t* user_data,
    void* callback_data_args
) {
    (void) user_data;
    (void) callback_data_args;
    
    // Find the profile for this agent
    auto it = agent_profiles.find(dispatch_data.dispatch_info.agent_id);
    if (it != agent_profiles.end()) {
        *config = it->second;
    }
}

// Callback for processing collected counter records
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
        fprintf(stderr, "[Kernel Tracer] Warning: Dropped %lu counter records\n", drop_count);
    }
    
    for (size_t i = 0; i < num_headers; i++) {
        rocprofiler_record_header_t* header = headers[i];
        
        if (header->category == ROCPROFILER_BUFFER_CATEGORY_COUNTERS &&
            header->kind == ROCPROFILER_COUNTER_RECORD_VALUE) {
            
            auto* record = static_cast<rocprofiler_counter_record_t*>(header->payload);
            
            // We can correlate with kernel info if we tracked correlation_id
            // For now, just print the counter values
            
            // Note: To get the kernel name here, we would need to map correlation_id 
            // to the kernel name stored in kernel_symbol_callback.
            // Since this is asynchronous, we might print it separately.
            
            // However, the record contains dispatch_id which we can use if we tracked it.
            // But let's keep it simple and just print the counter record.
            
            // Wait, rocprofiler_counter_record_t doesn't have the counter name directly.
            // It has counter_id. We would need to look up the name.
            // For simplicity in this step, we will just print the ID and value.
            // A full implementation would map IDs back to names.
            
            // To make it useful, let's try to print the kernel ID at least.
            // But the record structure is:
            // struct rocprofiler_counter_record_t {
            //   rocprofiler_dispatch_id_t dispatch_id;
            //   rocprofiler_counter_id_t counter_id;
            //   rocprofiler_counter_value_t value;
            //   ...
            // }
            
            // We'll just print: [Counters] Dispatch ID: X, Counter ID: Y, Value: Z
            
            // We'll just print: [Counters] Dispatch ID: X, Value: Z
            
            STATUS_PRINTF("[Counters] Dispatch ID: %lu, Value: %f\n",
                   record->dispatch_id, record->counter_value);
        }
    }
}

// Setup counter collection
int setup_counter_collection() {
    STATUS_PRINTF("[Kernel Tracer] Setting up counter collection...\n");
    
    // IMPORTANT: Counter collection also needs code object callback for kernel symbols
    // Configure callback tracing for code object/kernel symbol registration
    rocprofiler_status_t status = rocprofiler_configure_callback_tracing_service(
        client_ctx,
        ROCPROFILER_CALLBACK_TRACING_CODE_OBJECT,
        nullptr,
        0,
        kernel_symbol_callback,
        nullptr
    );
    
    if (status != ROCPROFILER_STATUS_SUCCESS) {
        fprintf(stderr, "[Kernel Tracer] Failed to configure code object callback tracing\n");
        return -1;
    }
    
    // 1. Query available agents
    std::vector<rocprofiler_agent_id_t> agents;
    rocprofiler_query_available_agents(
        ROCPROFILER_AGENT_INFO_VERSION_0,
        [](rocprofiler_agent_version_t version, const void** agents, size_t num_agents, void* data) {
            (void) version;
            auto* agents_vec = static_cast<std::vector<rocprofiler_agent_id_t>*>(data);
            
            for (size_t i = 0; i < num_agents; i++) {
                const auto* info = static_cast<const rocprofiler_agent_v0_t*>(agents[i]);
                if (info->type == ROCPROFILER_AGENT_TYPE_GPU) {
                    agents_vec->push_back(info->id);
                }
            }
            return ROCPROFILER_STATUS_SUCCESS;
        },
        sizeof(rocprofiler_agent_v0_t),
        &agents
    );
    
    if (agents.empty()) {
        STATUS_PRINTF("[Kernel Tracer] No GPU agents found for counter collection\n");
        return 0;
    }
    
    // 2. Check if any agent supports counters and create profiles
    bool any_agent_supported = false;
    
    for (auto agent_id : agents) {
        create_profile_for_agent(agent_id);
        if (agent_profiles.find(agent_id) != agent_profiles.end()) {
            any_agent_supported = true;
        }
    }
    
    if (!any_agent_supported) {
        STATUS_PRINTF("[Kernel Tracer] Warning: No agents support counter collection or no counters found. Counter collection disabled.\n");
        STATUS_PRINTF("[Kernel Tracer] Falling back to callback tracing mode...\n");
        // Fall back to regular callback tracing since counters aren't available
        // Code object callback is already configured above
        // Just need to add kernel dispatch callback
        status = rocprofiler_configure_callback_tracing_service(
            client_ctx,
            ROCPROFILER_CALLBACK_TRACING_KERNEL_DISPATCH,
            nullptr,
            0,
            kernel_dispatch_callback,
            nullptr
        );
        
        if (status != ROCPROFILER_STATUS_SUCCESS) {
            fprintf(stderr, "[Kernel Tracer] Failed to configure kernel dispatch callback tracing\n");
            return -1;
        }
        
        return 0;
    }
    
    // 3. Create buffer for counter records
    const size_t buffer_size = 64 * 1024; // 64 KB
    const size_t buffer_watermark = 56 * 1024;
    
    status = rocprofiler_create_buffer(
        client_ctx,
        buffer_size,
        buffer_watermark,
        ROCPROFILER_BUFFER_POLICY_LOSSLESS,
        counter_record_callback,
        nullptr,
        &counter_buffer
    );
    
    if (status != ROCPROFILER_STATUS_SUCCESS) {
        fprintf(stderr, "[Kernel Tracer] Failed to create counter buffer\n");
        return -1;
    }
    
    // 4. Configure dispatch counting service
    status = rocprofiler_configure_buffer_dispatch_counting_service(
        client_ctx,
        counter_buffer,
        dispatch_counting_callback,
        nullptr
    );
    
    if (status != ROCPROFILER_STATUS_SUCCESS) {
        fprintf(stderr, "[Kernel Tracer] Warning: Failed to configure dispatch counting service (status: %d)\n", status);
        fprintf(stderr, "[Kernel Tracer] This hardware/ROCm version may not support counter collection.\n");
        fprintf(stderr, "[Kernel Tracer] Falling back to callback tracing mode...\n");
        
        // Destroy the counter buffer since we won't use it
        if (counter_buffer.handle != 0) {
            rocprofiler_destroy_buffer(counter_buffer);
            counter_buffer.handle = 0;
        }
        
        // Fall back to callback tracing
        // Code object callback is already configured
        // Just need to add kernel dispatch callback
        status = rocprofiler_configure_callback_tracing_service(
            client_ctx,
            ROCPROFILER_CALLBACK_TRACING_KERNEL_DISPATCH,
            nullptr,
            0,
            kernel_dispatch_callback,
            nullptr
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

// Tool initialization callback

int tool_init(rocprofiler_client_finalize_t fini_func,
              void* tool_data) {
    (void) fini_func;
    (void) tool_data;
    
    STATUS_PRINTF("[Kernel Tracer] Initializing profiler tool...\n");
    
    // Check if timeline mode is enabled (from rpv3_options)
    timeline_enabled = (rpv3_timeline_enabled != 0);
    
    // Check if CSV mode is enabled (from rpv3_options)
    csv_enabled = (rpv3_csv_enabled != 0);
    
    // Check if backtrace mode is enabled (from rpv3_options)
    backtrace_enabled = (rpv3_backtrace_enabled != 0);
    
    // Validate: backtrace is incompatible with timeline and CSV
    // (This should already be caught in rpv3_parse_options, but double-check here)
    if (backtrace_enabled) {
        if (timeline_enabled) {
            fprintf(stderr, "[Kernel Tracer] Error: Backtrace mode is incompatible with timeline mode\n");
            return -1;
        }
        if (csv_enabled) {
            fprintf(stderr, "[Kernel Tracer] Error: Backtrace mode is incompatible with CSV mode\n");
            return -1;
        }
    }

    // Handle output redirection
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

    // Handle RocBLAS Log Pipe
    if (rpv3_rocblas_pipe) {
        // Check if it's a FIFO or regular file first
        struct stat st;
        bool is_reg_file = false;
        bool is_fifo = false;
        
        if (stat(rpv3_rocblas_pipe, &st) == 0) {
            if (S_ISREG(st.st_mode)) is_reg_file = true;
            if (S_ISFIFO(st.st_mode)) is_fifo = true;
        }

        // Verify against ROCBLAS_LOG_TRACE or ROCBLAS_LOG_TRACE_PATH environment variable
        // Only enforce this check if it is NOT a regular file (i.e. it is a pipe or we don't know yet)
        if (!is_reg_file) {
            const char* env_pipe = getenv("ROCBLAS_LOG_TRACE");
            if (!env_pipe) {
                env_pipe = getenv("ROCBLAS_LOG_TRACE_PATH");
            }
            
            if (!env_pipe) {
                fprintf(stderr, "[Kernel Tracer] Warning: --rocblas specified '%s' but ROCBLAS_LOG_TRACE/ROCBLAS_LOG_TRACE_PATH is not set.\n", rpv3_rocblas_pipe);
                fprintf(stderr, "[Kernel Tracer] RocBLAS will not write to the pipe. Logging disabled.\n");
                rpv3_rocblas_pipe = nullptr; // Disable logging
            } else if (strcmp(rpv3_rocblas_pipe, env_pipe) != 0) {
                fprintf(stderr, "[Kernel Tracer] Error: --rocblas '%s' does not match ROCBLAS_LOG_TRACE/PATH '%s'.\n", 
                        rpv3_rocblas_pipe, env_pipe);
                fprintf(stderr, "[Kernel Tracer] Logging disabled to prevent mismatch.\n");
                rpv3_rocblas_pipe = nullptr; // Disable logging
            }
        }
        
        if (rpv3_rocblas_pipe) {
            if (is_fifo || is_reg_file) {
                // Check for timeline mode + pipe incompatibility
                if (timeline_enabled && is_fifo) {
                    fprintf(stderr, "[Kernel Tracer] Warning: RocBLAS logging with named pipes is not supported in timeline mode.\n");
                    fprintf(stderr, "[Kernel Tracer] Please use a regular file for --rocblas with --timeline.\n");
                    rpv3_rocblas_pipe = nullptr; // Disable logging
                } else {
                    STATUS_PRINTF("[Kernel Tracer] Detected RocBLAS log file/pipe: %s\n", rpv3_rocblas_pipe);
                    
                    // Open non-blocking
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

    // Handle RocBLAS Log File
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
    
    // Check if counter mode is enabled
    counter_mode = rpv3_counter_mode;
    
    if (timeline_enabled) {
        STATUS_PRINTF("[Kernel Tracer] Timeline mode enabled\n");
        // Capture baseline timestamp when tracer starts
        rocprofiler_get_timestamp(&tracer_start_timestamp);
    } else if (csv_enabled) {
        // CSV mode needs start timestamp even without timeline
        rocprofiler_get_timestamp(&tracer_start_timestamp);
    }
    
    if (counter_mode != RPV3_COUNTER_MODE_NONE) {
        STATUS_PRINTF("[Kernel Tracer] Counter collection enabled (mode: %d)\n", counter_mode);
    }
    
    // Create a context for profiling
    if (rocprofiler_create_context(&client_ctx) != ROCPROFILER_STATUS_SUCCESS) {
        fprintf(stderr, "[Kernel Tracer] Failed to create context\n");
        return -1;
    }
    
    // Setup tracing based on mode
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
    
    // Verify context is valid
    int valid_ctx = 0;
    if (rocprofiler_context_is_valid(client_ctx, &valid_ctx) != ROCPROFILER_STATUS_SUCCESS ||
        valid_ctx == 0) {
        fprintf(stderr, "[Kernel Tracer] Context is not valid\n");
        return -1;
    }
    
    // Start the context
    if (rocprofiler_start_context(client_ctx) != ROCPROFILER_STATUS_SUCCESS) {
        fprintf(stderr, "[Kernel Tracer] Failed to start context\n");
        return -1;
    }
    
    STATUS_PRINTF("[Kernel Tracer] Profiler initialized successfully\n");
    
    return 0;
}

// Tool finalization callback
void tool_fini(void* tool_data) {
    (void) tool_data;
    
    STATUS_PRINTF("\n[Kernel Tracer] Finalizing profiler tool...\n");
    
    // Stop background thread
    
    if (rocblas_pipe_fd != -1) {
        close(rocblas_pipe_fd);
        rocblas_pipe_fd = -1;
    }

    if (rocblas_log_file) {
        fclose(rocblas_log_file);
        rocblas_log_file = NULL;
    }


    
    // Flush buffer if in timeline mode
    if (timeline_enabled && trace_buffer.handle != 0) {
        rocprofiler_flush_buffer(trace_buffer);
    }
    
    STATUS_PRINTF("[Kernel Tracer] Total kernels traced: %lu\n", kernel_count.load());
    STATUS_PRINTF("[Kernel Tracer] Unique kernel symbols tracked: %zu\n", kernel_names.size());
    
    // Stop context if still active
    if (client_ctx.handle != 0) {
        rocprofiler_stop_context(client_ctx);
    }
    
    // Destroy buffer if created
    if (timeline_enabled && trace_buffer.handle != 0) {
        rocprofiler_destroy_buffer(trace_buffer);
    }
    
    if (counter_buffer.handle != 0) {
        rocprofiler_destroy_buffer(counter_buffer);
    }

    // Close output file if open
    if (output_file) {
        fprintf(stderr, "[Kernel Tracer] Output saved to: %s\n", 
                rpv3_output_file ? rpv3_output_file : output_filename);
        fclose(output_file);
        output_file = nullptr;
    }
}

extern "C" {
    // Main entry point for the profiler tool
    rocprofiler_tool_configure_result_t*
    rocprofiler_configure(uint32_t version,
                         const char* runtime_version,
                         uint32_t priority,
                         rocprofiler_client_id_t* id) {
        
        // Parse options from environment variable
        if (rpv3_parse_options() == RPV3_OPTIONS_EXIT) {
            return nullptr;  // Exit early without initializing profiler
        }
        
        // Compute version components
        uint32_t major = version / 10000;
        uint32_t minor = (version % 10000) / 100;
        uint32_t patch = version % 100;
        
        STATUS_PRINTF("[Kernel Tracer] Configuring RPV3 v%s (Runtime: v%u.%u.%u, Priority: %u)\n",
           RPV3_VERSION, major, minor, patch, priority);
        
        // Store client ID
        client_id = id;
        
        // Set client name
        id->name = "KernelTracer";
        
        // Create and return configure result
        static rocprofiler_tool_configure_result_t result;
        result.size = sizeof(rocprofiler_tool_configure_result_t);
        result.initialize = tool_init;
        result.finalize = tool_fini;
        result.tool_data = nullptr;
        
        return &result;
    }
}
