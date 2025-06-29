#ifndef _C4_YML_FWD_HPP_
#define _C4_YML_FWD_HPP_

/** @file fwd.hpp forward declarations */

namespace c4 {
namespace yml {

struct NodeScalar;
struct NodeInit;
struct NodeData;
struct NodeType;
class NodeRef;
class ConstNodeRef;
class Tree;
struct ReferenceResolver;
template<class EventHandler> class ParseEngine;
struct EventHandlerTree;
using Parser = ParseEngine<EventHandlerTree>;

} // namespace c4
} // namespace yml

#endif /* _C4_YML_FWD_HPP_ */
