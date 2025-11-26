#include <rocprofiler-sdk/rocprofiler.h>
#include <stdio.h>

int main() {
    printf("Status 39: %s\n", rocprofiler_get_status_string((rocprofiler_status_t)39));
    return 0;
}
