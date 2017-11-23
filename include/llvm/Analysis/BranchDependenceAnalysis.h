#ifndef _LLVM_BRANCHDEPENDENCEANALYSIS_H_
#define _LLVM_BRANCHDEPENDENCEANALYSIS_H_

//===- BranchDependenceAnalysis.cpp ----------------*- C++ -*-===//
//
//                     The Region Vectorizer
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//

#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/DFG.h>
#include <llvm/Analysis/DivPathDecider.h>

namespace llvm {
  class Function;
  class BasicBlock;
  class TerminatorInst;
  class DominatorTree;
  struct DominatorTree;
  struct PostDominatorTree;

using ConstBlockSet = SmallPtrSet<const BasicBlock*, 4>;
using Edge = LoopBase<BasicBlock, Loop>::Edge;

/// BranchDependenceAnalysis
///
/// This is an analysis to be used in the context of SIMD/GPU execution of a function.
/// It enables the VectorizationAnalysis to correctly propagate divergent control from branches to phi nodes.
///
/// In the SPMD setting a group of threads executes a function in bulk-synchornouous fashion.
/// For every instruction each thread may see the same result (uniform value) or a different result (varying/divergent value).
/// If a varying instruction computes a branch condition control among the threads may diverge (p in the example).
/// If phi nodes are dependent on such a divergent branch the phis may receive values from different incoming blocks at once (phi node x).
/// The phis become divergent even if the incoming values per predecessor are uniform values.
///
/// if (p) {
//    x0 = 1
//  } else {
//    x1 = 2
//  }
//  C: x = phi [x0, x1]
///
/// The analysis result maps every branch to a set of basic blocks whose phi nodes will become varying if the branch is varying.
/// This is directly used by the VectorizationAnalysis to propagate control-induced value divergence.
///
class BranchDependenceAnalysis {
  static ConstBlockSet emptySet;
  DivPathDecider DPD;

  // iterated post dominance frontier
  DenseMap<const BasicBlock*, ConstBlockSet> pdClosureMap;
  DenseMap<const BasicBlock*, ConstBlockSet> domClosureMap;

  DenseMap<const TerminatorInst*, ConstBlockSet> effectedBlocks_old;
  mutable DenseMap<const TerminatorInst*, ConstBlockSet> effectedBlocks_new;
  CDG cdg;
  DFG dfg;
  const LoopInfo & loopInfo;

  void computePostDomClosure(const BasicBlock & x, ConstBlockSet & closure);
  void computeDomClosure(const BasicBlock & b, ConstBlockSet & closure);

public:
  BranchDependenceAnalysis(Function & F,
                           const PostDominatorTree & postDomTree,
                           const DominatorTree & domTree,
                           const LoopInfo & loopInfo);

  /// \brief returns the set of blocks whose PHI nodes become divergent if @branch is divergent
  const ConstBlockSet & join_blocks(const TerminatorInst & term) const;
};

} // namespace llvm

#endif // _LLVM_BRANCHDEPENDENCEANALYSIS_H_
