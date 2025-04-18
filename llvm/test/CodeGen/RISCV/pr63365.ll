; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc < %s -mtriple=riscv32 | FileCheck %s

@G = external global i32
@G.1 = external global i32
@G.2 = external global i64
@G.3 = external global i1
@G.4 = external global i1
@G.5 = external global i8

define void @f() {
; CHECK-LABEL: f:
; CHECK:       # %bb.0: # %BB
; CHECK-NEXT:    addi sp, sp, -16
; CHECK-NEXT:    .cfi_def_cfa_offset 16
; CHECK-NEXT:    addi sp, sp, 16
; CHECK-NEXT:    .cfi_def_cfa_offset 0
; CHECK-NEXT:    ret
BB:
  %A1 = alloca ptr, align 8
  %G4 = getelementptr i1, ptr %A1, i32 65536
  %G = getelementptr i1, ptr %A1, i64 4294967296
  %L2 = load i8, ptr %G4, align 1
  store i8 poison, ptr %G, align 1
  ret void
}
