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
class Function;
class BasicBlock;
class TerminatorInst;
class DominatorTree;
class PostDominatorTree;
class TerminatorInst;

// class DivPathDecider
//
// The DivPasthDecide class implements queries for disjoint paths in the CFG.
// The BranchDependenceAnalysis relies on this information to infer
// control-induced divergence in phi nodes, including LCSSA phis.
using ConstBlockSet = SmallPtrSet<const BasicBlock *, 4>;
class DivPathDecider {
public:
  DivPathDecider() = default;
  ~DivPathDecider();

  bool inducesDivergentExit(const BasicBlock &From, const BasicBlock &LoopExit,
                            const Loop &loop) const;

  // Find n node-divergent paths from A to B, return true iff successful
  bool disjointPaths(const BasicBlock &From, const BasicBlock &To,
                     unsigned n = 2U) const;

private:
  struct Node {
    enum SplitType { IN = 0, OUT = 1 } type;
    const BasicBlock &BB;

    Node(SplitType type, const llvm::BasicBlock &BB) : type(type), BB(BB) {}
  };
  using NodeList = SmallVector<const Node *, 8>;
  using Edge = std::pair<const Node *, const Node *>;
  using EdgeSet = DenseSet<Edge>;
  using PredecessorMap = DenseMap<const Node *, const Node *>;

  mutable DenseMap<const BasicBlock *, Node> innodes, outnodes;

  bool disjointPaths(const Node &source, const NodeList &sinks, unsigned n,
                     const Loop *loop) const;

  //! Finds a path from the source node to one of the sink nodes,
  //! of which any edge has non-positive flow.
  //! \param source The path source
  //! \param sinks The possible path sinks
  //! \param flow The network flow.
  //! \param parent A map in which the predecessor of each node is stored.
  //! \param TheLoop An optional loop out of which a path may not extend.
  //! \return The sink of the path
  const Node *findPath(const Node &source, const NodeList &sinks,
                       const EdgeSet &flow, PredecessorMap &parent,
                       const Loop *TheLoop) const;

  //! Takes a path description and adjusts the flow for every edge in it.
  //! \param start The start of the path.
  //! \param end The end of the path.
  //! \param parent A map containing a predecessor for each node n in the path.
  //! (n != start) \param flow The network flow.
  void injectFlow(const Node &start, const Node &end,
                  const PredecessorMap &parent, EdgeSet &flow) const;

  const Node *getInNode(const BasicBlock &BB) const;
  const Node *getOutNode(const BasicBlock &BB) const;
};

/// BranchDependenceAnalysis
///
/// This is an analysis to be used in the context of SIMD/GPU execution of a
/// function. It enables the VectorizationAnalysis to correctly propagate
/// divergent control from branches to phi nodes.
///
/// In the SPMD setting a group of threads executes a function in
/// bulk-synchornouous fashion. For every instruction each thread may see the
/// same result (uniform value) or a different result (varying/divergent value).
/// If a varying instruction computes a branch condition control among the
/// threads may diverge (p in the example). If phi nodes are dependent on such a
/// divergent branch the phis may receive values from different incoming blocks
/// at once (phi node x). The phis become divergent even if the incoming values
/// per predecessor are uniform values.
///
/// if (p) {
//    x0 = 1
//  } else {
//    x1 = 2
//  }
//  C: x = phi [x0, x1]
///
/// The analysis result maps every branch to a set of basic blocks whose phi
/// nodes will become varying if the branch is varying. This is directly used by
/// the DivergenceAnalysis to propagate control-induced value divergence.
///
class BranchDependenceAnalysis {
  static ConstBlockSet emptySet;
  mutable DivPathDecider DPD;

  Function &F;
  const DominatorTree &domTree;
  const PostDominatorTree &postDomTree;
  const LoopInfo &loopInfo;

  mutable DenseMap<const TerminatorInst *, ConstBlockSet> joinBlocks;

public:
  BranchDependenceAnalysis(Function &F, const DominatorTree &domTree,
                           const PostDominatorTree &postDomTree,
                           const LoopInfo &loopInfo);

  Loop* getLoopFor(const BasicBlock & block) const;

  bool dominates(const BasicBlock & A, const BasicBlock & B) const;

  // returns the set of blocks whose PHI nodes become divergent if @term is a
  // divergent branch
  const ConstBlockSet &join_blocks(const TerminatorInst &term) const;
};

} // namespace llvm

#endif // LLVM_ANALYSIS_BRANCHDEPENDENCEANALYSIS_H
