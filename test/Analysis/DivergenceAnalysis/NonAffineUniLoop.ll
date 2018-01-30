; RUN: opt -mtriple=x86-- -analyze -loop-divergence %s | FileCheck %s

; CHECK: Divergence of loop for.body {
; CHECK-NEXT: DIVERGENT:  %indvars.iv53 = phi i64 [ 0, %for.body.lr.ph ], [ %indvars.iv.next54, %for.cond.cleanup3 ]
; CHECK-NEXT: DIVERGENT:  %indvars.iv.next54 = add nuw nsw i64 %indvars.iv53, 1
; CHECK-NEXT: DIVERGENT:  %5 = add nsw i64 %4, %indvars.iv53
; CHECK-NEXT: DIVERGENT:  %arrayidx = getelementptr inbounds double, double* %A, i64 %5
; CHECK-NEXT: DIVERGENT:  %6 = load double, double* %arrayidx, align 8
; CHECK-NEXT: DIVERGENT:  %8 = add nsw i64 %7, %indvars.iv53
; CHECK-NEXT: DIVERGENT:  %arrayidx14 = getelementptr inbounds double, double* %A, i64 %8
; CHECK-NEXT: DIVERGENT:  %9 = load double, double* %arrayidx14, align 8
; CHECK-NEXT: DIVERGENT:  %add15 = fadd double %6, %9
; CHECK-NEXT: DIVERGENT:  store double %add15, double* %arrayidx14, align 8
; CHECK-NEXT: }
; CHECK: Divergence of loop for.body8.lr.ph {
; CHECK-NEXT: DIVERGENT:  %mul44 = phi i32 [ %mul, %for.cond.cleanup7 ], [ 2, %for.body8.lr.ph.preheader ]
; CHECK-NEXT: DIVERGENT:  %len.043 = phi i32 [ %mul44, %for.cond.cleanup7 ], [ 1, %for.body8.lr.ph.preheader ]
; CHECK-NEXT: DIVERGENT:  %1 = sext i32 %mul44 to i64
; CHECK-NEXT: DIVERGENT:  %2 = sext i32 %len.043 to i64
; CHECK-NEXT: DIVERGENT:  %mul = shl nsw i32 %mul44, 1
; CHECK-NEXT: DIVERGENT:  %indvars.iv = phi i64 [ 0, %for.body8.lr.ph ], [ %indvars.iv.next, %for.body8 ]
; CHECK-NEXT: DIVERGENT:  %3 = add nsw i64 %indvars.iv, %2
; CHECK-NEXT: DIVERGENT:  %4 = mul nsw i64 %3, %0
; CHECK-NEXT: DIVERGENT:  %5 = add nsw i64 %4, %indvars.iv53
; CHECK-NEXT: DIVERGENT:  %arrayidx = getelementptr inbounds double, double* %A, i64 %5
; CHECK-NEXT: DIVERGENT:  %6 = load double, double* %arrayidx, align 8
; CHECK-NEXT: DIVERGENT:  %7 = mul nsw i64 %indvars.iv, %0
; CHECK-NEXT: DIVERGENT:  %8 = add nsw i64 %7, %indvars.iv53
; CHECK-NEXT: DIVERGENT:  %arrayidx14 = getelementptr inbounds double, double* %A, i64 %8
; CHECK-NEXT: DIVERGENT:  %9 = load double, double* %arrayidx14, align 8
; CHECK-NEXT: DIVERGENT:  %add15 = fadd double %6, %9
; CHECK-NEXT: DIVERGENT:  store double %add15, double* %arrayidx14, align 8
; CHECK-NEXT: DIVERGENT:  %indvars.iv.next = add i64 %indvars.iv, %1
; CHECK-NEXT: DIVERGENT:  %cmp6 = icmp slt i64 %indvars.iv.next, %0
; CHECK-NEXT: DIVERGENT:  br i1 %cmp6, label %for.body8, label %for.cond.cleanup7
; CHECK-NEXT: }
; CHECK: Divergence of loop for.body8 {
; CHECK-NEXT: DIVERGENT:  %indvars.iv = phi i64 [ 0, %for.body8.lr.ph ], [ %indvars.iv.next, %for.body8 ]
; CHECK-NEXT: DIVERGENT:  %3 = add nsw i64 %indvars.iv, %2
; CHECK-NEXT: DIVERGENT:  %4 = mul nsw i64 %3, %0
; CHECK-NEXT: DIVERGENT:  %5 = add nsw i64 %4, %indvars.iv53
; CHECK-NEXT: DIVERGENT:  %arrayidx = getelementptr inbounds double, double* %A, i64 %5
; CHECK-NEXT: DIVERGENT:  %6 = load double, double* %arrayidx, align 8
; CHECK-NEXT: DIVERGENT:  %7 = mul nsw i64 %indvars.iv, %0
; CHECK-NEXT: DIVERGENT:  %8 = add nsw i64 %7, %indvars.iv53
; CHECK-NEXT: DIVERGENT:  %arrayidx14 = getelementptr inbounds double, double* %A, i64 %8
; CHECK-NEXT: DIVERGENT:  %9 = load double, double* %arrayidx14, align 8
; CHECK-NEXT: DIVERGENT:  %add15 = fadd double %6, %9
; CHECK-NEXT: DIVERGENT:  store double %add15, double* %arrayidx14, align 8
; CHECK-NEXT: DIVERGENT:  %indvars.iv.next = add i64 %indvars.iv, %1
; CHECK-NEXT: }

; Function Attrs: norecurse nounwind uwtable
define void @foo(double* nocapture %A, i32 %n) local_unnamed_addr #0 {
entry:
  %cmp45 = icmp sgt i32 %n, 0
  br i1 %cmp45, label %for.body.lr.ph, label %for.cond.cleanup

for.body.lr.ph:                                   ; preds = %entry
  %cmp242 = icmp sgt i32 %n, 2
  %0 = sext i32 %n to i64
  %wide.trip.count = zext i32 %n to i64
  br label %for.body

for.cond.cleanup:                                 ; preds = %for.cond.cleanup3, %entry
  ret void

for.body:                                         ; preds = %for.cond.cleanup3, %for.body.lr.ph
  %indvars.iv53 = phi i64 [ 0, %for.body.lr.ph ], [ %indvars.iv.next54, %for.cond.cleanup3 ]
  br i1 %cmp242, label %for.body8.lr.ph.preheader, label %for.cond.cleanup3

for.body8.lr.ph.preheader:                        ; preds = %for.body
  br label %for.body8.lr.ph

for.cond.cleanup3:                                ; preds = %for.cond.cleanup7, %for.body
  %indvars.iv.next54 = add nuw nsw i64 %indvars.iv53, 1
  %exitcond = icmp eq i64 %indvars.iv.next54, %wide.trip.count
  br i1 %exitcond, label %for.cond.cleanup, label %for.body

for.body8.lr.ph:                                  ; preds = %for.body8.lr.ph.preheader, %for.cond.cleanup7
  %mul44 = phi i32 [ %mul, %for.cond.cleanup7 ], [ 2, %for.body8.lr.ph.preheader ]
  %len.043 = phi i32 [ %mul44, %for.cond.cleanup7 ], [ 1, %for.body8.lr.ph.preheader ]
  %1 = sext i32 %mul44 to i64
  %2 = sext i32 %len.043 to i64
  br label %for.body8

for.cond.cleanup7:                                ; preds = %for.body8
  %mul = shl nsw i32 %mul44, 1
  %cmp2 = icmp slt i32 %mul, %n
  br i1 %cmp2, label %for.body8.lr.ph, label %for.cond.cleanup3

for.body8:                                        ; preds = %for.body8.lr.ph, %for.body8
  %indvars.iv = phi i64 [ 0, %for.body8.lr.ph ], [ %indvars.iv.next, %for.body8 ]
  %3 = add nsw i64 %indvars.iv, %2
  %4 = mul nsw i64 %3, %0
  %5 = add nsw i64 %4, %indvars.iv53
  %arrayidx = getelementptr inbounds double, double* %A, i64 %5
  %6 = load double, double* %arrayidx, align 8
  %7 = mul nsw i64 %indvars.iv, %0
  %8 = add nsw i64 %7, %indvars.iv53
  %arrayidx14 = getelementptr inbounds double, double* %A, i64 %8
  %9 = load double, double* %arrayidx14, align 8
  %add15 = fadd double %6, %9
  store double %add15, double* %arrayidx14, align 8
  %indvars.iv.next = add i64 %indvars.iv, %1
  %cmp6 = icmp slt i64 %indvars.iv.next, %0
  br i1 %cmp6, label %for.body8, label %for.cond.cleanup7
}

attributes #0 = { norecurse nounwind uwtable }
