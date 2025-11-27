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
EXAMPLE_ROCBLAS = example_rocblas
OPTIONS_OBJ = rpv3_options.o
UTILS_DIR = utils
UTILS_BIN = $(UTILS_DIR)/check_status $(UTILS_DIR)/diagnose_counters

.PHONY: all clean utils

all: $(PLUGIN_CPP) $(PLUGIN_C) $(EXAMPLE) $(EXAMPLE_ROCBLAS)

# Build utilities
utils: $(UTILS_BIN)

$(UTILS_DIR)/check_status: $(UTILS_DIR)/check_status.cpp
	$(HIPCC) --std=c++17 -O2 \
		-I$(ROCPROF_INCLUDE) \
		-L$(ROCPROF_LIB) \
		-lrocprofiler-sdk \
		-o $@ $<

$(UTILS_DIR)/diagnose_counters: $(UTILS_DIR)/diagnose_counters.cpp
	$(HIPCC) --std=c++17 -O2 \
		-I$(ROCPROF_INCLUDE) \
		-L$(ROCPROF_LIB) \
		-lrocprofiler-sdk \
		-o $@ $<

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

# Build the rocBLAS example application
$(EXAMPLE_ROCBLAS): example_rocblas.cpp
	$(HIPCC) --std=c++17 -O2 \
		-I$(ROCM_PATH)/include \
		-L$(ROCM_PATH)/lib \
		-lrocblas \
		-o $@ $<

clean:
	rm -f $(PLUGIN_CPP) $(PLUGIN_C) $(EXAMPLE) $(EXAMPLE_ROCBLAS) $(OPTIONS_OBJ) $(UTILS_BIN)

# Usage instructions
help:
	@echo "ROCm Profiler Kernel Tracer Build"
	@echo ""
	@echo "Targets:"
	@echo "  make all     - Build both plugin and example (default)"
	@echo "  make clean   - Remove built files"
	@echo "  make utils   - Build utility tools"
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
