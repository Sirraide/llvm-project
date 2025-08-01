; RUN: not --crash llc -mtriple=amdgcn < %s 2>&1 | FileCheck %s

; CHECK: invalid type for register "exec".

declare i32 @llvm.read_register.i32(metadata) #0

define amdgpu_kernel void @test_invalid_read_exec(ptr addrspace(1) %out) nounwind {
  store volatile i32 0, ptr addrspace(3) poison
  %m0 = call i32 @llvm.read_register.i32(metadata !0)
  store i32 %m0, ptr addrspace(1) %out
  ret void
}

!0 = !{!"exec"}
