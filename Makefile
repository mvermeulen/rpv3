# Makefile for ROCm Profiler Kernel Tracer
# Alternative to CMake for quick building

# Compiler settings
CXX = g++
HIPCC = hipcc
CXXFLAGS = -std=c++17 -fPIC -Wall -O2
LDFLAGS = -shared

# ROCm paths (adjust if needed)
ROCM_PATH ?= /opt/rocm
ROCPROF_INCLUDE = $(ROCM_PATH)/include
ROCPROF_LIB = $(ROCM_PATH)/lib

# Targets
PLUGIN = libkernel_tracer.so
EXAMPLE = example_app

.PHONY: all clean

all: $(PLUGIN) $(EXAMPLE)

# Build the profiler plugin
$(PLUGIN): kernel_tracer.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) \
		-I$(ROCPROF_INCLUDE) \
		-I$(ROCM_PATH)/include/hip \
		-L$(ROCPROF_LIB) \
		-lrocprofiler-sdk \
		-o $@ $<

# Build the example application
$(EXAMPLE): example_app.cpp
	$(HIPCC) --std=c++17 -O2 \
		-o $@ $<

clean:
	rm -f $(PLUGIN) $(EXAMPLE)

# Usage instructions
help:
	@echo "ROCm Profiler Kernel Tracer Build"
	@echo ""
	@echo "Targets:"
	@echo "  make all     - Build both plugin and example (default)"
	@echo "  make clean   - Remove built files"
	@echo ""
	@echo "Usage:"
	@echo "  HSA_TOOLS_LIB=./libkernel_tracer.so ./example_app"
