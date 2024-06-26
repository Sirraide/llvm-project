; RUN: llc -mtriple=arm64-apple-ios -aarch64-min-jump-table-entries=4 < %s | FileCheck %s
; RUN: llc -mtriple=arm64-linux-gnu -aarch64-min-jump-table-entries=4 < %s | FileCheck %s --check-prefix=CHECK-LINUX
; <rdar://11417675>

define void @sum(i32 %a, ptr %to, i32 %c) {
entry:
  switch i32 %a, label %exit [
    i32 1, label %bb1
    i32 2, label %exit.sink.split
    i32 3, label %bb3
    i32 4, label %bb4
  ]
bb1:
  %b = add i32 %c, 1
  br label %exit.sink.split
bb3:
  br label %exit.sink.split
bb4:
  br label %exit.sink.split
exit.sink.split:
  %.sink = phi i32 [ 5, %bb4 ], [ %b, %bb1 ], [ 7, %bb3 ], [ %a, %entry ]
  store i32 %.sink, ptr %to
  br label %exit
exit:
  ret void
}

; CHECK-LABEL: sum:
; CHECK: adrp    {{x[0-9]+}}, LJTI0_0@PAGE
; CHECK:  add    {{x[0-9]+}}, {{x[0-9]+}}, LJTI0_0@PAGEOFF

; CHECK-LINUX-LABEL: sum:
; CHECK-LINUX: adrp    {{x[0-9]+}}, .LJTI0_0
; CHECK-LINUX:  add    {{x[0-9]+}}, {{x[0-9]+}}, :lo12:.LJTI0_0
