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
#include <regex>
#include <cxxabi.h>

#include "rpv3_options.h"

namespace {
    std::atomic<uint64_t> kernel_count{0};
    rocprofiler_context_id_t client_ctx = {};
    rocprofiler_client_id_t* client_id = nullptr;
    std::unordered_map<rocprofiler_kernel_id_t, std::string> kernel_names;
    
    // Timeline mode state
    bool timeline_enabled = false;
    uint64_t tracer_start_timestamp = 0;  // Baseline timestamp when tracer starts
    rocprofiler_buffer_id_t trace_buffer = {};
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
            
            // Calculate time since tracer started
            double time_since_start_ms = (start_ns - tracer_start_timestamp) / 1000000.0;
            
            // Look up kernel name
            std::string kernel_name = "<unknown>";
            auto it = kernel_names.find(record->dispatch_info.kernel_id);
            if (it != kernel_names.end()) {
                kernel_name = it->second;
            }
            
            // Print all dispatch information with timeline data
            printf("\n[Kernel Trace #%lu]\n", count);
            printf("  Kernel Name: %s\n", kernel_name.c_str());
            printf("  Thread ID: %lu\n", record->thread_id);
            printf("  Correlation ID: %lu\n", record->correlation_id.internal);
            printf("  Kernel ID: %lu\n", record->dispatch_info.kernel_id);
            printf("  Dispatch ID: %lu\n", record->dispatch_info.dispatch_id);
            printf("  Grid Size: [%u, %u, %u]\n", 
                   record->dispatch_info.grid_size.x,
                   record->dispatch_info.grid_size.y,
                   record->dispatch_info.grid_size.z);
            printf("  Workgroup Size: [%u, %u, %u]\n",
                   record->dispatch_info.workgroup_size.x,
                   record->dispatch_info.workgroup_size.y,
                   record->dispatch_info.workgroup_size.z);
            printf("  Private Segment Size: %u bytes (scratch memory per work-item)\n",
                   record->dispatch_info.private_segment_size);
            printf("  Group Segment Size: %u bytes (LDS memory per work-group)\n",
                   record->dispatch_info.group_segment_size);
            
            // Timeline information (only in buffer mode)
            printf("  Start Timestamp: %lu ns\n", start_ns);
            printf("  End Timestamp: %lu ns\n", end_ns);
            printf("  Duration: %.3f μs\n", duration_ns / 1000.0);
            printf("  Time Since Start: %.3f ms\n", time_since_start_ms);
        }
    }
}


// Callback function for kernel dispatch events
void kernel_dispatch_callback(rocprofiler_callback_tracing_record_t record,
                              rocprofiler_user_data_t* user_data,
                              void* callback_data) {
    (void) user_data;
    (void) callback_data;
    
    // Only process kernel dispatch events on entry
    if (record.kind == ROCPROFILER_CALLBACK_TRACING_KERNEL_DISPATCH &&
        record.phase == ROCPROFILER_CALLBACK_PHASE_ENTER) {
        
        uint64_t count = kernel_count.fetch_add(1) + 1;
        
        // Cast payload to kernel dispatch data
        auto* dispatch_data = static_cast<rocprofiler_callback_tracing_kernel_dispatch_data_t*>(record.payload);
        
        if (!dispatch_data) {
            printf("[Kernel Trace #%lu] <no dispatch data>\n", count);
            return;
        }
        
        const auto& info = dispatch_data->dispatch_info;
        
        // Look up kernel name
        std::string kernel_name = "<unknown>";
        auto it = kernel_names.find(info.kernel_id);
        if (it != kernel_names.end()) {
            kernel_name = it->second;
        }
        
        printf("\n[Kernel Trace #%lu]\n", count);
        printf("  Kernel Name: %s\n", kernel_name.c_str());
        printf("  Thread ID: %lu\n", record.thread_id);
        printf("  Correlation ID: %lu\n", record.correlation_id.internal);
        printf("  Kernel ID: %lu\n", info.kernel_id);
        printf("  Dispatch ID: %lu\n", info.dispatch_id);
        printf("  Grid Size: [%u, %u, %u]\n", 
               info.grid_size.x, info.grid_size.y, info.grid_size.z);
        printf("  Workgroup Size: [%u, %u, %u]\n",
               info.workgroup_size.x, info.workgroup_size.y, info.workgroup_size.z);
        printf("  Private Segment Size: %u bytes (scratch memory per work-item)\n",
               info.private_segment_size);
        printf("  Group Segment Size: %u bytes (LDS memory per work-group)\n",
               info.group_segment_size);
    }
    else if (record.kind == ROCPROFILER_CALLBACK_TRACING_KERNEL_DISPATCH &&
             record.phase == ROCPROFILER_CALLBACK_PHASE_EXIT) {
        
        // Display timestamps on exit
        auto* dispatch_data = static_cast<rocprofiler_callback_tracing_kernel_dispatch_data_t*>(record.payload);
        
        if (dispatch_data && dispatch_data->end_timestamp > 0) {
            uint64_t duration_ns = dispatch_data->end_timestamp - dispatch_data->start_timestamp;
            double duration_us = duration_ns / 1000.0;
            
            printf("  Start Timestamp: %lu ns\n", dispatch_data->start_timestamp);
            printf("  End Timestamp: %lu ns\n", dispatch_data->end_timestamp);
            printf("  Duration: %.3f μs\n", duration_us);
        }
    }
}

// Setup buffer tracing (for timeline mode)
int setup_buffer_tracing() {
    printf("[Kernel Tracer] Setting up buffer tracing for timeline mode...\n");
    
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
    printf("[Kernel Tracer] Setting up callback tracing...\n");
    
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

// Tool initialization callback

int tool_init(rocprofiler_client_finalize_t fini_func,
              void* tool_data) {
    (void) fini_func;
    (void) tool_data;
    
    printf("[Kernel Tracer] Initializing profiler tool...\n");
    
    // Check if timeline mode is enabled (from rpv3_options)
    timeline_enabled = (rpv3_timeline_enabled != 0);
    
    if (timeline_enabled) {
        printf("[Kernel Tracer] Timeline mode enabled\n");
        // Capture baseline timestamp when tracer starts
        rocprofiler_get_timestamp(&tracer_start_timestamp);
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
    
    printf("[Kernel Tracer] Profiler initialized successfully\n");
    
    return 0;
}

// Tool finalization callback
void tool_fini(void* tool_data) {
    (void) tool_data;
    
    printf("\n[Kernel Tracer] Finalizing profiler tool...\n");
    
    // Flush buffer if in timeline mode
    if (timeline_enabled && trace_buffer.handle != 0) {
        rocprofiler_flush_buffer(trace_buffer);
    }
    
    printf("[Kernel Tracer] Total kernels traced: %lu\n", kernel_count.load());
    printf("[Kernel Tracer] Unique kernel symbols tracked: %zu\n", kernel_names.size());
    
    // Stop context if still active
    if (client_ctx.handle != 0) {
        rocprofiler_stop_context(client_ctx);
    }
    
    // Destroy buffer if created
    if (timeline_enabled && trace_buffer.handle != 0) {
        rocprofiler_destroy_buffer(trace_buffer);
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
        
        printf("[Kernel Tracer] Configuring profiler v%u.%u.%u [%s] (priority: %u)\n",
               major, minor, patch, runtime_version, priority);
        
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
