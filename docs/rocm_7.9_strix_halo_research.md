# ROCm 7.9 Strix Halo Support Research

## Executive Summary

ROCm 7.9 (Technology Preview/RC1) adds **official support** for AMD Strix Halo (Ryzen AI Max 300 series APUs, gfx1151/RDNA 3.5 architecture). This is significant for the RPV3 kernel tracer, particularly regarding performance counter support.

---

## Key Findings

### 1. Official Strix Halo Support

**Status**: ✅ **Officially Supported** in ROCm 7.9
- **Architecture**: gfx1151 (RDNA 3.5)
- **Products**: 
  - AMD Ryzen AI Max+ PRO 300 Series APUs
  - AMD Ryzen AI Max 300 Series APUs
- **Release Type**: Technology Preview (RC1)

**Timeline**:
- ROCm 6.4.1: Initial unofficial support
- ROCm 6.4.4: Formal support added (Windows + Linux)
- ROCm 7.0: Compatibility confirmed with AI workloads
- **ROCm 7.9**: Technology preview with explicit hardware listing

---

### 2. Performance Counter Support

#### GPU Performance API (GPUPerfAPI)
✅ **RDNA 3.5 Support Confirmed**

The latest GPUPerfAPI explicitly includes support for RDNA 3.5 APUs:
- AMD Ryzen AI 5 330 Processor with AMD Radeon 820M Graphics
- Other RDNA 3.5 architecture-based APUs

**Implications for RPV3**:
- Performance counters ARE available for gfx1151/RDNA 3.5
- GPUPerfAPI provides access to GPU Performance Counters
- Enables detailed performance analysis and execution characteristics

#### rocprofiler-sdk Support
✅ **Compatible with Strix Halo**

`rocprofiler-sdk` is a core ROCm component that works with Strix Halo:
- **Hardware performance counter monitoring**
- **PC sampling** for kernel execution analysis
- **Tracing mechanisms** (callback, buffer, etc.)
- Low-level hardware access + high-level profiling abstractions

**Current RPV3 Status**:
- Our tracer uses rocprofiler-sdk v1.0.0
- Counter collection implemented but hardware-dependent
- Graceful fallback when counters unsupported

---

### 3. ROCm 7.9 Improvements

#### Infrastructure Changes
- **TheRock**: New build and release infrastructure
- **Open Release Process**: More transparent development
- **Modular SDK**: Improved portability across Linux distributions
- **Streamlined Development**: Better developer experience

#### Profiling Tools Available
ROCm 7.9 includes comprehensive profiling tools:
1. **ROCProfiler**: Command-line profiling
2. **ROCm Compute Profiler**: GUI-based profiling
3. **ROCm Systems Profiler**: System-level analysis
4. **rocprofiler-sdk**: Library for custom profilers (like RPV3)

---

## Implications for RPV3 Kernel Tracer

### Counter Collection on Strix Halo

**Expected Behavior with ROCm 7.9**:

1. **Counter Support**: ✅ **LIKELY AVAILABLE**
   - GPUPerfAPI confirms RDNA 3.5 counter support
   - rocprofiler-sdk should expose these counters
   - Our `--counter` option should work

2. **What to Test**:
   ```bash
   # Test counter availability
   rocprofv3 -L  # List available counters for gfx1151
   
   # Test RPV3 counter collection
   RPV3_OPTIONS="--counter compute" LD_PRELOAD=./libkernel_tracer.so ./example_app
   RPV3_OPTIONS="--counter memory" LD_PRELOAD=./libkernel_tracer.so ./example_app
   RPV3_OPTIONS="--counter mixed" LD_PRELOAD=./libkernel_tracer.so ./example_app
   ```

3. **Expected Counters** (based on RDNA 3.5):
   - **Compute**: `SQ_INSTS_VALU`, `SQ_WAVES`, `SQ_INSTS_SALU`
   - **Memory**: `TCC_EA_RDREQ_sum`, `TCC_EA_WRREQ_sum`, etc.
   - **Mixed**: Combination of both

### Backtrace Support

**No Changes Expected**:
- Backtrace uses standard Linux APIs (`execinfo.h`, `dladdr`)
- Platform-independent functionality
- Should work identically on Strix Halo

### Timeline/Tracing Support

**Should Work Without Issues**:
- Buffer tracing via rocprofiler-sdk
- Callback tracing mechanisms
- GPU timestamp collection
- All core profiling features

---

## Recommendations

### For Current Users on Strix Halo

1. **Upgrade to ROCm 7.9** (when stable)
   - Official support vs. unofficial in earlier versions
   - Improved stability and compatibility
   - Better counter support

2. **Test Counter Collection**
   - Verify `rocprofv3 -L` shows counters
   - Test RPV3 `--counter` option
   - Report any issues with specific counter groups

3. **Update Documentation**
   - Note ROCm 7.9 as recommended for Strix Halo
   - Document counter availability status
   - Update requirements section

### For RPV3 Development

1. **No Code Changes Required**
   - Current implementation should work
   - Graceful fallback already implemented
   - Counter detection is automatic

2. **Testing Priorities**
   - Verify counter collection on gfx1151
   - Test all three counter modes (compute, memory, mixed)
   - Validate counter definitions match hardware

3. **Documentation Updates**
   - Add ROCm 7.9 to supported versions
   - Note Strix Halo as officially supported
   - Update counter availability matrix

---

## Counter Availability Matrix (Updated)

| Platform | Architecture | ROCm Version | Counter Support | RPV3 Status |
|----------|-------------|--------------|-----------------|-------------|
| Discrete GPUs | RDNA 2/3 | 6.0+ | ✅ Full | ✅ Tested |
| **Strix Halo** | **RDNA 3.5 (gfx1151)** | **7.9+** | **✅ Available** | **🔄 To Test** |
| Older APUs | RDNA 1/2 | 6.0+ | ⚠️ Limited | ⚠️ Varies |

---

## Technical Details

### rocprofiler-sdk Capabilities on Strix Halo

**Confirmed Working**:
- ✅ Kernel dispatch tracing
- ✅ Code object callbacks
- ✅ Buffer tracing (timeline mode)
- ✅ Callback tracing (standard mode)
- ✅ GPU timestamps

**Expected to Work** (needs testing):
- 🔄 Hardware performance counters
- 🔄 Dispatch counting service
- 🔄 Counter collection buffers

**API Features**:
- `rocprofiler_iterate_agent_supported_counters()` - Should list gfx1151 counters
- `rocprofiler_create_profile_config()` - Should accept RDNA 3.5 counters
- `rocprofiler_configure_buffer_dispatch_counting_service()` - Should work

---

## Known Limitations

### Early ROCm Versions (6.4.1 - 6.4.4)
- Some bugs and stability issues reported
- Required specific configurations
- Unofficial support status

### ROCm 7.9 (Current)
- **Technology Preview** status (RC1)
- May have some rough edges
- Production use should wait for stable release

### General APU Limitations
- Counter availability may be more limited than discrete GPUs
- Some advanced profiling features may not be available
- Performance characteristics differ from discrete GPUs

---

## Action Items

### Immediate
- [ ] Update README to mention ROCm 7.9 Strix Halo support
- [ ] Add note about counter availability on RDNA 3.5
- [ ] Update requirements documentation

### When ROCm 7.9 Stable Released
- [ ] Test counter collection on actual Strix Halo hardware
- [ ] Verify all counter groups work correctly
- [ ] Update counter availability matrix
- [ ] Add Strix Halo to tested platforms list

### Future Enhancements
- [ ] Add Strix Halo-specific optimizations if needed
- [ ] Document any platform-specific quirks
- [ ] Add Strix Halo to CI/CD if available

---

## Conclusion

**ROCm 7.9 brings significant improvements for Strix Halo**:

1. ✅ **Official Support**: No longer unofficial/experimental
2. ✅ **Counter Support**: GPUPerfAPI confirms RDNA 3.5 counter availability
3. ✅ **rocprofiler-sdk**: Full profiling API support expected
4. ✅ **RPV3 Compatibility**: Should work without code changes

**For RPV3 Users**:
- Upgrade to ROCm 7.9 when stable
- Counter collection (`--counter`) should work on Strix Halo
- All other features (backtrace, timeline, CSV) should work normally
- Report any issues for platform-specific fixes

**Bottom Line**: ROCm 7.9 makes Strix Halo a first-class citizen for GPU profiling, and RPV3 should benefit from improved counter support without requiring code changes.
