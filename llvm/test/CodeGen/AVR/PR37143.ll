; RUN: llc -mattr=avr6,sram < %s -mtriple=avr -verify-machineinstrs | FileCheck %s

; CHECK: ld {{r[0-9]+}}, [[PTR:[XYZ]]]
; CHECK: ldd {{r[0-9]+}}, [[PTR]]+1
; CHECK: std [[PTR2:[XYZ]]]+1, {{r[0-9]+}}
; CHECK: st [[PTR2]], {{r[0-9]+}}
define void @load_store_16(ptr nocapture %ptr) local_unnamed_addr #1 {
entry:
  %0 = load i16, ptr %ptr, align 2
  %add = add i16 %0, 5
  store i16 %add, ptr %ptr, align 2
  ret void
}
