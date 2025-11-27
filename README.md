# ROCm Profiler Kernel Tracer

A sample shared library that demonstrates using the **rocprofv3** (ROCm Profiler SDK) library to trace and instrument ROCm kernel dispatches. This tool intercepts kernel launches and extracts detailed information including kernel names, grid sizes, and workgroup dimensions.

## Table of Contents

- [Overview](#overview)
- [Getting Started](#getting-started)
  - [Prerequisites](#prerequisites)
  - [Building](#building)
  - [Quick Start](#quick-start)
- [Features](#features)
  - [Configuration Options](#configuration-options)
  - [Timeline Support](#timeline-support)
  - [CSV Output Support](#csv-output-support)
  - [Counter Collection](#counter-collection)
- [Example Output](#example-output)
  - [Basic Output](#basic-output)
  - [Timeline Mode](#timeline-mode-output)
  - [CSV Output](#csv-output-example)
  - [Counter Collection Output](#counter-collection-output)
- [Testing](#testing)
- [Advanced Topics](#advanced-topics)
  - [How It Works](#how-it-works)
  - [Implementation Details](#implementation-details)
  - [Using with Your Own Applications](#using-with-your-own-applications)
- [Troubleshooting](#troubleshooting)
- [Project Structure](#project-structure)
- [License](#license)

---

## Overview

This project consists of:
- **`libkernel_tracer.so`** - A profiler plugin library (C++ implementation)
- **`libkernel_tracer_c.so`** - A profiler plugin library (C implementation)
- **`example_app`** - A sample HIP application with multiple kernels to demonstrate tracing

Both profiler libraries provide identical functionality - use whichever fits your project's language requirements.

---

## Getting Started

### Prerequisites

- ROCm installed (tested with ROCm 5.x+)
- ROCm Profiler SDK (`rocprofiler-sdk`)
- HIP runtime
- CMake 3.16+
- C++17 compatible compiler

### Building

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

### Quick Start

To trace kernel calls, use `LD_PRELOAD` to load the profiler library before running your ROCm application:

```bash
# Basic usage
LD_PRELOAD=./libkernel_tracer.so ./example_app

# With timeline mode (GPU timestamps)
RPV3_OPTIONS="--timeline" LD_PRELOAD=./libkernel_tracer.so ./example_app

# With CSV output
RPV3_OPTIONS="--csv" LD_PRELOAD=./libkernel_tracer.so ./example_app

# With counter collection
RPV3_OPTIONS="--counter mixed" LD_PRELOAD=./libkernel_tracer.so ./example_app
```

---

## Features

### Configuration Options

The profiler supports configuration via the `RPV3_OPTIONS` environment variable. Options are space-separated.

**Available Options:**

- `--version` - Print version information and exit
- `--help` or `-h` - Print help message and exit
- `--timeline` - Enable timeline mode with GPU timestamps
- `--csv` - Enable CSV output mode for machine-readable data export
- `--counter <group>` - Enable counter collection. Groups: `compute`, `memory`, `mixed`

**Examples:**

```bash
# Print version
RPV3_OPTIONS="--version" LD_PRELOAD=./libkernel_tracer.so ./example_app

# Timeline mode
RPV3_OPTIONS="--timeline" LD_PRELOAD=./libkernel_tracer.so ./example_app

# CSV output
RPV3_OPTIONS="--csv" LD_PRELOAD=./libkernel_tracer.so ./example_app

# CSV with timeline (accurate timestamps)
RPV3_OPTIONS="--csv --timeline" LD_PRELOAD=./libkernel_tracer.so ./example_app

# Counter collection
RPV3_OPTIONS="--counter compute" LD_PRELOAD=./libkernel_tracer.so ./example_app
```

### Timeline Support

Timeline mode provides accurate GPU timestamps for kernel execution analysis.

**Features:**
- ✅ Accurate GPU timestamps (start, end)
- ✅ Kernel execution duration measurements
- ✅ Timeline analysis capabilities
- ⚠️ Asynchronous buffered output (traces appear after completion)

**Metrics Provided:**
- **Start Timestamp**: GPU kernel start time in nanoseconds
- **End Timestamp**: GPU kernel end time in nanoseconds
- **Duration**: Kernel execution time in microseconds (μs)
- **Time Since Start**: Elapsed time since profiler initialization in milliseconds (ms)

**Usage:**
```bash
RPV3_OPTIONS="--timeline" LD_PRELOAD=./libkernel_tracer.so ./example_app
```

### CSV Output Support

Export kernel execution data in CSV format for analysis in spreadsheet applications, data processing pipelines, and visualization tools.

**CSV Format (18 columns):**
```
KernelName,ThreadID,CorrelationID,KernelID,DispatchID,GridX,GridY,GridZ,WorkgroupX,WorkgroupY,WorkgroupZ,PrivateSeg,GroupSeg,StartTimestamp,EndTimestamp,DurationNs,DurationUs,TimeSinceStartMs
```

**Features:**
- Clean output (suppresses human-readable text)
- Quoted kernel names (handles commas in C++ function signatures)
- Standard CSV format (compatible with all parsers)
- Works with both C++ and C implementations

**Usage:**
```bash
# Basic CSV output
RPV3_OPTIONS="--csv" LD_PRELOAD=./libkernel_tracer.so ./example_app

# CSV with accurate timestamps
RPV3_OPTIONS="--csv --timeline" LD_PRELOAD=./libkernel_tracer.so ./example_app

# Save to file
RPV3_OPTIONS="--csv" LD_PRELOAD=./libkernel_tracer.so ./example_app > kernels.csv

# View as formatted table
RPV3_OPTIONS="--csv" LD_PRELOAD=./libkernel_tracer.so ./example_app | column -t -s,
```

**Parsing Examples:**
```bash
# Python pandas
import pandas as pd
df = pd.read_csv('kernels.csv')

# Import into Excel, Google Sheets, LibreOffice Calc, etc.
```

### Counter Collection

Collect per-kernel performance counters to diagnose performance bottlenecks (compute-bound vs. memory-bound).

**Counter Groups:**

| Group | Description | Counters Collected |
|-------|-------------|-------------------|
| `compute` | Compute-related metrics | `SQ_INSTS_VALU`, `SQ_WAVES`, `SQ_INSTS_SALU` |
| `memory` | Memory subsystem metrics | `TCC_EA_RDREQ_sum`, `TCC_EA_WRREQ_sum`, `TCC_EA_RDREQ_32B_sum`, `TCC_EA_RDREQ_64B_sum`, `TCP_TCC_WRITE_REQ_sum` |
| `mixed` | Both compute and memory | All of the above |

**Usage:**
```bash
RPV3_OPTIONS="--counter mixed" LD_PRELOAD=./libkernel_tracer.so ./example_app
```

**Note**: Counter collection requires hardware support. If the GPU does not support the requested counters, the feature will be gracefully disabled with a warning.

---

## Example Output

### Basic Output

C++ version with demangled kernel names:

```
[Kernel Tracer] Configuring profiler v1.3.0 [1.3.0] (priority: 0)
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

[Kernel Trace #2]
  Kernel Name: vectorMul(float const*, float const*, float*, int)
  ...

[Kernel Trace #3]
  Kernel Name: matrixTranspose(float const*, float*, int, int)
  ...

All kernels completed successfully!
[Kernel Tracer] Total kernels traced: 3
```

**Note**: The C version produces similar output but with mangled kernel names (e.g., `_Z9vectorAddPKfS0_Pfi.kd`).

### Timeline Mode Output

With `--timeline` option, includes GPU timestamps:

```
[RPV3] Timeline mode enabled
[Kernel Tracer] Configuring profiler v1.3.0 [1.3.0] (priority: 0)
...
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
```

**Note**: In timeline mode, kernel traces appear after all kernels complete (buffered output) rather than in real-time.

### CSV Output Example

With `--csv` option:

```csv
KernelName,ThreadID,CorrelationID,KernelID,DispatchID,GridX,GridY,GridZ,WorkgroupX,WorkgroupY,WorkgroupZ,PrivateSeg,GroupSeg,StartTimestamp,EndTimestamp,DurationNs,DurationUs,TimeSinceStartMs
"vectorAdd(float const*, float const*, float*, int)",5908,1,18,1,1048576,1,1,256,1,1,0,0,0,0,0,0.000,0.000
"vectorMul(float const*, float const*, float*, int)",5908,2,17,2,1048576,1,1,256,1,1,0,0,0,0,0,0.000,0.000
"matrixTranspose(float const*, float*, int, int)",5908,3,16,3,512,512,1,16,16,1,0,0,0,0,0,0.000,0.000
```

**Note**: Kernel names are quoted to handle commas in C++ function signatures.

### CSV with Timeline Example

With `--csv --timeline` options (includes accurate GPU timestamps):

```csv
KernelName,ThreadID,CorrelationID,KernelID,DispatchID,GridX,GridY,GridZ,WorkgroupX,WorkgroupY,WorkgroupZ,PrivateSeg,GroupSeg,StartTimestamp,EndTimestamp,DurationNs,DurationUs,TimeSinceStartMs
"vectorAdd(float const*, float const*, float*, int)",6215,1,18,1,1048576,1,1,256,1,1,0,0,961951699264,961951727998,28734,28.734,215.234
"vectorMul(float const*, float const*, float*, int)",6215,2,17,2,1048576,1,1,256,1,1,0,0,961951944508,961951971920,27412,27.412,216.244
"matrixTranspose(float const*, float*, int, int)",6215,3,16,3,512,512,1,16,16,1,0,0,961952375267,961952417026,41759,41.759,216.675
```

**Note**: Timeline mode populates timestamp columns with actual GPU timing data (nanosecond precision).

### Counter Collection Output

With `--counter mixed` option:

```
[Counters] Dispatch ID: 1, Value: 1048576.000000
[Counters] Dispatch ID: 1, Value: 256.000000
...
```

---

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
  - Counter collection tests
  - CSV output tests

- **Regression Tests** - Backward compatibility
  - Output format stability
  - Version string format
  - No unexpected breaking changes

See [`tests/README.md`](tests/README.md) for detailed testing documentation.

---

## Advanced Topics

### How It Works

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

**Example Application** (`example_app.cpp`):
A simple HIP application that launches three different kernels (vector addition, vector multiplication, matrix transpose) to demonstrate the profiler's capabilities.

### Implementation Details

**Profiler Architecture:**
- Uses the **rocprofiler SDK callback tracing API** with dual callbacks:
  - `ROCPROFILER_CALLBACK_TRACING_CODE_OBJECT` - Captures kernel symbol registrations
  - `ROCPROFILER_CALLBACK_TRACING_KERNEL_DISPATCH` - Intercepts kernel launches
- Extracts kernel names from code object load events and stores them in a lookup table
- C++ version uses `std::unordered_map` and `abi::__cxa_demangle` for demangled kernel names
- C version uses a fixed-size array (256 entries) with linear search for kernel name storage
- Captures detailed dispatch information from `rocprofiler_callback_tracing_kernel_dispatch_data_t`
- Thread-safe kernel counting using atomic operations
- Minimal performance overhead

**Options Parsing Module:**
- **`rpv3_options.c`** - Shared implementation compiled once and linked into both libraries
- **`rpv3_options.h`** - Header with function declarations and constants
- Parses the `RPV3_OPTIONS` environment variable for configuration
- Pure C implementation ensures compatibility with both C and C++ libraries

**Timeline Implementation:**
- Uses `rocprofiler_configure_buffer_tracing_service()` instead of callback tracing
- Creates an 8KB buffer with 87.5% watermark for efficient batching
- Kernel names are still obtained via code object callbacks
- Buffer is flushed during finalization to ensure all records are processed

### Using with Your Own Applications

To trace kernels in your own ROCm/HIP applications:

```bash
LD_PRELOAD=/path/to/libkernel_tracer.so ./your_application
```

The profiler will automatically intercept and trace all kernel dispatches in your application.

---

## Troubleshooting

### Build Issues

#### CMake Error: "Could not find a package configuration file provided by rocprofiler-sdk"

This is the most common build error and occurs when CMake cannot locate the ROCm Profiler SDK.

**Solution 1: Install rocprofiler-sdk (Recommended)**

If you have ROCm installed but are missing the profiler SDK development package:

```bash
# On Ubuntu/Debian-based systems
sudo apt-get install rocprofiler-sdk-dev

# Or install the full ROCm development suite
sudo apt-get install rocm-dev
```

**Solution 2: Set CMAKE_PREFIX_PATH (Most Common)**

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

**Solution 3: Set rocprofiler-sdk_DIR Directly**

Point CMake directly to the config files:

```bash
cmake -Drocprofiler-sdk_DIR=/opt/rocm/lib/cmake/rocprofiler-sdk -B build
cmake --build build
```

**Verify ROCm Installation:**

Check if the ROCm Profiler SDK is installed:

```bash
# Check for the config files
ls /opt/rocm/lib/cmake/rocprofiler-sdk/

# Verify ROCm installation
rocminfo
```

If the directory doesn't exist, you need to install the `rocprofiler-sdk-dev` package.

### Runtime Issues

#### Counter Collection Not Available

If you see warnings about counter collection not being available:

1. **Check Hardware Support**: Counter collection requires specific GPU hardware support
2. **Verify ROCm Version**: Ensure you're using ROCm 5.x or later
3. **Check System Requirements**: Run `utils/check_requirements.sh` to diagnose issues
4. **Use Diagnostic Tools**: Run `utils/diagnose_counters` to list available counters

The profiler will gracefully fall back to callback tracing if counter collection is unavailable.

#### Library Not Loading

If the profiler library fails to load:

```bash
# Verify the library exists
ls -la libkernel_tracer.so

# Check library dependencies
ldd libkernel_tracer.so

# Ensure ROCm libraries are in the path
export LD_LIBRARY_PATH=/opt/rocm/lib:$LD_LIBRARY_PATH
```

---

## Project Structure

```
rpv3/
├── kernel_tracer.cpp          # C++ profiler plugin implementation
├── kernel_tracer.c            # C profiler plugin implementation
├── rpv3_options.c             # Options parsing implementation (shared)
├── rpv3_options.h             # Options parsing header
├── example_app.cpp            # Sample HIP application for testing
├── docs/                      # Documentation
│   ├── counter_collection_research.md  # Performance counter collection research
│   └── research_kernel_args.md         # Kernel argument access research
├── tests/                     # Test suite
│   ├── test_rpv3_options.c    # Unit tests for options parser
│   ├── test_integration.sh    # Integration tests
│   ├── test_regression.sh     # Regression tests
│   ├── test_counters.sh       # Counter collection tests
│   ├── test_csv_output.sh     # CSV output tests
│   ├── run_tests.sh           # Master test runner
│   └── README.md              # Testing documentation
├── utils/                     # Utility tools
│   ├── diagnose_counters.cpp  # Tool to list supported counters
│   ├── check_status.cpp       # Tool to decode status codes
│   ├── check_requirements.sh  # Tool to check system requirements
│   └── README.md              # Utilities documentation
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

---

## License

This is a sample/demonstration project for educational purposes.

---

## Acknowledgments

This project was developed with assistance from **Google Antigravity**, an AI-powered coding assistant.
