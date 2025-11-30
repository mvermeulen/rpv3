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

**ALWAYS** run relevant tests after code changes. Use this decision tree:

### Quick Test Selection Guide

| Change Type | Required Tests | Command |
|-------------|---------------|---------|
| Options parser (`rpv3_options.c/h`) | Unit tests | `tests/run_unit_tests.sh` |
| Core tracer logic | Integration + Parity | `tests/test_integration.sh && tests/test_parity.sh` |
| RocBLAS integration | RocBLAS tests | `tests/test_rocblas_*.sh` |
| CSV output | CSV tests | `tests/test_csv_output.sh && tests/test_csv_summary.py` |
| Counter collection | Counter tests | `tests/test_counters.sh` |
| Timeline mode | Timeline tests | `tests/test_timeline_flag.c` |
| Backtrace feature | Backtrace tests | `tests/test_backtrace.sh` |
| Any code change | **ALL TESTS** | `cd tests && ./run_tests.sh` |

### Test Suite Overview

1. **Unit Tests**: `tests/run_unit_tests.sh`
   - Tests `rpv3_options.c` parser logic
   - Fast execution (~1 second)
   - Run after modifying options parsing
   
2. **Integration Tests**: `tests/test_integration.sh`
   - End-to-end tracing with `example_app`
   - Tests all major features
   - Run after modifying core tracer logic
   
3. **Parity Tests**: `tests/test_parity.sh`
   - Verifies C and C++ implementations match
   - **CRITICAL**: Must pass before any commit
   - Run after modifying either implementation
   
4. **Specialized Tests**:
   - **RocBLAS**: `tests/test_rocblas_*.sh`
     - Requires RocBLAS library
     - Tests named pipe and file-based logging
   - **CSV**: `tests/test_csv_output.sh`, `tests/test_csv_summary.py`
     - Validates CSV format and parsing
   - **Counters**: `tests/test_counters.sh`
     - May gracefully fail on unsupported hardware
   - **Timeline**: `tests/test_timeline_flag.c`
     - Tests GPU timestamp functionality
   - **Backtrace**: `tests/test_backtrace.sh`
     - Tests call stack capture and library resolution
   - **README Examples**: `tests/test_readme_examples.py`
     - Validates examples in documentation

### Running All Tests

```bash
# Recommended: Run from tests directory
cd tests && ./run_tests.sh

# Alternative: Run from project root
make test

# CMake/CTest integration
cd build && ctest --output-on-failure
```

### Test Failure Debugging

If tests fail:

1. **Check test output** for specific failure messages
2. **Run individual test** to isolate the issue:
   ```bash
   cd tests
   bash -x test_integration.sh  # Debug mode
   ```
3. **Verify build** is up-to-date:
   ```bash
   make clean && make
   ```
4. **Check environment**:
   ```bash
   # Verify ROCm installation
   rocminfo | grep "Name:"
   
   # Check library paths
   ldd ./libkernel_tracer.so
   
   # Verify GPU access
   rocminfo | grep "gfx"
   ```
5. **Check for stale artifacts**:
   ```bash
   # Clean all build artifacts
   make clean
   rm -rf build
   rm -f tests/*.o tests/test_rpv3_options
   ```

### Test Coverage Goals

- **Unit tests**: 100% coverage of options parser
- **Integration tests**: All major features exercised
- **Parity tests**: Identical output from C and C++ versions
- **Regression tests**: No performance degradation


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

### When Adding New Files

#### Source Files (`.c`, `.cpp`, `.h`)

1. **Determine file type and location**:
   - Core tracer code: Root directory (`kernel_tracer.cpp`, `kernel_tracer.c`)
   - Options/shared code: Root directory (`rpv3_options.c/h`)
   - Example applications: Root directory (`example_*.cpp`)
   - Utilities: `utils/` directory
   - Documentation: `docs/` directory
   - Tests: `tests/` directory

2. **Add to build system**:
   
   **CMakeLists.txt**:
   ```cmake
   # For new library source
   add_library(new_lib SHARED new_file.cpp)
   target_link_libraries(new_lib PRIVATE rocprofiler-sdk::rocprofiler-sdk)
   
   # For new executable
   add_executable(new_tool new_tool.cpp)
   target_link_libraries(new_tool PRIVATE hip::host)
   
   # For new utility
   # Add to utils/CMakeLists.txt if it exists, or create entry
   ```
   
   **Makefile**:
   ```makefile
   # Add to appropriate target (all, utils, tests, etc.)
   new_tool: new_tool.cpp
       $(HIPCC) -o new_tool new_tool.cpp $(CXXFLAGS) $(LDFLAGS)
   
   # Add to clean target
   clean:
       rm -f new_tool ...
   ```

3. **Add file header**:
   ```c
   /*
    * MIT License
    * 
    * Copyright (c) 2024 RPV3 Kernel Tracer
    * 
    * Brief description of file purpose
    */
   ```

4. **Update `.gitignore` if needed**:
   - Add build artifacts (executables, `.o` files)
   - Do NOT ignore source files (`.c`, `.cpp`, `.h`)
   - Example:
     ```
     # If adding new_tool executable
     new_tool
     ```

5. **Update documentation**:
   - Add to "File Organization" section in `README.md`
   - Add to "Project Structure" in `copilot-instructions.md` if significant
   - Document purpose and usage

#### Test Files

1. **Naming convention**:
   - Shell scripts: `tests/test_*.sh`
   - Python tests: `tests/test_*.py`
   - C/C++ tests: `tests/test_*.c` or `tests/test_*.cpp`

2. **Add to test suite**:
   - Update `tests/run_tests.sh` to include new test
   - Add to `tests/CMakeLists.txt` for CTest integration:
     ```cmake
     add_test(NAME test_new_feature COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test_new_feature.sh)
     ```

3. **Make executable**:
   ```bash
   chmod +x tests/test_new_feature.sh
   ```

4. **Follow test conventions**:
   - Use `tests/test_utils.sh` for common functions
   - Return 0 for success, non-zero for failure
   - Print clear pass/fail messages

#### Utility Files (`utils/`)

1. **Add to utils directory**:
   - Diagnostic tools: `check_*.cpp`, `diagnose_*.cpp`
   - Analysis tools: `*.py`, `*.sh`

2. **Update `utils/README.md`**:
   - Document tool purpose
   - Provide usage examples
   - List dependencies

3. **Add to build system**:
   - Update `Makefile` utils target
   - Add to `utils/CMakeLists.txt` if using CMake

4. **Add to clean target**:
   ```makefile
   clean:
       cd utils && rm -f new_util
   ```

#### Documentation Files (`.md`)

1. **Location**:
   - Root documentation: `README.md`, `CHANGELOG.md`, `TODO.md`, `LICENSE`
   - Research/design docs: `docs/` directory
   - Release docs: `RELEASE_CHECKLIST.md`, `RELEASE_NOTES_*.md`

2. **Update cross-references**:
   - Add links in `README.md` if user-facing
   - Reference in `TODO.md` if planning document
   - Update `.github/copilot-instructions.md` if procedural

3. **Follow markdown conventions**:
   - Use proper heading hierarchy
   - Include code examples with language tags
   - Add "Last Updated" date at bottom

#### Configuration Files

1. **Build configuration**:
   - `CMakeLists.txt`: CMake build
   - `Makefile`: Make build
   - `.github/workflows/*.yml`: CI/CD (if added)

2. **Git configuration**:
   - `.gitignore`: Ignore patterns
   - `.gitattributes`: Git attributes (if needed)

3. **IDE configuration**:
   - `.vscode/`: VS Code settings (add to `.gitignore`)
   - `.idea/`: IntelliJ settings (add to `.gitignore`)

#### Checklist for Adding Any File

- [ ] File is in correct directory
- [ ] File has proper header/license comment
- [ ] Added to build system (CMakeLists.txt and/or Makefile)
- [ ] Added to `.gitignore` if build artifact
- [ ] NOT in `.gitignore` if source file
- [ ] Executable permissions set if script (`chmod +x`)
- [ ] Documentation updated (README, copilot-instructions, etc.)
- [ ] Tests added if applicable
- [ ] Cross-references updated
- [ ] File follows project conventions (C11/C++17, style guide)
- [ ] Verified build succeeds: `make clean && make`
- [ ] Verified tests pass: `cd tests && ./run_tests.sh`
- [ ] Update CHANGELOG.md if user-facing
- [ ] Update TODO.md if part of planned work

#### Common Mistakes to Avoid

❌ **Don't**: Add build artifacts (`.o`, `.so`, executables) to git
✅ **Do**: Add them to `.gitignore`

❌ **Don't**: Forget to update both build systems (CMake AND Makefile)
✅ **Do**: Test both: `make clean && make` and `rm -rf build && cmake -B build && cmake --build build`

❌ **Don't**: Add files without documentation
✅ **Do**: Update README.md or add comments explaining purpose

❌ **Don't**: Add test files without integrating into test suite
✅ **Do**: Update `tests/run_tests.sh` and `tests/CMakeLists.txt`

❌ **Don't**: Use inconsistent naming conventions
✅ **Do**: Follow existing patterns (`test_*.sh`, `example_*.cpp`, etc.)

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
   - Ensure library was built: `ls -lh libkernel_tracer*.so`

2. **RocBLAS pipe hangs**:
   - Ensure pipe reader starts BEFORE writer
   - Use background process: `command &`
   - Check non-blocking mode implementation
   - Verify pipe exists: `ls -l /tmp/rocblas.log`

3. **Counters unavailable**:
   - Run `utils/diagnose_counters` to check support
   - Some counters hardware-specific (e.g., MI210)
   - Check kernel version: `uname -r` (may need 6.8+)
   - Verify group membership: `groups` (need render/video)

4. **Build failures**:
   - Check ROCm paths: `CMAKE_PREFIX_PATH=/opt/rocm`
   - Verify compiler versions (C++17, C11 support)
   - Ensure rocprofiler-sdk installed: `ls /opt/rocm/include/rocprofiler-sdk`
   - Check for missing dependencies: `ldd libkernel_tracer.so`

5. **Parity test failures**:
   - Ensure both implementations updated identically
   - Check for platform-specific differences (printf formatting, etc.)
   - Verify atomic operations match (`std::atomic` vs `atomic_uint_fast64_t`)
   - Compare outputs manually: `diff <(LD_PRELOAD=./libkernel_tracer.so ./example_app) <(LD_PRELOAD=./libkernel_tracer_c.so ./example_app)`

6. **Test suite failures**:
   - Run `make clean` before rebuilding
   - Check for stale build artifacts: `rm -rf build tests/*.o`
   - Verify ROCm environment variables set correctly
   - Ensure GPU is accessible: `rocminfo | grep gfx`
   - Check test permissions: `chmod +x tests/*.sh`

7. **Version mismatch errors**:
   - Run version consistency check (see Version Bump Procedures)
   - Rebuild after version changes: `make clean && make`
   - Check that `--version` output matches expected version
   - Verify all 6 files updated: `grep -r "x.y.z" rpv3_options.h CHANGELOG.md TODO.md CMakeLists.txt .github/copilot-instructions.md`

8. **CSV parsing issues**:
   - Verify CSV format with `head -n 5 trace.csv`
   - Check for quoted kernel names with commas
   - Use `utils/summarize_trace.py` for validation
   - Ensure CSV header is present (first line)

9. **Backtrace not showing libraries**:
   - Ensure libraries are dynamically linked (check with `ldd`)
   - Verify `libunwind` is installed: `ldconfig -p | grep libunwind`
   - Check that symbols are not stripped (`-g` flag)
   - Backtrace incompatible with `--timeline` and `--csv` modes

10. **Timeline mode shows zero duration**:
    - Ensure using buffer tracing (not callback mode)
    - Check that GPU timestamps are available
    - Verify `rocprofiler_get_timestamp()` is working
    - Timeline incompatible with `--backtrace` mode

11. **Permission denied errors**:
    - Check GPU device permissions: `ls -l /dev/kfd /dev/dri/render*`
    - Verify user in correct groups: `groups` (need render, video)
    - May need to log out/in after adding to groups
    - Check SELinux/AppArmor policies if applicable

12. **Segmentation faults**:
    - Run with debug symbols: `make debug`
    - Use gdb: `gdb --args env LD_PRELOAD=./libkernel_tracer.so ./example_app`
    - Check for null pointer dereferences
    - Verify thread safety of shared variables
    - Enable sanitizers: `CXXFLAGS="-fsanitize=address" make`

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
5. `TODO.md`: Update "Current Version" at bottom
6. `.github/copilot-instructions.md`: Update version references (2 locations)

**Versioning scheme** (Semantic Versioning):
- **Major**: Breaking API changes
- **Minor**: New features (backward compatible)
- **Patch**: Bug fixes

## Version Bump Procedures

### Semantic Versioning Guidelines

- **Major (x.0.0)**: Breaking API changes, incompatible modifications
- **Minor (0.x.0)**: New features, backward compatible additions
- **Patch (0.0.x)**: Bug fixes, documentation updates, minor improvements

### Step-by-Step Version Bump Workflow

#### 1. Determine Version Type

Ask yourself:
- Does this break existing functionality? → **Major**
- Does this add new features? → **Minor**
- Is this just a bug fix or documentation update? → **Patch**

#### 2. Update Version Numbers

Update the version in **all** of the following files (in order):

1. **`rpv3_options.h`** (3 locations):
   ```c
   #define RPV3_VERSION "x.y.z"
   #define RPV3_VERSION_MAJOR x
   #define RPV3_VERSION_MINOR y
   #define RPV3_VERSION_PATCH z
   ```

2. **`CMakeLists.txt`**:
   ```cmake
   project(rpv3 VERSION x.y.z LANGUAGES C CXX)
   ```

3. **`.github/copilot-instructions.md`** (this file):
   - Line 336: `Current version: **x.y.z**`
   - Update "Last Updated" date at bottom

4. **`TODO.md`**:
   - Last line: `**Current Version**: x.y.z`

#### 3. Update CHANGELOG.md

Add a new version section at the top (after line 9):

```markdown
## [x.y.z] - YYYY-MM-DD

### Added
- New features and capabilities

### Changed
- Modifications to existing functionality

### Fixed
- Bug fixes and corrections

### Deprecated (if applicable)
- Features marked for removal

### Removed (if applicable)
- Deleted features

### Security (if applicable)
- Security-related changes
```

**Guidelines**:
- Use present tense for headings, past tense for descriptions
- Be specific and link to relevant files/functions
- Include test coverage information
- Mention documentation updates

#### 4. Update TODO.md

1. Move completed items from "High Priority" to a new "Completed" section:
   ```markdown
   ### Version x.y.z (YYYY-MM-DD)
   - [x] Feature that was completed
   - [x] Bug that was fixed
   ```

2. Update the "Last Updated" date at the bottom

#### 5. Run Verification Checks

**CRITICAL**: Before committing version changes, verify:

```bash
# 1. Check version consistency across all files
grep -r "x.y.z" rpv3_options.h CHANGELOG.md TODO.md CMakeLists.txt .github/copilot-instructions.md

# 2. Run full test suite
cd tests && ./run_tests.sh

# 3. Verify both build systems work
make clean && make
rm -rf build && mkdir build && cd build && cmake .. && make

# 4. Test both implementations
LD_PRELOAD=./libkernel_tracer.so ./example_app
LD_PRELOAD=./libkernel_tracer_c.so ./example_app

# 5. Verify --version output
RPV3_OPTIONS="--version" LD_PRELOAD=./libkernel_tracer.so ./example_app
```

#### 6. Commit Version Bump

```bash
# Single commit for version bump
git add rpv3_options.h CMakeLists.txt CHANGELOG.md TODO.md .github/copilot-instructions.md
git commit -m "Bump version to x.y.z"
```

#### 7. For Release Versions (Optional)

If preparing for a GitHub release:

1. **Review RELEASE_CHECKLIST.md** - Ensure all items are complete
2. **Create git tag**:
   ```bash
   git tag -a vx.y.z -m "Release vx.y.z - Brief description"
   git push origin vx.y.z
   ```
3. **Create GitHub release** with notes from CHANGELOG.md

### Common Version Bump Scenarios

#### Scenario 1: Bug Fix (Patch Bump)

**Example**: Fixing RocBLAS pipe blocking issue

```
Current: 1.5.0 → New: 1.5.1
```

**Files to update**: rpv3_options.h, CMakeLists.txt, CHANGELOG.md, TODO.md, copilot-instructions.md

**CHANGELOG entry**:
```markdown
## [1.5.1] - 2024-11-29

### Fixed
- Fixed RocBLAS named pipe blocking issue in timeline mode
- Corrected error handling for empty pipe reads
```

#### Scenario 2: New Feature (Minor Bump)

**Example**: Adding backtrace support

```
Current: 1.4.5 → New: 1.5.0
```

**Files to update**: Same as above + README.md (feature documentation)

**CHANGELOG entry**:
```markdown
## [1.5.0] - 2024-11-28

### Added
- **Backtrace Support**: New `--backtrace` option to capture CPU-side call stacks
  - Full call stack from kernel dispatch to application entry
  - Shared library identification (RocBLAS, hipBLAS, MIOpen, etc.)
  - Function name resolution with C++ demangling
```

#### Scenario 3: Breaking Change (Major Bump)

**Example**: Changing output format or API

```
Current: 1.5.0 → New: 2.0.0
```

**Additional requirements**:
- Update migration guide in README.md
- Add deprecation warnings in previous version
- Document breaking changes prominently in CHANGELOG.md

**CHANGELOG entry**:
```markdown
## [2.0.0] - 2024-12-01

### Changed
- **BREAKING**: CSV output format now includes additional columns
- **BREAKING**: Renamed `--output` to `--output-file` for clarity

### Migration Guide
- Update scripts parsing CSV output to handle new columns
- Replace `--output` with `--output-file` in all invocations
```

### Version Bump Checklist

Use this quick checklist for every version bump:

- [ ] Determine version type (major/minor/patch)
- [ ] Update `rpv3_options.h` (3 locations)
- [ ] Update `CMakeLists.txt`
- [ ] Update `CHANGELOG.md` with dated entry
- [ ] Update `TODO.md` (move completed items)
- [ ] Update `.github/copilot-instructions.md` (2 locations)
- [ ] Run version consistency check (`grep -r "x.y.z" ...`)
- [ ] Run full test suite (`./run_tests.sh`)
- [ ] Test both build systems (Make and CMake)
- [ ] Test both implementations (C++ and C)
- [ ] Verify `--version` output
- [ ] Commit with message: "Bump version to x.y.z"
- [ ] (Optional) Create git tag for releases
- [ ] (Optional) Update RELEASE_CHECKLIST.md status

## Release Workflow

For official releases, follow the comprehensive checklist in `RELEASE_CHECKLIST.md`.

### Quick Release Reference

1. **Pre-Release**: Complete all items in `RELEASE_CHECKLIST.md`
2. **Version Bump**: Follow "Version Bump Procedures" above
3. **Testing**: Run full test suite on clean build
4. **Tagging**: Create annotated git tag
5. **GitHub Release**: Create release with CHANGELOG notes

### Release Checklist Quick Links

- Full checklist: `RELEASE_CHECKLIST.md`
- Version history: `CHANGELOG.md`
- Roadmap: `TODO.md`

### Pre-Release Verification

Before creating a release tag:

```bash
# 1. Verify clean working directory
git status

# 2. Verify version consistency
grep -r "$(grep '#define RPV3_VERSION' rpv3_options.h | cut -d'"' -f2)" \
  rpv3_options.h CHANGELOG.md TODO.md CMakeLists.txt .github/copilot-instructions.md

# 3. Run full test suite
cd tests && ./run_tests.sh

# 4. Test clean build
make clean && make && make test

# 5. Verify both implementations
for lib in libkernel_tracer.so libkernel_tracer_c.so; do
  echo "Testing $lib..."
  RPV3_OPTIONS="--version" LD_PRELOAD=./$lib ./example_app
done
```

### Release Types

#### Patch Release (x.y.Z)
- Bug fixes only
- No new features
- Minimal testing required
- Can be released quickly

#### Minor Release (x.Y.0)
- New features
- Backward compatible
- Full test suite required
- Update documentation

#### Major Release (X.0.0)
- Breaking changes
- Extensive testing required
- Migration guide needed
- Deprecation warnings in previous version


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

## Quick Start for AI Assistants

### Common Task Workflows

#### Task: Add a New Command-Line Option

1. Edit `rpv3_options.h`: Add extern declaration
2. Edit `rpv3_options.c`: Add variable and parsing logic
3. Update `rpv3_print_help()` in `rpv3_options.c`
4. Implement feature in **both** `kernel_tracer.cpp` and `kernel_tracer.c`
5. Add unit test in `tests/test_rpv3_options.c`
6. Add integration test in `tests/test_integration.sh`
7. Update `README.md` with usage example
8. Run parity test: `tests/test_parity.sh`
9. Update `CHANGELOG.md` and `TODO.md`
10. Bump version (see "Version Bump Procedures")

#### Task: Fix a Bug

1. Verify bug exists in both C and C++ implementations
2. Fix in **both** `kernel_tracer.cpp` and `kernel_tracer.c`
3. Add regression test to prevent recurrence
4. Run full test suite: `cd tests && ./run_tests.sh`
5. Update `CHANGELOG.md` under "Fixed" section
6. Bump patch version (see "Version Bump Procedures")

#### Task: Add a New Feature

1. Research implementation approach (create doc in `docs/` if complex)
2. Update `TODO.md` with implementation checklist
3. Implement in **both** C and C++ versions
4. Add comprehensive tests
5. Update `README.md` with feature documentation
6. Run all tests: `cd tests && ./run_tests.sh`
7. Update `CHANGELOG.md` under "Added" section
8. Bump minor version (see "Version Bump Procedures")

#### Task: Prepare for Release

1. Review `RELEASE_CHECKLIST.md`
2. Complete all pending TODO items for this version
3. Run full test suite on clean build
4. Update version numbers (see "Version Bump Procedures")
5. Update `CHANGELOG.md` with release date
6. Create git tag and GitHub release (see "Release Workflow")

#### Task: Add a New File

1. Determine file type and location (see "When Adding New Files")
2. Create file with proper header/license comment
3. Add to build system:
   - Update `CMakeLists.txt` with appropriate target
   - Update `Makefile` with build rules and clean target
4. Update `.gitignore` if file is a build artifact
5. Set executable permissions if script: `chmod +x filename`
6. Update documentation:
   - Add to `README.md` if user-facing
   - Update "File Organization" section
   - Add to `copilot-instructions.md` if significant
7. Add tests if applicable
8. Verify build: `make clean && make`
9. Verify tests: `cd tests && ./run_tests.sh`
10. Update `CHANGELOG.md` if user-facing addition

#### Task: Update Documentation

1. Identify all affected documentation files (README, CHANGELOG, TODO, copilot-instructions)
2. Update examples with current version numbers
3. Verify all code examples are tested (add to `tests/test_readme_examples.py` if needed)
4. Check for broken links or outdated references
5. Update "Last Updated" dates
6. Run documentation validation: `grep -r "TODO\|FIXME" *.md`

### Critical Reminders

- ⚠️ **ALWAYS maintain C/C++ parity** - Update both implementations
- ⚠️ **ALWAYS run tests** - Especially `test_parity.sh`
- ⚠️ **ALWAYS update docs** - README, CHANGELOG, TODO
- ⚠️ **ALWAYS check version consistency** - 6 files must match
- ⚠️ **NEVER auto-run destructive commands** - Always get user approval
- ⚠️ **ALWAYS verify before committing** - Run full test suite

### Quick Reference: Files to Update

| Task Type | Files to Update |
|-----------|----------------|
| New option | `rpv3_options.h`, `rpv3_options.c`, both tracers, tests, README |
| Bug fix | Both tracers, tests, CHANGELOG, version files |
| New feature | Both tracers, tests, README, CHANGELOG, TODO, version files |
| New file | `CMakeLists.txt`, `Makefile`, `.gitignore` (if artifact), README, tests (if applicable) |
| Version bump | `rpv3_options.h`, `CMakeLists.txt`, `CHANGELOG.md`, `TODO.md`, `copilot-instructions.md` |
| Release | All of the above + `RELEASE_CHECKLIST.md` |

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

**Last Updated**: November 29, 2024
**For Questions**: Refer to README.md, TODO.md, and docs/ directory
