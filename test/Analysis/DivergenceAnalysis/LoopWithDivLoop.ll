; RUN: opt -mtriple=x86-- -analyze -loop-divergence %s | FileCheck %s

; CHECK: Divergence of loop for.body {
; CHECK-NEXT: DIVERGENT:  %indvars.iv = phi i64 [ 0, %for.body.lr.ph ], [ %indvars.iv.next, %for.latch ]
; CHECK-NEXT: DIVERGENT:  %indvars.iv2 = phi i64 [ 0, %for.body ], [ %indvars.iv.next2, %for.body2 ]
; CHECK-NEXT: DIVERGENT:  %row = mul i64 %n, %indvars.iv
; CHECK-NEXT: DIVERGENT:  %idx = add i64 %row, %indvars.iv2
; CHECK-NEXT: DIVERGENT:  %trunc = trunc i64 %idx to i32
; CHECK-NEXT: DIVERGENT:  %val = sitofp i32 %trunc to float
; CHECK-NEXT: DIVERGENT:  %arrayidx = getelementptr inbounds float, float* %ptr, i64 %idx
; CHECK-NEXT: DIVERGENT:  store float %val, float* %arrayidx, align 4
; CHECK-NEXT: DIVERGENT:  %indvars.iv.next2 = add nuw nsw i64 %indvars.iv2, 1
; CHECK-NEXT: DIVERGENT:  %exitcond2 = icmp sge i64 %indvars.iv.next2, %indvars.iv
; CHECK-NEXT: DIVERGENT:  br i1 %exitcond2, label %for.latch, label %for.body2
; CHECK-NEXT: DIVERGENT:  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
; CHECK-NEXT: }
; CHECK: Divergence of loop for.body2 {
; CHECK-NEXT: DIVERGENT:  %indvars.iv2 = phi i64 [ 0, %for.body ], [ %indvars.iv.next2, %for.body2 ]
; CHECK-NEXT: DIVERGENT:  %idx = add i64 %row, %indvars.iv2
; CHECK-NEXT: DIVERGENT:  %trunc = trunc i64 %idx to i32
; CHECK-NEXT: DIVERGENT:  %val = sitofp i32 %trunc to float
; CHECK-NEXT: DIVERGENT:  %arrayidx = getelementptr inbounds float, float* %ptr, i64 %idx
; CHECK-NEXT: DIVERGENT:  store float %val, float* %arrayidx, align 4
; CHECK-NEXT: DIVERGENT:  %indvars.iv.next2 = add nuw nsw i64 %indvars.iv2, 1
; CHECK-NEXT: }
define void @test1(float* nocapture %ptr, i64 %n) #0 {
  entry:
  %cmp = icmp sgt i64 %n, 0
  br i1 %cmp, label %for.body.lr.ph, label %exit

for.body.lr.ph:                                   ; preds = %entry
  br label %for.body

exit:
  ret void

for.body:                                         
  %indvars.iv = phi i64 [ 0, %for.body.lr.ph ], [ %indvars.iv.next, %for.latch ]
  br label %for.body2

for.body2:
  %indvars.iv2 = phi i64 [ 0, %for.body ], [ %indvars.iv.next2, %for.body2 ]
  %row = mul i64 %n, %indvars.iv
  %idx = add i64 %row, %indvars.iv2
  %trunc = trunc i64 %idx to i32
  %val = sitofp i32 %trunc to float
  %arrayidx = getelementptr inbounds float, float* %ptr, i64 %idx
  store float %val, float* %arrayidx, align 4
  %indvars.iv.next2 = add nuw nsw i64 %indvars.iv2, 1
  %exitcond2 = icmp sge i64 %indvars.iv.next2, %indvars.iv
  br i1 %exitcond2, label %for.latch, label %for.body2

for.latch:
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond = icmp eq i64 %indvars.iv.next, %n
  br i1 %exitcond, label %exit, label %for.body
}

attributes #0 = { nounwind }
