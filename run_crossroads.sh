#!/bin/bash
set -e

DEFAULT_PARAM="aAA:bBD:cCD:dDB:fAB5.12:gAC6.13"
PARAMETER=${1:-$DEFAULT_PARAM}

# Check if threads/build directory exists
if [ ! -d "threads/build" ]; then
  echo "[+] threads/build directory not found, running make in threads/"
  cd threads/
  make
  cd ..
fi

echo "[+] Cleaning and building..."
cd threads/build/
make clean > ../../make_clean_result 2>&1
cd ../
make > ../make_result 2>&1

echo "[+] Running Pintos with crossroads $PARAMETER..."
cd build/
../../utils/pintos crossroads "$PARAMETER" 2>&1 | tee ../../output.txt

