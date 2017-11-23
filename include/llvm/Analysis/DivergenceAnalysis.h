//===- llvm/Analysis/DivergenceAnalysis.h - Divergence Analysis -*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// The divergence analysis is an LLVM pass which can be used to find out
// if a branch instruction in a loop is divergent or not when the loop is vectorized. It can help
// branch optimizations such as jump threading and loop unswitching to make
// better decisions.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/DenseSet.h"
#include "llvm/IR/Function.h"
#include "llvm/Analysis/BranchDependenceAnalysis.h"
#include "llvm/Pass.h"
#include <vector>

namespace llvm {
class Value;
class Instruction;
class Loop;
class DivergenceAnalysis {
  const Loop & loop;
  BranchDependenceAnalysis & BDA;
  DenseSet<const Value *> divergentValues;
  std::vector<const Instruction*> worklist;

  bool updateTerminator(const TerminatorInst & term) const;
  bool updatePHINode(const PHINode & phi) const;
  // non-phi, non-terminator instruction
  bool updateNormalInstruction(const Instruction & term) const;

public:
  DivergenceAnalysis(const Loop & loop, BranchDependenceAnalysis & BDA);
  void markDivergent(const Value & divVal);
  bool isDivergent(const Value & val) const;

  void compute();
};

class LoopDivergenceAnalysis {
  BranchDependenceAnalysis BDA;
  DivergenceAnalysis DA;
public:
  LoopDivergenceAnalysis(Function & Func, const Loop & loop, const PostDominatorTree & postDomTree, const DominatorTree & domTree, const LoopInfo & loopInfo);
  bool isDivergent(const Value & val) const;
  bool isUniform(const Value & val) const { return !isDivergent(val); }
};

} // End llvm namespace
