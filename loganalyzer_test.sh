#!/usr/bin/env bash
# Test script for loganalyzer
# Author: batoul el sayed mohamaad
# Date: 04/28/2025

set -e

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

pass() { echo -e "${GREEN}✓ PASS:${NC} $1"; }
fail() { echo -e "${RED}✗ FAIL:${NC} $1"; }

echo "============================================"
echo "TESTING LOGANALYZER COMMAND"
echo "============================================"

# Ensure demo.log exists with known content
printf "[INFO] Boot OK\n[WARN] Low battery\n" > demo.log

# Test 1 – Basic scan on demo.log
echo "Test 1: Count lines in demo.log"
if ./loganalyzer -f demo.log | grep -q "Total lines analyzed : 2"; then
  pass "demo.log counts are correct"
else
  fail "Incorrect counts for demo.log"
fi

# Test 2 – Thread flag (-t 4)
echo "Test 2: Multithread scan (4 threads)"
if ./loganalyzer -f demo.log -t 4 | grep -q "Total lines analyzed : 2"; then
  pass "Multithreaded run matches single-thread"
else
  fail "Multithreaded counts mismatch"
fi

# Test 3 – Level filter (-l warn) should drop INFO
echo "Test 3: Severity filter (WARN+)"
if [[ $(./loganalyzer -f demo.log -l warn | grep "Total lines" | awk '{print $5}') == 1 ]]; then
  pass "Filter correctly excluded INFO lines"
else
  fail "Filter did not work as expected"
fi

# Test 4 – Large file sanity (big.log) – just checks it finishes
# Generate big.log if it is missing
if [[ ! -f big.log ]]; then
  python - <<'PY'
levels=["INFO","WARN","ERROR","DEBUG","TRACE"]
with open("big.log","w") as f:
  for i in range(100000):
    lvl=levels[i%len(levels)]
    f.write(f"[{lvl}] message #{i}\n")
PY
fi

echo "Test 4: Large file run (big.log)"
if ./loganalyzer -f big.log -t 8 | grep -q "===== loganalyzer SUMMARY"; then
  pass "Processed big.log successfully"
else
  fail "big.log run failed"
fi

# Test 5 – Invalid file handling
echo "Test 5: Invalid file path"
if ./loganalyzer -f does_not_exist.log 2>&1 | grep -q "No such file"; then
  pass "Properly handled missing file"
else
  fail "Did not report missing file"
fi

# Test 6 – Help flag
echo "Test 6: Help flag"
if ./loganalyzer --help 2>&1 | grep -q "Usage:"; then
  pass "Help text displayed"
else
  fail "Help text missing"
fi

echo "============================================"
echo "TEST SUMMARY COMPLETE"
echo "============================================"
