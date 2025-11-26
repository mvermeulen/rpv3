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
OPTIONS_OBJ = rpv3_options.o

.PHONY: all clean

all: $(PLUGIN_CPP) $(PLUGIN_C) $(EXAMPLE)

# Build the options parser object file
$(OPTIONS_OBJ): rpv3_options.c rpv3_options.h
	$(CC) $(CFLAGS) -c -o $@ $<

# Build the C++ profiler plugin
$(PLUGIN_CPP): kernel_tracer.cpp rpv3_options.h $(OPTIONS_OBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) \
		-I$(ROCPROF_INCLUDE) \
		-I$(ROCM_PATH)/include/hip \
		-L$(ROCPROF_LIB) \
		-lrocprofiler-sdk \
		-o $@ kernel_tracer.cpp $(OPTIONS_OBJ)

# Build the C profiler plugin
$(PLUGIN_C): kernel_tracer.c rpv3_options.h $(OPTIONS_OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) \
		-I$(ROCPROF_INCLUDE) \
		-I$(ROCM_PATH)/include/hip \
		-L$(ROCPROF_LIB) \
		-lrocprofiler-sdk \
		-o $@ kernel_tracer.c $(OPTIONS_OBJ)

# Build the example application
$(EXAMPLE): example_app.cpp
	$(HIPCC) --std=c++17 -O2 \
		-o $@ $<

clean:
	rm -f $(PLUGIN_CPP) $(PLUGIN_C) $(EXAMPLE) $(OPTIONS_OBJ)

# Usage instructions
help:
	@echo "ROCm Profiler Kernel Tracer Build"
	@echo ""
	@echo "Targets:"
	@echo "  make all     - Build both plugin and example (default)"
	@echo "  make clean   - Remove built files"
	@echo "  make test    - Run all tests"
	@echo ""
	@echo "Usage:"
	@echo "  HSA_TOOLS_LIB=./libkernel_tracer.so ./example_app"

# Test targets
test: all
	@cd tests && ./run_tests.sh

test-unit: all
	@cd tests && ./run_unit_tests.sh

test-integration: all
	@cd tests && ./test_integration.sh

test-regression: all
	@cd tests && ./test_regression.sh

.PHONY: test test-unit test-integration test-regression
