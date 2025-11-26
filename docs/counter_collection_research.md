# ROCprofiler-SDK Counter Collection Research

## Objective
Investigate rocprofiler-sdk alternatives for adding per-kernel performance counter measurements to diagnose compute-bound vs memory-bound kernels.

## Summary
The rocprofiler-sdk provides a **Counter Collection Service** that enables per-kernel hardware performance counter collection through **Dispatch Counting** mode. This is the primary mechanism for collecting detailed performance metrics to determine if kernels are compute-bound or memory-bound.

---

## Key Approaches

### 1. Dispatch Counting Mode (Recommended for Per-Kernel Analysis)

**Purpose**: Collect hardware performance counters for individual kernel launches, allowing granular per-kernel analysis.

**How It Works**:
- When a kernel is dispatched, a callback is triggered before the kernel is enqueued
- The tool can specify which counters to collect by providing a profile configuration
- After kernel execution completes, another callback delivers the collected counter data
- Kernels typically execute serially to ensure accurate counter isolation

**Key API Components**:
```cpp
// 1. Configure the dispatch counting service
rocprofiler_configure_buffered_dispatch_counting_service(
    ctx,                    // Context ID
    buff,                   // Buffer ID for results
    dispatch_callback,      // Callback to select counters
    user_data              // Optional user data
);

// 2. Dispatch callback - called before kernel launch
void dispatch_callback(
    rocprofiler_dispatch_counting_service_data_t dispatch_data,
    rocprofiler_counter_config_id_t* config,
    rocprofiler_user_data_t* user_data,
    void* callback_data_args
) {
    // dispatch_data contains kernel name and info
    // Set *config to the profile you want to use
    // If no profile is set, no counters are collected
}

// 3. Record callback - called after kernel completes
void record_callback(
    rocprofiler_context_id_t context,
    rocprofiler_buffer_id_t buffer_id,
    rocprofiler_record_header_t** headers,
    size_t num_headers,
    void* user_data,
    uint64_t drop_count
) {
    // Process collected counter records
}
```

**Setup Steps**:
1. Create a context and buffer in `tool_init()`
2. Create profile configurations with desired counters (per-agent)
3. Configure the dispatch counting service
4. In the dispatch callback, return the appropriate profile for each kernel
5. Process counter data in the record callback

**Advantages**:
- ✅ Per-kernel granularity
- ✅ Precise counter attribution to specific kernel launches
- ✅ Can selectively profile specific kernels

**Limitations**:
- ⚠️ Requires kernel serialization (one kernel at a time)
- ⚠️ Can cause deadlocks if kernels are co-dependent
- ⚠️ Hardware limits on number of counters per run (may require multiple passes)

---

### 2. Device Counting Mode (Alternative for Device-Level Analysis)

**Purpose**: Collect counters at the device level over a time range, not tied to specific kernel launches.

**How It Works**:
- Counters are collected for all activity on the device during a specified time window
- Useful for general device-level performance monitoring
- Does not provide per-kernel breakdown

**Key API**:
```cpp
rocprofiler_configure_device_counting_service(
    ctx,
    buff,
    agent_id,
    set_profile_callback,
    user_data
);
```

**Use Cases**:
- Device-level performance overview
- Situations where kernel serialization would cause deadlocks
- Aggregate performance metrics across multiple kernels

---

## Performance Counters for Compute vs Memory Bound Analysis

### Key Metrics to Collect

#### Compute Metrics:
- **`SQ_INSTS_VALU`** - Vector arithmetic instructions
- **MFMA counters** - Matrix fused multiply-add operations
- **Wavefront statistics** - Wavefront occupancy and execution
- **Instruction mix counters** - Breakdown of instruction types

#### Memory Metrics:
- **L1/L2 Cache counters** - Cache hit/miss rates, access patterns
- **HBM counters** - High Bandwidth Memory traffic
  - `TCC_EA_RDREQ_sum` - HBM read requests
  - `TCC_EA_WRREQ_sum` - HBM write requests
- **`FETCH_SIZE`** / **`WRITE_SIZE`** - Bytes transferred
- **LDS counters** - Local Data Share usage

### Arithmetic Intensity Calculation

**Arithmetic Intensity (AI)** = FLOPs / Bytes Transferred

- **Low AI** → Memory-bound (performance limited by memory bandwidth)
- **High AI** → Compute-bound (performance limited by compute throughput)

### Roofline Analysis

The **ROCm Compute Profiler** (formerly Omniperf) automates this analysis:
- Plots achieved performance vs arithmetic intensity
- Compares against hardware rooflines (peak compute, peak memory bandwidth)
- Identifies bottlenecks visually

**Command-line tool**:
```bash
rocprof-compute profile -n my_profile --roof-only -- ./my_app
rocprof-compute analyze -n my_profile
```

---

## Profile Configuration

**Profiles** are agent-specific configurations that specify which counters to collect.

### Creating a Profile:
```cpp
rocprofiler_counter_id_t counter_ids[] = {
    // List of counter IDs to collect
    // Must be valid for the specific agent/GPU
};

rocprofiler_profile_config_id_t profile;
rocprofiler_create_profile_config(
    agent_id,
    counter_ids,
    num_counters,
    &profile
);
```

### Profile Characteristics:
- **Agent-specific** - Only valid for the GPU it was created for
- **Immutable** - Cannot be modified after creation
- **Reusable** - Can be used multiple times on the same agent
- **Counter limits** - Hardware limits number of counters per pass

### Querying Available Counters:
```cpp
// Iterate through available counters for an agent
rocprofiler_iterate_agent_supported_counters(
    agent_id,
    counter_info_callback,
    user_data
);
```

---

## Integration with Current Implementation

### Current RPV3 Implementation:
Your current `kernel_tracer.cpp` uses:
- **Callback tracing** for real-time kernel dispatch events
- **Buffer tracing** (timeline mode) for GPU timestamps
- **Code object callbacks** for kernel name mapping

### Adding Counter Collection:

**Option 1: Extend Timeline Mode**
- Add dispatch counting service alongside buffer tracing
- Collect both timestamps AND performance counters
- Requires switching from callback to buffered dispatch counting

**Option 2: New Counter Mode**
- Add a new `--counters` option to RPV3_OPTIONS
- Enable dispatch counting service when specified
- Allow user to specify which counters to collect

**Option 3: Hybrid Approach**
- Use existing buffer tracing for timestamps
- Add separate dispatch counting for performance counters
- Correlate data using correlation IDs

---

## Code Examples and Resources

### Official Documentation:
- **Counter Collection Services**: https://rocm.docs.amd.com/projects/rocprofiler-sdk/en/latest/api-reference/counter_collection_services.html
- **ROCprofiler-SDK API**: https://rocm.docs.amd.com/projects/rocprofiler-sdk/en/latest/api-reference/rocprofiler-sdk_api_reference.html

### Sample Code Locations:

**Local installation** (AVAILABLE ON YOUR SYSTEM):
- **Path**: `/opt/rocm/share/rocprofiler-sdk/samples/counter_collection/`
- **Available samples**:
  - `buffered_client.cpp` - Dispatch counting with buffered callback (18KB, comprehensive example)
  - `callback_client.cpp` - Dispatch counting with direct callback (10KB)
  - `device_counting_async_client.cpp` - Asynchronous device-level counting (14KB)
  - `device_counting_sync_client.cpp` - Synchronous device-level counting (19KB)
  - `print_functional_counters_client.cpp` - Validates all available counters (17KB)
  - `main.cpp` - Example application launcher
  - `README.md` - Documentation for the samples

**Key sample features**:
- Demonstrates profile creation for counter collection
- Shows buffered vs callback approaches
- Example: Collects `SQ_WAVES` counter for all kernel dispatches
- Uses `ROCPROFILER_BUFFER_CATEGORY_COUNTERS` enum
- Complete working examples with CMakeLists.txt

**GitHub repositories**:
- `ROCm/rocm-systems` (rocprofiler-sdk moved here)
- `ROCm/rocm-examples` - Additional examples

### Command-line Tool:
```bash
# List available counters
rocprofv3 --list-counters

# Collect specific counters
rocprofv3 --pmc GRBM_COUNT,GRBM_GUI_ACTIVE -- ./my_app

# Use ROCm Compute Profiler for automated analysis
rocprof-compute profile --roof-only -- ./my_app
```

---

## Recommendations

### For Compute vs Memory Bound Diagnosis:

1. **Quick Analysis**: Use `rocprof-compute` command-line tool
   - Automated roofline analysis
   - Pre-configured counter sets
   - Visual output with bottleneck identification

2. **Custom Integration**: Implement dispatch counting in RPV3
   - Add `--counters` option to specify counter list
   - Create profiles for common analysis patterns:
     - Compute-bound profile (VALU, MFMA counters)
     - Memory-bound profile (HBM, cache counters)
     - Balanced profile (mix of both)
   - Output arithmetic intensity alongside kernel info

3. **Hybrid Approach**: Combine timeline + counters
   - Use existing timeline mode for timing data
   - Add dispatch counting for performance counters
   - Correlate using correlation IDs
   - Provide comprehensive per-kernel performance report

### Implementation Considerations:

- **Hardware limits**: May need multiple kernel runs to collect all counters
- **Serialization**: Dispatch counting requires serial execution (performance impact)
- **Agent-specific**: Profiles must be created per-GPU
- **Counter availability**: Varies by GPU architecture (MI200, MI300, etc.)

---

## Next Steps

1. **Explore samples**: Check `/opt/rocm/share/rocprofiler-sdk/samples` for working examples
2. **Test counter availability**: Use `rocprofv3 --list-counters` to see what's available on your GPU
3. **Prototype integration**: Add basic dispatch counting to RPV3 with a small counter set
4. **Define counter profiles**: Create meaningful counter groups for compute/memory analysis
5. **Validate results**: Compare with `rocprof-compute` output for accuracy

