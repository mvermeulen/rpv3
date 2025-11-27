#include <hip/hip_runtime.h>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

__global__ void dummy_kernel() {}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <pipe_path>" << std::endl;
        return 1;
    }

    const char* pipe_path = argv[1];
    
    std::cout << "Mock Logger: Opening pipe " << pipe_path << std::endl;
    // Open pipe for writing
    int fd = open(pipe_path, O_WRONLY);
    if (fd == -1) {
        perror("open pipe");
        return 1;
    }

    // Write log
    const char* msg = "Mock RocBLAS Log Entry\n";
    std::cout << "Mock Logger: Writing to pipe" << std::endl;
    write(fd, msg, strlen(msg));
    close(fd);

    // Launch kernel
    std::cout << "Mock Logger: Launching kernel" << std::endl;
    dummy_kernel<<<1, 1>>>();
    hipDeviceSynchronize();
    std::cout << "Mock Logger: Done" << std::endl;

    return 0;
}
