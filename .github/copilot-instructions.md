# Copilot Instructions for RPV3 Kernel Tracer

## Project Overview

**RPV3 (ROCm Profiler v3 Kernel Tracer)** is a profiler plugin for AMD ROCm that intercepts and traces GPU kernel dispatches using the rocprofiler-sdk API. The project provides both C and C++ implementations with identical functionality.

**Key Purpose**: Instrument ROCm kernel launches to extract detailed information including kernel names, grid sizes, workgroup dimensions, performance counters, and integrate with RocBLAS logging for comprehensive GPU workload analysis.

**Version**: 1.5.0
**License**: MIT

## Architecture & Design

### Core Components

1. **Profiler Libraries** (C and C++ implementations):
   - `libkernel_tracer.so` (C++ version from `kernel_tracer.cpp`)
   - `libkernel_tracer_c.so` (C version from `kernel_tracer.c`)
   - Both implement identical functionality using rocprofiler-sdk callbacks
   - Loaded via `LD_PRELOAD` to intercept kernel dispatches

2. **Options Parser** (`rpv3_options.c/h`):
   - Shared C library for parsing `RPV3_OPTIONS` environment variable
   - Used by both C and C++ implementations
   - Handles configuration flags and output settings

3. **Example Applications**:
   - `example_app.cpp`: HIP kernel demonstrations
   - `example_rocblas.cpp`: RocBLAS library tracing demonstrations

4. **Utilities** (`utils/`):
   - `summarize_trace.py`: CSV trace analysis tool
   - `check_status.cpp`: ROCm environment diagnostics
   - `diagnose_counters.cpp`: Counter availability checks

### Key Features

- **Kernel Tracing**: Captures kernel name, grid/block dimensions, timestamps
- **Timeline Mode**: GPU timestamps with begin/end markers (`--timeline`)
- **CSV Output**: Machine-readable format for analysis (`--csv`)
- **Performance Counters**: Collect GPU metrics (`--counter compute|memory|mixed`)
- **RocBLAS Integration**: Correlate kernels with BLAS operations via named pipes
- **Output Redirection**: File or directory-based output (`--output`, `--outputdir`)

## Language Standards & Conventions

### C++ Implementation (`kernel_tracer.cpp`)

- **Standard**: C++17
- **Style**:
  - Use anonymous namespaces for internal linkage: `namespace { ... }`
  - Prefer `std::atomic` for thread-safe counters
  - Use `nullptr` (not `NULL`)
  - Modern C++ idioms (range-based loops, auto, etc.)
  - C++ style comments: `//`
  
### C Implementation (`kernel_tracer.c`)

- **Standard**: C11
- **Style**:
  - Use `static` for file-scope variables
  - Use `<stdatomic.h>` for atomics: `atomic_uint_fast64_t`
  - Use `NULL` for null pointers
  - Explicit casts: `(type)value`
  - C style comments: `/* */`

### Shared Options Parser (`rpv3_options.c/h`)

- **Standard**: C11
- **Header guards**: `#ifndef RPV3_OPTIONS_H`
- **Extern C guards** for C++ compatibility
- **No dependencies** on C++ STL or ROCm headers

## Critical Implementation Details

### 1. Parity Between C and C++ Versions

**CRITICAL**: The C and C++ implementations MUST produce identical output and behavior. When modifying functionality:

- Update BOTH `kernel_tracer.cpp` AND `kernel_tracer.c`
- Use equivalent constructs (e.g., `std::atomic` ↔ `atomic_uint_fast64_t`)
- Run parity tests: `tests/test_parity.sh`
- Maintain identical output formats

### 2. RocBLAS Named Pipe Handling

The RocBLAS integration uses named pipes with special considerations:

```bash
# Single-run workflow (named pipe blocks)
mkfifo /tmp/rocblas.log
LD_PRELOAD=./libkernel_tracer.so RPV3_OPTIONS="--csv --rocblas /tmp/rocblas.log" \
  ROCBLAS_LOG_TRACE_PATH=/tmp/rocblas.log ./example_rocblas &
```

**Key Points**:
- Named pipes block until both reader and writer are ready
- Use **non-blocking** reads with `poll()` to prevent deadlocks
- Set pipes to `O_NONBLOCK` mode
- Drain pipes completely before cleanup
- Intercept `fopen()`/`fdopen()` to disable buffering: `setvbuf(fp, nullptr, _IONBF, 0)`

### 3. Counter Collection

Counter support varies by GPU hardware:

- Use `--counter compute`, `--counter memory`, or `--counter mixed`
- Not all counters available on all GPUs (e.g., MI210 limitations)
- Check availability with `utils/diagnose_counters`
- Gracefully handle unavailable counters

### 4. Timeline Mode

Timeline mode (`--timeline`) enables GPU timestamps:

- Requires buffer-based tracing (not callback-based)
- Outputs "BEGIN" and "END" markers with timestamps
- Essential for accurate duration measurement

### 5. CSV Output Format

CSV mode (`--csv`) produces:
```
Timestamp,Duration,KernelName,GridX,GridY,GridZ,BlockX,BlockY,BlockZ,M,N,K
```

- `M,N,K` values extracted from RocBLAS logs when available
- Use `utils/summarize_trace.py` for analysis

## Build System

### CMake (Recommended)

```bash
mkdir build && cd build
cmake ..
make
```

**Key CMake settings**:
- `CMAKE_CXX_STANDARD=17`, `CMAKE_C_STANDARD=11`
- Find packages: `rocprofiler-sdk`, `hip`, `rocblas` (optional)
- Object library for options: `rpv3_options OBJECT`
- Shared libraries with `-fPIC`

### Makefile (Alternative)

- Uses `hipcc` for utilities
- Manual path configuration: `ROCM_PATH=/opt/rocm`
- Debug build: `make debug`

## Testing Requirements

**ALWAYS** run relevant tests after code changes:

1. **Unit Tests**: `tests/run_unit_tests.sh`
   - Tests `rpv3_options.c` parser logic
   
2. **Integration Tests**: `tests/test_integration.sh`
   - End-to-end tracing with `example_app`
   
3. **Parity Tests**: `tests/test_parity.sh`
   - Verifies C and C++ implementations match
   
4. **Specialized Tests**:
   - RocBLAS: `tests/test_rocblas_*.sh`
   - CSV: `tests/test_csv_output.sh`, `tests/test_csv_summary.py`
   - Counters: `tests/test_counters.sh`
   - Timeline: `tests/test_timeline_flag.c`

**Run all tests**: `cd tests && ./run_tests.sh`

## Code Modification Guidelines

### When Adding New Features

1. **Update version** in `CMakeLists.txt` and `rpv3_options.h`
2. **Add to BOTH implementations** (C and C++)
3. **Update documentation**:
   - `README.md` (features, examples)
   - `CHANGELOG.md` (version entry)
   - `TODO.md` (mark completed items)
4. **Create tests** in `tests/` directory
5. **Update help text** in `rpv3_options.c` (rpv3_print_help)

### When Fixing Bugs

1. **Verify in both implementations** (check if bug exists in C and C++)
2. **Add regression test** to prevent recurrence
3. **Update CHANGELOG.md** with fix description

### When Modifying Options

1. **Edit `rpv3_options.c` and `rpv3_options.h`**
2. **Update help text**: `rpv3_print_help()` function
3. **Add unit tests** in `tests/test_rpv3_options.c`
4. **Document in README.md** under "Configuration Options"

## Common Patterns

### Adding a New Command-Line Option

```c
// In rpv3_options.h
extern int rpv3_new_feature_enabled;

// In rpv3_options.c
int rpv3_new_feature_enabled = 0;

// In rpv3_parse_options():
if (strcmp(token, "--new-feature") == 0) {
    rpv3_new_feature_enabled = 1;
    continue;
}

// Update rpv3_print_help() with description
```

### Accessing ROCm Profiler Data

```cpp
// C++ version
void kernel_dispatch_callback(rocprofiler_callback_tracing_record_t record, 
                               rocprofiler_user_data_t* user_data) {
    auto* data = static_cast<rocprofiler_kernel_dispatch_info_t*>(record.payload);
    // Access: data->dispatch_info.grid_size.x, data->kernel_name, etc.
}

// C version  
void kernel_dispatch_callback(rocprofiler_callback_tracing_record_t record,
                               rocprofiler_user_data_t* user_data) {
    rocprofiler_kernel_dispatch_info_t* data = 
        (rocprofiler_kernel_dispatch_info_t*)record.payload;
    /* Access: data->dispatch_info.grid_size.x, etc. */
}
```

### Thread-Safe Counter Increment

```cpp
// C++
std::atomic<uint64_t> counter{0};
uint64_t count = counter.fetch_add(1) + 1;

// C
atomic_uint_fast64_t counter = ATOMIC_VAR_INIT(0);
uint64_t count = atomic_fetch_add(&counter, 1) + 1;
```

## Documentation Standards

### Code Comments

- **File headers**: Include MIT license, brief description
- **Function comments**: Describe purpose, parameters, return values
- **Complex logic**: Explain why, not just what
- **TODOs**: Use `TODO:` or `FIXME:` with description

### Markdown Documents

- Use proper heading hierarchy
- Include code examples with language tags: ` ```bash`, ` ```cpp`
- Keep README.md current with features
- Update TODO.md to track work status

## Dependencies & Environment

### Required

- **ROCm**: 5.x+ (tested with 5.0+)
- **rocprofiler-sdk**: Core profiling API
- **HIP runtime**: GPU runtime
- **CMake**: 3.16+
- **Compiler**: C++17 (g++, clang++) and C11 (gcc, clang)

### Optional

- **RocBLAS**: For RocBLAS tracing examples
- **Python 3**: For utility scripts

### Environment Variables

- `RPV3_OPTIONS`: Space-separated options (e.g., `"--csv --timeline"`)
- `ROCBLAS_LOG_TRACE_PATH`: RocBLAS log output path
- `ROCBLAS_LAYER`: Enable RocBLAS logging (set to `3`)
- `LD_PRELOAD`: Load profiler library

## Troubleshooting Tips

### Common Issues

1. **No output / Library not loaded**:
   - Check `LD_PRELOAD` path (use absolute or `./` prefix)
   - Verify ROCm installation: `/opt/rocm`

2. **RocBLAS pipe hangs**:
   - Ensure pipe reader starts BEFORE writer
   - Use background process: `command &`
   - Check non-blocking mode implementation

3. **Counters unavailable**:
   - Run `utils/diagnose_counters` to check support
   - Some counters hardware-specific (e.g., MI210)

4. **Build failures**:
   - Check ROCm paths: `CMAKE_PREFIX_PATH=/opt/rocm`
   - Verify compiler versions (C++17, C11 support)

## File Organization

```
rpv3/
├── kernel_tracer.cpp          # C++ profiler implementation
├── kernel_tracer.c            # C profiler implementation  
├── rpv3_options.c/h           # Shared options parser
├── example_app.cpp            # HIP example
├── example_rocblas.cpp        # RocBLAS example
├── CMakeLists.txt             # Build configuration
├── Makefile                   # Alternative build
├── README.md                  # Main documentation
├── TODO.md                    # Task tracking
├── CHANGELOG.md               # Version history
├── tests/                     # Test suite
│   ├── run_tests.sh          # Master test runner
│   ├── test_*.sh             # Shell-based tests
│   └── test_*.py             # Python-based tests
├── utils/                     # Utility tools
│   ├── summarize_trace.py    # CSV analysis
│   ├── check_status.cpp      # Diagnostics
│   └── diagnose_counters.cpp # Counter checks
└── docs/                      # Research & planning docs
```

## Version Management

Current version: **1.5.0**

**Update locations when bumping version**:
1. `CMakeLists.txt`: `project(... VERSION x.y.z)`
2. `rpv3_options.h`: `#define RPV3_VERSION "x.y.z"`
3. `rpv3_options.h`: `RPV3_VERSION_MAJOR/MINOR/PATCH`
4. `CHANGELOG.md`: Add new version section

**Versioning scheme** (Semantic Versioning):
- **Major**: Breaking API changes
- **Minor**: New features (backward compatible)
- **Patch**: Bug fixes

## AI Assistant Guidelines

When helping with this project:

1. **Always maintain parity**: If changing one implementation, change the other
2. **Test before claiming completion**: Suggest running relevant test scripts
3. **Preserve formatting**: Match existing code style (C vs C++ conventions)
4. **Update docs**: Remind to update README, CHANGELOG, TODO as needed
5. **Check compatibility**: Consider ROCm version compatibility
6. **Explain ROCm APIs**: rocprofiler-sdk functions may be unfamiliar
7. **Watch for threading**: Callbacks are multi-threaded, use atomics
8. **Mind the pipes**: RocBLAS pipe handling is tricky, review carefully

## Quick Reference: Common Commands

```bash
# Build
mkdir build && cd build && cmake .. && make

# Basic trace
LD_PRELOAD=./libkernel_tracer.so ./example_app

# Timeline mode
RPV3_OPTIONS="--timeline" LD_PRELOAD=./libkernel_tracer.so ./example_app

# CSV output
RPV3_OPTIONS="--csv" LD_PRELOAD=./libkernel_tracer.so ./example_app > trace.csv

# Counters
RPV3_OPTIONS="--counter mixed" LD_PRELOAD=./libkernel_tracer.so ./example_app

# RocBLAS tracing
mkfifo /tmp/rocblas.log
RPV3_OPTIONS="--csv --rocblas /tmp/rocblas.log" LD_PRELOAD=./libkernel_tracer.so \
  ROCBLAS_LAYER=3 ROCBLAS_LOG_TRACE_PATH=/tmp/rocblas.log ./example_rocblas &

# Run tests
cd tests && ./run_tests.sh

# Analyze CSV
python3 utils/summarize_trace.py trace.csv
```

---

**Last Updated**: November 28, 2025
**For Questions**: Refer to README.md, TODO.md, and docs/ directory
