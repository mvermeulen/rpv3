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

echo ""
