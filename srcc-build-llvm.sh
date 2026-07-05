#!/usr/bin/env bash

set -eu

die() {
    echo "$@" >&2
    exit 1
}

export CC=clang
export CXX=clang++

## We need to build compiler-rt too because we need ASAN.
cmake -G "Ninja" \
  -S llvm \
  -B out \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_ENABLE_PROJECTS='clang;clang-tools-extra;lld;mlir' \
  -DLLVM_ENABLE_RUNTIMES='libcxx;libcxxabi;libunwind;compiler-rt' \
  -DLLVM_TARGETS_TO_BUILD=X86 \
  -DLLVM_USE_LINKER=mold \
  -DLLVM_ENABLE_BINDINGS=OFF \
  -DLLVM_APPEND_VC_REV=OFF \
  -DLLVM_OPTIMIZED_TABLEGEN=ON \
  -DLLVM_ENABLE_UNWIND_TABLES=OFF \
  -DLLVM_ENABLE_ASSERTIONS=ON \
  -DLLVM_UNREACHABLE_OPTIMIZE=OFF \
  -DLLVM_ENABLE_DUMP=ON \
  -DLLVM_CCACHE_BUILD=ON \
  -DLLVM_ENABLE_FFI=ON \
  -DLLVM_INCLUDE_BENCHMARKS=OFF \
  -DLLVM_INCLUDE_EXAMPLES=OFF \
  -DLIBCXX_INCLUDE_TESTS=OFF \
  -DLIBCXX_INCLUDE_BENCHMARKS=OFF \
  -DLLVM_ENABLE_IDE=ON \
  -DCLANG_ENABLE_OBJC_REWRITER=OFF

cmake --build out -- -j $((`nproc` - 2))
