# TODO List

This document tracks planned features, improvements, and known issues for the RPV3 Kernel Tracer project.

## High Priority

### Features
- [ ] Implement backtrace support for library context tracking
  - [ ] Research backtrace APIs (libunwind, execinfo, etc.)
  - [ ] Capture call stack during kernel dispatch
  - [ ] Identify calling library (RocBLAS, hipBLAS, MIOpen, etc.)
  - [ ] Add library context to trace output (CSV and human-readable)
  - [ ] Enable library-specific filtering and analysis
  - [ ] Support future library-specific integrations

- [ ] Implement performance counter collection
  - [x] Research rocprofiler-sdk counter APIs (see `docs/counter_collection_research.md`)
  - [x] Add `--counter` option support (C++ and C implementations)
  - [x] Create counter collection tests
  - [ ] Investigate counter unavailability on specific hardware
  - [ ] Add counter visualization/reporting tools

- [ ] CSV Output Enhancement
  - [x] Implement basic CSV output mode with `--csv` option
  - [x] Add RocBLAS log integration to CSV output
  - [x] Create CSV summary tool (`utils/summarize_trace.py`)
  - [ ] Improve M, N, K size extraction accuracy from RocBLAS logs
  - [ ] Add CSV validation utilities

### Bug Fixes
- [ ] Fix RocBLAS pipe blocking issues
  - [x] Implement non-blocking pipe reads
  - [x] Add proper pipe draining logic
  - [ ] Test with various RocBLAS workloads
  - [ ] Handle edge cases (empty pipes, closed pipes, etc.)

## Medium Priority

### Testing
- [x] Set up basic testing infrastructure
- [x] Add unit tests for options parser
- [x] Add integration tests for end-to-end functionality
- [x] Add RocBLAS multi-step workflow test
- [x] Add error handling tests
- [x] Add C vs. C++ parity tests
- [x] Integrate README examples into test suite
- [ ] Add performance regression tests
- [ ] Add stress tests for long-running applications
- [ ] Improve test coverage reporting

### Documentation
- [x] Document counter collection feature in README
- [x] Create research documents for kernel arguments
- [x] Add CSV implementation plan
- [ ] Create user guide with common use cases
- [ ] Add troubleshooting section to README
- [ ] Document RocBLAS integration workflow
- [ ] Create architecture/design document
- [ ] Add API reference documentation

### Code Quality
- [x] Refactor options parsing into separate `.c` file
- [x] Achieve C/C++ implementation parity
- [ ] Add static analysis to build process
- [ ] Improve error messages and user feedback
- [ ] Add logging levels (verbose, debug, etc.)
- [ ] Code coverage analysis

## Low Priority

### Enhancements
- [ ] Add support for additional profiling backends
- [ ] Implement filtering options (by kernel name, duration, etc.)
- [ ] Add JSON output format
- [ ] Create GUI/TUI for trace visualization
- [ ] Add support for multi-GPU tracing
- [ ] Implement trace comparison tools
- [ ] Add export to common trace formats (Chrome Tracing, etc.)

### Performance
- [ ] Optimize buffer management
- [ ] Reduce overhead in hot paths
- [ ] Add configurable buffer sizes
- [ ] Implement zero-copy optimizations where possible

### Build System
- [ ] Add CMake presets for common configurations
- [ ] Improve cross-platform support
- [ ] Add packaging/installation scripts
- [ ] Create Docker container for testing
- [ ] Add CI/CD pipeline

## Completed

### Version 1.5.0 (2025-11-27)
- [x] Implement CSV summary tool with M, N, K extraction
- [x] Create `utils/summarize_trace.py` for trace analysis
- [x] Add test suite in `tests/test_csv_summary.py`

### Version 1.4.5 (2025-11-27)
- [x] Add RocBLAS multi-step workflow test
- [x] Add error handling test
- [x] Add C vs. C++ parity test
- [x] Update README to clarify RocBLAS named pipe usage

### Version 1.4.4 (2025-11-27)
- [x] Relax RocBLAS environment variable checks for regular files
- [x] Improve RocBLAS log reading (single lines per kernel dispatch)
- [x] Add timeline mode support for RocBLAS logging from regular files

### Version 1.4.3 (2025-11-27)
- [x] Fix RocBLAS logging with file opening function interception
- [x] Add support for reading RocBLAS logs from regular files
- [x] Update C tracer to match C++ functionality

### Version 1.4.2 (2025-11-27)
- [x] Add `debug` target to Makefile

### Version 1.4.1 (2025-11-27)
- [x] Refactor RocBLAS pipe support (remove background thread)
- [x] Implement synchronous reading for Tensile kernels
- [x] Add dynamic README example verification test

### Version 1.4.0 (2025-11-27)
- [x] Add `--rocblas <pipe>` option for RocBLAS log integration
- [x] Add `--rocblas-log <file>` option for log redirection
- [x] Support `ROCBLAS_LAYER=1` with `ROCBLAS_LOG_TRACE_PATH`
- [x] Include RocBLAS logs in CSV output

### Version 1.3.x (2025-11-26)
- [x] Implement CSV output support with `--csv` option
- [x] Add file output support (`--output`, `--outputdir`)
- [x] Reorganize README documentation

### Version 1.2.x (2025-11-26)
- [x] Implement counter collection feature
- [x] Add `--counter` option (compute, memory, mixed modes)
- [x] Create counter collection tests
- [x] Add diagnostic tools in utils directory
- [x] Improve requirements checking with fallback

### Version 1.1.x (2025-11-26)
- [x] Implement timeline support with GPU timestamps
- [x] Add buffer tracing implementation (C++ and C)
- [x] Implement environment variable options (`RPV3_OPTIONS`)
- [x] Add `--version` and `--help` options
- [x] Refactor options parsing to separate `.c` file

### Version 1.0.x (2025-11-25)
- [x] Add comprehensive testing infrastructure
- [x] Create initial release with C++ and C implementations

## Known Issues

- Counter collection may not work on all hardware configurations
  - Requires investigation of hardware-specific requirements
  - May need fallback mechanisms for unsupported platforms

- RocBLAS log parsing edge cases
  - Some log formats may not be correctly parsed
  - M, N, K extraction depends on specific comment format

- Group membership check inconsistencies
  - `check_requirements.sh` may not accurately reflect all group memberships
  - Needs dynamic detection improvement

## Future Research

- [ ] Investigate kernel argument access capabilities
- [ ] Research alternative profiling APIs
- [ ] Explore integration with other AMD tools (rocprof, Omnitrace)
- [ ] Investigate machine learning-based performance analysis
- [ ] Research distributed tracing for multi-node applications

## Notes

- Keep this document synchronized with CHANGELOG.md for completed items
- Link to relevant research documents in `docs/` for complex features
- Use issue tracker for detailed bug reports and feature discussions
- Update version numbers in sync with releases

---

**Last Updated**: 2025-11-28  
**Current Version**: 1.5.0

