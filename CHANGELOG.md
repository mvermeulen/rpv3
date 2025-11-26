# Changelog

All notable changes to the RPV3 Kernel Tracer project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
