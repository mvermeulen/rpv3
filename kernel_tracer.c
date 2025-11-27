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
#include <rocprofiler-sdk/buffer.h>
#include <rocprofiler-sdk/buffer_tracing.h>

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

/* Timeline mode state */
static int timeline_enabled = 0;
static uint64_t tracer_start_timestamp = 0;  /* Baseline timestamp when tracer starts */
static rocprofiler_buffer_id_t trace_buffer = {0};

/* CSV output mode state */
static int csv_enabled = 0;

/* Counter collection state */
static rpv3_counter_mode_t counter_mode = RPV3_COUNTER_MODE_NONE;
static rocprofiler_buffer_id_t counter_buffer = {0};

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
            printf("KernelName,ThreadID,CorrelationID,KernelID,DispatchID,GridX,GridY,GridZ,WorkgroupX,WorkgroupY,WorkgroupZ,PrivateSeg,GroupSeg,StartTimestamp,EndTimestamp,DurationNs,DurationUs,TimeSinceStartMs\n");
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
                printf("\"%s\",%lu,%lu,%lu,%lu,%u,%u,%u,%u,%u,%u,%u,%u,%lu,%lu,%lu,%.3f,%.3f\n",
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
                       (unsigned long)duration_ns,
                       duration_us,
                       time_since_start_ms);
            } else {
                /* Human-readable output */
                printf("\n[Kernel Trace #%lu]\n", (unsigned long)count);
                printf("  Kernel Name: %s\n", kernel_name);
                printf("  Thread ID: %lu\n", (unsigned long)record->thread_id);
                printf("  Correlation ID: %lu\n", (unsigned long)record->correlation_id.internal);
                printf("  Kernel ID: %lu\n", (unsigned long)record->dispatch_info.kernel_id);
                printf("  Dispatch ID: %lu\n", (unsigned long)record->dispatch_info.dispatch_id);
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
                
                /* Timeline information (only in buffer mode) */
                printf("  Start Timestamp: %lu ns\n", (unsigned long)start_ns);
                printf("  End Timestamp: %lu ns\n", (unsigned long)end_ns);
                printf("  Duration: %.3f μs\n", duration_us);
                printf("  Time Since Start: %.3f ms\n", time_since_start_ms);
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
            printf("KernelName,ThreadID,CorrelationID,KernelID,DispatchID,GridX,GridY,GridZ,WorkgroupX,WorkgroupY,WorkgroupZ,PrivateSeg,GroupSeg,StartTimestamp,EndTimestamp,DurationNs,DurationUs,TimeSinceStartMs\n");
            csv_header_printed = 1;
        }
    }
    
    /* Only process kernel dispatch events on entry */
    if (record.kind == ROCPROFILER_CALLBACK_TRACING_KERNEL_DISPATCH &&
        record.phase == ROCPROFILER_CALLBACK_PHASE_ENTER) {
        
        /* In CSV mode, suppress ENTER phase output */
        if (csv_enabled) {
            return;
        }
        
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
        
        /* Cast payload to kernel dispatch data */
        rocprofiler_callback_tracing_kernel_dispatch_data_t* dispatch_data = 
            (rocprofiler_callback_tracing_kernel_dispatch_data_t*)record.payload;
        
        if (!dispatch_data) {
            return;
        }
        
        if (csv_enabled) {
            /* CSV mode: output complete line on EXIT */
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
            
            printf("\"%s\",%lu,%lu,%lu,%lu,%u,%u,%u,%u,%u,%u,%u,%u,%lu,%lu,%lu,%.3f,%.3f\n",
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
            /* Standard mode: display timestamps on exit */
            if (dispatch_data->end_timestamp > 0) {
                uint64_t duration_ns = dispatch_data->end_timestamp - dispatch_data->start_timestamp;
                double duration_us = duration_ns / 1000.0;
                
                printf("  Start Timestamp: %lu ns\n", (unsigned long)dispatch_data->start_timestamp);
                printf("  End Timestamp: %lu ns\n", (unsigned long)dispatch_data->end_timestamp);
                printf("  Duration: %.3f μs\n", duration_us);
            }
        }
    }
}

/* Setup buffer tracing (for timeline mode) */
int setup_buffer_tracing() {
    printf("[Kernel Tracer] Setting up buffer tracing for timeline mode...\n");
    
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
    printf("[Kernel Tracer] Setting up callback tracing...\n");
    
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
            
            printf("[Counters] Dispatch ID: %lu, Value: %f\n",
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
    
    printf("[Kernel Tracer] Creating profile for agent. Targets: %d, Supported: %zu\n", 
           num_targets, supported_counters.count);
           
    for (int i = 0; i < num_targets; i++) {
        int found = 0;
        for (size_t j = 0; j < supported_counters.count; j++) {
            if (strcmp(target_names[i], supported_counters.counters[j].name) == 0) {
                selected_counters[selected_count++] = supported_counters.counters[j].id;
                printf("  + Added counter: %s\n", target_names[i]);
                found = 1;
                break;
            }
        }
        if (!found) {
            printf("  - Counter not found: %s\n", target_names[i]);
        }
    }
    
    if (selected_count == 0) {
        printf("[Kernel Tracer] Warning: No matching counters found for this agent\n");
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
        printf("[Kernel Tracer] Profile created successfully with %zu counters\n", selected_count);
    } else {
        fprintf(stderr, "[Kernel Tracer] Failed to create profile config: %d\n", status);
    }
}

/* Setup counter collection */
int setup_counter_collection() {
    printf("[Kernel Tracer] Setting up counter collection...\n");
    
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
        printf("[Kernel Tracer] No GPU agents found for counter collection\n");
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
        printf("[Kernel Tracer] Warning: No agents support counter collection or no counters found. Counter collection disabled.\n");
        printf("[Kernel Tracer] Falling back to callback tracing mode...\n");
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
    
    printf("[Kernel Tracer] Counter collection configured successfully\n");
    return 0;
}

/* Tool initialization callback */

int tool_init(rocprofiler_client_finalize_t fini_func,
              void* tool_data) {
    (void) fini_func;
    (void) tool_data;
    
    printf("[Kernel Tracer] Initializing profiler tool...\n");
    
    /* Check if timeline mode is enabled (from rpv3_options) */
    timeline_enabled = (rpv3_timeline_enabled != 0);
    
    /* Check if CSV mode is enabled (from rpv3_options) */
    csv_enabled = (rpv3_csv_enabled != 0);
    
    /* Check if counter mode is enabled */
    counter_mode = rpv3_counter_mode;
    
    if (timeline_enabled) {
        printf("[Kernel Tracer] Timeline mode enabled\n");
        /* Capture baseline timestamp when tracer starts */
        rocprofiler_get_timestamp(&tracer_start_timestamp);
    } else if (csv_enabled) {
        /* CSV mode needs start timestamp even without timeline */
        rocprofiler_get_timestamp(&tracer_start_timestamp);
    }
    
    if (counter_mode != RPV3_COUNTER_MODE_NONE) {
        printf("[Kernel Tracer] Counter collection enabled (mode: %d)\n", counter_mode);
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
    
    printf("[Kernel Tracer] Profiler initialized successfully\n");
    
    return 0;
}

/* Tool finalization callback */
void tool_fini(void* tool_data) {
    (void) tool_data;
    
    printf("\n[Kernel Tracer] Finalizing profiler tool...\n");
    
    /* Flush buffer if in timeline mode */
    if (timeline_enabled && trace_buffer.handle != 0) {
        rocprofiler_flush_buffer(trace_buffer);
    }
    
    printf("[Kernel Tracer] Total kernels traced: %lu\n", 
           (unsigned long)atomic_load(&kernel_count));
    printf("[Kernel Tracer] Unique kernel symbols tracked: %d\n",
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

