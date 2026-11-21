#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
task_cxx_flags=""
task_link_flags=""
if command -v cmake >/dev/null && command -v g++ >/dev/null; then
    task_cxx="$(command -v g++)"
elif [[ -x /tmp/mapf-toolchain/root/usr/bin/g++-13 && -x /tmp/mapf-toolchain/root/usr/bin/cmake ]]; then
    task_toolchain=/tmp/mapf-toolchain/root
    export PATH="$task_toolchain/usr/bin:$PATH"
    export LD_LIBRARY_PATH="$task_toolchain/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    task_cxx="$task_toolchain/usr/bin/g++-13"
    task_cxx_flags="-I$task_toolchain/usr/include -I$task_toolchain/usr/include/x86_64-linux-gnu"
    task_link_flags="-L$task_toolchain/usr/lib/x86_64-linux-gnu"
else
    echo 'Cần CMake, G++ và Make. Trên Ubuntu, chạy: sudo apt install build-essential cmake python3' >&2
    exit 1
fi
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_COMPILER="$task_cxx" \
    -DCMAKE_CXX_FLAGS="$task_cxx_flags" \
    -DCMAKE_EXE_LINKER_FLAGS="$task_link_flags"
cmake --build build -j 2
TV4_CLI="$PWD/build/tv4_cli" ctest --test-dir build --output-on-failure
