# Implementation Plan - Add --csv Option

This plan details the steps required to add a `--csv` option to the RPV3 kernel tracer, enabling machine-readable output for kernel execution data.

## Goal Description
Add a `--csv` option to the `RPV3_OPTIONS` environment variable. When enabled, the tracer will output kernel execution details in a CSV format instead of the default human-readable format. This facilitates easier data analysis and integration with other tools.

## Proposed Changes

### Options Parsing
#### [MODIFY] [rpv3_options.h](file:///home/mev/source/rpv3/rpv3_options.h)
- Add `extern int rpv3_csv_enabled;` declaration.
- Update `rpv3_parse_options` documentation to include `--csv`.

#### [MODIFY] [rpv3_options.c](file:///home/mev/source/rpv3/rpv3_options.c)
- Define `int rpv3_csv_enabled = 0;`.
- In `rpv3_parse_options`, handle the `--csv` token to set `rpv3_csv_enabled = 1`.
- Update help message to include `--csv`.

### Kernel Tracer Implementation
#### [MODIFY] [kernel_tracer.c](file:///home/mev/source/rpv3/kernel_tracer.c)
- In `tool_init`:
    - Read `rpv3_csv_enabled` into a local static variable (e.g., `csv_enabled`).
    - **Crucial**: If `csv_enabled` is true, capture `tracer_start_timestamp` even if `timeline_enabled` is false, so `TimeSinceStartMs` is always available.
- In `timeline_buffer_callback` (timeline mode):
    - Check `csv_enabled`.
    - If true, print CSV line (header printed once on first output).
    - If false, use existing human-readable format.
- In `kernel_dispatch_callback` (standard mode):
    - Check `csv_enabled`.
    - If true:
        - **On ENTER phase**: Do nothing (suppress output).
        - **On EXIT phase**: Print the full CSV line. Use `dispatch_data->dispatch_info` to get kernel details (grid size, etc.) which are available in the payload.
    - If false:
        - Keep existing behavior (print details on ENTER, timestamps on EXIT).
- CSV Columns (Unified for both modes): `KernelName,ThreadID,CorrelationID,KernelID,DispatchID,GridX,GridY,GridZ,WorkgroupX,WorkgroupY,WorkgroupZ,PrivateSeg,GroupSeg,StartTimestamp,EndTimestamp,DurationNs,DurationUs,TimeSinceStartMs`

#### [MODIFY] [kernel_tracer.cpp](file:///home/mev/source/rpv3/kernel_tracer.cpp)
- Apply identical logic as `kernel_tracer.c`:
    - Capture start timestamp if CSV is enabled.
    - In `kernel_dispatch_callback`, only print CSV line on `ROCPROFILER_CALLBACK_PHASE_EXIT`.
    - Ensure `dispatch_data` on EXIT contains all necessary dispatch info (it does).

## Verification Plan

### Automated Tests
#### [NEW] [tests/test_csv_output.sh](file:///home/mev/source/rpv3/tests/test_csv_output.sh)
- Create a new test script to verify CSV output.
- Steps:
    1. Build the project.
    2. Run `example_app` with `RPV3_OPTIONS="--csv"`.
    3. Capture output.
    4. Verify header row exists and matches expected columns.
    5. Verify data rows exist and have correct number of fields.
    6. Verify specific values (e.g., kernel names) are present in the CSV data.

#### [MODIFY] [tests/test_rpv3_options.c](file:///home/mev/source/rpv3/tests/test_rpv3_options.c)
- Add a unit test `TEST(csv_option)` to verify that `--csv` correctly sets the `rpv3_csv_enabled` flag.

### Manual Verification
- Run the following command and inspect output:
  ```bash
  RPV3_OPTIONS="--csv" LD_PRELOAD=./build/libkernel_tracer.so ./build/example_app
  ```
- Verify output can be parsed by a CSV reader (e.g., `column -t -s,`).
