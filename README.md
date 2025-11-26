# ROCm Profiler Kernel Tracer

A sample shared library that demonstrates using the **rocprofv3** (ROCm Profiler SDK) library to trace and instrument ROCm kernel dispatches. This tool intercepts kernel launches and extracts detailed information including kernel names, grid sizes, and workgroup dimensions.

## Overview

This project consists of:
- **`libkernel_tracer.so`** - A profiler plugin library (C++ implementation)
- **`libkernel_tracer_c.so`** - A profiler plugin library (C implementation)
- **`example_app`** - A sample HIP application with multiple kernels to demonstrate tracing

Both profiler libraries provide identical functionality - use whichever fits your project's language requirements.

## Project Structure

```
rpv3/
├── kernel_tracer.cpp          # C++ profiler plugin implementation
├── kernel_tracer.c            # C profiler plugin implementation
├── rpv3_options.c             # Options parsing implementation (shared)
├── rpv3_options.h             # Options parsing header
├── example_app.cpp            # Sample HIP application for testing
├── docs/                      # Documentation
│   └── counter_collection_research.md  # Performance counter collection research
│   └── [Kernel Argument Access Research](docs/research_kernel_args.md)
├── tests/                     # Test suite
│   ├── test_rpv3_options.c    # Unit tests for options parser
│   ├── test_integration.sh    # Integration tests
│   ├── test_regression.sh     # Regression tests
│   ├── run_tests.sh           # Master test runner
│   └── README.md              # Testing documentation
├── Makefile                   # Make-based build system
├── CMakeLists.txt             # CMake-based build system
└── README.md                  # This file
```

### Code Sharing Between C and C++ Libraries

The options parsing functionality (`rpv3_options.c`) is compiled once as an object file and linked into both the C++ and C profiler libraries. This approach:
- Avoids code duplication
- Ensures consistent behavior across both implementations
- Follows standard C/C++ project structure practices
- Reduces binary size by sharing compiled code

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

## Testing

The project includes a comprehensive test suite to ensure continued functionality.

### Running Tests

```bash
# Run all tests (unit, integration, and regression)
make test

# Or using CMake/CTest
cd build
ctest --output-on-failure

# Run specific test suites
make test-unit          # Unit tests only
make test-integration   # Integration tests only
make test-regression    # Regression tests only
```

### Test Coverage

The test suite includes:

- **Unit Tests** - Test the options parser (`rpv3_options.c`)
  - Environment variable parsing
  - Option handling (`--version`, `--help`, etc.)
  - Edge cases and error handling

- **Integration Tests** - End-to-end testing
  - Library loading verification
  - Kernel tracing with example application
  - Output format validation
  - C vs C++ implementation comparison

- **Regression Tests** - Backward compatibility
  - Output format stability
  - Version string format
  - No unexpected breaking changes

See [`tests/README.md`](tests/README.md) for detailed testing documentation.

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
- `--timeline` - Enable timeline mode with GPU timestamps (uses buffer tracing)

### Examples

```bash
# Print version information
RPV3_OPTIONS="--version" LD_PRELOAD=./libkernel_tracer.so ./example_app

# Print help message
RPV3_OPTIONS="--help" LD_PRELOAD=./libkernel_tracer.so ./example_app

# Enable timeline mode with GPU timestamps
RPV3_OPTIONS="--timeline" LD_PRELOAD=./libkernel_tracer.so ./example_app

# Multiple options can be combined (space-separated)
RPV3_OPTIONS="--version --help" LD_PRELOAD=./libkernel_tracer.so ./example_app
```

### Timeline Mode

When `--timeline` is enabled, the profiler switches from callback tracing to buffer tracing mode, which provides:

- **GPU Timestamps**: Accurate kernel start and end times in nanoseconds
- **Duration**: Kernel execution time in microseconds
- **Time Since Start**: Elapsed time since profiler initialization in milliseconds (includes setup overhead)

**Note**: Timeline mode uses buffered output, so kernel traces appear after kernels complete rather than in real-time.

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

### Timeline Mode (With `--timeline` Option)

When timeline mode is enabled, the output includes GPU timestamps:

```
[RPV3] Timeline mode enabled
[Kernel Tracer] Configuring profiler v1.0.0 [1.0.0] (priority: 0)
[Kernel Tracer] Initializing profiler tool...
[Kernel Tracer] Timeline mode enabled
[Kernel Tracer] Setting up buffer tracing for timeline mode...
[Kernel Tracer] Profiler initialized successfully
=== ROCm Kernel Tracing Example ===

Using device: AMD Radeon Graphics

Launching vector addition kernel...
Launching vector multiplication kernel...
Launching matrix transpose kernel...

All kernels completed successfully!
Results verification:
  Vector Addition: PASS
  Vector Multiplication: PASS
  Matrix Transpose: PASS

[Kernel Tracer] Finalizing profiler tool...

[Kernel Trace #1]
  Kernel Name: vectorAdd(float const*, float const*, float*, int)
  Thread ID: 6215
  Correlation ID: 1
  Kernel ID: 18
  Dispatch ID: 1
  Grid Size: [1048576, 1, 1]
  Workgroup Size: [256, 1, 1]
  Private Segment Size: 0 bytes (scratch memory per work-item)
  Group Segment Size: 0 bytes (LDS memory per work-group)
  Start Timestamp: 961951699264 ns
  End Timestamp: 961951727998 ns
  Duration: 28.734 μs
  Time Since Start: 215.234 ms

[Kernel Trace #2]
  Kernel Name: vectorMul(float const*, float const*, float*, int)
  ...
  Duration: 27.412 μs
  Time Since Start: 216.244 ms

[Kernel Trace #3]
  Kernel Name: matrixTranspose(float const*, float*, int, int)
  ...
  Duration: 41.759 μs
  Time Since Start: 216.675 ms

[Kernel Tracer] Total kernels traced: 3
[Kernel Tracer] Unique kernel symbols tracked: 18
```

**Note**: In timeline mode, kernel traces appear after all kernels complete (buffered output) rather than in real-time.

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

### Profiler Architecture

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

### Options Parsing Module

- **`rpv3_options.c`** - Shared implementation compiled once and linked into both libraries
- **`rpv3_options.h`** - Header with function declarations and constants
- Parses the `RPV3_OPTIONS` environment variable for configuration
- Supports `--version`, `--help`, and future extensibility
- Pure C implementation ensures compatibility with both C and C++ libraries

## Timeline Support

### Implementation

The ROCm Profiler SDK provides two tracing modes, and RPV3 supports both:

1. **Callback Tracing** (default mode)
   - ✅ Real-time synchronous callbacks
   - ✅ Immediate output as kernels are dispatched
   - ✅ Simple implementation
   - ❌ No kernel execution timestamps

2. **Buffer Tracing** (enabled with `--timeline`)
   - ✅ Accurate GPU timestamps (start, end)
   - ✅ Kernel execution duration measurements
   - ✅ Timeline analysis capabilities
   - ⚠️ Asynchronous buffered output (traces appear after completion)

### Usage

Enable timeline mode by setting the `--timeline` option:

```bash
RPV3_OPTIONS="--timeline" LD_PRELOAD=./libkernel_tracer.so ./example_app
```

### Timeline Output

Timeline mode provides the following additional metrics:

- **Start Timestamp**: GPU kernel start time in nanoseconds
- **End Timestamp**: GPU kernel end time in nanoseconds
- **Duration**: Kernel execution time in microseconds (μs)
- **Time Since Start**: Elapsed time since first kernel in milliseconds (ms)

### Implementation Details

When timeline mode is enabled:
- The profiler uses `rocprofiler_configure_buffer_tracing_service()` instead of callback tracing
- Creates an 8KB buffer with 87.5% watermark for efficient batching
- Kernel names are still obtained via code object callbacks
- Buffer is flushed during finalization to ensure all records are processed

Both C++ and C implementations provide identical timeline functionality. The only difference is kernel name formatting (demangled in C++, mangled in C).

## License

This is a sample/demonstration project for educational purposes.
