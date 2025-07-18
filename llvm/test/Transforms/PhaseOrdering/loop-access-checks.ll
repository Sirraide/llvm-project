; NOTE: Assertions have been autogenerated by utils/update_test_checks.py UTC_ARGS: --version 2
; RUN: opt -passes='default<O2>' -S %s | FileCheck %s

; Slightly reduced test case for a loop iterating over a std::span with libc++ hardening.
; TODO: The runtime check in the loop should be removed.
;
; #include <span>
; #include <iostream>
;
; void use(unsigned&);
;
; void fill_with_foreach(std::span<unsigned> elems) {
;  for (unsigned& x : elems)
;    use(x);
; }

%"class.std::__1::span" = type { ptr, i64 }
%"struct.std::__1::__bounded_iter" = type { ptr, ptr, ptr }

define void @test_fill_with_foreach([2 x i64] %elems.coerce) {
; CHECK-LABEL: define void @test_fill_with_foreach
; CHECK-SAME: ([2 x i64] [[ELEMS_COERCE:%.*]]) local_unnamed_addr {
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[ELEMS_COERCE_FCA_0_EXTRACT:%.*]] = extractvalue [2 x i64] [[ELEMS_COERCE]], 0
; CHECK-NEXT:    [[TMP0:%.*]] = inttoptr i64 [[ELEMS_COERCE_FCA_0_EXTRACT]] to ptr
; CHECK-NEXT:    [[ELEMS_COERCE_FCA_1_EXTRACT:%.*]] = extractvalue [2 x i64] [[ELEMS_COERCE]], 1
; CHECK-NEXT:    [[ADD_PTR_I_IDX:%.*]] = shl nsw i64 [[ELEMS_COERCE_FCA_1_EXTRACT]], 2
; CHECK-NEXT:    [[ADD_PTR_I:%.*]] = getelementptr inbounds i8, ptr [[TMP0]], i64 [[ADD_PTR_I_IDX]]
; CHECK-NEXT:    [[CMP_NOT_I_I_I_I:%.*]] = icmp slt i64 [[ELEMS_COERCE_FCA_1_EXTRACT]], 0
; CHECK-NEXT:    br i1 [[CMP_NOT_I_I_I_I]], label [[ERROR:%.*]], label [[FOR_COND_PREHEADER_SPLIT:%.*]]
; CHECK:       for.cond.preheader.split:
; CHECK-NEXT:    [[CMP_I_NOT2:%.*]] = icmp eq i64 [[ELEMS_COERCE_FCA_1_EXTRACT]], 0
; CHECK-NEXT:    br i1 [[CMP_I_NOT2]], label [[COMMON_RET:%.*]], label [[FOR_BODY:%.*]]
; CHECK:       common.ret:
; CHECK-NEXT:    ret void
; CHECK:       error:
; CHECK-NEXT:    tail call void @error()
; CHECK-NEXT:    br label [[COMMON_RET]]
; CHECK:       for.body:
; CHECK-NEXT:    [[__BEGIN1_SROA_0_03:%.*]] = phi ptr [ [[INCDEC_PTR_I:%.*]], [[FOR_BODY]] ], [ [[TMP0]], [[FOR_COND_PREHEADER_SPLIT]] ]
; CHECK-NEXT:    tail call void @use(ptr noundef nonnull align 4 dereferenceable(4) [[__BEGIN1_SROA_0_03]])
; CHECK-NEXT:    [[INCDEC_PTR_I]] = getelementptr inbounds nuw i8, ptr [[__BEGIN1_SROA_0_03]], i64 4
; CHECK-NEXT:    [[CMP_I_NOT:%.*]] = icmp eq ptr [[INCDEC_PTR_I]], [[ADD_PTR_I]]
; CHECK-NEXT:    br i1 [[CMP_I_NOT]], label [[COMMON_RET]], label [[FOR_BODY]]
;
entry:
  %elems = alloca %"class.std::__1::span", align 8
  %__begin1 = alloca %"struct.std::__1::__bounded_iter", align 8
  %__end1 = alloca %"struct.std::__1::__bounded_iter", align 8
  %elems.coerce.fca.0.extract = extractvalue [2 x i64] %elems.coerce, 0
  store i64 %elems.coerce.fca.0.extract, ptr %elems, align 8
  %elems.coerce.fca.1.extract = extractvalue [2 x i64] %elems.coerce, 1
  %elems.coerce.fca.1.gep = getelementptr inbounds [2 x i64], ptr %elems, i64 0, i64 1
  store i64 %elems.coerce.fca.1.extract, ptr %elems.coerce.fca.1.gep, align 8
  call void @llvm.lifetime.start.p0(i64 24, ptr nonnull %__begin1) #6
  %0 = load ptr, ptr %elems, align 8
  %__size_.i.i = getelementptr inbounds %"class.std::__1::span", ptr %elems, i64 0, i32 1
  %1 = load i64, ptr %__size_.i.i, align 8
  %add.ptr.i = getelementptr inbounds i32, ptr %0, i64 %1
  store ptr %0, ptr %__begin1, align 8
  %__begin_.i.i.i.i = getelementptr inbounds %"struct.std::__1::__bounded_iter", ptr %__begin1, i64 0, i32 1
  store ptr %0, ptr %__begin_.i.i.i.i, align 8
  %__end_.i.i.i.i = getelementptr inbounds %"struct.std::__1::__bounded_iter", ptr %__begin1, i64 0, i32 2
  store ptr %add.ptr.i, ptr %__end_.i.i.i.i, align 8
  %cmp.not.i.i.i.i = icmp slt i64 %1, 0
  br i1 %cmp.not.i.i.i.i, label %error, label %check.2

check.2:
  call void @llvm.lifetime.start.p0(i64 24, ptr nonnull %__end1) #6
  %l4 = load ptr, ptr %elems, align 8
  %__size_.i.i4 = getelementptr inbounds %"class.std::__1::span", ptr %elems, i64 0, i32 1
  %l5 = load i64, ptr %__size_.i.i4, align 8
  %add.ptr.i5 = getelementptr inbounds i32, ptr %l4, i64 %l5
  store ptr %add.ptr.i5, ptr %__end1, align 8
  %__begin_.i.i.i.i6 = getelementptr inbounds %"struct.std::__1::__bounded_iter", ptr %__end1, i64 0, i32 1
  store ptr %l4, ptr %__begin_.i.i.i.i6, align 8
  %__end_.i.i.i.i7 = getelementptr inbounds %"struct.std::__1::__bounded_iter", ptr %__end1, i64 0, i32 2
  store ptr %add.ptr.i5, ptr %__end_.i.i.i.i7, align 8
  %cmp.not.i.i.i.i8 = icmp slt i64 %l5, 0
  br i1 %cmp.not.i.i.i.i8, label %error, label %for.cond

error:
  call void @error()
  ret void

for.cond:
  %l8 = load ptr, ptr %__begin1, align 8
  %l9 = load ptr, ptr %__end1, align 8
  %cmp.i = icmp ne ptr %l8, %l9
  br i1 %cmp.i, label %for.body, label %for.cond.cleanup

for.cond.cleanup:                                 ; preds = %for.cond
  call void @llvm.lifetime.end.p0(i64 24, ptr nonnull %__end1)
  call void @llvm.lifetime.end.p0(i64 24, ptr nonnull %__begin1)
  ret void

for.body:                                         ; preds = %for.cond
  %l10 = load ptr, ptr %__begin1, align 8
  %__begin_.i.i = getelementptr inbounds %"struct.std::__1::__bounded_iter", ptr %__begin1, i64 0, i32 1
  %l11 = load ptr, ptr %__begin_.i.i, align 8
  %cmp.not.i.i = icmp uge ptr %l10, %l11
  %__end_.i.i = getelementptr inbounds %"struct.std::__1::__bounded_iter", ptr %__begin1, i64 0, i32 2
  %l12 = load ptr, ptr %__end_.i.i, align 8
  %cmp2.i.i = icmp ult ptr %l10, %l12
  %sel = select i1 %cmp.not.i.i, i1 %cmp2.i.i, i1 false
  br i1 %sel, label %for.latch, label %error

for.latch:
  call void @use(ptr noundef nonnull align 4 dereferenceable(4) %l10)
  %l = load ptr, ptr %__begin1, align 8
  %incdec.ptr.i = getelementptr inbounds i32, ptr %l, i64 1
  store ptr %incdec.ptr.i, ptr %__begin1, align 8
  br label %for.cond
}

declare void @error()

declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture)

declare void @use(ptr noundef nonnull align 4 dereferenceable(4))

declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture)


; -------------------------------------------------------------------------
; Test case for runtime check removal when accessing vector elements with
; hardened glibc++ (PR63125)

%Vector_base = type { %Vector_impl }
%Vector_impl = type { %Vector_impl_data }
%Vector_impl_data = type { ptr, ptr, ptr }

define void @foo(ptr noundef nonnull align 8 dereferenceable(24) noalias %vec) #0 {
; CHECK-LABEL: define void @foo
; CHECK-SAME: (ptr noalias noundef nonnull readonly align 8 captures(none) dereferenceable(24) [[VEC:%.*]]) local_unnamed_addr #[[ATTR0:[0-9]+]] {
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[_M_FINISH_I_I:%.*]] = getelementptr inbounds nuw i8, ptr [[VEC]], i64 8
; CHECK-NEXT:    [[TMP0:%.*]] = load ptr, ptr [[_M_FINISH_I_I]], align 8, !tbaa [[TBAA0:![0-9]+]]
; CHECK-NEXT:    [[TMP1:%.*]] = load ptr, ptr [[VEC]], align 8, !tbaa [[TBAA5:![0-9]+]]
; CHECK-NEXT:    [[SUB_PTR_LHS_CAST_I_I:%.*]] = ptrtoint ptr [[TMP0]] to i64
; CHECK-NEXT:    [[SUB_PTR_RHS_CAST_I_I:%.*]] = ptrtoint ptr [[TMP1]] to i64
; CHECK-NEXT:    [[SUB_PTR_SUB_I_I:%.*]] = sub i64 [[SUB_PTR_LHS_CAST_I_I]], [[SUB_PTR_RHS_CAST_I_I]]
; CHECK-NEXT:    [[SUB_PTR_DIV_I_I:%.*]] = ashr exact i64 [[SUB_PTR_SUB_I_I]], 3
; CHECK-NEXT:    [[CMP_NOT9:%.*]] = icmp eq ptr [[TMP0]], [[TMP1]]
; CHECK-NEXT:    br i1 [[CMP_NOT9]], label [[FOR_COND_CLEANUP:%.*]], label [[FOR_BODY:%.*]]
; CHECK:       for.cond.cleanup:
; CHECK-NEXT:    ret void
; CHECK:       for.body:
; CHECK-NEXT:    [[I_010:%.*]] = phi i64 [ [[INC:%.*]], [[FOR_BODY]] ], [ 0, [[ENTRY:%.*]] ]
; CHECK-NEXT:    [[ADD_PTR_I:%.*]] = getelementptr inbounds double, ptr [[TMP1]], i64 [[I_010]]
; CHECK-NEXT:    [[TMP2:%.*]] = load double, ptr [[ADD_PTR_I]], align 8
; CHECK-NEXT:    [[ADD:%.*]] = fadd double [[TMP2]], 1.000000e+00
; CHECK-NEXT:    store double [[ADD]], ptr [[ADD_PTR_I]], align 8
; CHECK-NEXT:    [[INC]] = add nuw i64 [[I_010]], 1
; CHECK-NEXT:    [[CMP_NOT:%.*]] = icmp eq i64 [[INC]], [[SUB_PTR_DIV_I_I]]
; CHECK-NEXT:    br i1 [[CMP_NOT]], label [[FOR_COND_CLEANUP]], label [[FOR_BODY]]
;
entry:
  %vec.addr = alloca ptr, align 8
  %count = alloca i64, align 8
  %i = alloca i64, align 8
  store ptr %vec, ptr %vec.addr, align 8
  call void @llvm.lifetime.start.p0(i64 8, ptr %count)
  %0 = load ptr, ptr %vec.addr, align 8
  %call = call noundef i64 @alloc(ptr noundef nonnull align 8 dereferenceable(24) %0)
  store i64 %call, ptr %count, align 8
  call void @llvm.lifetime.start.p0(i64 8, ptr %i)
  store i64 0, ptr %i, align 8
  br label %for.cond

for.cond:
  %1 = load i64, ptr %i, align 8
  %2 = load i64, ptr %count, align 8
  %cmp = icmp ne i64 %1, %2
  br i1 %cmp, label %for.body, label %for.cond.cleanup

for.cond.cleanup:
  call void @llvm.lifetime.end.p0(i64 8, ptr %i)
  br label %for.end

for.body:
  %3 = load ptr, ptr %vec.addr, align 8
  %4 = load i64, ptr %i, align 8
  %call1 = call noundef nonnull align 8 dereferenceable(8) ptr @operator_acc(ptr noundef nonnull align 8 dereferenceable(24) %3, i64 noundef %4)
  %5 = load double, ptr %call1, align 8
  %add = fadd double %5, 1.000000e+00
  store double %add, ptr %call1, align 8
  br label %for.inc

for.inc:
  %6 = load i64, ptr %i, align 8
  %inc = add i64 %6, 1
  store i64 %inc, ptr %i, align 8
  br label %for.cond

for.end:
  call void @llvm.lifetime.end.p0(i64 8, ptr %count) #5
  ret void
}

define internal noundef i64 @alloc(ptr noundef nonnull align 8 dereferenceable(24) %__cont) {
entry:
  %__cont.addr = alloca ptr, align 8
  store ptr %__cont, ptr %__cont.addr, align 8
  %0 = load ptr, ptr %__cont.addr, align 8
  %call = call noundef i64 @size(ptr noundef nonnull align 8 dereferenceable(24) %0) #5
  ret i64 %call
}

define internal noundef nonnull align 8 dereferenceable(8) ptr @operator_acc(ptr noundef nonnull align 8 dereferenceable(24) %this, i64 noundef %__n) {
entry:
  %this.addr = alloca ptr, align 8
  %__n.addr = alloca i64, align 8
  store ptr %this, ptr %this.addr, align 8
  store i64 %__n, ptr %__n.addr, align 8
  %this1 = load ptr, ptr %this.addr, align 8
  br label %do.body

do.body:
  %0 = load i64, ptr %__n.addr, align 8
  %call = call noundef i64 @size(ptr noundef nonnull align 8 dereferenceable(24) %this1)
  %cmp = icmp ult i64 %0, %call
  %lnot = xor i1 %cmp, true
  %conv = zext i1 %lnot to i64
  %tobool = icmp ne i64 %conv, 0
  br i1 %tobool, label %if.then, label %if.end

if.then:
  call void @abort() readnone
  unreachable

if.end:
  br label %do.cond

do.cond:
  br label %do.end

do.end:
  %_M_impl = getelementptr inbounds %Vector_base, ptr %this1, i32 0, i32 0
  %_M_start = getelementptr inbounds %Vector_impl_data, ptr %_M_impl, i32 0, i32 0
  %1 = load ptr, ptr %_M_start, align 8, !tbaa !0
  %2 = load i64, ptr %__n.addr, align 8, !tbaa !5
  %add.ptr = getelementptr inbounds double, ptr %1, i64 %2
  ret ptr %add.ptr
}

define internal noundef i64 @size(ptr noundef nonnull align 8 dereferenceable(24) %this) {
entry:
  %this.addr = alloca ptr, align 8
  store ptr %this, ptr %this.addr, align 8
  %this1 = load ptr, ptr %this.addr, align 8
  %_M_impl = getelementptr inbounds %Vector_base, ptr %this1, i32 0, i32 0
  %_M_finish = getelementptr inbounds %Vector_impl_data, ptr %_M_impl, i32 0, i32 1
  %0 = load ptr, ptr %_M_finish, align 8, !tbaa !7
  %_M_impl2 = getelementptr inbounds %Vector_base, ptr %this1, i32 0, i32 0
  %_M_start = getelementptr inbounds %Vector_impl_data, ptr %_M_impl2, i32 0, i32 0
  %1 = load ptr, ptr %_M_start, align 8, !tbaa !0
  %sub.ptr.lhs.cast = ptrtoint ptr %0 to i64
  %sub.ptr.rhs.cast = ptrtoint ptr %1 to i64
  %sub.ptr.sub = sub i64 %sub.ptr.lhs.cast, %sub.ptr.rhs.cast
  %sub.ptr.div = sdiv exact i64 %sub.ptr.sub, 8
  ret i64 %sub.ptr.div
}

declare void @abort()

; -------------------------------------------------------------------------
; Test case for runtime check removal when accessing vector elements with
; hardened glibc++ and a signed induction variable.
; https://github.com/llvm/llvm-project/issues/63126

define void @loop_with_signed_induction(ptr noundef nonnull align 8 dereferenceable(24) %vec) {
; CHECK-LABEL: define void @loop_with_signed_induction
; CHECK-SAME: (ptr noundef nonnull readonly align 8 captures(none) dereferenceable(24) [[VEC:%.*]]) local_unnamed_addr #[[ATTR0]] {
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[_M_FINISH_I_I:%.*]] = getelementptr inbounds nuw i8, ptr [[VEC]], i64 8
; CHECK-NEXT:    [[TMP0:%.*]] = load ptr, ptr [[_M_FINISH_I_I]], align 8, !tbaa [[TBAA0]]
; CHECK-NEXT:    [[TMP1:%.*]] = load ptr, ptr [[VEC]], align 8, !tbaa [[TBAA5]]
; CHECK-NEXT:    [[SUB_PTR_LHS_CAST_I_I:%.*]] = ptrtoint ptr [[TMP0]] to i64
; CHECK-NEXT:    [[SUB_PTR_RHS_CAST_I_I:%.*]] = ptrtoint ptr [[TMP1]] to i64
; CHECK-NEXT:    [[SUB_PTR_SUB_I_I:%.*]] = sub i64 [[SUB_PTR_LHS_CAST_I_I]], [[SUB_PTR_RHS_CAST_I_I]]
; CHECK-NEXT:    [[SUB_PTR_DIV_I_I:%.*]] = ashr exact i64 [[SUB_PTR_SUB_I_I]], 3
; CHECK-NEXT:    [[CMP9:%.*]] = icmp sgt i64 [[SUB_PTR_DIV_I_I]], 0
; CHECK-NEXT:    br i1 [[CMP9]], label [[FOR_BODY:%.*]], label [[FOR_COND_CLEANUP:%.*]]
; CHECK:       for.cond.cleanup:
; CHECK-NEXT:    ret void
; CHECK:       for.body:
; CHECK-NEXT:    [[I_010:%.*]] = phi i64 [ [[INC:%.*]], [[FOR_BODY]] ], [ 0, [[ENTRY:%.*]] ]
; CHECK-NEXT:    [[ADD_PTR_I:%.*]] = getelementptr inbounds nuw double, ptr [[TMP1]], i64 [[I_010]]
; CHECK-NEXT:    [[TMP2:%.*]] = load double, ptr [[ADD_PTR_I]], align 8, !tbaa [[TBAA6:![0-9]+]]
; CHECK-NEXT:    [[ADD:%.*]] = fadd double [[TMP2]], 1.000000e+00
; CHECK-NEXT:    store double [[ADD]], ptr [[ADD_PTR_I]], align 8, !tbaa [[TBAA6]]
; CHECK-NEXT:    [[INC]] = add nuw nsw i64 [[I_010]], 1
; CHECK-NEXT:    [[CMP:%.*]] = icmp slt i64 [[INC]], [[SUB_PTR_DIV_I_I]]
; CHECK-NEXT:    br i1 [[CMP]], label [[FOR_BODY]], label [[FOR_COND_CLEANUP]]
;
entry:
  %vec.addr = alloca ptr, align 8
  %count = alloca i64, align 8
  %i = alloca i64, align 8
  store ptr %vec, ptr %vec.addr, align 8
  call void @llvm.lifetime.start.p0(i64 8, ptr %count)
  %0 = load ptr, ptr %vec.addr, align 8
  %call = call noundef i64 @alloc(ptr noundef nonnull align 8 dereferenceable(24) %0)
  store i64 %call, ptr %count, align 8
  call void @llvm.lifetime.start.p0(i64 8, ptr %i)
  store i64 0, ptr %i, align 8
  br label %for.cond

for.cond:
  %1 = load i64, ptr %i, align 8
  %2 = load i64, ptr %count, align 8
  %cmp = icmp slt i64 %1, %2
  br i1 %cmp, label %for.body, label %for.cond.cleanup

for.cond.cleanup:
  call void @llvm.lifetime.end.p0(i64 8, ptr %i)
  br label %for.end

for.body:
  %3 = load ptr, ptr %vec.addr, align 8
  %4 = load i64, ptr %i, align 8
  %call1 = call noundef nonnull align 8 dereferenceable(8) ptr @operator_acc(ptr noundef nonnull align 8 dereferenceable(24) %3, i64 noundef %4)
  %5 = load double, ptr %call1, align 8, !tbaa !8
  %add = fadd double %5, 1.000000e+00
  store double %add, ptr %call1, align 8, !tbaa !8
  br label %for.inc

for.inc:
  %6 = load i64, ptr %i, align 8
  %inc = add nsw i64 %6, 1
  store i64 %inc, ptr %i, align 8
  br label %for.cond

for.end:
  call void @llvm.lifetime.end.p0(i64 8, ptr %count)
  ret void
}

; -------------------------------------------------------------------------
; Test case for runtime check removal when accessing elements in a nested loop
; (PR64881)


define void @monkey(ptr noundef %arr, i32 noundef %len) {
; CHECK-LABEL: define void @monkey
; CHECK-SAME: (ptr noundef captures(none) [[ARR:%.*]], i32 noundef [[LEN:%.*]]) local_unnamed_addr #[[ATTR1:[0-9]+]] {
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[CMP8:%.*]] = icmp ugt i32 [[LEN]], 1
; CHECK-NEXT:    br i1 [[CMP8]], label [[FOR_BODY4_PREHEADER:%.*]], label [[FOR_COND_CLEANUP:%.*]]
; CHECK:       for.body4.preheader:
; CHECK-NEXT:    [[I_09:%.*]] = phi i32 [ [[INC:%.*]], [[FOR_COND_CLEANUP3:%.*]] ], [ 1, [[ENTRY:%.*]] ]
; CHECK-NEXT:    br label [[FOR_BODY4:%.*]]
; CHECK:       for.cond.cleanup:
; CHECK-NEXT:    ret void
; CHECK:       for.cond.cleanup3:
; CHECK-NEXT:    [[INC]] = add nuw i32 [[I_09]], 1
; CHECK-NEXT:    [[CMP:%.*]] = icmp ult i32 [[INC]], [[LEN]]
; CHECK-NEXT:    br i1 [[CMP]], label [[FOR_BODY4_PREHEADER]], label [[FOR_COND_CLEANUP]]
; CHECK:       for.body4:
; CHECK-NEXT:    [[K_07:%.*]] = phi i32 [ [[DEC:%.*]], [[FOR_BODY4]] ], [ [[I_09]], [[FOR_BODY4_PREHEADER]] ]
; CHECK-NEXT:    [[IDX_EXT_I:%.*]] = zext i32 [[K_07]] to i64
; CHECK-NEXT:    [[ADD_PTR_I:%.*]] = getelementptr inbounds nuw i32, ptr [[ARR]], i64 [[IDX_EXT_I]]
; CHECK-NEXT:    [[TMP0:%.*]] = load i32, ptr [[ADD_PTR_I]], align 4
; CHECK-NEXT:    [[ADD:%.*]] = add nsw i32 [[TMP0]], 1
; CHECK-NEXT:    store i32 [[ADD]], ptr [[ADD_PTR_I]], align 4
; CHECK-NEXT:    [[DEC]] = add i32 [[K_07]], -1
; CHECK-NEXT:    [[CMP2_NOT:%.*]] = icmp eq i32 [[DEC]], 0
; CHECK-NEXT:    br i1 [[CMP2_NOT]], label [[FOR_COND_CLEANUP3]], label [[FOR_BODY4]]
;
entry:
  %arr.addr = alloca ptr, align 8
  %len.addr = alloca i32, align 4
  %i = alloca i32, align 4
  %cleanup.dest.slot = alloca i32, align 4
  %k = alloca i32, align 4
  store ptr %arr, ptr %arr.addr, align 8
  store i32 %len, ptr %len.addr, align 4
  call void @llvm.lifetime.start.p0(i64 4, ptr %i) #3
  store i32 1, ptr %i, align 4
  br label %for.cond

for.cond:                                         ; preds = %for.inc5, %entry
  %0 = load i32, ptr %i, align 4
  %1 = load i32, ptr %len.addr, align 4
  %cmp = icmp ult i32 %0, %1
  br i1 %cmp, label %for.body, label %for.cond.cleanup

for.cond.cleanup:                                 ; preds = %for.cond
  store i32 2, ptr %cleanup.dest.slot, align 4
  call void @llvm.lifetime.end.p0(i64 4, ptr %i) #3
  br label %for.end6

for.body:                                         ; preds = %for.cond
  call void @llvm.lifetime.start.p0(i64 4, ptr %k) #3
  %2 = load i32, ptr %i, align 4
  store i32 %2, ptr %k, align 4
  br label %for.cond1

for.cond1:                                        ; preds = %for.inc, %for.body
  %3 = load i32, ptr %k, align 4
  %cmp2 = icmp ugt i32 %3, 0
  br i1 %cmp2, label %for.body4, label %for.cond.cleanup3

for.cond.cleanup3:                                ; preds = %for.cond1
  store i32 5, ptr %cleanup.dest.slot, align 4
  call void @llvm.lifetime.end.p0(i64 4, ptr %k) #3
  br label %for.end

for.body4:                                        ; preds = %for.cond1
  %4 = load ptr, ptr %arr.addr, align 8
  %5 = load i32, ptr %len.addr, align 4
  %6 = load i32, ptr %k, align 4
  %call = call noundef ptr @at(ptr noundef %4, i32 noundef %5, i32 noundef %6)
  %7 = load i32, ptr %call, align 4
  %add = add nsw i32 %7, 1
  store i32 %add, ptr %call, align 4
  br label %for.inc

for.inc:                                          ; preds = %for.body4
  %8 = load i32, ptr %k, align 4
  %dec = add i32 %8, -1
  store i32 %dec, ptr %k, align 4
  br label %for.cond1

for.end:                                          ; preds = %for.cond.cleanup3
  br label %for.inc5

for.inc5:                                         ; preds = %for.end
  %9 = load i32, ptr %i, align 4
  %inc = add i32 %9, 1
  store i32 %inc, ptr %i, align 4
  br label %for.cond

for.end6:                                         ; preds = %for.cond.cleanup
  ret void
}

define internal noundef ptr @at(ptr noundef %arr, i32 noundef %len, i32 noundef %idx) {
entry:
  %arr.addr = alloca ptr, align 8
  %len.addr = alloca i32, align 4
  %idx.addr = alloca i32, align 4
  store ptr %arr, ptr %arr.addr, align 8
  store i32 %len, ptr %len.addr, align 4
  store i32 %idx, ptr %idx.addr, align 4
  %0 = load i32, ptr %idx.addr, align 4
  %1 = load i32, ptr %len.addr, align 4
  %cmp = icmp uge i32 %0, %1
  br i1 %cmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  call void @abort()
  unreachable

if.end:                                           ; preds = %entry
  %2 = load ptr, ptr %arr.addr, align 8
  %3 = load i32, ptr %idx.addr, align 4
  %idx.ext = zext i32 %3 to i64
  %add.ptr = getelementptr inbounds i32, ptr %2, i64 %idx.ext
  ret ptr %add.ptr
}


!0 = !{!1, !2, i64 0}
!1 = !{!"_ZTSNSt12_Vector_baseIdSaIdEE17_Vector_impl_dataE", !2, i64 0, !2, i64 8, !2, i64 16}
!2 = !{!"any pointer", !3, i64 0}
!3 = !{!"omnipotent char", !4, i64 0}
!4 = !{!"Simple C++ TBAA"}
!5 = !{!6, !6, i64 0}
!6 = !{!"long", !3, i64 0}
!7 = !{!1, !2, i64 8}
!8 = !{!9, !9, i64 0}
!9 = !{!"double", !3, i64 0}
