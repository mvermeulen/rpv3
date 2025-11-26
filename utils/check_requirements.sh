#!/bin/bash
# check_requirements.sh
# Checks software requirements for AMD GPU performance counters

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_header() {
    echo -e "\n${BLUE}=== $1 ===${NC}"
}

print_pass() {
    echo -e "${GREEN}✓ PASS${NC}: $1"
}

print_fail() {
    echo -e "${RED}✗ FAIL${NC}: $1"
}

print_warn() {
    echo -e "${YELLOW}⚠ WARN${NC}: $1"
}

print_info() {
    echo -e "  $1"
}

echo "Checking system requirements for AMD GPU Performance Counters..."

# 1. Check Kernel Version
print_header "Linux Kernel Version"
KERNEL_VERSION=$(uname -r)
print_info "Current Kernel: $KERNEL_VERSION"

# Extract major and minor version
MAJOR=$(echo "$KERNEL_VERSION" | cut -d. -f1)
MINOR=$(echo "$KERNEL_VERSION" | cut -d. -f2)

if [ "$MAJOR" -gt 6 ] || { [ "$MAJOR" -eq 6 ] && [ "$MINOR" -ge 10 ]; }; then
    print_pass "Kernel version 6.10+ (Recommended for RDNA 3.5/gfx1151)"
elif [ "$MAJOR" -eq 6 ] && [ "$MINOR" -ge 8 ]; then
    print_warn "Kernel version 6.8+ (Minimum for initial RDNA 3.5 support)"
else
    print_warn "Kernel version < 6.8. Upgrade recommended for newer hardware."
fi

# 2. Check ROCm Version
print_header "ROCm Version"
if [ -f "/opt/rocm/.info/version" ]; then
    ROCM_VERSION=$(cat /opt/rocm/.info/version)
    print_info "Installed ROCm: $ROCM_VERSION"
    
    # Simple check for 6.x
    if [[ "$ROCM_VERSION" == 6.* ]] || [[ "$ROCM_VERSION" == 7.* ]]; then
        print_pass "ROCm 6.0+ detected"
    else
        print_warn "Older ROCm detected. 6.2+ recommended for gfx1151."
    fi
else
    print_fail "ROCm version file not found at /opt/rocm/.info/version"
fi

# 3. Check amdgpu.ppfeaturemask
print_header "AMDGPU PowerPlay Feature Mask"
if [ -f "/sys/module/amdgpu/parameters/ppfeaturemask" ]; then
    MASK=$(cat /sys/module/amdgpu/parameters/ppfeaturemask)
    print_info "Current Mask: $MASK"
    
    if [ "$MASK" == "0xffffffff" ] || [ "$MASK" == "4294967295" ]; then
        print_pass "ppfeaturemask is enabled (0xffffffff)"
    else
        print_fail "ppfeaturemask is NOT fully enabled."
        print_info "Performance counters on consumer GPUs often require this mask."
        print_info "Try adding 'amdgpu.ppfeaturemask=0xffffffff' to your kernel boot parameters."
    fi
else
    print_fail "amdgpu module parameter 'ppfeaturemask' not found."
    print_info "Is the amdgpu driver loaded?"
fi

# 4. Check GPU Hardware
print_header "GPU Hardware"
if command -v rocminfo &> /dev/null; then
    GPU_NAME=$(rocminfo | grep -m 1 "Name:" | cut -d: -f2 | xargs)
    print_info "Detected GPU: $GPU_NAME"
    
    if [[ "$GPU_NAME" == *"gfx1151"* ]]; then
        print_info "Detected gfx1151 (Strix Halo/RDNA 3.5)"
    fi
else
    print_warn "rocminfo command not found."
fi


# 5. Check User Permissions
print_header "User Permissions"
CURRENT_USER=$(whoami)
USER_GROUPS=$(groups)
print_info "Current User: $CURRENT_USER"
print_info "Groups: $USER_GROUPS"

if [[ "$USER_GROUPS" == *"render"* ]] && [[ "$USER_GROUPS" == *"video"* ]]; then
    print_pass "User is in 'render' and 'video' groups"
else
    if [[ "$USER_GROUPS" != *"render"* ]]; then
        print_warn "User is NOT in 'render' group. Required for direct GPU access."
    fi
    if [[ "$USER_GROUPS" != *"video"* ]]; then
        print_warn "User is NOT in 'video' group. Often required for GPU access."
    fi
    print_info "Fix: sudo usermod -aG render,video $CURRENT_USER"
fi

# 6. Check Device Permissions
print_header "Device Permissions"
if [ -w "/dev/kfd" ]; then
    print_pass "Write access to /dev/kfd"
else
    print_fail "No write access to /dev/kfd"
fi

RENDER_DEV=$(ls /dev/dri/renderD* 2>/dev/null | head -n 1)
if [ -n "$RENDER_DEV" ]; then
    if [ -w "$RENDER_DEV" ]; then
        print_pass "Write access to $RENDER_DEV"
    else
        print_fail "No write access to $RENDER_DEV"
    fi
else
    print_fail "No render device found in /dev/dri/"
fi

# 7. Check Perf Event Paranoid
print_header "Perf Event Paranoid"
if [ -f "/proc/sys/kernel/perf_event_paranoid" ]; then
    PARANOID=$(cat /proc/sys/kernel/perf_event_paranoid)
    print_info "Value: $PARANOID"
    if [ "$PARANOID" -le 2 ]; then
        print_pass "perf_event_paranoid is <= 2"
    else
        print_warn "perf_event_paranoid is > 2 ($PARANOID). This may block performance counters."
        print_info "Try: sudo sh -c 'echo 2 > /proc/sys/kernel/perf_event_paranoid'"
    fi
else
    print_info "/proc/sys/kernel/perf_event_paranoid not found (OK for some containers)"
fi

# 8. Check Environment Variables
print_header "Environment Variables"
if [ -n "$HSA_OVERRIDE_GFX_VERSION" ]; then
    print_warn "HSA_OVERRIDE_GFX_VERSION is set to '$HSA_OVERRIDE_GFX_VERSION'"
    print_info "This can cause mismatches if not set correctly."
else
    print_pass "HSA_OVERRIDE_GFX_VERSION is not set"
fi

# 9. Check dmesg for AMDGPU errors (if possible)
print_header "Kernel Log (dmesg)"
if dmesg &> /dev/null; then
    ERRORS=$(dmesg | grep -i "amdgpu" | grep -iE "error|fault|fail|reject" | tail -n 5)
    if [ -n "$ERRORS" ]; then
        print_warn "Found recent AMDGPU errors in dmesg:"
        echo "$ERRORS"
    else
        print_pass "No recent AMDGPU errors found in tail of dmesg"
    fi
else
    print_info "Cannot read dmesg (permission denied). Skipping log check."
fi


# 10. Check rocprofv3 availability and counters
print_header "rocprofv3 Counter Check"
if command -v rocprofv3 &> /dev/null; then
    print_info "rocprofv3 found at $(which rocprofv3)"
    
    # Run rocprofv3 -L to list available counters
    print_info "Running 'rocprofv3 -L' to list counters..."
    ROCPROF_OUTPUT=$(rocprofv3 -L 2>&1)
    EXIT_CODE=$?
    
    if [ $EXIT_CODE -ne 0 ]; then
        print_fail "rocprofv3 -L failed with exit code $EXIT_CODE"
        echo "$ROCPROF_OUTPUT" | head -n 5 | sed 's/^/    /'
    else
        # Check if we have any counters listed. 
        # If output is short (just GPU name), it likely failed to find counters.
        LINE_COUNT=$(echo "$ROCPROF_OUTPUT" | wc -l)
        
        if [ "$LINE_COUNT" -le 5 ]; then
             print_fail "rocprofv3 listed no counters (Output too short)"
             echo "$ROCPROF_OUTPUT" | sed 's/^/    /'
        else
             print_pass "rocprofv3 listed counters (Output length: $LINE_COUNT lines)"
        fi
    fi
else
    print_warn "rocprofv3 not found in PATH"
fi

echo ""
