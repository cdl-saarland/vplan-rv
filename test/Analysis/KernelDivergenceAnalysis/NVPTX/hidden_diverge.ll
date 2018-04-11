; RUN: opt %s -analyze -divergence | FileCheck %s

target datalayout = "e-i64:64-v16:16-v32:32-n16:32:64"
target triple = "nvptx64-nvidia-cuda"

; return (n < 0 ? a + threadIdx.x : b + threadIdx.x)
define i32 @hidden_diverge(i32 %n, i32 %a, i32 %b) {
; CHECK-LABEL: Printing analysis 'Kernel Divergence Analysis' for function 'hidden_diverge'
entry:
  %tid = call i32 @llvm.nvvm.read.ptx.sreg.tid.x()
  %cond.var = icmp slt i32 %tid, 0
  br i1 %cond.var, label %B, label %C ; divergent
; CHECK: DIVERGENT: br i1 %cond.var,
B:
  %cond.uni = icmp slt i32 %n, 0
  br i1 %cond.uni, label %C, label %merge ; uniform
; CHECK-NOT: DIVERGENT: br i1 %cond.uni,
C:
  %phi.var.hidden = phi i32 [ 1, %entry ], [ 2, %B  ]
; CHECK: DIVERGENT: %phi.var.hidden = phi i32
  br label %merge
merge:
  %phi.ipd = phi i32 [ %a, %B ], [ %b, %C ]
; CHECK: DIVERGENT: %phi.ipd = phi i32
  ret i32 %phi.ipd
}

declare i32 @llvm.nvvm.read.ptx.sreg.tid.x()
declare i32 @llvm.nvvm.read.ptx.sreg.tid.y()
declare i32 @llvm.nvvm.read.ptx.sreg.tid.z()
declare i32 @llvm.nvvm.read.ptx.sreg.laneid()

!nvvm.annotations = !{!0}
!0 = !{i32 (i32, i32, i32)* @hidden_diverge, !"kernel", i32 1}
