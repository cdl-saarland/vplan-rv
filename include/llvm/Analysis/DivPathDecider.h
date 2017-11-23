//
// Created by tkloessner on 08.05.17.
//

#ifndef LLVM_DIVPATHDECIDER_H
#define LLVM_DIVPATHDECIDER_H

#include <vector>
#include <set>
#include <map>
#include <utility>

namespace llvm {
class Loop;
class BasicBlock;
}

class DivPathDecider {
  struct Node;

  using NodeList = std::vector<const Node*>;
  using Edge = std::pair<const Node*, const Node*>;
  using EdgeSet = std::set<Edge>;
  using Path = std::vector<const llvm::BasicBlock*>;
  using PredecessorMap = std::map<const Node*, const Node*>;

  mutable llvm::DenseMap<const llvm::BasicBlock*, Node*> innodes, outnodes;

public:
  DivPathDecider() {}
  ~DivPathDecider();

  bool inducesDivergentExit(const llvm::BasicBlock* From,
                            const llvm::BasicBlock* Exit,
                            const llvm::Loop* loop) const;

  // Find n node-divergent paths from A to B, return true iff successful
  bool divergentPaths(const llvm::BasicBlock* From,
                      const llvm::BasicBlock* To,
                      unsigned n = 2U) const;

private:
  bool divergentPaths(const Node* source,
                      const NodeList& sinks,
                      unsigned n,
                      const llvm::Loop* loop) const;

  //! Finds a path from the source node to one of the sink nodes,
  //! of which any edge has non-positive flow.
  //! \param source The path source
  //! \param sinks The possible path sinks
  //! \param flow The network flow.
  //! \param parent A map in which the predecessor of each node is stored.
  //! \param TheLoop An optional loop out of which a path may not extend.
  //! \return The sink of the path
  const Node* findPath(const Node* source,
                       const NodeList& sinks,
                       const EdgeSet& flow,
                       PredecessorMap& parent,
                       const llvm::Loop* TheLoop) const;

  //! Takes a path description and adjusts the flow for every edge in it.
  //! \param start The start of the path.
  //! \param end The end of the path.
  //! \param parent A map containing a predecessor for each node n in the path. (n != start)
  //! \param flow The network flow.
  void injectFlow(const Node* start,
                  const Node* end,
                  const PredecessorMap& parent,
                  EdgeSet& flow) const;

  //! Extracts a path from the flow of the network.
  //! \param source The source node of every path.
  //! \param sinks The possible sink nodes of every path.
  //! \param flow The network flow.
  //! \param path A vector in which the path is inserted.
  void extractPath(const Node* source, const NodeList& sinks, EdgeSet& flow, Path& path) const;

  const Node* getInNode(const llvm::BasicBlock* BB) const;
  const Node* getOutNode(const llvm::BasicBlock* BB) const;
};


#endif // LLVM_DIVPATHDECIDER_H
