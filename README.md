# ROCm Profiler Kernel Tracer

A sample shared library that demonstrates using the **rocprofv3** (ROCm Profiler SDK) library to trace and instrument ROCm kernel dispatches. This tool intercepts kernel launches and extracts detailed information including kernel names, grid sizes, and workgroup dimensions.

## Overview

This project consists of:
- **`libkernel_tracer.so`** - A profiler plugin library (C++ implementation)
- **`libkernel_tracer_c.so`** - A profiler plugin library (C implementation)
- **`example_app`** - A sample HIP application with multiple kernels to demonstrate tracing

Both profiler libraries provide identical functionality - use whichever fits your project's language requirements.

## Prerequisites

- ROCm installed (tested with ROCm 5.x+)
- ROCm Profiler SDK (`rocprofiler-sdk`)
- HIP runtime
- CMake 3.16+
- C++17 compatible compiler

## Building

```bash
mkdir build
cd build
cmake ..
make
```

This will produce:
- `libkernel_tracer.so` - The profiler plugin (C++ version)
- `libkernel_tracer_c.so` - The profiler plugin (C version)
- `example_app` - The example application

## Troubleshooting Build Issues

### CMake Error: "Could not find a package configuration file provided by rocprofiler-sdk"

This is the most common build error and occurs when CMake cannot locate the ROCm Profiler SDK.

#### **Solution 1: Install rocprofiler-sdk (Recommended)**

If you have ROCm installed but are missing the profiler SDK development package:

```bash
# On Ubuntu/Debian-based systems
sudo apt-get install rocprofiler-sdk-dev

# Or install the full ROCm development suite
sudo apt-get install rocm-dev
```

#### **Solution 2: Set CMAKE_PREFIX_PATH (Most Common)**

If rocprofiler-sdk is already installed in `/opt/rocm` but CMake can't find it:

```bash
cmake -DCMAKE_PREFIX_PATH=/opt/rocm -B build
cmake --build build
```

Or set it as an environment variable:

```bash
export CMAKE_PREFIX_PATH=/opt/rocm
cmake -B build
cmake --build build
```

#### **Solution 3: Set rocprofiler-sdk_DIR Directly**

Point CMake directly to the config files:

```bash
cmake -Drocprofiler-sdk_DIR=/opt/rocm/lib/cmake/rocprofiler-sdk -B build
cmake --build build
```

#### **Verify ROCm Installation**

Check if the ROCm Profiler SDK is installed:

```bash
# Check for the config files
ls /opt/rocm/lib/cmake/rocprofiler-sdk/

# Verify ROCm installation
rocminfo
```

If the directory doesn't exist, you need to install the `rocprofiler-sdk-dev` package.

## Usage

To trace kernel calls, use `LD_PRELOAD` to load the profiler library before running your ROCm application:

```bash
# From the build directory
LD_PRELOAD=$PWD/libkernel_tracer.so ./example_app
```

Or from the project root:

```bash
LD_PRELOAD=./build/libkernel_tracer.so ./build/example_app
```

## Configuration Options

The profiler supports configuration via the `RPV3_OPTIONS` environment variable. Options are space-separated.

### Available Options

- `--version` - Print version information and exit without initializing the profiler
- `--help` or `-h` - Print help message and exit without initializing the profiler

### Examples

```bash
# Print version information
RPV3_OPTIONS="--version" LD_PRELOAD=./libkernel_tracer.so ./example_app

# Print help message
RPV3_OPTIONS="--help" LD_PRELOAD=./libkernel_tracer.so ./example_app

# Multiple options can be combined (space-separated)
RPV3_OPTIONS="--version --help" LD_PRELOAD=./libkernel_tracer.so ./example_app
```

## Expected Output

When running the example application with the profiler, you should see detailed output like:

### C++ Version (Demangled Kernel Names)

```
[Kernel Tracer] Configuring profiler v1.0.0 [1.0.0] (priority: 0)
[Kernel Tracer] Initializing profiler tool...
[Kernel Tracer] Profiler initialized successfully
=== ROCm Kernel Tracing Example ===

Using device: AMD Radeon Graphics

Launching vector addition kernel...

[Kernel Trace #1]
  Kernel Name: vectorAdd(float const*, float const*, float*, int)
  Thread ID: 6454
  Correlation ID: 1
  Kernel ID: 18
  Dispatch ID: 1
  Grid Size: [1048576, 1, 1]
  Workgroup Size: [256, 1, 1]
  Private Segment Size: 0 bytes (scratch memory per work-item)
  Group Segment Size: 0 bytes (LDS memory per work-group)
Launching vector multiplication kernel...

[Kernel Trace #2]
  Kernel Name: vectorMul(float const*, float const*, float*, int)
  Thread ID: 6454
  Correlation ID: 2
  Kernel ID: 17
  Dispatch ID: 2
  Grid Size: [1048576, 1, 1]
  Workgroup Size: [256, 1, 1]
  Private Segment Size: 0 bytes (scratch memory per work-item)
  Group Segment Size: 0 bytes (LDS memory per work-group)
Launching matrix transpose kernel...

[Kernel Trace #3]
  Kernel Name: matrixTranspose(float const*, float*, int, int)
  Thread ID: 6454
  Correlation ID: 3
  Kernel ID: 16
  Dispatch ID: 3
  Grid Size: [512, 512, 1]
  Workgroup Size: [16, 16, 1]
  Private Segment Size: 0 bytes (scratch memory per work-item)
  Group Segment Size: 0 bytes (LDS memory per work-group)

All kernels completed successfully!
Results verification:
  Vector Addition: PASS
  Vector Multiplication: PASS
  Matrix Transpose: PASS

[Kernel Tracer] Finalizing profiler tool...
[Kernel Tracer] Total kernels traced: 3
[Kernel Tracer] Unique kernel symbols tracked: 0
```

### C Version (Mangled Kernel Names)

The C version produces similar output but with mangled kernel names (e.g., `_Z9vectorAddPKfS0_Pfi.kd` instead of `vectorAdd(...)`). All other information is identical.

## How It Works

### Profiler Plugin (`kernel_tracer.cpp` / `kernel_tracer.c`)

The profiler library implements the rocprofiler SDK plugin interface with a dual-callback architecture:

1. **`rocprofiler_configure()`** - Entry point called by the ROCm runtime when the library is loaded
2. **`tool_init()`** - Initializes the profiler context and registers two callbacks:
   - **Code object callback** - Captures kernel symbol registrations to build kernel_id → kernel_name mappings
   - **Kernel dispatch callback** - Intercepts kernel launches to extract dispatch information
3. **`kernel_symbol_callback()`** - Stores kernel names as they are loaded (C++ version demangles names)
4. **`kernel_dispatch_callback()`** - Called for each kernel dispatch, extracts and prints:
   - Kernel name (looked up from kernel_id)
   - Grid and workgroup dimensions
   - Memory segment sizes (scratch and LDS)
   - Timestamps and duration
5. **`tool_fini()`** - Cleanup and final statistics

### Example Application (`example_app.cpp`)

A simple HIP application that launches three different kernels:
- Vector addition
- Vector multiplication  
- Matrix transpose

This provides a workload to demonstrate the profiler's capabilities.

## Using with Your Own Applications

To trace kernels in your own ROCm/HIP applications:

```bash
LD_PRELOAD=/path/to/libkernel_tracer.so ./your_application
```

The profiler will automatically intercept and trace all kernel dispatches in your application.

## Implementation Details

- Uses the **rocprofiler SDK callback tracing API** with dual callbacks:
  - `ROCPROFILER_CALLBACK_TRACING_CODE_OBJECT` - Captures kernel symbol registrations
  - `ROCPROFILER_CALLBACK_TRACING_KERNEL_DISPATCH` - Intercepts kernel launches
- Extracts kernel names from code object load events and stores them in a lookup table
- C++ version uses `std::unordered_map` and `abi::__cxa_demangle` for demangled kernel names
- C version uses a fixed-size array (256 entries) with linear search for kernel name storage
- Captures detailed dispatch information from `rocprofiler_callback_tracing_kernel_dispatch_data_t`:
  - Grid size and workgroup size (x, y, z dimensions)
  - Private segment size (scratch memory per work-item)
  - Group segment size (LDS memory per work-group)
  - Kernel ID and dispatch ID for correlation
- Thread-safe kernel counting using atomic operations
- Minimal performance overhead

## Timeline Support and Limitations

### Callback Tracing vs Buffer Tracing

The ROCm Profiler SDK provides two tracing modes:

1. **Callback Tracing** (used by this tool)
   - ✅ Real-time synchronous callbacks
   - ✅ Immediate output as kernels are dispatched
   - ✅ Simple implementation
   - ❌ **No kernel execution timestamps**
   - ❌ Cannot measure actual kernel duration

2. **Buffer Tracing** (not currently implemented)
   - ✅ Accurate GPU timestamps (DispatchNs, BeginNs, EndNs, CompleteNs)
   - ✅ Kernel execution duration measurements
   - ✅ Timeline analysis capabilities
   - ❌ Asynchronous buffered output
   - ❌ More complex implementation
   - ❌ Requires buffer management

### Current Limitations

The current implementation uses **callback tracing**, which provides rich dispatch information but **does not include kernel execution timestamps**. The `start_timestamp` and `end_timestamp` fields in `rocprofiler_callback_tracing_kernel_dispatch_data_t` are always `0` in callback tracing mode.

**What you CAN get:**
- Kernel name and dispatch information
- Grid and workgroup dimensions
- Memory segment sizes
- Dispatch order and correlation IDs

**What you CANNOT get:**
- Actual kernel execution start/end times
- Kernel execution duration
- GPU timeline information

### Adding Timeline Support

To add accurate timeline support with kernel execution timestamps, the tracer would need to be redesigned to use **buffer tracing** mode:

```cpp
// Example: Configure buffer tracing instead of callback tracing
rocprofiler_configure_buffer_tracing_service(
    client_ctx,
    ROCPROFILER_BUFFER_TRACING_KERNEL_DISPATCH,
    // ... buffer configuration
);
```

This would enable access to accurate GPU timestamps but would change the output model from real-time to buffered/asynchronous.

## License

This is a sample/demonstration project for educational purposes.
