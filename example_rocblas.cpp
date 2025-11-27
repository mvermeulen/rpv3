/*
 * Simple rocBLAS Example
 * Performs a matrix multiplication (SGEMM): C = alpha * A * B + beta * C
 * Used to verify rocBLAS logging in the kernel tracer.
 */

#include <iostream>
#include <vector>
#include <hip/hip_runtime.h>
#include <rocblas/rocblas.h>

#define CHECK_HIP(status) \
    if (status != hipSuccess) { \
        std::cerr << "HIP Error: " << hipGetErrorString(status) << std::endl; \
        return 1; \
    }

#define CHECK_ROCBLAS(status) \
    if (status != rocblas_status_success) { \
        std::cerr << "rocBLAS Error: " << status << std::endl; \
        return 1; \
    }

int main() {
    std::cout << "rocBLAS Example: SGEMM" << std::endl;

    rocblas_int m = 1024;
    rocblas_int n = 1024;
    rocblas_int k = 1024;

    float alpha = 1.0f;
    float beta = 0.0f;

    std::cout << "Matrix size: " << m << "x" << n << "x" << k << std::endl;

    // Allocate host memory
    std::vector<float> h_a(m * k);
    std::vector<float> h_b(k * n);
    std::vector<float> h_c(m * n);

    // Initialize host memory
    for (int i = 0; i < m * k; ++i) h_a[i] = 1.0f;
    for (int i = 0; i < k * n; ++i) h_b[i] = 1.0f;
    for (int i = 0; i < m * n; ++i) h_c[i] = 0.0f;

    // Allocate device memory
    float *d_a, *d_b, *d_c;
    CHECK_HIP(hipMalloc(&d_a, m * k * sizeof(float)));
    CHECK_HIP(hipMalloc(&d_b, k * n * sizeof(float)));
    CHECK_HIP(hipMalloc(&d_c, m * n * sizeof(float)));

    // Copy data to device
    CHECK_HIP(hipMemcpy(d_a, h_a.data(), m * k * sizeof(float), hipMemcpyHostToDevice));
    CHECK_HIP(hipMemcpy(d_b, h_b.data(), k * n * sizeof(float), hipMemcpyHostToDevice));
    CHECK_HIP(hipMemcpy(d_c, h_c.data(), m * n * sizeof(float), hipMemcpyHostToDevice));

    // Create rocBLAS handle
    rocblas_handle handle;
    CHECK_ROCBLAS(rocblas_create_handle(&handle));

    std::cout << "Launching SGEMM kernel..." << std::endl;

    // Perform SGEMM
    // Note: rocBLAS is column-major
    CHECK_ROCBLAS(rocblas_sgemm(handle,
                                rocblas_operation_none, rocblas_operation_none,
                                m, n, k,
                                &alpha,
                                d_a, m,
                                d_b, k,
                                &beta,
                                d_c, m));

    // Wait for completion
    CHECK_HIP(hipDeviceSynchronize());

    std::cout << "SGEMM completed successfully." << std::endl;

    // Clean up
    CHECK_ROCBLAS(rocblas_destroy_handle(handle));
    CHECK_HIP(hipFree(d_a));
    CHECK_HIP(hipFree(d_b));
    CHECK_HIP(hipFree(d_c));

    return 0;
}
