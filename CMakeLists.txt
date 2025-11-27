cmake_minimum_required(VERSION 3.16)
project(rpv3_kernel_tracer VERSION 1.4.0 LANGUAGES C CXX)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find ROCm packages
find_package(rocprofiler-sdk REQUIRED)
find_package(hip REQUIRED)

# Build the options parser as an object library (shared between C and C++ plugins)
add_library(rpv3_options OBJECT rpv3_options.c)
set_target_properties(rpv3_options PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    C_STANDARD 11
)

# Build the profiler plugin library (C++ version)
add_library(kernel_tracer SHARED kernel_tracer.cpp)
target_link_libraries(kernel_tracer 
    PRIVATE 
    rocprofiler-sdk::rocprofiler-sdk
    rpv3_options
)
set_target_properties(kernel_tracer PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
)

# Build the profiler plugin library (C version)
add_library(kernel_tracer_c SHARED kernel_tracer.c)
target_link_libraries(kernel_tracer_c 
    PRIVATE 
    rocprofiler-sdk::rocprofiler-sdk
    rpv3_options
)
set_target_properties(kernel_tracer_c PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    C_VISIBILITY_PRESET hidden
)

# Build the example application using hipcc
# We need to use a custom command because the app contains device code
# Add explicit C++ include paths to work around header detection issues
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/example_app
    COMMAND hipcc 
        -std=c++17 
        -I/usr/include/c++/13
        -I/usr/include/x86_64-linux-gnu/c++/13
        -I/usr/include/c++/13/backward
        -L/usr/lib/gcc/x86_64-linux-gnu/13
        ${CMAKE_CURRENT_SOURCE_DIR}/example_app.cpp 
        -o ${CMAKE_CURRENT_BINARY_DIR}/example_app
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/example_app.cpp
    COMMENT "Building example_app with hipcc"
)

add_custom_target(example_app ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/example_app)

# Build the rocBLAS example application
find_package(rocblas REQUIRED)
add_executable(example_rocblas example_rocblas.cpp)
target_link_libraries(example_rocblas PRIVATE roc::rocblas hip::host)

# Enable testing
enable_testing()
add_subdirectory(tests)

# Installation rules
install(TARGETS kernel_tracer kernel_tracer_c
    LIBRARY DESTINATION lib
)
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/example_app
    DESTINATION bin
)
