#!/bin/bash

HIPCC=${HIPCC:-hipcc}

# Check for required build artifacts
if [ ! -f "./libkernel_tracer.so" ] || [ ! -f "./example_rocblas" ]; then
    echo "Error: Build artifacts not found (libkernel_tracer.so or example_rocblas missing)."
    echo "Please run 'make' to build the tracer and 'make example_rocblas' to build the application."
    exit 1
fi

# Compile a dummy app (reuse test_pipe_2 or create a simple one)
# We need an app that launches a kernel. 
# Since we don't have a real GPU app handy that we can easily control to sync with pipe writes,
# we might need to rely on the fact that the tracer works with *any* HIP app.
# But wait, we need to simulate the *write* to the pipe happening *before* the kernel dispatch finishes.
# Or at least available when the dispatch callback runs.

# Let's create a simple dummy app that simulates a "kernel" by just sleeping?
# No, the tracer hooks into HIP. We need a HIP app.
# We can use the 'vector_add' example if available, or 'example_app'.
# 'example_app' is in the repo.



FIFO_PATH="/tmp/rocblas_log_fifo"
rm -f $FIFO_PATH output.csv
mkfifo $FIFO_PATH

echo "Created FIFO at $FIFO_PATH"

# Start a background process to write to the FIFO
# We'll write multiple lines to ensure one is picked up.
(
    echo "Writing to FIFO..."
    echo "rocBLAS kernel launch info: size=1024, alpha=1.0" > $FIFO_PATH
    echo "Another log entry" > $FIFO_PATH
) &
BG_PID=$!

# Run the app with the tracer and environment variables
echo "Running app with tracer..."
export RPV3_OPTIONS="--csv --rocblas $FIFO_PATH"
export ROCBLAS_LAYER=1
export ROCBLAS_LOG_TRACE=$FIFO_PATH
export LD_PRELOAD=./libkernel_tracer.so

./example_rocblas > output.csv 2>&1
# Kill background writer if still running
kill $BG_PID 2>/dev/null
wait $BG_PID 2>/dev/null

echo "Checking output..."
cat output.csv

if grep -q "# rocBLAS kernel launch info" output.csv; then
    echo "SUCCESS: Found rocBLAS log in trace output"
else
    echo "FAILURE: Did not find rocBLAS log in trace output"
    exit 1
fi

# Compile the mock logger
echo "Compiling mock logger..."
$HIPCC -o mock_rocblas_logger tests/mock_rocblas_logger.cpp

# Test 2: Verify --rocblas-log redirection
echo "Testing --rocblas-log redirection..."
rm -f $FIFO_PATH output.csv rocblas_test.log
mkfifo $FIFO_PATH

export RPV3_OPTIONS="--csv --rocblas $FIFO_PATH --rocblas-log rocblas_test.log"
# We don't need ROCBLAS env vars for the mock logger as it writes directly to the pipe passed as arg
# export ROCBLAS_LAYER=1
# export ROCBLAS_LOG_TRACE=$FIFO_PATH

./mock_rocblas_logger $FIFO_PATH > output.csv 2>&1 &
APP_PID=$!

# Wait for app to finish
wait $APP_PID

if [ -f "rocblas_test.log" ] && grep -q "Mock RocBLAS Log Entry" rocblas_test.log; then
    echo "SUCCESS: Found mock log in redirected file"
else
    echo "FAILURE: Did not find mock log in redirected file"
    if [ -f "rocblas_test.log" ]; then
        echo "File content:"
        cat rocblas_test.log
    else
        echo "File rocblas_test.log not created"
    fi
    echo "Trace Output (output.csv):"
    cat output.csv
    exit 1
fi

rm -f $FIFO_PATH output.csv rocblas_test.log
exit 0
