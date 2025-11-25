/* MIT License
 * Sample ROCm Profiler SDK kernel tracer - C implementation (Enhanced)
 * Demonstrates using rocprofiler-sdk to trace kernel dispatches with detailed information
 */

/* Enable POSIX features for strdup */
#define _POSIX_C_SOURCE 200809L

/* Include HIP headers first to satisfy RCCL dependencies in rocprofiler headers */
#define __HIP_PLATFORM_AMD__
#include <hip/hip_runtime_api.h>

#include <rocprofiler-sdk/registration.h>
#include <rocprofiler-sdk/rocprofiler.h>
#include <rocprofiler-sdk/callback_tracing.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

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

/* Callback function for kernel dispatch events */
void kernel_dispatch_callback(rocprofiler_callback_tracing_record_t record,
                              rocprofiler_user_data_t* user_data,
                              void* callback_data) {
    (void) user_data;
    (void) callback_data;
    
    /* Only process kernel dispatch events on entry */
    if (record.kind == ROCPROFILER_CALLBACK_TRACING_KERNEL_DISPATCH &&
        record.phase == ROCPROFILER_CALLBACK_PHASE_ENTER) {
        
        uint64_t count = atomic_fetch_add(&kernel_count, 1) + 1;
        
        /* Cast payload to kernel dispatch data */
        rocprofiler_callback_tracing_kernel_dispatch_data_t* dispatch_data = 
            (rocprofiler_callback_tracing_kernel_dispatch_data_t*)record.payload;
        
        if (!dispatch_data) {
            printf("[Kernel Trace #%lu] <no dispatch data>\n", (unsigned long)count);
            return;
        }
        
        rocprofiler_kernel_dispatch_info_t info = dispatch_data->dispatch_info;
        
        /* Look up kernel name */
        const char* kernel_name = lookup_kernel_name(info.kernel_id);
        
        printf("\n[Kernel Trace #%lu]\n", (unsigned long)count);
        printf("  Kernel Name: %s\n", kernel_name);
        printf("  Thread ID: %lu\n", (unsigned long)record.thread_id);
        printf("  Correlation ID: %lu\n", (unsigned long)record.correlation_id.internal);
        printf("  Kernel ID: %lu\n", (unsigned long)info.kernel_id);
        printf("  Dispatch ID: %lu\n", (unsigned long)info.dispatch_id);
        printf("  Grid Size: [%u, %u, %u]\n", 
               info.grid_size.x, 
               info.grid_size.y, 
               info.grid_size.z);
        printf("  Workgroup Size: [%u, %u, %u]\n",
               info.workgroup_size.x,
               info.workgroup_size.y,
               info.workgroup_size.z);
        printf("  Private Segment Size: %u bytes (scratch memory per work-item)\n",
               info.private_segment_size);
        printf("  Group Segment Size: %u bytes (LDS memory per work-group)\n",
               info.group_segment_size);
    }
    else if (record.kind == ROCPROFILER_CALLBACK_TRACING_KERNEL_DISPATCH &&
             record.phase == ROCPROFILER_CALLBACK_PHASE_EXIT) {
        
        /* Display timestamps on exit */
        rocprofiler_callback_tracing_kernel_dispatch_data_t* dispatch_data = 
            (rocprofiler_callback_tracing_kernel_dispatch_data_t*)record.payload;
        
        if (dispatch_data && dispatch_data->end_timestamp > 0) {
            uint64_t duration_ns = dispatch_data->end_timestamp - dispatch_data->start_timestamp;
            double duration_us = duration_ns / 1000.0;
            
            printf("  Start Timestamp: %lu ns\n", (unsigned long)dispatch_data->start_timestamp);
            printf("  End Timestamp: %lu ns\n", (unsigned long)dispatch_data->end_timestamp);
            printf("  Duration: %.3f μs\n", duration_us);
        }
    }
}

/* Tool initialization callback */
int tool_init(rocprofiler_client_finalize_t fini_func,
              void* tool_data) {
    (void) fini_func;
    (void) tool_data;
    
    printf("[Kernel Tracer] Initializing profiler tool...\n");
    
    /* Initialize kernel table */
    memset(kernel_table, 0, sizeof(kernel_table));
    
    /* Create a context for profiling */
    if (rocprofiler_create_context(&client_ctx) != ROCPROFILER_STATUS_SUCCESS) {
        fprintf(stderr, "[Kernel Tracer] Failed to create context\n");
        return -1;
    }
    
    /* Configure callback tracing for code object/kernel symbol registration */
    if (rocprofiler_configure_callback_tracing_service(
            client_ctx,
            ROCPROFILER_CALLBACK_TRACING_CODE_OBJECT,
            NULL,  /* operations (NULL = all) */
            0,     /* operations count */
            kernel_symbol_callback,
            NULL   /* callback data */
        ) != ROCPROFILER_STATUS_SUCCESS) {
        fprintf(stderr, "[Kernel Tracer] Failed to configure code object callback tracing\n");
        return -1;
    }
    
    /* Configure callback tracing for kernel dispatches */
    if (rocprofiler_configure_callback_tracing_service(
            client_ctx,
            ROCPROFILER_CALLBACK_TRACING_KERNEL_DISPATCH,
            NULL,  /* operations (NULL = all) */
            0,     /* operations count */
            kernel_dispatch_callback,
            NULL   /* callback data */
        ) != ROCPROFILER_STATUS_SUCCESS) {
        fprintf(stderr, "[Kernel Tracer] Failed to configure kernel dispatch callback tracing\n");
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
    
    printf("[Kernel Tracer] Profiler initialized successfully\n");
    
    return 0;
}

/* Tool finalization callback */
void tool_fini(void* tool_data) {
    (void) tool_data;
    
    printf("\n[Kernel Tracer] Finalizing profiler tool...\n");
    printf("[Kernel Tracer] Total kernels traced: %lu\n", 
           (unsigned long)atomic_load(&kernel_count));
    printf("[Kernel Tracer] Unique kernel symbols tracked: %d\n",
           atomic_load(&kernel_table_size));
    
    /* Stop context if still active */
    if (client_ctx.handle != 0) {
        rocprofiler_stop_context(client_ctx);
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
    
    printf("[Kernel Tracer] Configuring profiler v%u.%u.%u [%s] (priority: %u)\n",
           major, minor, patch, runtime_version, priority);
    
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

