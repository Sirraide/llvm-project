#!/usr/bin/env bash

set -eu

die() {
    echo "$@" >&2
    exit 1
}

export CC=clang
export CXX=clang++

cmake -G "Ninja" \
  -S llvm \
  -B out-debug \
  -DCMAKE_BUILD_TYPE=Debug \
  -DLLVM_ENABLE_PROJECTS='clang;clang-tools-extra;mlir' \
  -DLLVM_ENABLE_RUNTIMES='libcxx;libcxxabi;libunwind;compiler-rt' \
  -DLLVM_TARGETS_TO_BUILD=X86 \
  -DLLVM_USE_LINKER=mold \
  -DLLVM_ENABLE_BINDINGS=OFF \
  -DLLVM_APPEND_VC_REV=OFF \
  -DLLVM_USE_SPLIT_DWARF=ON \
  -DLLVM_OPTIMIZED_TABLEGEN=ON \
  -DLLVM_ENABLE_ASSERTIONS=ON \
  -DLLVM_UNREACHABLE_OPTIMIZE=OFF \
  -DLLVM_ENABLE_DUMP=ON \
  -DLLVM_CCACHE_BUILD=ON \
  -DLLVM_ENABLE_FFI=ON \
  -DLLVM_INCLUDE_BENCHMARKS=OFF \
  -DLLVM_INCLUDE_EXAMPLES=OFF \
  -DLLVM_ENABLE_IDE=ON \
  -DLLVM_INCLUDE_TESTS=OFF

cmake --build out-debug -- -j $((`nproc` - 2))
