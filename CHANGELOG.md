# Changelog

All notable changes to the RPV3 Kernel Tracer project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.1] - 2025-11-25

### Added
- Environment variable-based options system via `RPV3_OPTIONS`
- `--version` option to print version information and exit
- `--help` / `-h` option to display usage information
- Shared header file `rpv3_options.h` for code sharing between C and C++ implementations
- Build target for C version in Makefile

### Changed
- Updated Makefile to build both C and C++ versions by default
- Updated Makefile clean target to remove both library versions
- Enhanced README with RPV3_OPTIONS configuration documentation

### Fixed
- C compatibility issue with `strdup()` by adding `_POSIX_C_SOURCE` macro

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
