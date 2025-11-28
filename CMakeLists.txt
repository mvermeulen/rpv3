cmake_minimum_required(VERSION 3.16)
project(rpv3_kernel_tracer VERSION 1.5.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find ROCm packages
list(APPEND CMAKE_PREFIX_PATH "/opt/rocm")
find_package(rocprofiler-sdk REQUIRED)
find_package(hip REQUIRED)

# Options object library
add_library(rpv3_options OBJECT rpv3_options.c)
target_include_directories(rpv3_options PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# C++ Plugin
add_library(kernel_tracer SHARED kernel_tracer.cpp $<TARGET_OBJECTS:rpv3_options>)
target_link_libraries(kernel_tracer PRIVATE rocprofiler-sdk::rocprofiler-sdk)
target_include_directories(kernel_tracer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# C Plugin
add_library(kernel_tracer_c SHARED kernel_tracer.c $<TARGET_OBJECTS:rpv3_options>)
target_link_libraries(kernel_tracer_c PRIVATE rocprofiler-sdk::rocprofiler-sdk)
target_include_directories(kernel_tracer_c PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Example App
add_executable(example_app example_app.cpp)
target_link_libraries(example_app PRIVATE hip::host)

# RocBLAS Example App
# Try to find rocblas, otherwise warn
find_package(rocblas QUIET)
if(rocblas_FOUND)
    add_executable(example_rocblas example_rocblas.cpp)
    target_link_libraries(example_rocblas PRIVATE roc::rocblas)
else()
    message(STATUS "rocBLAS not found via CMake, trying manual link")
    # Fallback for manual linking if cmake config not found
    add_executable(example_rocblas example_rocblas.cpp)
    target_include_directories(example_rocblas PRIVATE /opt/rocm/include)
    target_link_directories(example_rocblas PRIVATE /opt/rocm/lib)
    target_link_libraries(example_rocblas PRIVATE rocblas)
endif()

# Testing
enable_testing()
add_subdirectory(tests)
