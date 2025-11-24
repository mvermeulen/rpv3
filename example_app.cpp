#include <hip/hip_runtime.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>

#define HIP_CHECK(cmd) \
    do { \
        hipError_t error = (cmd); \
        if (error != hipSuccess) { \
            fprintf(stderr, "HIP error: %s at %s:%d\n", \
                    hipGetErrorString(error), __FILE__, __LINE__); \
            exit(EXIT_FAILURE); \
        } \
    } while(0)

// Vector addition kernel
__global__ void vectorAdd(const float* a, const float* b, float* c, int n) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        c[idx] = a[idx] + b[idx];
    }
}

// Vector multiplication kernel
__global__ void vectorMul(const float* a, const float* b, float* c, int n) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        c[idx] = a[idx] * b[idx];
    }
}

// Matrix transpose kernel
__global__ void matrixTranspose(const float* input, float* output, int rows, int cols) {
    int col = blockIdx.x * blockDim.x + threadIdx.x;
    int row = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (row < rows && col < cols) {
        output[col * rows + row] = input[row * cols + col];
    }
}

int main() {
    printf("=== ROCm Kernel Tracing Example ===\n\n");
    
    // Get device properties
    int deviceCount = 0;
    HIP_CHECK(hipGetDeviceCount(&deviceCount));
    
    if (deviceCount == 0) {
        printf("No HIP devices found.\n");
        return 1;
    }
    
    hipDeviceProp_t prop;
    HIP_CHECK(hipGetDeviceProperties(&prop, 0));
    printf("Using device: %s\n\n", prop.name);
    
    // ========== Vector Addition ==========
    printf("Launching vector addition kernel...\n");
    const int vecSize = 1024 * 1024;
    size_t vecBytes = vecSize * sizeof(float);
    
    float *h_a, *h_b, *h_c;
    float *d_a, *d_b, *d_c;
    
    // Allocate host memory
    h_a = (float*)malloc(vecBytes);
    h_b = (float*)malloc(vecBytes);
    h_c = (float*)malloc(vecBytes);
    
    // Initialize vectors
    for (int i = 0; i < vecSize; i++) {
        h_a[i] = (float)i;
        h_b[i] = (float)(i * 2);
    }
    
    // Allocate device memory
    HIP_CHECK(hipMalloc(&d_a, vecBytes));
    HIP_CHECK(hipMalloc(&d_b, vecBytes));
    HIP_CHECK(hipMalloc(&d_c, vecBytes));
    
    // Copy to device
    HIP_CHECK(hipMemcpy(d_a, h_a, vecBytes, hipMemcpyHostToDevice));
    HIP_CHECK(hipMemcpy(d_b, h_b, vecBytes, hipMemcpyHostToDevice));
    
    // Launch vector addition kernel
    int blockSize = 256;
    int gridSize = (vecSize + blockSize - 1) / blockSize;
    hipLaunchKernelGGL(vectorAdd, dim3(gridSize), dim3(blockSize), 0, 0, 
                       d_a, d_b, d_c, vecSize);
    
    // Copy result back
    HIP_CHECK(hipMemcpy(h_c, d_c, vecBytes, hipMemcpyDeviceToHost));
    HIP_CHECK(hipDeviceSynchronize());
    
    // Verify result
    bool addCorrect = true;
    for (int i = 0; i < 10; i++) {
        if (fabs(h_c[i] - (h_a[i] + h_b[i])) > 1e-5) {
            addCorrect = false;
            break;
        }
    }
    
    // Cleanup vector addition
    HIP_CHECK(hipFree(d_a));
    HIP_CHECK(hipFree(d_b));
    HIP_CHECK(hipFree(d_c));
    free(h_a);
    free(h_b);
    free(h_c);
    
    // ========== Vector Multiplication ==========
    printf("Launching vector multiplication kernel...\n");
    
    // Allocate host memory
    h_a = (float*)malloc(vecBytes);
    h_b = (float*)malloc(vecBytes);
    h_c = (float*)malloc(vecBytes);
    
    // Initialize vectors
    for (int i = 0; i < vecSize; i++) {
        h_a[i] = (float)(i + 1);
        h_b[i] = 2.0f;
    }
    
    // Allocate device memory
    HIP_CHECK(hipMalloc(&d_a, vecBytes));
    HIP_CHECK(hipMalloc(&d_b, vecBytes));
    HIP_CHECK(hipMalloc(&d_c, vecBytes));
    
    // Copy to device
    HIP_CHECK(hipMemcpy(d_a, h_a, vecBytes, hipMemcpyHostToDevice));
    HIP_CHECK(hipMemcpy(d_b, h_b, vecBytes, hipMemcpyHostToDevice));
    
    // Launch vector multiplication kernel
    hipLaunchKernelGGL(vectorMul, dim3(gridSize), dim3(blockSize), 0, 0,
                       d_a, d_b, d_c, vecSize);
    
    // Copy result back
    HIP_CHECK(hipMemcpy(h_c, d_c, vecBytes, hipMemcpyDeviceToHost));
    HIP_CHECK(hipDeviceSynchronize());
    
    // Verify result
    bool mulCorrect = true;
    for (int i = 0; i < 10; i++) {
        if (fabs(h_c[i] - (h_a[i] * h_b[i])) > 1e-5) {
            mulCorrect = false;
            break;
        }
    }
    
    // Cleanup vector multiplication
    HIP_CHECK(hipFree(d_a));
    HIP_CHECK(hipFree(d_b));
    HIP_CHECK(hipFree(d_c));
    free(h_a);
    free(h_b);
    free(h_c);
    
    // ========== Matrix Transpose ==========
    printf("Launching matrix transpose kernel...\n");
    
    const int matRows = 512;
    const int matCols = 512;
    size_t matBytes = matRows * matCols * sizeof(float);
    
    float *h_input, *h_output;
    float *d_input, *d_output;
    
    // Allocate host memory
    h_input = (float*)malloc(matBytes);
    h_output = (float*)malloc(matBytes);
    
    // Initialize matrix
    for (int i = 0; i < matRows * matCols; i++) {
        h_input[i] = (float)i;
    }
    
    // Allocate device memory
    HIP_CHECK(hipMalloc(&d_input, matBytes));
    HIP_CHECK(hipMalloc(&d_output, matBytes));
    
    // Copy to device
    HIP_CHECK(hipMemcpy(d_input, h_input, matBytes, hipMemcpyHostToDevice));
    
    // Launch matrix transpose kernel
    dim3 blockDim(16, 16);
    dim3 gridDim((matCols + blockDim.x - 1) / blockDim.x,
                 (matRows + blockDim.y - 1) / blockDim.y);
    hipLaunchKernelGGL(matrixTranspose, gridDim, blockDim, 0, 0,
                       d_input, d_output, matRows, matCols);
    
    // Copy result back
    HIP_CHECK(hipMemcpy(h_output, d_output, matBytes, hipMemcpyDeviceToHost));
    HIP_CHECK(hipDeviceSynchronize());
    
    // Verify result
    bool transposeCorrect = true;
    for (int row = 0; row < 10 && transposeCorrect; row++) {
        for (int col = 0; col < 10; col++) {
            float expected = h_input[row * matCols + col];
            float actual = h_output[col * matRows + row];
            if (fabs(expected - actual) > 1e-5) {
                transposeCorrect = false;
                break;
            }
        }
    }
    
    // Cleanup matrix transpose
    HIP_CHECK(hipFree(d_input));
    HIP_CHECK(hipFree(d_output));
    free(h_input);
    free(h_output);
    
    // ========== Summary ==========
    printf("\nAll kernels completed successfully!\n");
    printf("Results verification:\n");
    printf("  Vector Addition: %s\n", addCorrect ? "PASS" : "FAIL");
    printf("  Vector Multiplication: %s\n", mulCorrect ? "PASS" : "FAIL");
    printf("  Matrix Transpose: %s\n", transposeCorrect ? "PASS" : "FAIL");
    
    return 0;
}
