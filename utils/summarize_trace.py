#!/usr/bin/env python3
import csv
import sys
import re
import argparse
from collections import defaultdict

def parse_mnk(log_line):
    """
    Extracts M, N, K from a RocBLAS log line.
    
    RocBLAS log format is comma-separated:
    # rocblas_Xgemm,transA,transB,M,N,K,...
    
    For gemm operations, M, N, K are at positions 3, 4, 5 (0-indexed).
    Also tries to match patterns like 'm=123, n=456, k=789' as fallback.
    
    Returns a string representation "M=..., N=..., K=..." or None.
    """
    # First try comma-separated format (RocBLAS standard)
    if 'gemm' in log_line.lower():
        # Split by comma and try to extract M, N, K
        parts = log_line.split(',')
        if len(parts) >= 6:
            try:
                # Skip the '#' and 'rocblas_Xgemm' (part 0)
                # Skip transA, transB (parts 1, 2)
                # M, N, K are parts 3, 4, 5
                m = parts[3].strip()
                n = parts[4].strip()
                k = parts[5].strip()
                
                # Validate they are numeric
                int(m)
                int(n)
                int(k)
                
                return f"M={m}, N={n}, K={k}"
            except (ValueError, IndexError):
                pass
    
    # Fallback: try to match m=, n=, k= patterns
    m_match = re.search(r'\bm\s*[=:]\s*(\d+)', log_line, re.IGNORECASE)
    n_match = re.search(r'\bn\s*[=:]\s*(\d+)', log_line, re.IGNORECASE)
    k_match = re.search(r'\bk\s*[=:]\s*(\d+)', log_line, re.IGNORECASE)
    
    if m_match and n_match and k_match:
        return f"M={m_match.group(1)}, N={n_match.group(1)}, K={k_match.group(1)}"
    
    return None

def main():
    parser = argparse.ArgumentParser(description="Summarize RPV3 CSV trace output.")
    parser.add_argument("input_file", help="Path to the CSV trace file")
    args = parser.parse_args()

    # Data structure:
    # key = (KernelName, MNK_String)
    # value = {'count': 0, 'total_ns': 0}
    stats = defaultdict(lambda: {'count': 0, 'total_ns': 0})
    
    last_kernel_key = None
    
    try:
        with open(args.input_file, 'r', newline='') as f:
            # We need to handle mixed CSV and comment lines.
            # Standard csv module might choke on comments if we don't filter them,
            # but we NEED the comments.
            # So we'll read line by line.
            
            headers = None
            reader = None
            
            for line in f:
                line = line.strip()
                if not line:
                    continue
                    
                if line.startswith('#'):
                    # This is a log line (potential RocBLAS output)
                    # It applies to the *last seen* kernel dispatch
                    if last_kernel_key:
                        mnk = parse_mnk(line)
                        if mnk:
                            # We found M, N, K.
                            # We need to "move" the stats from the generic key to the specific key
                            # OR just update the key for future aggregation?
                            # The problem is we've already added the stats to the generic key.
                            
                            # Let's retrieve the current stats for the generic key
                            # (KernelName, None)
                            generic_key = last_kernel_key
                            
                            # Check if we just added this kernel (count should be at least 1)
                            # To avoid double counting or complex logic, we can just
                            # subtract from generic and add to specific.
                            # But wait, what if multiple log lines? We only want the first valid one?
                            # Or what if we have multiple kernels of same name?
                            
                            # Simpler approach:
                            # When we parse a kernel, we don't commit it to 'stats' immediately?
                            # No, that's hard because we might not get a log line.
                            
                            # "Move" logic:
                            # 1. Get the generic entry.
                            # 2. Decrement count/time by the *last* added amount? 
                            #    We don't track per-instance time easily here.
                            
                            # Better approach:
                            # Store the last parsed kernel data in a temporary variable
                            # and only commit it when we see the NEXT kernel or EOF.
                            # But the file might be huge.
                            
                            # Let's assume the log line comes immediately after.
                            # We can modify the key of the *last* entry if we keep track of it.
                            # But 'stats' is a dict of aggregates.
                            
                            # Revised approach:
                            # We need to know *which* entry in 'stats' corresponds to the last line.
                            # Since we aggregate, we can't easily "undo" without storing the last delta.
                            pass 

            # Let's restart the logic with a "pending" kernel concept.
            pass
            
    except FileNotFoundError:
        print(f"Error: File not found: {args.input_file}")
        sys.exit(1)

    # Re-implementing with the "pending" logic
    stats = defaultdict(lambda: {'count': 0, 'total_ns': 0})
    
    # pending_kernel = { 'name': ..., 'duration': ..., 'mnk': None }
    pending_kernel = None
    
    try:
        with open(args.input_file, 'r') as f:
            # First, find headers to map columns
            # We'll use csv.reader for the split logic but handle flow manually
            
            # We need to detect the header line first
            # It starts with "KernelName"
            
            csv_headers = None
            
            for line in f:
                line = line.strip()
                if not line:
                    continue
                
                if line.startswith('#'):
                    # Log line
                    if pending_kernel:
                        mnk = parse_mnk(line)
                        if mnk:
                            # Update the pending kernel's MNK info
                            # If multiple lines match, we take the first or last? Let's take first.
                            if pending_kernel['mnk'] is None:
                                pending_kernel['mnk'] = mnk
                    continue
                
                
                # Check if it's the header
                if line.startswith('KernelName,') or line.startswith('"KernelName",'):
                    csv_headers = next(csv.reader([line]))
                    try:
                        name_idx = csv_headers.index('KernelName')
                        dur_idx = csv_headers.index('DurationNs')
                    except ValueError:
                        print("Error: Could not find 'KernelName' or 'DurationNs' in headers.")
                        sys.exit(1)
                    continue
                
                # It's potentially a data line
                # If we have a pending kernel, commit it now
                if pending_kernel:
                    key = (pending_kernel['name'], pending_kernel['mnk'])
                    stats[key]['count'] += 1
                    stats[key]['total_ns'] += pending_kernel['duration']
                    pending_kernel = None
                
                # Parse the new line (only if we have headers)
                if csv_headers:
                    try:
                        row = next(csv.reader([line]))
                        # Validate it's a proper CSV row with correct number of columns
                        if len(row) != len(csv_headers):
                            # Not a valid data row, skip it
                            continue
                            
                        name = row[name_idx]
                        duration = int(row[dur_idx])
                        
                        pending_kernel = {
                            'name': name,
                            'duration': duration,
                            'mnk': None
                        }
                    except (ValueError, IndexError):
                        # Skip malformed lines
                        continue

            # Commit the very last kernel if exists
            if pending_kernel:
                key = (pending_kernel['name'], pending_kernel['mnk'])
                stats[key]['count'] += 1
                stats[key]['total_ns'] += pending_kernel['duration']

    except Exception as e:
        print(f"Error processing file: {e}")
        sys.exit(1)

    # Calculate totals and sort
    results = []
    grand_total_ns = 0
    
    for (name, mnk), data in stats.items():
        count = data['count']
        total_ns = data['total_ns']
        grand_total_ns += total_ns
        
        # Truncate kernel name if too long, BEFORE appending MNK
        # Reserve space for MNK suffix (e.g., " [M=1024, N=1024, K=1024]" is ~30 chars)
        display_name = name
        if mnk:
            # If we have MNK, truncate the kernel name to make room
            max_kernel_len = 80  # Allow more space for long kernel names
            if len(name) > max_kernel_len:
                display_name = name[:max_kernel_len-3] + "..."
            display_name += f" [{mnk}]"
        else:
            # No MNK, just use the kernel name (can be longer)
            max_kernel_len = 110
            if len(name) > max_kernel_len:
                display_name = name[:max_kernel_len-3] + "..."
            
        results.append({
            'name': display_name,
            'count': count,
            'total_ns': total_ns,
            'avg_ns': total_ns / count if count > 0 else 0
        })
        
    # Sort by total time descending
    results.sort(key=lambda x: x['total_ns'], reverse=True)
    
    # Print table (wider to accommodate MNK)
    print(f"{'Kernel Name':<115} | {'Count':>8} | {'Total Time (ms)':>15} | {'Avg Time (us)':>15} | {'% Total':>8}")
    print("-" * 170)
    
    for r in results:
        total_ms = r['total_ns'] / 1_000_000.0
        avg_us = r['avg_ns'] / 1_000.0
        percent = (r['total_ns'] / grand_total_ns * 100.0) if grand_total_ns > 0 else 0.0
        
        print(f"{r['name']:<115} | {r['count']:>8} | {total_ms:>15.3f} | {avg_us:>15.3f} | {percent:>7.1f}%")

if __name__ == "__main__":
    main()
