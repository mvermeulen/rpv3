# Makefile for ROCm Profiler Kernel Tracer
# Alternative to CMake for quick building

# Compiler settings
CXX = g++
CC = gcc
HIPCC = hipcc
CXXFLAGS = -std=c++17 -fPIC -Wall -O2
CFLAGS = -std=c11 -fPIC -Wall -O2
LDFLAGS = -shared

# ROCm paths (adjust if needed)
ROCM_PATH ?= /opt/rocm
ROCPROF_INCLUDE = $(ROCM_PATH)/include
ROCPROF_LIB = $(ROCM_PATH)/lib

# Targets
PLUGIN_CPP = libkernel_tracer.so
PLUGIN_C = libkernel_tracer_c.so
EXAMPLE = example_app

.PHONY: all clean

all: $(PLUGIN_CPP) $(PLUGIN_C) $(EXAMPLE)

# Build the C++ profiler plugin
$(PLUGIN_CPP): kernel_tracer.cpp rpv3_options.h
	$(CXX) $(CXXFLAGS) $(LDFLAGS) \
		-I$(ROCPROF_INCLUDE) \
		-I$(ROCM_PATH)/include/hip \
		-L$(ROCPROF_LIB) \
		-lrocprofiler-sdk \
		-o $@ $<

# Build the C profiler plugin
$(PLUGIN_C): kernel_tracer.c rpv3_options.h
	$(CC) $(CFLAGS) $(LDFLAGS) \
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
	rm -f $(PLUGIN_CPP) $(PLUGIN_C) $(EXAMPLE)

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
