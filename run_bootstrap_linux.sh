#!/usr/bin/env bash
set -e

for f in 01_hello 02_arithmetic 03_control_flow 04_pointers 05_structs 06_arrays 07_functions; do
    echo "=== Testing $f ==="
    /yukinary/build/stage0_linux --target linux /yukinary/tests/bootstrap/${f}.ykn /yukinary/build/${f}_linux.elf
    chmod +x /yukinary/build/${f}_linux.elf
    /yukinary/build/${f}_linux.elf
    echo "EXIT: $?"
    echo ""
done

