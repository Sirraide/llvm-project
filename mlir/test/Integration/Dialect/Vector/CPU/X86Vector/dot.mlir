// RUN: mlir-opt %s -convert-vector-to-llvm="enable-x86vector" -test-lower-to-llvm | \
// RUN: mlir-translate --mlir-to-llvmir | \
// RUN: %lli --entry-function=entry --mattr="avx" --dlopen=%mlir_c_runner_utils | \
// RUN: FileCheck %s

func.func @entry() -> i32 {
  %i0 = arith.constant 0 : i32
  %i4 = arith.constant 4 : i32

  %a = arith.constant dense<[1.0,  2.0,  3.0,  4.0,  5.0,  6.0,  7.0,  8.0]> : vector<8xf32>
  %b = arith.constant dense<[9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0]> : vector<8xf32>
  %r = x86vector.avx.intr.dot %a, %b : vector<8xf32>

  %1 = vector.extract %r[%i0] : f32 from vector<8xf32>
  %2 = vector.extract %r[%i4] : f32 from vector<8xf32>
  %d = arith.addf %1, %2 : f32

  // CHECK: ( 110, 110, 110, 110, 382, 382, 382, 382 )
  // CHECK: 492
  vector.print %r : vector<8xf32>
  vector.print %d : f32

  return %i0 : i32
}
