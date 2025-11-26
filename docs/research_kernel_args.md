# Research Report: Accessing Kernel Argument Values

## Objective
Investigate alternatives to find the actual parameters passed for kernel arguments, not just the type signature from the kernel name.

## Findings

### 1. Callback Tracing with HIP Runtime API
The most viable method to access kernel argument values is by tracing the HIP Runtime API, specifically the `hipLaunchKernel` family of functions.

**Key Mechanism:**
- Enable `ROCPROFILER_CALLBACK_TRACING_HIP_RUNTIME_API`.
- In the callback, check for `ROCPROFILER_HIP_RUNTIME_API_ID_hipLaunchKernel` (and variants like `hipLaunchCooperativeKernel`).
- Access the `args` member of the data structure.

**Data Structure:**
The `hipLaunchKernel` arguments are exposed in `rocprofiler-sdk/hip/api_args.h`:
```c
struct {
    const void*        function_address;
    rocprofiler_dim3_t numBlocks;
    rocprofiler_dim3_t dimBlocks;
    void**             args;            // <--- Array of pointers to arguments
    size_t             sharedMemBytes;
    hipStream_t        stream;
} hipLaunchKernel;
```

**Argument Iteration:**
The `rocprofiler-sdk` provides a helper API to iterate over arguments in a generic way, which handles indirection and type formatting:
`rocprofiler_iterate_callback_tracing_kind_operation_args`

### 2. Implementation Approach

To implement this, we would need to:
1.  **Configure Tracing**: Add `ROCPROFILER_CALLBACK_TRACING_HIP_RUNTIME_API` to the configured tracing services.
2.  **Callback Handler**: Implement a callback that handles HIP API events.
3.  **Filter**: Inside the callback, filter for `hipLaunchKernel` operations.
4.  **Extraction**: Use `rocprofiler_iterate_callback_tracing_kind_operation_args` to extract and format the arguments.

### 3. Limitations & Considerations
-   **Performance Overhead**: Tracing every HIP API call (or even just kernel launches) with argument inspection will have higher overhead than just tracing kernel dispatches.
-   **Argument Types**: The `args` array gives raw pointers. The `iterate` API helps, but for complex custom types, we might only get raw bytes or addresses unless RTTI/debug info is available (which the SDK tries to leverage).
-   **Correlation**: To map these arguments to the actual kernel execution on the GPU, we need to correlate the HIP API callback (CPU timeline) with the Kernel Dispatch callback (GPU timeline) using the correlation ID.

## Recommendation
Implement a "detailed mode" or specific option (e.g., `--trace-args`) that enables HIP API tracing specifically for `hipLaunchKernel` to capture and print argument values. This should be separate from the default kernel tracing to avoid unnecessary overhead when not needed.
