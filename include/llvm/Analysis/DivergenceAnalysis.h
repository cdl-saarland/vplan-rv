//===- llvm/Analysis/DivergenceAnalysis.h - Divergence Analysis -*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// The divergence analysis determines which instructions and branches are
// divergent given a set of divergent source instructions.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ANALYSIS_DIVERGENCEANALYSIS_H
#define LLVM_ANALYSIS_DIVERGENCEANALYSIS_H

#include "llvm/ADT/DenseSet.h"
#include "llvm/Analysis/BranchDependenceAnalysis.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include <vector>

namespace llvm {
class Module;
class Value;
class Instruction;
class Loop;
class raw_ostream;
class TargetTransformInfo;

// generic divergence analysis
class DivergenceAnalysis {
  const Function & F;
  // if regionLoop != nullptr, analyze only in the scope of the loop
  // Otw, analyze the whole function
  const Loop * regionLoop;

  BranchDependenceAnalysis &BDA;
  DenseSet<const Value *> uniformOverrides;

  // blocks with joining divergent control from different predecessors
  DenseSet<const BasicBlock*> divergentJoinBlocks;

  // blocks with joining divergent control from multiple loop iterations
  DenseSet<const BasicBlock*> temporalDivergentBlocks;

  // detected/marked divergent values
  DenseSet<const Value *> divergentValues;
  std::vector<const Instruction *> worklist;

  bool updateTerminator(const TerminatorInst &term) const;
  bool updatePHINode(const PHINode &phi) const;

  // non-phi, non-terminator instruction
  bool updateNormalInstruction(const Instruction &term) const;

  // taints all loop live out users
  void taintLoopLiveOuts(const BasicBlock & loopHeader);

  //mark all phis in @joinBlock as divergent
  void markPHIsDivergent(const BasicBlock & joinBlock);

  // push all in-region users of @I
  void pushUsers(const Instruction & I);

  void markBlockTemporalDivergent(const BasicBlock & block) {
    temporalDivergentBlocks.insert(&block);
  }

  void markBlockJoinDivergent(const BasicBlock & block) {
    divergentJoinBlocks.insert(&block);
  }

  bool isTemporalDivergent(const BasicBlock & block) const {
    return temporalDivergentBlocks.count(&block);
  }

  bool isJoinDivergent(const BasicBlock & block) const {
    return divergentJoinBlocks.count(&block);
  }

public:
  const Loop* getRegionLoop() const { return regionLoop; }
  const Function& getFunction() const { return F; }

  DivergenceAnalysis(const Function & F, const Loop * regionLoop, BranchDependenceAnalysis &BDA);
  bool inRegion(const Instruction & I) const;
  void addUniformOverride(const Value& uniVal);
  void markDivergent(const Value &divVal);
  void compute(bool isLCSSA);

  bool isDivergent(const Value &val) const;
  void print(raw_ostream &OS, const Module *) const;
};

// divergence analysis frontend for GPU kernels
class GPUDivergenceAnalysis {
  BranchDependenceAnalysis BDA;
  DivergenceAnalysis DA;

public:
  GPUDivergenceAnalysis(
      Function & F,
      const DominatorTree & DT,
      const PostDominatorTree & PDT,
      const LoopInfo & LI,
      const TargetTransformInfo & TTI);

  // Returns true if V is divergent.
  bool isDivergent(const Value &val) const;

  // Returns true if V is uniform/non-divergent.
  bool isUniform(const Value &val) const { return !isDivergent(val); }

  // Print all divergent values in the loop.
  void print(raw_ostream &OS, const Module *) const;
};

// divergence analysis frontend for loops
class LoopDivergenceAnalysis {
  DivergenceAnalysis DA;

public:
  LoopDivergenceAnalysis(BranchDependenceAnalysis &BDA, const Loop &loop);

  // Returns true if V is divergent.
  bool isDivergent(const Value &val) const;

  // Returns true if V is uniform/non-divergent.
  bool isUniform(const Value &val) const { return !isDivergent(val); }

  // Print all divergent values in the loop.
  void print(raw_ostream &OS, const Module *) const;
};

// loop divergence printer pass - for standalone testing
class LoopDivergencePrinter : public FunctionPass {
  std::unique_ptr<BranchDependenceAnalysis> BDA;
  SmallVector<std::unique_ptr<LoopDivergenceAnalysis>, 6> loopDivInfo;

public:
  static char ID;

  LoopDivergencePrinter() : FunctionPass(ID) {
    initializeLoopDivergencePrinterPass(*PassRegistry::getPassRegistry());
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override;

  bool runOnFunction(Function &F) override;

  // Print all divergent values in the function.
  void print(raw_ostream &OS, const Module *) const override;
};

} // namespace llvm

#endif // LLVM_ANALYSIS_DIVERGENCEANALYSIS_H
