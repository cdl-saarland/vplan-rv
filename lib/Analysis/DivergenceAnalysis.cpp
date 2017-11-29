//===- DivergenceAnalysis.cpp --------- Divergence Analysis Implementation -==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements divergence analysis which determines whether a branch
// in a GPU program is divergent.It can help branch optimizations such as jump
// threading and loop unswitching to make better decisions.
//
// GPU programs typically use the SIMD execution model, where multiple threads
// in the same execution group have to execute in lock-step. Therefore, if the
// code contains divergent branches (i.e., threads in a group do not agree on
// which path of the branch to take), the group of threads has to execute all
// the paths from that branch with different subsets of threads enabled until
// they converge at the immediately post-dominating BB of the paths.
//
// Due to this execution model, some optimizations such as jump
// threading and loop unswitching can be unfortunately harmful when performed on
// divergent branches. Therefore, an analysis that computes which branches in a
// GPU program are divergent can help the compiler to selectively run these
// optimizations.
//
// This file defines divergence analysis which computes a conservative but
// non-trivial approximation of all divergent branches in a GPU program. It
// partially implements the approach described in
//
//   Divergence Analysis
//   Sampaio, Souza, Collange, Pereira
//   TOPLAS '13
//
// The divergence analysis identifies the sources of divergence (e.g., special
// variables that hold the thread ID), and recursively marks variables that are
// data or sync dependent on a source of divergence as divergent.
//
// While data dependency is a well-known concept, the notion of sync dependency
// is worth more explanation. Sync dependence characterizes the control flow
// aspect of the propagation of branch divergence. For example,
//
//   %cond = icmp slt i32 %tid, 10
//   br i1 %cond, label %then, label %else
// then:
//   br label %merge
// else:
//   br label %merge
// merge:
//   %a = phi i32 [ 0, %then ], [ 1, %else ]
//
// Suppose %tid holds the thread ID. Although %a is not data dependent on %tid
// because %tid is not on its use-def chains, %a is sync dependent on %tid
// because the branch "br i1 %cond" depends on %tid and affects which value %a
// is assigned to.
//
// The current implementation has the following limitations:
// 1. intra-procedural. It conservatively considers the arguments of a
//    non-kernel-entry function and the return value of a function call as
//    divergent.
// 2. memory as black box. It conservatively considers values loaded from
//    generic or local address as divergent. This can be improved by leveraging
//    pointer analysis.
//
//===----------------------------------------------------------------------===//

#include "llvm/Analysis/DivergenceAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <vector>

using namespace llvm;

// class DivergenceAnalysis
DivergenceAnalysis::DivergenceAnalysis(const Loop &_loop,
                                       BranchDependenceAnalysis &_BDA)
    : loop(_loop), BDA(_BDA), divergentValues() {}

void DivergenceAnalysis::markDivergent(const Value &divVal) {
  divergentValues.insert(&divVal);
}

bool DivergenceAnalysis::updateTerminator(const TerminatorInst &term) const {
  if (term.getNumSuccessors() <= 1)
    return false;
  if (auto *branchInst = dyn_cast<BranchInst>(&term)) {
    assert(branchInst->isConditional());
    return isDivergent(*branchInst->getCondition());
  } else if (auto *switchInst = dyn_cast<SwitchInst>(&term)) {
    return isDivergent(*switchInst->getCondition());
  } else if (isa<InvokeInst>(term)) {
    return false; // ignore abnormal executions through landingpad
  } else {
    abort();
  }
}

bool DivergenceAnalysis::updateNormalInstruction(const Instruction &I) const {
  // TODO function calls with side effects, etc
  for (const auto &op : I.operands()) {
    if (isDivergent(*op))
      return true;
  }
  return false;
}

bool DivergenceAnalysis::updatePHINode(const PHINode &phi) const {
  // join in divergence of parent block
  if (isDivergent(*phi.getParent()))
    return true;
  // join in incoming value divergence
  for (size_t i = 0; i < phi.getNumIncomingValues(); ++i) {
    if (isDivergent(*phi.getIncomingValue(i)))
      return true;
  }
  return false;
}

void DivergenceAnalysis::compute() {
  // push all users of seed values to worklist
  for (auto *divVal : divergentValues) {
    for (const auto *user : divVal->users()) {
      const auto *userInst = dyn_cast<const Instruction>(user);
      if (!userInst)
        continue;
      worklist.push_back(userInst);
    }
  }

  // propagate divergence
  while (!worklist.empty()) {
    const Instruction &I = *worklist.back();
    worklist.pop_back();
    bool wasDivergent = isDivergent(I);
    if (wasDivergent)
      continue;

    if (isa<TerminatorInst>(I)) {
      // spread to phi nodes
      auto &term = cast<TerminatorInst>(I);
      if (updateTerminator(term)) {
        markDivergent(term);
        for (const auto *joinBlock : BDA.join_blocks(term)) {
          markDivergent(*joinBlock);
          for (auto &blockInst : *joinBlock) {
            if (!isa<PHINode>(blockInst))
              break;
            worklist.push_back(&blockInst);
          }
        }
        continue;
      }
    }

    // update divergence info for this instruction
    bool divergentUpd = false;
    if (isa<PHINode>(I)) {
      divergentUpd = updatePHINode(cast<PHINode>(I));
    } else {
      divergentUpd = updateNormalInstruction(I);
    }

    // spread divergence to users
    if (divergentUpd) {
      markDivergent(I);
      for (const auto *user : I.users()) {
        const auto *userInst = dyn_cast<const Instruction>(user);
        if (!userInst)
          continue;

        // only compute divergent inside loop
        if (!loop.contains(userInst->getParent()))
          continue;
        worklist.push_back(userInst);
      }
    }
  }
}

bool DivergenceAnalysis::isDivergent(const Value &val) const {
  if (divergentValues.count(&val))
    return true;
  return false;
}

void DivergenceAnalysis::print(raw_ostream &OS, const Module *) const {
  if (divergentValues.empty())
    return;
  const Value *FirstDivergentValue = *divergentValues.begin();
  const Function *F;
  if (const Argument *Arg = dyn_cast<Argument>(FirstDivergentValue)) {
    F = Arg->getParent();
  } else if (const Instruction *I =
                 dyn_cast<Instruction>(FirstDivergentValue)) {
    F = I->getParent()->getParent();
  } else {
    llvm_unreachable("Only arguments and instructions can be divergent");
  }

  OS << "Divergence of loop " << loop.getName() << " {\n";
  // Iterate instructions using instructions() to ensure a deterministic order.
  for (auto &I : instructions(F)) {
    if (divergentValues.count(&I))
      OS << "DIVERGENT:" << I << "\n";
  }
  OS << "}\n";
}

// class LoopDivergenceAnalysis
LoopDivergenceAnalysis::LoopDivergenceAnalysis(BranchDependenceAnalysis &BDA,
                                               const Loop &loop)
    : DA(loop, BDA) {
  for (const auto &I : *loop.getHeader()) {
    if (!isa<PHINode>(I))
      break;
    DA.markDivergent(I);
  }
  DA.compute();
}

bool LoopDivergenceAnalysis::isDivergent(const Value &val) const {
  return DA.isDivergent(val);
}

void LoopDivergenceAnalysis::print(raw_ostream &OS, const Module *mod) const {
  DA.print(OS, mod);
}

// class LoopDivergencePrinter
bool LoopDivergencePrinter::runOnFunction(Function &F) {
  const PostDominatorTree &postDomTree =
      getAnalysis<PostDominatorTreeWrapperPass>().getPostDomTree();
  const DominatorTree &domTree =
      getAnalysis<DominatorTreeWrapperPass>().getDomTree();
  const LoopInfo &loopInfo = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
  BDA =
      make_unique<BranchDependenceAnalysis>(F, domTree, postDomTree, loopInfo);

  std::vector<const Loop *> loopStack;
  for (const auto *loop : loopInfo) {
    loopStack.push_back(loop);
  }

  while (!loopStack.empty()) {
    const auto *loop = loopStack.back();
    loopStack.pop_back();

    loopDivInfo[loop] = make_unique<LoopDivergenceAnalysis>(*BDA, *loop);
    for (const auto *childLoop : *loop) {
      loopStack.push_back(childLoop);
    }
  }

  return false;
}

void LoopDivergencePrinter::print(raw_ostream &OS, const Module *mod) const {
  for (auto &it : loopDivInfo) {
    it.second->print(OS, mod);
  }
}

// Register this pass.
char LoopDivergencePrinter::ID = 0;
INITIALIZE_PASS_BEGIN(LoopDivergencePrinter, "loop-divergence",
                      "Loop Divergence Printer", false, true)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_DEPENDENCY(PostDominatorTreeWrapperPass)
INITIALIZE_PASS_DEPENDENCY(LoopInfoWrapperPass)
INITIALIZE_PASS_END(LoopDivergencePrinter, "loop-divergence",
                    "Loop Divergence Printer", false, true)

FunctionPass *llvm::createLoopDivergencePrinterPass() {
  return new LoopDivergencePrinter();
}

void LoopDivergencePrinter::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<DominatorTreeWrapperPass>();
  AU.addRequired<PostDominatorTreeWrapperPass>();
  AU.addRequired<LoopInfoWrapperPass>();
  AU.setPreservesAll();
}
