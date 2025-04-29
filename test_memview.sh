#!/bin/bash

# Test script for memview command
# Author: batoul el sayed mohamaad
# Date: 04/28/2025

echo "============================================"
echo "TESTING MEMVIEW COMMAND"
echo "============================================"

# Test 1: Basic functionality - Process memory information
echo "Test 1: Process memory info for PID 1"
sudo ./memview -p 1 | grep -q "\[heap\]"
if [ $? -eq 0 ]; then
    echo "✓ PASS: Successfully displayed process memory info"
else
    echo "✗ FAIL: Could not display process memory info"
fi

# Test 2: System memory information
echo "Test 2: System memory info"
./memview -s | grep -q "MemTotal"
if [ $? -eq 0 ]; then
    echo "✓ PASS: Successfully displayed system memory info"
else
    echo "✗ FAIL: Could not display system memory info"
fi

# Test 3: Shared memory segments
echo "Test 3: Shared memory segments"
./memview -m
if [ $? -eq 0 ]; then
    echo "✓ PASS: Successfully displayed shared memory segments"
else
    echo "✗ FAIL: Could not display shared memory segments"
fi

# Test 4: Filter functionality
echo "Test 4: Filter functionality"
sudo ./memview -p 1 -f heap
if [ $? -eq 0 ]; then
    echo "✓ PASS: Successfully filtered memory regions"
else
    echo "✗ FAIL: Could not filter memory regions"
fi

# Test 5: Verbose mode
echo "Test 5: Verbose mode"
sudo ./memview -p 1 -v | grep -q "Address Range"
if [ $? -eq 0 ]; then
    echo "✓ PASS: Successfully displayed verbose output"
else
    echo "✗ FAIL: Could not display verbose output"
fi

# Test 6: Error handling - Invalid PID
echo "Test 6: Error handling - Invalid PID"
./memview -p 999999 2>&1 | grep -q "Error"
if [ $? -eq 0 ]; then
    echo "✓ PASS: Properly handled invalid PID"
else
    echo "✗ FAIL: Did not properly handle invalid PID"
fi

# Test 7: Error handling - Missing arguments
echo "Test 7: Error handling - Missing arguments"
./memview 2>&1 | grep -q "Usage"
if [ $? -eq 0 ]; then
    echo "✓ PASS: Properly displayed usage information"
else
    echo "✗ FAIL: Did not display usage information"
fi

echo "============================================"
echo "TEST SUMMARY"
echo "============================================"
