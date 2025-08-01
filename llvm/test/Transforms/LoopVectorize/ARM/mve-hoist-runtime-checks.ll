; NOTE: Assertions have been autogenerated by utils/update_test_checks.py UTC_ARGS: --version 2
; REQUIRES: asserts
; RUN: opt < %s -hoist-runtime-checks -p 'loop-vectorize' -force-vector-interleave=1 -S \
; RUN:   -force-vector-width=4 -debug-only=loop-accesses,loop-vectorize,loop-utils 2> %t | FileCheck %s
; RUN: cat %t | FileCheck %s --check-prefix=DEBUG

target datalayout = "e-m:e-p:32:32-Fi8-i64:64-v128:64:128-a:0:32-n32-S64"
target triple = "thumbv8.1m.main-none-unknown-eabi"

; Equivalent example in C:
; void diff_checks(int32_t *dst, int32_t *src, int m, int n) {
;   for (int i = 0; i < m; i++) {
;     for (int j = 0; j < n; j++) {
;       dst[(i * (n + 1)) + j] = src[(i * n) + j];
;     }
;   }
; }
; NOTE: The strides of the starting address values in the inner loop differ, i.e.
; '(i * (n + 1))' vs '(i * n)'.

; DEBUG-LABEL: 'diff_checks'
; DEBUG:      LAA: Found an analyzable loop: inner.loop
; DEBUG:      LAA: Not creating diff runtime check, since these  cannot be hoisted out of the outer loop
; DEBUG:      LAA: Adding RT check for range:
; DEBUG-NEXT: LAA: Expanded RT check for range to include outer loop in order to permit hoisting
; DEBUG-NEXT: LAA: ... but need to check stride is positive: (4 + (4 * %n))
; DEBUG-NEXT: Start: %dst End: ((4 * %n) + ((4 + (4 * %n)) * (-1 + %m)) + %dst)
; DEBUG-NEXT: LAA: Adding RT check for range:
; DEBUG-NEXT: LAA: Expanded RT check for range to include outer loop in order to permit hoisting
; DEBUG-NEXT: LAA: ... but need to check stride is positive: (4 * %n)
; DEBUG-NEXT: Start: %src End: ((4 * %m * %n) + %src)

define void @diff_checks(ptr nocapture noundef writeonly %dst, ptr nocapture noundef readonly %src, i32 noundef %m, i32 noundef %n) #0 {
; CHECK-LABEL: define void @diff_checks
; CHECK-SAME: (ptr noundef writeonly captures(none) [[DST:%.*]], ptr noundef readonly captures(none) [[SRC:%.*]], i32 noundef [[M:%.*]], i32 noundef [[N:%.*]]) #[[ATTR0:[0-9]+]] {
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[ADD5:%.*]] = add nsw i32 [[N]], 1
; CHECK-NEXT:    [[TMP0:%.*]] = add i32 [[M]], -1
; CHECK-NEXT:    [[TMP1:%.*]] = shl i32 [[N]], 2
; CHECK-NEXT:    [[TMP2:%.*]] = add i32 [[TMP1]], 4
; CHECK-NEXT:    [[TMP3:%.*]] = mul i32 [[TMP0]], [[TMP2]]
; CHECK-NEXT:    [[TMP4:%.*]] = add i32 [[TMP3]], [[TMP1]]
; CHECK-NEXT:    [[SCEVGEP:%.*]] = getelementptr i8, ptr [[DST]], i32 [[TMP4]]
; CHECK-NEXT:    [[TMP5:%.*]] = mul i32 [[N]], [[M]]
; CHECK-NEXT:    [[TMP6:%.*]] = shl i32 [[TMP5]], 2
; CHECK-NEXT:    [[SCEVGEP1:%.*]] = getelementptr i8, ptr [[SRC]], i32 [[TMP6]]
; CHECK-NEXT:    br label [[OUTER_LOOP:%.*]]
; CHECK:       outer.loop:
; CHECK-NEXT:    [[I_023_US:%.*]] = phi i32 [ [[INC10_US:%.*]], [[INNER_LOOP_EXIT:%.*]] ], [ 0, [[ENTRY:%.*]] ]
; CHECK-NEXT:    [[MUL_US:%.*]] = mul nsw i32 [[I_023_US]], [[N]]
; CHECK-NEXT:    [[TMP7:%.*]] = getelementptr i32, ptr [[SRC]], i32 [[MUL_US]]
; CHECK-NEXT:    [[MUL6_US:%.*]] = mul nsw i32 [[I_023_US]], [[ADD5]]
; CHECK-NEXT:    [[TMP8:%.*]] = getelementptr i32, ptr [[DST]], i32 [[MUL6_US]]
; CHECK-NEXT:    br i1 false, label [[SCALAR_PH:%.*]], label [[VECTOR_MEMCHECK:%.*]]
; CHECK:       vector.memcheck:
; CHECK-NEXT:    [[BOUND0:%.*]] = icmp ult ptr [[DST]], [[SCEVGEP1]]
; CHECK-NEXT:    [[BOUND1:%.*]] = icmp ult ptr [[SRC]], [[SCEVGEP]]
; CHECK-NEXT:    [[FOUND_CONFLICT:%.*]] = and i1 [[BOUND0]], [[BOUND1]]
; CHECK-NEXT:    [[STRIDE_CHECK:%.*]] = icmp slt i32 [[TMP2]], 0
; CHECK-NEXT:    [[TMP9:%.*]] = or i1 [[FOUND_CONFLICT]], [[STRIDE_CHECK]]
; CHECK-NEXT:    [[STRIDE_CHECK2:%.*]] = icmp slt i32 [[TMP1]], 0
; CHECK-NEXT:    [[TMP10:%.*]] = or i1 [[TMP9]], [[STRIDE_CHECK2]]
; CHECK-NEXT:    br i1 [[TMP10]], label [[SCALAR_PH]], label [[VECTOR_PH:%.*]]
; CHECK:       vector.ph:
; CHECK-NEXT:    [[N_RND_UP:%.*]] = add i32 [[N]], 3
; CHECK-NEXT:    [[N_MOD_VF:%.*]] = urem i32 [[N_RND_UP]], 4
; CHECK-NEXT:    [[N_VEC:%.*]] = sub i32 [[N_RND_UP]], [[N_MOD_VF]]
; CHECK-NEXT:    br label [[VECTOR_BODY:%.*]]
; CHECK:       vector.body:
; CHECK-NEXT:    [[INDEX:%.*]] = phi i32 [ 0, [[VECTOR_PH]] ], [ [[INDEX_NEXT:%.*]], [[VECTOR_BODY]] ]
; CHECK-NEXT:    [[ACTIVE_LANE_MASK:%.*]] = call <4 x i1> @llvm.get.active.lane.mask.v4i1.i32(i32 [[INDEX]], i32 [[N]])
; CHECK-NEXT:    [[TMP11:%.*]] = getelementptr i32, ptr [[TMP7]], i32 [[INDEX]]
; CHECK-NEXT:    [[WIDE_MASKED_LOAD:%.*]] = call <4 x i32> @llvm.masked.load.v4i32.p0(ptr [[TMP11]], i32 4, <4 x i1> [[ACTIVE_LANE_MASK]], <4 x i32> poison), !alias.scope [[META0:![0-9]+]]
; CHECK-NEXT:    [[TMP12:%.*]] = getelementptr i32, ptr [[TMP8]], i32 [[INDEX]]
; CHECK-NEXT:    call void @llvm.masked.store.v4i32.p0(<4 x i32> [[WIDE_MASKED_LOAD]], ptr [[TMP12]], i32 4, <4 x i1> [[ACTIVE_LANE_MASK]]), !alias.scope [[META3:![0-9]+]], !noalias [[META0]]
; CHECK-NEXT:    [[INDEX_NEXT]] = add i32 [[INDEX]], 4
; CHECK-NEXT:    [[TMP13:%.*]] = icmp eq i32 [[INDEX_NEXT]], [[N_VEC]]
; CHECK-NEXT:    br i1 [[TMP13]], label [[MIDDLE_BLOCK:%.*]], label [[VECTOR_BODY]], !llvm.loop [[LOOP5:![0-9]+]]
; CHECK:       middle.block:
; CHECK-NEXT:    br label [[INNER_LOOP_EXIT]]
; CHECK:       scalar.ph:
; CHECK-NEXT:    [[BC_RESUME_VAL:%.*]] = phi i32 [ 0, [[OUTER_LOOP]] ], [ 0, [[VECTOR_MEMCHECK]] ]
; CHECK-NEXT:    br label [[INNER_LOOP:%.*]]
; CHECK:       inner.loop:
; CHECK-NEXT:    [[J_021_US:%.*]] = phi i32 [ [[BC_RESUME_VAL]], [[SCALAR_PH]] ], [ [[INC_US:%.*]], [[INNER_LOOP]] ]
; CHECK-NEXT:    [[ARRAYIDX_US:%.*]] = getelementptr i32, ptr [[TMP7]], i32 [[J_021_US]]
; CHECK-NEXT:    [[TMP14:%.*]] = load i32, ptr [[ARRAYIDX_US]], align 4
; CHECK-NEXT:    [[ARRAYIDX8_US:%.*]] = getelementptr i32, ptr [[TMP8]], i32 [[J_021_US]]
; CHECK-NEXT:    store i32 [[TMP14]], ptr [[ARRAYIDX8_US]], align 4
; CHECK-NEXT:    [[INC_US]] = add nuw nsw i32 [[J_021_US]], 1
; CHECK-NEXT:    [[EXITCOND_NOT:%.*]] = icmp eq i32 [[INC_US]], [[N]]
; CHECK-NEXT:    br i1 [[EXITCOND_NOT]], label [[INNER_LOOP_EXIT]], label [[INNER_LOOP]], !llvm.loop [[LOOP8:![0-9]+]]
; CHECK:       inner.loop.exit:
; CHECK-NEXT:    [[INC10_US]] = add nuw nsw i32 [[I_023_US]], 1
; CHECK-NEXT:    [[EXITCOND26_NOT:%.*]] = icmp eq i32 [[INC10_US]], [[M]]
; CHECK-NEXT:    br i1 [[EXITCOND26_NOT]], label [[EXIT:%.*]], label [[OUTER_LOOP]]
; CHECK:       exit:
; CHECK-NEXT:    ret void
;
entry:
  %add5 = add nsw i32 %n, 1
  br label %outer.loop

outer.loop:
  %i.023.us = phi i32 [ %inc10.us, %inner.loop.exit ], [ 0, %entry ]
  %mul.us = mul nsw i32 %i.023.us, %n
  %0 = getelementptr i32, ptr %src, i32 %mul.us
  %mul6.us = mul nsw i32 %i.023.us, %add5
  %1 = getelementptr i32, ptr %dst, i32 %mul6.us
  br label %inner.loop

inner.loop:
  %j.021.us = phi i32 [ 0, %outer.loop ], [ %inc.us, %inner.loop ]
  %arrayidx.us = getelementptr i32, ptr %0, i32 %j.021.us
  %2 = load i32, ptr %arrayidx.us, align 4
  %arrayidx8.us = getelementptr i32, ptr %1, i32 %j.021.us
  store i32 %2, ptr %arrayidx8.us, align 4
  %inc.us = add nuw nsw i32 %j.021.us, 1
  %exitcond.not = icmp eq i32 %inc.us, %n
  br i1 %exitcond.not, label %inner.loop.exit, label %inner.loop

inner.loop.exit:
  %inc10.us = add nuw nsw i32 %i.023.us, 1
  %exitcond26.not = icmp eq i32 %inc10.us, %m
  br i1 %exitcond26.not, label %exit, label %outer.loop

exit:
  ret void
}

attributes #0 = { "target-cpu"="cortex-m55" }
