# Backtrace Support Research

## Objective

Research and design a `--backtrace` option for the RPV3 kernel tracer that captures CPU-side function call stacks at kernel dispatch points. This feature will help identify which high-level libraries (RocBLAS, hipBLAS, MIOpen, etc.) and application code paths trigger specific kernel launches.

---

## Background

When profiling GPU applications, it's often valuable to understand not just *which* kernels are launched, but *from where* they are launched. The call stack (backtrace) provides this context by showing the sequence of function calls leading to a kernel dispatch.

### Use Cases

1. **Library Attribution**: Identify which library (RocBLAS, hipBLAS, MIOpen) triggered a kernel
2. **Application Context**: Understand which application code path led to a kernel launch
3. **Debugging**: Trace unexpected kernel launches back to their source
4. **Performance Analysis**: Correlate kernel performance with calling context
5. **Library Integration**: Support future library-specific optimizations and filtering

---

## Backtrace API Options

### Option 1: `libunwind` (Recommended)

**Overview:**
`libunwind` is a portable library for programmatic stack unwinding. It provides robust, async-signal-safe stack walking with detailed frame information.

**Advantages:**
- ✅ **Async-signal-safe**: Can be used in signal handlers and crash dumps
- ✅ **Portable**: Works across different architectures and platforms
- ✅ **Detailed information**: Provides register values, instruction pointers, and frame details
- ✅ **Remote unwinding**: Can unwind stacks of other processes
- ✅ **Better reliability**: More robust than glibc's `backtrace()`

**Disadvantages:**
- ⚠️ **External dependency**: Requires `libunwind` to be installed
- ⚠️ **Complexity**: More complex API than `backtrace()`
- ⚠️ **Build requirement**: Adds build-time dependency

**API Example:**
```c
#include <libunwind.h>

void print_backtrace_libunwind() {
    unw_context_t context;
    unw_cursor_t cursor;
    
    // Capture current context
    unw_getcontext(&context);
    unw_init_local(&cursor, &context);
    
    // Walk the stack
    while (unw_step(&cursor) > 0) {
        unw_word_t ip, offset;
        char symbol[256];
        
        unw_get_reg(&cursor, UNW_REG_IP, &ip);
        
        if (unw_get_proc_name(&cursor, symbol, sizeof(symbol), &offset) == 0) {
            printf("  %s + 0x%lx [0x%lx]\n", symbol, offset, ip);
        } else {
            printf("  [0x%lx]\n", ip);
        }
    }
}
```

**Link flags:** `-lunwind`

---

### Option 2: `execinfo.h` (glibc backtrace)

**Overview:**
The GNU C Library provides `backtrace()` and `backtrace_symbols()` functions for capturing and symbolizing call stacks.

**Advantages:**
- ✅ **Built-in**: Part of glibc, no external dependencies
- ✅ **Simple API**: Easy to use with minimal code
- ✅ **Widely available**: Present on most Linux systems

**Disadvantages:**
- ⚠️ **Not async-signal-safe**: Cannot be used in signal handlers reliably
- ⚠️ **Requires `-rdynamic`**: Must link with `-rdynamic` to export symbols
- ⚠️ **Less robust**: Can fail with optimized code (inlining, tail calls)
- ⚠️ **Limited information**: Only provides return addresses and symbols

**API Example:**
```c
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>

void print_backtrace_execinfo() {
    void* buffer[64];
    int nptrs = backtrace(buffer, 64);
    char** strings = backtrace_symbols(buffer, nptrs);
    
    if (strings == NULL) {
        perror("backtrace_symbols");
        return;
    }
    
    for (int i = 0; i < nptrs; i++) {
        printf("  %s\n", strings[i]);
    }
    
    free(strings);
}
```

**Link flags:** `-rdynamic` (to export dynamic symbols)

---

## Shared Library Resolution

To identify which shared library a function belongs to, we use `dladdr()` from `<dlfcn.h>`:

```c
#include <dlfcn.h>

void print_frame_with_library(void* addr) {
    Dl_info info;
    
    if (dladdr(addr, &info)) {
        const char* lib_name = info.dli_fname ? info.dli_fname : "???";
        const char* func_name = info.dli_sname ? info.dli_sname : "???";
        void* offset = (void*)((char*)addr - (char*)info.dli_saddr);
        
        printf("  %s: %s + %p\n", lib_name, func_name, offset);
    } else {
        printf("  [0x%lx]\n", (unsigned long)addr);
    }
}
```

This provides:
- `dli_fname`: Path to the shared library
- `dli_sname`: Symbol name (function name)
- `dli_saddr`: Symbol address (function start)

---

## Argument Extraction (Future Enhancement)

While the initial implementation will focus on capturing the call stack, future enhancements could extract function arguments for selected functions.

### Approach 1: DWARF Debug Information

Use `libdw` (from elfutils) to parse DWARF debug information and extract argument values from registers/stack.

**Complexity:** High  
**Reliability:** Depends on debug symbols  
**Performance:** Significant overhead

### Approach 2: HIP API Tracing

Intercept HIP runtime API calls (e.g., `hipLaunchKernel`) to capture kernel arguments directly.

**Complexity:** Medium  
**Reliability:** High for HIP kernels  
**Performance:** Moderate overhead

**Note:** This is already documented in `docs/research_kernel_args.md` and could be combined with backtrace support.

---

## Design Decisions

### 1. API Choice: Hybrid Approach

**Decision:** Use `libunwind` if available, fall back to `execinfo`

**Rationale:**
- `libunwind` provides better reliability and features
- `execinfo` ensures the feature works even without external dependencies
- CMake can detect `libunwind` availability at build time

**Implementation:**
```cpp
#ifdef HAVE_LIBUNWIND
    print_backtrace_libunwind();
#else
    print_backtrace_execinfo();
#endif
```

### 2. Incompatibility with Timeline and CSV

**Decision:** `--backtrace` is incompatible with `--timeline` and `--csv`

**Rationale:**
- **Timeline mode**: Backtrace capture adds significant overhead that would distort GPU timing measurements
- **CSV mode**: Variable-length stack traces don't fit the structured CSV schema
- **Use case separation**: Backtrace is for debugging/analysis, not production profiling

**Implementation:**
- Validate in `rpv3_parse_options()` and `tool_init()`
- Print clear error message if incompatible options are used together
- Exit early to prevent initialization

### 3. Output Format

**Decision:** Human-readable text format with frame-by-frame listing

**Format:**
```
[Kernel Trace #N]
  Kernel Name: <kernel_name>
  Dispatch ID: <id>
  
Call Stack (M frames):
  #0  <library>: <function> + <offset>
  #1  <library>: <function> + <offset>
  ...
```

**Rationale:**
- Clear and readable for debugging
- Shows library attribution at a glance
- Compatible with existing trace output style

### 4. Frame Filtering

**Decision:** Skip internal profiler frames, show all application/library frames

**Implementation:**
- Skip frames from `libkernel_tracer.so` and `librocprofiler-sdk.so`
- Start showing frames from HIP runtime and above
- Limit to 64 frames maximum to prevent excessive output

### 5. Symbol Demangling

**Decision:** Demangle C++ symbols in C++ implementation only

**Rationale:**
- C++ kernel names and library functions are mangled
- Demangling improves readability significantly
- C implementation doesn't need demangling (C symbols aren't mangled)

**Implementation:**
```cpp
// C++ version
std::string demangled = demangle_kernel_name(symbol);

// C version
// No demangling needed
```

---

## Performance Considerations

### Overhead Analysis

**Backtrace capture overhead:**
- `backtrace()`: ~10-50 microseconds per call
- `backtrace_symbols()`: ~50-200 microseconds per call
- `dladdr()`: ~5-10 microseconds per frame
- Symbol demangling: ~10-50 microseconds per symbol

**Total per kernel dispatch:** ~100-500 microseconds

**Impact:**
- Negligible for long-running kernels (>1ms)
- Significant for short kernels (<100μs)
- Not suitable for high-frequency kernel launches

### Mitigation Strategies

1. **Sampling**: Only capture backtrace for every Nth kernel (future enhancement)
2. **Filtering**: Only capture for specific kernel names (future enhancement)
3. **Caching**: Cache backtraces for repeated call paths (future enhancement)

**Current approach:** Accept overhead, document as debugging tool, not production profiler

---

## Implementation Phases

### Phase 1: Basic Backtrace (Current Plan)
- ✅ Capture call stack with `execinfo` or `libunwind`
- ✅ Resolve shared library names with `dladdr()`
- ✅ Print human-readable output
- ✅ Validate incompatibility with `--timeline` and `--csv`
- ✅ Implement in both C++ and C versions

### Phase 2: Enhanced Filtering (Future)
- Filter by kernel name pattern
- Filter by library name
- Sampling mode (every Nth kernel)
- Depth limiting (max frames to show)

### Phase 3: Argument Extraction (Future)
- Integrate with HIP API tracing
- Extract kernel launch arguments
- Show selected function arguments
- Correlate with kernel dispatch

---

## Testing Strategy

### Unit Tests
- Test backtrace capture in isolation
- Test library resolution with known addresses
- Test symbol demangling

### Integration Tests
- Test with `example_app` (HIP kernels)
- Test with `example_rocblas` (library kernels)
- Verify library names appear correctly
- Verify function names are readable

### Compatibility Tests
- Verify error when used with `--timeline`
- Verify error when used with `--csv`
- Test C vs C++ parity

### Performance Tests
- Measure overhead per kernel dispatch
- Compare with/without backtrace enabled
- Document performance impact

---

## Alternatives Considered

### Alternative 1: Always-On Lightweight Backtrace

**Idea:** Capture minimal backtrace (top 3-5 frames) for every kernel without option flag

**Rejected because:**
- Still adds overhead to all kernel dispatches
- Users may not need this information most of the time
- Conflicts with CSV mode design

### Alternative 2: Post-Processing with `perf`

**Idea:** Use Linux `perf` tool to capture call stacks separately

**Rejected because:**
- Requires separate tool invocation
- Harder to correlate with kernel traces
- Less integrated user experience

### Alternative 3: eBPF-based Tracing

**Idea:** Use eBPF to capture call stacks with minimal overhead

**Rejected because:**
- Requires root privileges or special capabilities
- More complex to implement and deploy
- Overkill for this use case

---

## Recommendations

1. **Implement Phase 1** as described in the implementation plan
2. **Use hybrid approach** (libunwind + execinfo fallback)
3. **Document clearly** that this is a debugging tool, not for production
4. **Consider Phase 2 enhancements** based on user feedback
5. **Integrate with HIP API tracing** (Phase 3) for argument extraction

---

## References

- [libunwind documentation](http://www.nongnu.org/libunwind/)
- [glibc backtrace manual](https://www.gnu.org/software/libc/manual/html_node/Backtraces.html)
- [dladdr man page](https://man7.org/linux/man-pages/man3/dladdr.3.html)
- ROCm Profiler SDK documentation
- `docs/research_kernel_args.md` - Kernel argument access research

---

**Author:** AI Assistant (Antigravity)  
**Date:** 2025-11-28  
**Status:** Research Complete, Ready for Implementation
