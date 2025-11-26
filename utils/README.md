# RPV3 Utilities

This directory contains utility tools for debugging and diagnosing the ROCm Profiler Kernel Tracer.

## Tools

### `diagnose_counters`
Queries the GPU agents and lists all supported performance counters. Useful for verifying hardware support for counter collection.

**Usage:**
```bash
make utils
./utils/diagnose_counters
```

### `check_status`
A simple utility to print the string representation of a `rocprofiler_status_t` code. Useful for decoding error codes returned by the SDK.

**Usage:**
```bash
make utils
./utils/check_status
```

## Building

These tools can be built using the main project `Makefile`:

```bash
make utils
```

## Cleaning

To remove the built utilities:

```bash
make clean
```
