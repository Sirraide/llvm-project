; NOTE: Assertions have been autogenerated by utils/update_test_checks.py UTC_ARGS: --version 5
; RUN: opt -S -passes=instcombine < %s | FileCheck %s

; Make sure this does not crash.
define void @test(ptr %arg) {
; CHECK-LABEL: define void @test(
; CHECK-SAME: ptr [[ARG:%.*]]) {
; CHECK-NEXT:    store i1 true, ptr poison, align 1
; CHECK-NEXT:    ret void
;
  %a = alloca i32
  store ptr %a, ptr %arg
  store i1 true, ptr poison
  call void @llvm.lifetime.end.p0(i64 4, ptr %a)
  ret void
}
