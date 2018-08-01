//===- BranchDependenceAnalysis.h - Divergent Branch Dependence -*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the BranchDependenceAnalysis class, which computes for
// every divergent branch the set of phi nodes that the branch will make
// divergent.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ANALYSIS_BRANCHDEPENDENCEANALYSIS_H
#define LLVM_ANALYSIS_BRANCHDEPENDENCEANALYSIS_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Analysis/LoopInfo.h"

namespace llvm {

class BasicBlock;
class TerminatorInst;
class DominatorTree;
class PostDominatorTree;
class TerminatorInst;

using ConstBlockSet = SmallPtrSet<const BasicBlock *, 4>;

class BranchDependenceAnalysis {
  static ConstBlockSet emptyBlockSet;

  const DominatorTree &domTree;
  const PostDominatorTree &postDomTree;
  const LoopInfo &loopInfo;

  std::map<const TerminatorInst *, ConstBlockSet *> cachedJoinBlocks;

public:
  bool inRegion(const BasicBlock &BB) const;

  ~BranchDependenceAnalysis();
  BranchDependenceAnalysis(const DominatorTree &domTree,
                           const PostDominatorTree &postDomTree,
                           const LoopInfo &loopInfo);

  /// \brief returns the set of blocks whose PHI nodes become divergent if
  /// @branch is divergent
  const ConstBlockSet &join_blocks(const TerminatorInst &term);
};

} // namespace llvm

#endif // LLVM_ANALYSIS_BRANCHDEPENDENCEANALYSIS_H
