# Changelog

All notable changes to the RPV3 Kernel Tracer project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).


## [1.3.0] - 2025-11-26
### Added
- **CSV Output Support**: Machine-readable CSV export for kernel execution data
  - New `--csv` option to enable CSV output mode
  - 18-column CSV format with kernel name, dimensions, memory segments, and timing data
  - Kernel names properly quoted to handle commas in C++ function signatures
  - Compatible with timeline mode (`--csv --timeline`)
  - Integration test suite for CSV validation (`tests/test_csv_output.sh`)
  - Unit test for CSV option parsing

### Changed
- CSV output suppresses human-readable text for clean data export
- Timeline mode now captures start timestamp even when CSV-only mode is enabled
- **Documentation**: Reorganized README.md with improved structure
  - Added table of contents for easy navigation
  - Grouped all example output in dedicated section
  - Consolidated troubleshooting into single comprehensive section
  - Reorganized features for better discoverability
  - Improved quick start guide

## [1.2.3] - 2025-11-26
### Fixed
- Counter collection now properly registers code object callback for kernel symbol tracking.
- Implemented graceful fallback to callback tracing when counter collection is unsupported.
- Added fallback when no agents support counter collection (returns to callback mode).
- Added fallback when dispatch counting service configuration fails.
- Counter collection now handles hardware limitations gracefully without crashes.

### Changed
- Improved error messages for counter collection failures with informative warnings.
- Counter tests now properly validate graceful degradation on unsupported hardware.

## [1.2.2] - 2025-11-26
### Changed
- Improved `utils/check_requirements.sh` with fallback check for `gfx1151` in `counter_defs.yaml` when `rocprofv3 -L` fails to list counters.

## [1.2.1] - 2025-11-26
### Added
- Diagnostic tool `utils/check_requirements.sh` to verify system requirements for performance counters.
- Updated documentation with minimum kernel and driver requirements for RDNA 3.5/gfx1151.

## [1.2.0] - 2025-11-26
### Added
- Counter collection support (`--counter` option) with `compute`, `memory`, and `mixed` groups.
- Dispatch counting service integration for both C and C++ implementations.
- Graceful handling of unsupported hardware for counter collection.
- Diagnostic tools (`check_status`, `diagnose_counters`) in `utils/` directory.
- Integration tests for counter collection (`tests/test_counters.sh`).
- Updated `Makefile` with `utils` target and improved cleanup.

## [1.1.3] - 2025-11-26

### Added
- **Documentation**: Kernel Argument Access Research
  - Detailed research on accessing actual kernel argument values via HIP Runtime API tracing
  - Identification of `hipLaunchKernel` argument structure and iteration APIs
  - Implementation recommendations for future "detailed mode"

## [1.1.2] - 2025-11-26

### Added
- **Documentation**: Counter collection research for future implementation
  - Comprehensive research document on rocprofiler-sdk counter collection service
  - Detailed analysis of dispatch counting mode for per-kernel performance counters
  - Performance counter recommendations for compute vs memory bound diagnosis
  - API examples and integration strategies with current RPV3 implementation
  - Local sample code references (`/opt/rocm/share/rocprofiler-sdk/samples/counter_collection/`)
  - Created `docs/` directory for project documentation
  - Updated README project structure to include docs directory

## [1.1.1] - 2025-11-26

### Changed
- Timeline baseline timestamp now captures tracer initialization time instead of first kernel dispatch
  - "Time Since Start" now shows elapsed time from when profiler starts, not from first kernel
  - Provides more accurate profiling duration including initialization overhead
  - Uses `rocprofiler_get_timestamp()` to capture baseline when timeline mode is enabled
  - First kernel now shows ~200ms+ instead of 0.000ms, revealing initialization time

## [1.1.0] - 2025-11-26


### Added
- **Timeline Support**: Full GPU timestamp functionality via buffer tracing
  - `--timeline` option now functional (previously placeholder)
  - GPU timestamps: kernel start and end times in nanoseconds
  - Duration calculations: kernel execution time in microseconds
  - Time since start tracking: elapsed time from first kernel in milliseconds
  - Dual-mode implementation: callback tracing (default) and buffer tracing (timeline)
- Buffer tracing implementation in both C++ and C versions
  - `timeline_buffer_callback()` for processing batched kernel dispatch records
  - `setup_buffer_tracing()` and `setup_callback_tracing()` helper functions
  - 8KB buffer with 87.5% watermark for efficient batching
  - Kernel name preservation for buffer callback processing
- Timeline integration tests
  - 5 new tests in `test_integration.sh` validating timeline functionality
  - Timestamp validation and duration verification tests
- Comprehensive timeline documentation
  - Updated README with timeline usage examples and output samples
  - Implementation details and buffer tracing architecture
  - Timeline mode section with feature comparison

### Changed
- Updated help message to show `--timeline` as functional
- README "Timeline Support and Limitations" section replaced with "Timeline Support" implementation guide
- Version bumped from 1.0.3 to 1.1.0 (minor version for new feature)

## [1.0.3] - 2025-11-25


### Added
- Comprehensive testing infrastructure
  - Unit tests for options parser (`tests/test_rpv3_options.c`) with 11 test cases
  - Integration tests (`tests/test_integration.sh`) with 10 test scenarios
  - Regression tests (`tests/test_regression.sh`) with 10 test scenarios
  - Test utilities (`tests/test_utils.sh`) with color-coded output and assertion helpers
  - Master test runner (`tests/run_tests.sh`)
- CMake/CTest integration for automated testing
- Makefile test targets: `test`, `test-unit`, `test-integration`, `test-regression`
- Testing documentation in `tests/README.md`
- Testing section in main README with quick start guide
- Test artifacts added to `.gitignore`

### Changed
- Updated project structure in README to include `tests/` directory
- CMakeLists.txt now includes `enable_testing()` and test subdirectory
- Makefile help output now mentions test targets

## [1.0.2] - 2025-11-25

### Added
- `--timeline` option placeholder (prints "not yet implemented" error message)
- Separate `rpv3_options.c` implementation file for options parsing
- Project structure section in README documenting file organization
- Options parsing module documentation in README

### Changed
- **BREAKING**: Refactored options parsing from header-only to compiled object file
  - `rpv3_options.h` now contains only declarations (no inline implementation)
  - `rpv3_options.c` contains the implementation, compiled once and linked into both libraries
  - Updated Makefile to compile `rpv3_options.o` and link into both plugins
  - Updated CMakeLists.txt to create `rpv3_options` object library
- Enhanced help message to include `--timeline` option
- Updated README with code sharing architecture explanation

### Fixed
- Version string in `rpv3_options.h` (was "1.0." now "1.0.2")

## [1.0.1] - 2025-11-25

### Added
- Environment variable-based options system via `RPV3_OPTIONS`
- `--version` option to print version information and exit
- `--help` / `-h` option to display usage information
- Shared header file `rpv3_options.h` for code sharing between C and C++ implementations
- Build target for C version in Makefile

## [1.0.0] - 2025-11-24

### Added
- Initial release of ROCm Profiler Kernel Tracer
- C++ implementation (`kernel_tracer.cpp`) with demangled kernel names
- C implementation (`kernel_tracer.c`) for C-only projects
- Dual-callback architecture for kernel symbol registration and dispatch tracing
- Detailed kernel dispatch information extraction:
  - Kernel names (demangled in C++ version)
  - Grid and workgroup dimensions
  - Memory segment sizes (private/group)
  - Thread ID and correlation ID tracking
- Example HIP application with multiple kernel types
- CMake and Makefile build systems
- Comprehensive README with usage instructions and troubleshooting
