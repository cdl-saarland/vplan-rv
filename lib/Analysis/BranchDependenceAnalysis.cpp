//===- BranchDependenceAnalysis.cpp - Divergent Branch Dependence Calculation --===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements an algorithm that returns for a divergent branch
// the set of basic blocks whose phi nodes become divergent due to divergent control.
// These are the blocks that are reachable by two disjoint paths from the branch
// or loop exits that have a reaching path that is disjoint from a path to the loop latch.
//
// The BranchDependenceAnalysis is used in the DivergenceAnalysis to model control-induced
// divergence in phi nodes.
//
//===----------------------------------------------------------------------===//
#include <llvm/Analysis/BranchDependenceAnalysis.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/Analysis/PostDominators.h>
#include <llvm/ADT/SmallPtrSet.h>

#include <stack>

namespace llvm {



// class DivPathDecider

DivPathDecider::~DivPathDecider() {}

const DivPathDecider::Node*
DivPathDecider::findPath(const Node& source,
                         const NodeList& sinks,
                         const EdgeSet& flow,
                         PredecessorMap& parent,
                         const Loop* TheLoop) const
{
  DenseSet<const Node*> visited;
  std::stack<const Node*> stack;
  stack.push(&source);

  while (!stack.empty()) {
    const Node* node = stack.top();
    stack.pop();
    visited.insert(node);

    const BasicBlock& runner = node->BB;

    auto found = std::find(sinks.begin(), sinks.end(), node);
    if (found != sinks.end()) {
      return *found;
    }

    if (node->type == Node::OUT) {
      // Successors
      for (auto* succ : llvm::successors(&runner)) {
        const Node* next = getInNode(*succ);
        if (!TheLoop || TheLoop->contains(&runner)) {
          if (visited.count(next) == 0 && !flow.count({node, next})) {
            stack.push(next);
            parent[next] = node;
          }
        }
      }

      // Backwards split edge
      const Node* splitIN = getInNode(runner);
      if (visited.count(splitIN) == 0 && flow.count({splitIN, node})) {
        stack.push(splitIN);
        parent[splitIN] = node;
      }
    } else {
      // Traverse split edge
      const Node* splitOUT = getOutNode(runner);
      if (visited.count(splitOUT) == 0 && !flow.count({node, splitOUT})) {
        stack.push(splitOUT);
        parent[splitOUT] = node;
      }

      // Predecessors
      for (auto* pred : llvm::predecessors(&runner)) {
        const Node* next = getOutNode(*pred);
        if (!TheLoop || TheLoop->contains(&runner)) {
          if (visited.count(next) == 0 && flow.count({next, node})) {
            stack.push(next);
            parent[next] = node;
          }
        }
      }
    }
  }

  return nullptr;
}

bool DivPathDecider::inducesDivergentExit(const BasicBlock& From,
                                          const BasicBlock& LoopExit,
                                          const Loop& loop) const
{
  if (&From == loop.getLoopLatch()) {
    return LoopExit.getUniquePredecessor() == &From;
  }

  const Node* source = getOutNode(From);
  NodeList sinks = { getOutNode(LoopExit), getInNode(*loop.getHeader()) };
  return disjointPaths(*source, sinks, 2, &loop);
}

// Find n vertex-disjoint paths from A to B, this algorithm is a specialization of Ford-Fulkerson,
// that terminates after a flow of n is found. Running time is thus O(Edges) * n
bool DivPathDecider::disjointPaths(const BasicBlock& From, const BasicBlock& To, unsigned int n) const {
  const Node* source = getOutNode(From);
  NodeList sinks = { getInNode(To) };
  return disjointPaths(*source, sinks, n, nullptr);
}

bool DivPathDecider::disjointPaths(const Node& source,
                                   const NodeList& sinks,
                                   unsigned int n,
                                   const Loop* loop) const
{
  EdgeSet flow;

  for (unsigned i = 0; i < n; ++i) {
    PredecessorMap parent;
    const Node* sink = findPath(source, sinks, flow, parent, loop);
    if (!sink) {
      // Could not find a path.
      return false;
    }

    injectFlow(source, *sink, parent, flow);
  }

  return true;
}

void DivPathDecider::injectFlow(const Node& start,
                                const Node& end,
                                const PredecessorMap& parent,
                                EdgeSet& flow) const
{
  const Node *prev;
  for (
      const Node * tail = &end;
      tail && tail != &start;
      tail = prev)
  {
    prev = parent.find(tail)->second;

    // Backwards edge reset
    if (flow.erase({tail, prev})) {
      continue;
    } else {
      // Ordinary edge insert
      flow.insert({prev, tail});
    }
  }
}

const DivPathDecider::Node* DivPathDecider::getInNode(const BasicBlock& BB) const {
  auto found = innodes.find(&BB);
  if (found != innodes.end()) {
    return &found->second;
  }

  auto itInsert = innodes.insert(std::make_pair(&BB, Node(Node::IN, BB))).first;
  return &itInsert->second;
}

const DivPathDecider::Node* DivPathDecider::getOutNode(const BasicBlock& BB) const {
  auto found = outnodes.find(&BB);
  if (found != outnodes.end()) {
    return &found->second;
  }

  auto itInsert = outnodes.insert(std::make_pair(&BB, Node(Node::OUT, BB))).first;
  return &itInsert->second;
}




ConstBlockSet BranchDependenceAnalysis::emptySet;

BranchDependenceAnalysis::BranchDependenceAnalysis(Function & F,
                                                   const DominatorTree & _domTree,
                                                   const PostDominatorTree & _postDomTree,
                                                   const LoopInfo & _loopInfo)
: DPD()
, F(F)
, domTree(_domTree)
, postDomTree(_postDomTree)
, loopInfo(_loopInfo)
, joinBlocks()
{}

template<typename TreeT>
static auto
GetNode(const TreeT & tree, const BasicBlock & block) { return tree.getNode(const_cast<BasicBlock*>(&block)); }

const ConstBlockSet&
BranchDependenceAnalysis::join_blocks(const llvm::TerminatorInst& term) const {
  {
    auto it = joinBlocks.find(&term);
    if (it != joinBlocks.end()) return it->second;
  }

  const BasicBlock& termBlock = *term.getParent();
  auto * termDomNode = GetNode(domTree, termBlock);
  auto * termPDNode = GetNode(postDomTree, termBlock);
  auto * pdBoundNode = termPDNode ? termPDNode->getIDom() : nullptr;

  ConstBlockSet termJoinBlocks;

  // Find two disjoint paths from @termBlock to @phiBlock
  // @phiBlocks have to be part of the enclosing SESE region of @termBlock
  // we rule out any phiBlocks that are not part of the same SESE region as the terminator
  // as these can not have two disjoint paths

  for (const auto & phiBlock : F) {
    // not a value join point
    if (!isa<PHINode>(phiBlock.begin())) continue;

    // termBlock not below idom of phiBlock -> no disjoint paths possible
    auto * phiDomNode = GetNode(domTree, phiBlock);
    assert(phiDomNode);
    auto * domBoundNode = phiDomNode->getIDom();
    if (domBoundNode && !domTree.dominates(domBoundNode, termDomNode))
      continue;

    // phiBlock not below imm post dom of termBlock -> no disjoint paths possible
    auto * phiPDNode = GetNode(postDomTree, phiBlock);
    if (pdBoundNode && !domTree.dominates(pdBoundNode, phiPDNode)) {
      continue;
    }

    if (DPD.disjointPaths(termBlock, phiBlock)) {
      // TODO pass dominance bounds to DPD to speedup search further
      termJoinBlocks.insert(&phiBlock);
    }
  }

  // Find divergent loop exits
  if (const Loop* loop = loopInfo.getLoopFor(&termBlock)) {
    llvm::SmallVector<BasicBlock*, 4> loopExits;
    loop->getExitBlocks(loopExits);

    for (const BasicBlock* loopExit : loopExits) {
      if (DPD.inducesDivergentExit(termBlock, *loopExit, *loop)) {
        termJoinBlocks.insert(loopExit);
      }
    }
  }

  auto it = joinBlocks.insert(std::make_pair(&term, termJoinBlocks));
  assert(it.second);
  return it.first->second;
}

}
