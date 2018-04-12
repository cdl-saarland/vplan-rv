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
DivergenceAnalysis::DivergenceAnalysis(const Function & F,
                                       const Loop *regionLoop,
                                       BranchDependenceAnalysis & BDA)
    : F(F), regionLoop(regionLoop), BDA(BDA), divergentValues() {}

void DivergenceAnalysis::markDivergent(const Value &divVal) {
  divergentValues.insert(&divVal);
}

void DivergenceAnalysis::addUniformOverride(const Value& uniVal) {
  uniformOverrides.insert(&uniVal);
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

bool
DivergenceAnalysis::inRegion(const Instruction & I) const {
  return !regionLoop || regionLoop->contains(I.getParent());
}

void
DivergenceAnalysis::markPHIsDivergent(const BasicBlock & joinBlock) {
   markDivergent(joinBlock); // all PHI nodes in this will become divergent
   for (auto &blockInst : joinBlock) {
     if (!isa<PHINode>(blockInst))
       break;
     worklist.push_back(&blockInst);
   }
}

// marks all users of loop carried values of the loop headed by @loopHeader as divergent
void
DivergenceAnalysis::taintLoopLiveOuts(const BasicBlock & loopHeader) {
  auto * divLoop = BDA.getLoopFor(loopHeader);

  SmallVector<BasicBlock*, 8> taintStack;
  divLoop->getExitBlocks(taintStack);

  DenseSet<const BasicBlock*> visited;
  for (auto * block : taintStack) {
    visited.insert(block);
  }
  visited.insert(&loopHeader);

  while (!taintStack.empty()) {
    auto * userBlock = taintStack.back();
    taintStack.pop_back();

    assert(!divLoop->contains(userBlock) && "irreducible control flow detected");

    // phi nodes at the fringes of the dominance region
    if (!BDA.dominates(loopHeader, *userBlock)) {
      markPHIsDivergent(*userBlock);
      continue;
    }

    // taint outside users of values carried by divLoop
    for (auto & I : *userBlock) {
      if (isDivergent(I)) continue;

      for (auto & Op : I.operands()) {
        auto * opInst = dyn_cast<Instruction>(&Op);
        if (!opInst) continue;
        if (divLoop->contains(opInst->getParent())) {
          markDivergent(I); // FIXME this pre-empts re-evaluation
          pushUsers(I);
          break;
        }
      }
    }

    // visit all blocks in the dominance region
    for (auto * succBlock : successors(userBlock)) {
      if (!visited.insert(userBlock).second) continue;
      taintStack.push_back(succBlock);
    }
  }
}

void
DivergenceAnalysis::pushUsers(const Instruction & I) {
  for (const auto *user : I.users()) {
    const auto *userInst = dyn_cast<const Instruction>(user);
    if (!userInst)
      continue;

    // only compute divergent inside loop
    if (!inRegion(*userInst))
      continue;
    worklist.push_back(userInst);
  }
}

void DivergenceAnalysis::compute(bool IsLCSSA) {
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

    // maintain uniformity of overrides
    if (uniformOverrides.count(&I)) continue;

    bool wasDivergent = isDivergent(I);
    if (wasDivergent)
      continue;

    if (isa<TerminatorInst>(I)) {
      // spread control divergence to phi nodes
      auto &term = cast<TerminatorInst>(I);
      if (updateTerminator(term)) {
        markDivergent(term);

        auto * branchLoop = BDA.getLoopFor(*term.getParent());

        for (const auto *joinBlock : BDA.join_blocks(term)) {
          auto * joinLoop = BDA.getLoopFor(*joinBlock);

          if ((joinLoop == branchLoop) || // same loop level
              IsLCSSA // it is sufficient to taint LCSSA phi nodes
          ) {
            markDivergent(*joinBlock);
            for (auto &blockInst : *joinBlock) {
              if (!isa<PHINode>(blockInst))
                break;
              worklist.push_back(&blockInst);
            }

          } else {
            // users of values carried by (branchLoop) outside the loop become divergent
            // these users have to be dominated by the header of branchLoop or they are PHI nodes at the fringes of that dominated region

            taintLoopLiveOuts(*branchLoop->getHeader());
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
      pushUsers(I);
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
  // Iterate instructions using instructions() to ensure a deterministic order.
  for (auto &I : instructions(F)) {
    if (divergentValues.count(&I))
      OS << "DIVERGENT:" << I << "\n";
  }
}

// class GPUDivergenceAnalysis
GPUDivergenceAnalysis::GPUDivergenceAnalysis(Function & F, const DominatorTree & DT, const PostDominatorTree & PDT, const LoopInfo & loopInfo, const TargetTransformInfo & TTI)
    : BDA(F, DT, PDT, loopInfo)
    , DA(F, nullptr, BDA) {
  for (auto &I : instructions(F)) {
    if (TTI.isSourceOfDivergence(&I)) {
      DA.markDivergent(I);
    }
  }
  for (auto &Arg : F.args()) {
    if (TTI.isSourceOfDivergence(&Arg)) {
      DA.markDivergent(Arg);
    }
  }

  DA.compute(false); // not in LCSSA form
}

bool GPUDivergenceAnalysis::isDivergent(const Value &val) const {
  return DA.isDivergent(val);
}

void GPUDivergenceAnalysis::print(raw_ostream &OS, const Module *mod) const {
  OS << "Divergence of kernel " << DA.getFunction().getName() << " {\n";
  DA.print(OS, mod);
  OS << "}\n";
}



// class LoopDivergenceAnalysis
LoopDivergenceAnalysis::LoopDivergenceAnalysis(BranchDependenceAnalysis &BDA,
                                               const Loop &loop)
    : DA(*loop.getHeader()->getParent(), &loop, BDA) {
  for (const auto &I : *loop.getHeader()) {
    if (!isa<PHINode>(I))
      break;
    DA.markDivergent(I);
  }

  // after the scalar remainder loop is extracted, the loop exit condition will be uniform
  auto loopExitingInst = loop.getExitingBlock()->getTerminator();
  auto loopExitCond = cast<BranchInst>(loopExitingInst)->getCondition();
  DA.addUniformOverride(*loopExitCond);

  DA.compute(true); // LCSSA form
}

bool LoopDivergenceAnalysis::isDivergent(const Value &val) const {
  return DA.isDivergent(val);
}

void LoopDivergenceAnalysis::print(raw_ostream &OS, const Module *mod) const {
  OS << "Divergence of loop " << DA.getRegionLoop()->getName() << " {\n";
  DA.print(OS, mod);
  OS << "}\n";
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

  for (auto & BB : F) {
    auto * loop = loopInfo.getLoopFor(&BB);
    if (!loop || loop->getHeader() != &BB) continue;
    loopDivInfo.push_back(make_unique<LoopDivergenceAnalysis>(*BDA, *loop));
  }

  return false;
}

void LoopDivergencePrinter::print(raw_ostream &OS, const Module *mod) const {
  for (auto & divInfo : loopDivInfo) {
    divInfo->print(OS, mod);
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
