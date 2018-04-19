## VPlan + RV

This is the LLVM fork of the VPlan+RV project, a joint effort by Saarland University and Intel.
The aim of the VPlan+RV project is to bring functionality of [RV](https://github.com/cdl-saarland/rv), the Region Vectorizer, to LLVM.
The project was first [presented](https://www.youtube.com/watch?v=svMEphbFukw&t=1s) at the US LLVM Developers' Meeting in 2017.

### A new DivergenceAnalysis

VPlan+RV includes a new divergence analysis that can be used in the LoopVectorizer and as a replacement for the existing DivergenceAnalysis.
This new DivergenceAnalysis supports unstructured, reducible control flow and has a statically-precise notion of sync dependence.

#### Divergence Analysis for GPU kernels (`GPUDivergenceAnalysis`)

Pass `-use-rv-da` to opt and the existing DivergenceAnalysis pass will turn into a wrapper for the new DivergenceAnalysis.
In that mode, all divergence queries in the LLVM code base will be handled by the new implementation (AMDGPU, NVPTX, StructurizeCFG, ...).

#### Divergence Analysis for LoopVectorizer (`LoopDivergenceAnalysis`)

The tests (test/Analysis/DivergenceAnalysis/Loops) show some divergence queries that will occur once outer-loop vectorization is supported by LLVM.
