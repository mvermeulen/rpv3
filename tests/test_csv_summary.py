#!/usr/bin/env python3
import os
import sys
import subprocess
import tempfile
import re

def main():
    # Create a mock CSV file with some RocBLAS logs
    csv_content = """KernelName,ThreadID,CorrelationID,KernelID,DispatchID,GridX,GridY,GridZ,WorkgroupX,WorkgroupY,WorkgroupZ,PrivateSeg,GroupSeg,StartTimestamp,EndTimestamp,DurationNs,DurationUs,TimeSinceStartMs
"test_kernel_A",1,1,1,1,1,1,1,1,1,1,0,0,1000,2000,1000,1.0,0.001
# rocblas_Xgemm(..., m=1024, n=2048, k=512, ...)
"test_kernel_A",1,2,1,2,1,1,1,1,1,1,0,0,3000,5000,2000,2.0,0.003
# rocblas_Xgemm(..., m=1024, n=2048, k=512, ...)
"test_kernel_B",1,3,2,3,1,1,1,1,1,1,0,0,6000,6500,500,0.5,0.006
"test_kernel_A",1,4,1,4,1,1,1,1,1,1,0,0,7000,8500,1500,1.5,0.007
# rocblas_Xgemm(..., m=128, n=128, k=128, ...)
"""
    
    with tempfile.NamedTemporaryFile(mode='w', suffix='.csv', delete=False) as tmp:
        tmp.write(csv_content)
        tmp_path = tmp.name

    try:
        # Run the summary tool
        tool_path = os.path.join(os.path.dirname(__file__), '../utils/summarize_trace.py')
        result = subprocess.run([sys.executable, tool_path, tmp_path], capture_output=True, text=True)
        
        if result.returncode != 0:
            print("Tool failed with error:")
            print(result.stderr)
            sys.exit(1)
            
        output = result.stdout
        print("Tool Output:")
        print(output)
        
        # Verification checks
        
        # 1. Check for Kernel A with M=1024... (should be 2 calls, total 3000ns)
        # Note: The tool groups by (Name, MNK).
        # We have:
        # - A, MNK(1024,2048,512): 1000ns (from first line? Wait, log follows line 1?)
        #   Line 1: A, 1000ns. Log follows: m=1024... -> So this A is 1024...
        #   Line 2: A, 2000ns. Log follows: m=1024... -> So this A is 1024...
        #   Total A(1024...): 3000ns, count 2.
        
        # - B, No MNK: 500ns, count 1.
        
        # - A, MNK(128...): 1500ns, count 1.
        
        if "test_kernel_A [M=1024, N=2048, K=512]" not in output:
            print("FAIL: Missing grouped kernel A with MNK 1024...")
            sys.exit(1)
            
        if "test_kernel_A [M=128, N=128, K=128]" not in output:
            print("FAIL: Missing grouped kernel A with MNK 128...")
            sys.exit(1)
            
        if "test_kernel_B" not in output:
            print("FAIL: Missing kernel B")
            sys.exit(1)
            
        # Check counts/times (rough string check)
        # A(1024) should have count 2
        # We can regex search the output line
        # "test_kernel_A [M=1024, N=2048, K=512] ... |        2 |"
        
        if not re.search(r"test_kernel_A \[M=1024, N=2048, K=512\].*\|\s*2\s*\|", output):
             print("FAIL: Incorrect count for A(1024...)")
             sys.exit(1)

        print("SUCCESS: All checks passed.")
        
    finally:
        os.remove(tmp_path)

if __name__ == "__main__":
    main()
