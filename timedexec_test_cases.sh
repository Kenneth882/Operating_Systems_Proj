#!/bin/bash

# Bash script to test various timedexec command scenarios

# Test 1: Normal execution within time limit
./timedexec --time 4.5 sleep 3

# Test 2: Missing argument to sleep (should fail)
./timedexec --time 4.5 sleep

# Test 3: Missing command entirely (should error out)
./timedexec --

# Test 4: Exact match with timeout, may terminate
./timedexec --time 5 sleep 5

# Test 5: Invalid sleep argument (negative number)
./timedexec --time 5 sleep -5

# Test 6: Display help using -h
./timedexec -h

# Test 7: Use short option -t for timeout
./timedexec -t 5 sleep 5

# Test 8: Echo within time limit
./timedexec --time 2 echo "test 1"

# Test 9: Repeat echo for consistency
./timedexec --time 2 echo "test 1"