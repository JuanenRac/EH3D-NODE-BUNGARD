#!/usr/bin/env bash
# EH3D-NODE-BUNGARD: builds and runs the host tests of the core. The firmware itself is built with tools/build_node.sh.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
cmake -S tests -B build/host
cmake --build build/host
ctest --test-dir build/host -C Debug --output-on-failure
