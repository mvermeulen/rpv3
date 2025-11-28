#!/usr/bin/env python3
import os
import re
import subprocess
import sys
import time

def parse_readme(readme_path):
    with open(readme_path, 'r') as f:
        content = f.read()

    # Find RocBLAS Logging section
    section_match = re.search(r'### RocBLAS Logging', content)
    if not section_match:
        print("Error: Could not find 'RocBLAS Logging' section in README.md")
        return None

    # Find the code block after the section
    # Look for ```bash ... ```
    code_block_pattern = r'```bash\n(.*?)```'
    code_blocks = list(re.finditer(code_block_pattern, content[section_match.end():], re.DOTALL))
    
    if not code_blocks:
        print("Error: Could not find bash code block in RocBLAS Logging section")
        return None

    # The first block should be the usage example
    return code_blocks[0].group(1)

def run_example(script_content):
    print("Extracted script content:")
    print("-" * 40)
    print(script_content)
    print("-" * 40)

    # Create a temporary script file
    script_name = "temp_readme_example.sh"
    with open(script_name, 'w') as f:
        f.write("#!/bin/bash\n")
        f.write("set -e\n") # Exit on error
        f.write(script_content)

    os.chmod(script_name, 0o755)

    # Clean up any existing artifacts
    if os.path.exists("rocblas_log_pipe"):
        os.remove("rocblas_log_pipe")
    if os.path.exists("rocblas.log"):
        os.remove("rocblas.log")

    print("Running extracted script...")
    try:
        # Capture output
        result = subprocess.run(
            ["./" + script_name],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            timeout=30 # Timeout to prevent hanging
        )
        
        print("Script output:")
        print(result.stdout)

        if result.returncode != 0:
            print(f"Error: Script failed with return code {result.returncode}")
            return False

        # Verify output contains RocBLAS logs
        # The script runs two commands. Both should produce logs.
        # We check if the output contains at least one RocBLAS log entry.
        # RocBLAS logs start with "# " and contain rocblas function names.
        # But our tracer prefixes them with "# ".
        
        if "# rocblas_" in result.stdout:
            print("SUCCESS: Found RocBLAS logs in output.")
            return True
        else:
            print("FAILURE: Did not find RocBLAS logs in output.")
            return False

    except subprocess.TimeoutExpired:
        print("Error: Script timed out")
        return False
    finally:
        # Cleanup
        if os.path.exists(script_name):
            os.remove(script_name)
        if os.path.exists("rocblas_log_pipe"):
            os.remove("rocblas_log_pipe")
        if os.path.exists("rocblas.log"):
            os.remove("rocblas.log")

def main():
    # Assume we are in the project root or tests directory
    # Find README.md
    if os.path.exists("README.md"):
        readme_path = "README.md"
    elif os.path.exists("../README.md"):
        readme_path = "../README.md"
        os.chdir("..") # Move to root to run examples
    else:
        print("Error: Could not find README.md")
        sys.exit(1)

    print(f"Parsing {readme_path}...")
    script_content = parse_readme(readme_path)
    
    if script_content:
        if run_example(script_content):
            sys.exit(0)
        else:
            sys.exit(1)
    else:
        sys.exit(1)

if __name__ == "__main__":
    main()
