; RUN: opt -mtriple=x86-- -analyze -loop-divergence %s | FileCheck %s

; CHECK: Divergence of loop for.body {
; CHECK-NEXT: DIVERGENT:  %indvars.iv29 = phi i64 [ 0, %for.body.lr.ph ], [ %indvars.iv.next30, %for.cond.cleanup3 ]
; CHECK-NEXT: DIVERGENT:  %x.0.lcssa = phi double [ 0.000000e+00, %for.body ], [ %add, %for.body4 ]
; CHECK-NEXT: DIVERGENT:  %arrayidx10 = getelementptr inbounds double, double* %C, i64 %indvars.iv29
; CHECK-NEXT: DIVERGENT:  store double %x.0.lcssa, double* %arrayidx10, align 8
; CHECK-NEXT: DIVERGENT:  %indvars.iv.next30 = add nuw nsw i64 %indvars.iv29, 1
; CHECK-NEXT: DIVERGENT:  %x.025 = phi double [ %add, %for.body4 ], [ 0.000000e+00, %for.body4.preheader ]
; CHECK-NEXT: DIVERGENT:  %arrayidx8 = getelementptr inbounds double, double* %1, i64 %indvars.iv29
; CHECK-NEXT: DIVERGENT:  %2 = load double, double* %arrayidx8, align 8
; CHECK-NEXT: DIVERGENT:  %add = fadd double %x.025, %2
; CHECK-NEXT: }
; CHECK: Divergence of loop for.body4 {
; CHECK-NEXT: DIVERGENT:  %indvars.iv = phi i64 [ %indvars.iv.next, %for.body4 ], [ 0, %for.body4.preheader ]
; CHECK-NEXT: DIVERGENT:  %x.025 = phi double [ %add, %for.body4 ], [ 0.000000e+00, %for.body4.preheader ]
; CHECK-NEXT: DIVERGENT:  %arrayidx = getelementptr inbounds i32, i32* %Index, i64 %indvars.iv
; CHECK-NEXT: DIVERGENT:  %0 = load i32, i32* %arrayidx, align 4
; CHECK-NEXT: DIVERGENT:  %idxprom5 = sext i32 %0 to i64
; CHECK-NEXT: DIVERGENT:  %arrayidx6 = getelementptr inbounds double*, double** %A, i64 %idxprom5
; CHECK-NEXT: DIVERGENT:  %1 = load double*, double** %arrayidx6, align 8
; CHECK-NEXT: DIVERGENT:  %arrayidx8 = getelementptr inbounds double, double* %1, i64 %indvars.iv29
; CHECK-NEXT: DIVERGENT:  %2 = load double, double* %arrayidx8, align 8
; CHECK-NEXT: DIVERGENT:  %add = fadd double %x.025, %2
; CHECK-NEXT: DIVERGENT:  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
; CHECK-NEXT: }

; Function Attrs: norecurse nounwind uwtable
define void @test(i32* nocapture readonly %Index, double** nocapture readonly %A, double* nocapture %C, i32 %m, i32 %n) #0 {
entry:
  %cmp27 = icmp sgt i32 %n, 0
  br i1 %cmp27, label %for.body.lr.ph, label %for.cond.cleanup

for.body.lr.ph:                                   ; preds = %entry
  %cmp224 = icmp sgt i32 %m, 0
  %wide.trip.count = zext i32 %m to i64
  %wide.trip.count31 = zext i32 %n to i64
  br label %for.body

for.cond.cleanup:                                 ; preds = %for.cond.cleanup3, %entry
  ret void

for.body:                                         ; preds = %for.cond.cleanup3, %for.body.lr.ph
  %indvars.iv29 = phi i64 [ 0, %for.body.lr.ph ], [ %indvars.iv.next30, %for.cond.cleanup3 ]
  br i1 %cmp224, label %for.body4.preheader, label %for.cond.cleanup3

for.body4.preheader:                              ; preds = %for.body
  br label %for.body4

for.cond.cleanup3:                                ; preds = %for.body4, %for.body
  %x.0.lcssa = phi double [ 0.000000e+00, %for.body ], [ %add, %for.body4 ]
  %arrayidx10 = getelementptr inbounds double, double* %C, i64 %indvars.iv29
  store double %x.0.lcssa, double* %arrayidx10, align 8 
  %indvars.iv.next30 = add nuw nsw i64 %indvars.iv29, 1
  %exitcond32 = icmp eq i64 %indvars.iv.next30, %wide.trip.count31
  br i1 %exitcond32, label %for.cond.cleanup, label %for.body

for.body4:                                        ; preds = %for.body4.preheader, %for.body4
  %indvars.iv = phi i64 [ %indvars.iv.next, %for.body4 ], [ 0, %for.body4.preheader ]
  %x.025 = phi double [ %add, %for.body4 ], [ 0.000000e+00, %for.body4.preheader ]
  %arrayidx = getelementptr inbounds i32, i32* %Index, i64 %indvars.iv
  %0 = load i32, i32* %arrayidx, align 4
  %idxprom5 = sext i32 %0 to i64
  %arrayidx6 = getelementptr inbounds double*, double** %A, i64 %idxprom5
  %1 = load double*, double** %arrayidx6, align 8
  %arrayidx8 = getelementptr inbounds double, double* %1, i64 %indvars.iv29
  %2 = load double, double* %arrayidx8, align 8
  %add = fadd double %x.025, %2
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond = icmp eq i64 %indvars.iv.next, %wide.trip.count
  br i1 %exitcond, label %for.cond.cleanup3, label %for.body4
}

attributes #0 = { norecurse nounwind }
