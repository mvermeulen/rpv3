# RPV3 v1.5.0 - CSV Summary Tool and Enhanced Testing

## Highlights

### New Features
- **CSV Summary Tool**: Analyze trace output with `utils/summarize_trace.py`
  - Groups kernels by name and M, N, K dimensions (extracted from RocBLAS logs)
  - Calculates statistics: count, total time, average time, and percentage of total
  - Sorts by total time descending for easy bottleneck identification
  - Test suite included in `tests/test_csv_summary.py`

### Improvements
- Enhanced test coverage with multi-step workflow tests
- Improved error handling validation
- C vs C++ parity verification tests

## What's New

See [CHANGELOG.md](https://github.com/mvermeulen/rpv3/blob/v1.5.0/CHANGELOG.md#150---2025-11-27) for complete details.

## Installation

```bash
git clone https://github.com/mvermeulen/rpv3.git
cd rpv3
git checkout v1.5.0
make
```

## Quick Start

```bash
# Basic tracing
LD_PRELOAD=./libkernel_tracer.so ./your_app

# CSV output with summary analysis
RPV3_OPTIONS="--csv" LD_PRELOAD=./libkernel_tracer.so ./your_app > trace.csv
python3 utils/summarize_trace.py trace.csv
```

## Requirements
- ROCm 5.x or later
- rocprofiler-sdk
- HIP runtime

## Documentation
- [README.md](https://github.com/mvermeulen/rpv3/blob/v1.5.0/README.md) - Complete usage guide
- [CHANGELOG.md](https://github.com/mvermeulen/rpv3/blob/v1.5.0/CHANGELOG.md) - Version history
- [TODO.md](https://github.com/mvermeulen/rpv3/blob/v1.5.0/TODO.md) - Roadmap

## Known Issues
- Counter collection may not work on all hardware (graceful fallback provided)

## License
This project is licensed under the MIT License - see the [LICENSE](https://github.com/mvermeulen/rpv3/blob/v1.5.0/LICENSE) file for details.
