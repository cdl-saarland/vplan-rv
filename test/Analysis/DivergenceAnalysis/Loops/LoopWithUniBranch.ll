; RUN: opt -mtriple=x86-- -analyze -loop-divergence %s | FileCheck %s

; CHECK: Divergence of loop for.body {
; CHECK-NEXT: DIVERGENT:  %indvars.iv = phi i64 [ 0, %for.body.lr.ph ], [ %indvars.iv.next, %B ]
; CHECK-NEXT: DIVERGENT:  %arrayidx = getelementptr inbounds float, float* %ptr, i64 %indvars.iv
; CHECK-NEXT: DIVERGENT:  store float %divphi, float* %arrayidx, align 4
; CHECK-NEXT: DIVERGENT:  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
; CHECK-NEXT: }
define void @test1(float* nocapture %ptr, i64 %n) #0 {
  entry:
  %cmp = icmp sgt i64 %n, 0
  br i1 %cmp, label %for.body.lr.ph, label %for.cond.cleanup

for.body.lr.ph:                                   ; preds = %entry
  br label %for.body

for.cond.cleanup:                                 ; preds = %for.body, %entry
  ret void

for.body:                                         ; preds = %for.body, %for.body.lr.ph
  %indvars.iv = phi i64 [ 0, %for.body.lr.ph ], [ %indvars.iv.next, %B ]
  %invar = trunc i64 %n to i1
  br i1 %invar, label %A, label %B

A:
  %trunc = trunc i64 %n to i32
  %cast = sitofp i32 %trunc to float
  br label %B

B:
  %divphi = phi float [ %cast, %A ], [ 4.200000e+01, %for.body ]
  %arrayidx = getelementptr inbounds float, float* %ptr, i64 %indvars.iv
  store float %divphi, float* %arrayidx, align 4
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond = icmp eq i64 %indvars.iv.next, %n
  br i1 %exitcond, label %for.cond.cleanup, label %for.body
}

attributes #0 = { nounwind }
