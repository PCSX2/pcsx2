#ifndef _C4_YML_NODE_HPP_
#define _C4_YML_NODE_HPP_

/** @file node.hpp Node classes */

#include <cstddef>

#include "c4/yml/tree.hpp"
#include "c4/base64.hpp"

#ifdef __clang__
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wtype-limits"
#   pragma clang diagnostic ignored "-Wold-style-cast"
#elif defined(__GNUC__)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wtype-limits"
#   pragma GCC diagnostic ignored "-Wold-style-cast"
#   pragma GCC diagnostic ignored "-Wuseless-cast"
#elif defined(_MSC_VER)
#   pragma warning(push)
#   pragma warning(disable: 4251/*needs to have dll-interface to be used by clients of struct*/)
#   pragma warning(disable: 4296/*expression is always 'boolean_value'*/)
#endif

namespace c4 {
namespace yml {

/** @addtogroup doc_node_classes
 *
 * @{
 */


/** @defgroup doc_serialization_helpers Serialization helpers
 *
 * @{
 */
template<class K> struct Key { K & k; }; // NOLINT
template<> struct Key<fmt::const_base64_wrapper> { fmt::const_base64_wrapper wrapper; };
template<> struct Key<fmt::base64_wrapper> { fmt::base64_wrapper wrapper; };

template<class K> C4_ALWAYS_INLINE Key<K> key(K & k) { return Key<K>{k}; }
C4_ALWAYS_INLINE Key<fmt::const_base64_wrapper> key(fmt::const_base64_wrapper w) { return {w}; }
C4_ALWAYS_INLINE Key<fmt::base64_wrapper> key(fmt::base64_wrapper w) { return {w}; }


template<class T> void write(NodeRef *n, T const& v);

template<class T> inline bool read(ConstNodeRef const& C4_RESTRICT n, T *v);
template<class T> inline bool read(NodeRef const& C4_RESTRICT n, T *v);
template<class T> inline bool readkey(ConstNodeRef const& C4_RESTRICT n, T *v);
template<class T> inline bool readkey(NodeRef const& C4_RESTRICT n, T *v);

/** @} */


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

// forward decls
class NodeRef;
class ConstNodeRef;


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/** @cond dev */
namespace detail {

template<class NodeRefType>
struct child_iterator
{
    using value_type = NodeRefType;
    using tree_type = typename NodeRefType::tree_type;

    tree_type * C4_RESTRICT m_tree;
    id_type m_child_id;

    child_iterator(tree_type * t, id_type id) : m_tree(t), m_child_id(id) {}

    child_iterator& operator++ () { RYML_ASSERT(m_child_id != NONE); m_child_id = m_tree->next_sibling(m_child_id); return *this; }
    child_iterator& operator-- () { RYML_ASSERT(m_child_id != NONE); m_child_id = m_tree->prev_sibling(m_child_id); return *this; }

    NodeRefType operator*  () const { return NodeRefType(m_tree, m_child_id); }
    NodeRefType operator-> () const { return NodeRefType(m_tree, m_child_id); }

    bool operator!= (child_iterator that) const { RYML_ASSERT(m_tree == that.m_tree); return m_child_id != that.m_child_id; }
    bool operator== (child_iterator that) const { RYML_ASSERT(m_tree == that.m_tree); return m_child_id == that.m_child_id; }
};

template<class NodeRefType>
struct children_view_
{
    using n_iterator = child_iterator<NodeRefType>;

    n_iterator b, e;

    children_view_(n_iterator const& C4_RESTRICT b_,
                          n_iterator const& C4_RESTRICT e_) : b(b_), e(e_) {}

    n_iterator begin() const { return b; }
    n_iterator end  () const { return e; }
};

template<class NodeRefType, class Visitor>
bool _visit(NodeRefType &node, Visitor fn, id_type indentation_level, bool skip_root=false)
{
    id_type increment = 0;
    if( ! (node.is_root() && skip_root))
    {
        if(fn(node, indentation_level))
            return true;
        ++increment;
    }
    if(node.has_children())
    {
        for(auto ch : node.children())
        {
            if(_visit(ch, fn, indentation_level + increment, false)) // no need to forward skip_root as it won't be root
            {
                return true;
            }
        }
    }
    return false;
}

template<class NodeRefType, class Visitor>
bool _visit_stacked(NodeRefType &node, Visitor fn, id_type indentation_level, bool skip_root=false)
{
    id_type increment = 0;
    if( ! (node.is_root() && skip_root))
    {
        if(fn(node, indentation_level))
        {
            return true;
        }
        ++increment;
    }
    if(node.has_children())
    {
        fn.push(node, indentation_level);
        for(auto ch : node.children())
        {
            if(_visit_stacked(ch, fn, indentation_level + increment, false)) // no need to forward skip_root as it won't be root
            {
                fn.pop(node, indentation_level);
                return true;
            }
        }
        fn.pop(node, indentation_level);
    }
    return false;
}

template<class Impl, class ConstImpl>
struct RoNodeMethods;
} // detail
/** @endcond */

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


/** a CRTP base providing read-only methods for @ref ConstNodeRef and @ref NodeRef */
namespace detail {
template<class Impl, class ConstImpl>
struct RoNodeMethods
{
    C4_SUPPRESS_WARNING_GCC_CLANG_WITH_PUSH("-Wcast-align")
    /** @cond dev */
    // helper CRTP macros, undefined at the end
    #define tree_ ((ConstImpl const* C4_RESTRICT)this)->m_tree
    #define id_ ((ConstImpl const* C4_RESTRICT)this)->m_id
    #define tree__ ((Impl const* C4_RESTRICT)this)->m_tree
    #define id__ ((Impl const* C4_RESTRICT)this)->m_id
    // require readable: this is a precondition for reading from the
    // tree using this object.
    #define _C4RR()                                       \
        RYML_ASSERT(tree_ != nullptr);                    \
        _RYML_CB_ASSERT(tree_->m_callbacks, id_ != NONE); \
        _RYML_CB_ASSERT(tree_->m_callbacks, (((Impl const* C4_RESTRICT)this)->readable()))
    // a SFINAE beautifier to enable a function only if the
    // implementation is mutable
    #define _C4_IF_MUTABLE(ty) typename std::enable_if<!std::is_same<U, ConstImpl>::value, ty>::type
    /** @endcond */

public:

    /** @name node property getters */
    /** @{ */

    /** returns the data or null when the id is NONE */
    C4_ALWAYS_INLINE NodeData const* get() const RYML_NOEXCEPT { return ((Impl const*)this)->readable() ? tree_->get(id_) : nullptr; }
    /** returns the data or null when the id is NONE */
    template<class U=Impl>
    C4_ALWAYS_INLINE auto get() RYML_NOEXCEPT -> _C4_IF_MUTABLE(NodeData*) { return ((Impl const*)this)->readable() ? tree__->get(id__) : nullptr; }

    C4_ALWAYS_INLINE NodeType    type()     const RYML_NOEXCEPT { _C4RR(); return tree_->type(id_); }     /**< Forward to @ref Tree::type_str(). Node must be readable. */
    C4_ALWAYS_INLINE const char* type_str() const RYML_NOEXCEPT { _C4RR(); return tree_->type_str(id_); } /**< Forward to @ref Tree::type_str(). Node must be readable. */

    C4_ALWAYS_INLINE csubstr key()        const RYML_NOEXCEPT { _C4RR(); return tree_->key(id_); }        /**< Forward to @ref Tree::key(). Node must be readable. */
    C4_ALWAYS_INLINE csubstr key_tag()    const RYML_NOEXCEPT { _C4RR(); return tree_->key_tag(id_); }    /**< Forward to @ref Tree::key_tag(). Node must be readable. */
    C4_ALWAYS_INLINE csubstr key_ref()    const RYML_NOEXCEPT { _C4RR(); return tree_->key_ref(id_); }    /**< Forward to @ref Tree::key_ref(). Node must be readable. */
    C4_ALWAYS_INLINE csubstr key_anchor() const RYML_NOEXCEPT { _C4RR(); return tree_->key_anchor(id_); } /**< Forward to @ref Tree::key_anchor(). Node must be readable. */

    C4_ALWAYS_INLINE csubstr val()        const RYML_NOEXCEPT { _C4RR(); return tree_->val(id_); }        /**< Forward to @ref Tree::val(). Node must be readable. */
    C4_ALWAYS_INLINE csubstr val_tag()    const RYML_NOEXCEPT { _C4RR(); return tree_->val_tag(id_); }    /**< Forward to @ref Tree::val_tag(). Node must be readable. */
    C4_ALWAYS_INLINE csubstr val_ref()    const RYML_NOEXCEPT { _C4RR(); return tree_->val_ref(id_); }    /**< Forward to @ref Tree::val_ref(). Node must be readable. */
    C4_ALWAYS_INLINE csubstr val_anchor() const RYML_NOEXCEPT { _C4RR(); return tree_->val_anchor(id_); } /**< Forward to @ref Tree::val_anchor(). Node must be readable. */

    C4_ALWAYS_INLINE NodeScalar const& keysc() const RYML_NOEXCEPT { _C4RR(); return tree_->keysc(id_); } /**< Forward to @ref Tree::keysc(). Node must be readable. */
    C4_ALWAYS_INLINE NodeScalar const& valsc() const RYML_NOEXCEPT { _C4RR(); return tree_->valsc(id_); } /**< Forward to @ref Tree::valsc(). Node must be readable. */

    C4_ALWAYS_INLINE bool key_is_null() const RYML_NOEXCEPT { _C4RR(); return tree_->key_is_null(id_); } /**< Forward to @ref Tree::key_is_null(). Node must be readable. */
    C4_ALWAYS_INLINE bool val_is_null() const RYML_NOEXCEPT { _C4RR(); return tree_->val_is_null(id_); } /**< Forward to @ref Tree::val_is_null(). Node must be readable. */

    C4_ALWAYS_INLINE bool is_key_unfiltered() const noexcept { _C4RR(); return tree_->is_key_unfiltered(id_); } /**< Forward to @ref Tree::is_key_unfiltered(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_val_unfiltered() const noexcept { _C4RR(); return tree_->is_val_unfiltered(id_); } /**< Forward to @ref Tree::is_val_unfiltered(). Node must be readable. */

    /** @} */

public:

    /** @name node type predicates */
    /** @{ */

    C4_ALWAYS_INLINE bool empty()            const RYML_NOEXCEPT { _C4RR(); return tree_->empty(id_); } /**< Forward to @ref Tree::empty(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_stream()        const RYML_NOEXCEPT { _C4RR(); return tree_->is_stream(id_); } /**< Forward to @ref Tree::is_stream(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_doc()           const RYML_NOEXCEPT { _C4RR(); return tree_->is_doc(id_); } /**< Forward to @ref Tree::is_doc(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_container()     const RYML_NOEXCEPT { _C4RR(); return tree_->is_container(id_); } /**< Forward to @ref Tree::is_container(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_map()           const RYML_NOEXCEPT { _C4RR(); return tree_->is_map(id_); } /**< Forward to @ref Tree::is_map(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_seq()           const RYML_NOEXCEPT { _C4RR(); return tree_->is_seq(id_); } /**< Forward to @ref Tree::is_seq(). Node must be readable. */
    C4_ALWAYS_INLINE bool has_val()          const RYML_NOEXCEPT { _C4RR(); return tree_->has_val(id_); } /**< Forward to @ref Tree::has_val(). Node must be readable. */
    C4_ALWAYS_INLINE bool has_key()          const RYML_NOEXCEPT { _C4RR(); return tree_->has_key(id_); } /**< Forward to @ref Tree::has_key(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_val()           const RYML_NOEXCEPT { _C4RR(); return tree_->is_val(id_); } /**< Forward to @ref Tree::is_val(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_keyval()        const RYML_NOEXCEPT { _C4RR(); return tree_->is_keyval(id_); } /**< Forward to @ref Tree::is_keyval(). Node must be readable. */
    C4_ALWAYS_INLINE bool has_key_tag()      const RYML_NOEXCEPT { _C4RR(); return tree_->has_key_tag(id_); } /**< Forward to @ref Tree::has_key_tag(). Node must be readable. */
    C4_ALWAYS_INLINE bool has_val_tag()      const RYML_NOEXCEPT { _C4RR(); return tree_->has_val_tag(id_); } /**< Forward to @ref Tree::has_val_tag(). Node must be readable. */
    C4_ALWAYS_INLINE bool has_key_anchor()   const RYML_NOEXCEPT { _C4RR(); return tree_->has_key_anchor(id_); } /**< Forward to @ref Tree::has_key_anchor(). Node must be readable. */
    C4_ALWAYS_INLINE bool has_val_anchor()   const RYML_NOEXCEPT { _C4RR(); return tree_->has_val_anchor(id_); } /**< Forward to @ref Tree::has_val_anchor(). Node must be readable. */
    C4_ALWAYS_INLINE bool has_anchor()       const RYML_NOEXCEPT { _C4RR(); return tree_->has_anchor(id_); } /**< Forward to @ref Tree::has_anchor(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_key_ref()       const RYML_NOEXCEPT { _C4RR(); return tree_->is_key_ref(id_); } /**< Forward to @ref Tree::is_key_ref(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_val_ref()       const RYML_NOEXCEPT { _C4RR(); return tree_->is_val_ref(id_); } /**< Forward to @ref Tree::is_val_ref(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_ref()           const RYML_NOEXCEPT { _C4RR(); return tree_->is_ref(id_); } /**< Forward to @ref Tree::is_ref(). Node must be readable. */
    C4_ALWAYS_INLINE bool parent_is_seq()    const RYML_NOEXCEPT { _C4RR(); return tree_->parent_is_seq(id_); } /**< Forward to @ref Tree::parent_is_seq(). Node must be readable. */
    C4_ALWAYS_INLINE bool parent_is_map()    const RYML_NOEXCEPT { _C4RR(); return tree_->parent_is_map(id_); } /**< Forward to @ref Tree::parent_is_map(). Node must be readable. */

    RYML_DEPRECATED("use has_key_anchor()")  bool is_key_anchor() const noexcept { _C4RR(); return tree_->has_key_anchor(id_); }
    RYML_DEPRECATED("use has_val_anchor()")  bool is_val_hanchor() const noexcept { _C4RR(); return tree_->has_val_anchor(id_); }
    RYML_DEPRECATED("use has_anchor()")      bool is_anchor()     const noexcept { _C4RR(); return tree_->has_anchor(id_); }
    RYML_DEPRECATED("use has_anchor() || is_ref()") bool is_anchor_or_ref() const noexcept { _C4RR(); return tree_->is_anchor_or_ref(id_); }

    /** @} */

public:

    /** @name style predicates */
    /** @{ */

    // documentation to the right -->

    C4_ALWAYS_INLINE bool type_has_any(NodeType_e bits)  const RYML_NOEXCEPT { _C4RR(); return tree_->type_has_any(id_, bits); }  /**< Forward to @ref Tree::type_has_any(). Node must be readable. */
    C4_ALWAYS_INLINE bool type_has_all(NodeType_e bits)  const RYML_NOEXCEPT { _C4RR(); return tree_->type_has_all(id_, bits); }  /**< Forward to @ref Tree::type_has_all(). Node must be readable. */
    C4_ALWAYS_INLINE bool type_has_none(NodeType_e bits) const RYML_NOEXCEPT { _C4RR(); return tree_->type_has_none(id_, bits); } /**< Forward to @ref Tree::type_has_none(). Node must be readable. */

    C4_ALWAYS_INLINE NodeType key_style()       const RYML_NOEXCEPT { _C4RR(); return tree_->key_style(id_); } /**< Forward to @ref Tree::key_style(). Node must be readable. */
    C4_ALWAYS_INLINE NodeType val_style()       const RYML_NOEXCEPT { _C4RR(); return tree_->val_style(id_); } /**< Forward to @ref Tree::val_style(). Node must be readable. */

    C4_ALWAYS_INLINE bool is_container_styled() const RYML_NOEXCEPT { _C4RR(); return tree_->is_container_styled(id_); } /**< Forward to @ref Tree::is_container_styled(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_block()            const RYML_NOEXCEPT { _C4RR(); return tree_->is_block(id_); }   /**< Forward to @ref Tree::is_block(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_flow_sl()          const RYML_NOEXCEPT { _C4RR(); return tree_->is_flow_sl(id_); } /**< Forward to @ref Tree::is_flow_sl(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_flow_ml()          const RYML_NOEXCEPT { _C4RR(); return tree_->is_flow_ml(id_); } /**< Forward to @ref Tree::is_flow_ml(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_flow()             const RYML_NOEXCEPT { _C4RR(); return tree_->is_flow(id_); }    /**< Forward to @ref Tree::is_flow(). Node must be readable. */

    C4_ALWAYS_INLINE bool is_key_styled()       const RYML_NOEXCEPT { _C4RR(); return tree_->is_key_styled(id_); }  /**< Forward to @ref Tree::is_key_styled(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_val_styled()       const RYML_NOEXCEPT { _C4RR(); return tree_->is_val_styled(id_); }  /**< Forward to @ref Tree::is_val_styled(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_key_literal()      const RYML_NOEXCEPT { _C4RR(); return tree_->is_key_literal(id_); } /**< Forward to @ref Tree::is_key_literal(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_val_literal()      const RYML_NOEXCEPT { _C4RR(); return tree_->is_val_literal(id_); } /**< Forward to @ref Tree::is_val_literal(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_key_folded()       const RYML_NOEXCEPT { _C4RR(); return tree_->is_key_folded(id_); }  /**< Forward to @ref Tree::is_key_folded(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_val_folded()       const RYML_NOEXCEPT { _C4RR(); return tree_->is_val_folded(id_); }  /**< Forward to @ref Tree::is_val_folded(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_key_squo()         const RYML_NOEXCEPT { _C4RR(); return tree_->is_key_squo(id_); }    /**< Forward to @ref Tree::is_key_squo(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_val_squo()         const RYML_NOEXCEPT { _C4RR(); return tree_->is_val_squo(id_); }    /**< Forward to @ref Tree::is_val_squo(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_key_dquo()         const RYML_NOEXCEPT { _C4RR(); return tree_->is_key_dquo(id_); }    /**< Forward to @ref Tree::is_key_dquo(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_val_dquo()         const RYML_NOEXCEPT { _C4RR(); return tree_->is_val_dquo(id_); }    /**< Forward to @ref Tree::is_val_dquo(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_key_plain()        const RYML_NOEXCEPT { _C4RR(); return tree_->is_key_plain(id_); }   /**< Forward to @ref Tree::is_key_plain(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_val_plain()        const RYML_NOEXCEPT { _C4RR(); return tree_->is_val_plain(id_); }   /**< Forward to @ref Tree::is_val_plain(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_key_quoted()       const RYML_NOEXCEPT { _C4RR(); return tree_->is_key_quoted(id_); }  /**< Forward to @ref Tree::is_key_quoted(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_val_quoted()       const RYML_NOEXCEPT { _C4RR(); return tree_->is_val_quoted(id_); }  /**< Forward to @ref Tree::is_val_quoted(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_quoted()           const RYML_NOEXCEPT { _C4RR(); return tree_->is_quoted(id_); }      /**< Forward to @ref Tree::is_quoted(). Node must be readable. */

    /** @} */

public:

    /** @name hierarchy predicates */
    /** @{ */

    // documentation to the right -->

    C4_ALWAYS_INLINE bool is_root()    const RYML_NOEXCEPT { _C4RR(); return tree_->is_root(id_); } /**< Forward to @ref Tree::is_root(). Node must be readable. */
    C4_ALWAYS_INLINE bool has_parent() const RYML_NOEXCEPT { _C4RR(); return tree_->has_parent(id_); } /**< Forward to @ref Tree::has_parent()  Node must be readable. */
    C4_ALWAYS_INLINE bool is_ancestor(ConstImpl const& ancestor) const RYML_NOEXCEPT { _C4RR(); return tree_->is_ancestor(id_, ancestor.m_id); } /**< Forward to @ref Tree::is_ancestor()  Node must be readable. */

    C4_ALWAYS_INLINE bool has_child(ConstImpl const& n) const RYML_NOEXCEPT { _C4RR(); return n.readable() ? tree_->has_child(id_, n.m_id) : false; } /**< Forward to @ref Tree::has_child(). Node must be readable. */
    C4_ALWAYS_INLINE bool has_child(id_type node) const RYML_NOEXCEPT { _C4RR(); return tree_->has_child(id_, node); } /**< Forward to @ref Tree::has_child(). Node must be readable. */
    C4_ALWAYS_INLINE bool has_child(csubstr name) const RYML_NOEXCEPT { _C4RR(); return tree_->has_child(id_, name); } /**< Forward to @ref Tree::has_child(). Node must be readable. */
    C4_ALWAYS_INLINE bool has_children() const RYML_NOEXCEPT { _C4RR(); return tree_->has_children(id_); } /**< Forward to @ref Tree::has_child(). Node must be readable. */

    C4_ALWAYS_INLINE bool has_sibling(ConstImpl const& n) const RYML_NOEXCEPT { _C4RR(); return n.readable() ? tree_->has_sibling(id_, n.m_id) : false; } /**< Forward to @ref Tree::has_sibling(). Node must be readable. */
    C4_ALWAYS_INLINE bool has_sibling(id_type node) const RYML_NOEXCEPT { _C4RR(); return tree_->has_sibling(id_, node); } /**< Forward to @ref Tree::has_sibling(). Node must be readable. */
    C4_ALWAYS_INLINE bool has_sibling(csubstr name) const RYML_NOEXCEPT { _C4RR(); return tree_->has_sibling(id_, name); } /**< Forward to @ref Tree::has_sibling(). Node must be readable. */
    C4_ALWAYS_INLINE bool has_other_siblings() const RYML_NOEXCEPT { _C4RR(); return tree_->has_other_siblings(id_); }  /**< Forward to @ref Tree::has_sibling(). Node must be readable. */

    RYML_DEPRECATED("use has_other_siblings()") bool has_siblings() const RYML_NOEXCEPT { _C4RR(); return tree_->has_siblings(id_); }

    /** @} */

public:

    /** @name hierarchy getters */
    /** @{ */

    // documentation to the right -->

    template<class U=Impl>
    C4_ALWAYS_INLINE auto doc(id_type i) RYML_NOEXCEPT -> _C4_IF_MUTABLE(Impl) { RYML_ASSERT(tree_); return {tree__, tree__->doc(i)}; } /**< Forward to @ref Tree::doc(). Node must be readable. */
    C4_ALWAYS_INLINE ConstImpl doc(id_type i) const RYML_NOEXCEPT { RYML_ASSERT(tree_); return {tree_, tree_->doc(i)}; }                /**< Forward to @ref Tree::doc(). Node must be readable. succeeds even when the node may have invalid or seed id */

    template<class U=Impl>
    C4_ALWAYS_INLINE auto parent() RYML_NOEXCEPT -> _C4_IF_MUTABLE(Impl) { _C4RR(); return {tree__, tree__->parent(id__)}; } /**< Forward to @ref Tree::parent(). Node must be readable. */
    C4_ALWAYS_INLINE ConstImpl parent() const RYML_NOEXCEPT { _C4RR(); return {tree_, tree_->parent(id_)}; }                 /**< Forward to @ref Tree::parent(). Node must be readable. */

    template<class U=Impl>
    C4_ALWAYS_INLINE auto first_child() RYML_NOEXCEPT -> _C4_IF_MUTABLE(Impl) { _C4RR(); return {tree__, tree__->first_child(id__)}; }  /**< Forward to @ref Tree::first_child(). Node must be readable. */
    C4_ALWAYS_INLINE ConstImpl first_child() const RYML_NOEXCEPT { _C4RR(); return {tree_, tree_->first_child(id_)}; }                  /**< Forward to @ref Tree::first_child(). Node must be readable. */

    template<class U=Impl>
    C4_ALWAYS_INLINE auto last_child() RYML_NOEXCEPT -> _C4_IF_MUTABLE(Impl) { _C4RR(); return {tree__, tree__->last_child(id__)}; }  /**< Forward to @ref Tree::last_child(). Node must be readable. */
    C4_ALWAYS_INLINE ConstImpl last_child () const RYML_NOEXCEPT { _C4RR(); return {tree_, tree_->last_child (id_)}; }                /**< Forward to @ref Tree::last_child(). Node must be readable. */

    template<class U=Impl>
    C4_ALWAYS_INLINE auto child(id_type pos) RYML_NOEXCEPT -> _C4_IF_MUTABLE(Impl) { _C4RR(); return {tree__, tree__->child(id__, pos)}; }  /**< Forward to @ref Tree::child(). Node must be readable. */
    C4_ALWAYS_INLINE ConstImpl child(id_type pos) const RYML_NOEXCEPT { _C4RR(); return {tree_, tree_->child(id_, pos)}; }                  /**< Forward to @ref Tree::child(). Node must be readable. */

    template<class U=Impl>
    C4_ALWAYS_INLINE auto find_child(csubstr name)  RYML_NOEXCEPT -> _C4_IF_MUTABLE(Impl) { _C4RR(); return {tree__, tree__->find_child(id__, name)}; }  /**< Forward to @ref Tree::first_child(). Node must be readable. */
    C4_ALWAYS_INLINE ConstImpl find_child(csubstr name) const RYML_NOEXCEPT { _C4RR(); return {tree_, tree_->find_child(id_, name)}; }                   /**< Forward to @ref Tree::first_child(). Node must be readable. */

    template<class U=Impl>
    C4_ALWAYS_INLINE auto prev_sibling() RYML_NOEXCEPT -> _C4_IF_MUTABLE(Impl) { _C4RR(); return {tree__, tree__->prev_sibling(id__)}; }  /**< Forward to @ref Tree::prev_sibling(). Node must be readable. */
    C4_ALWAYS_INLINE ConstImpl prev_sibling() const RYML_NOEXCEPT { _C4RR(); return {tree_, tree_->prev_sibling(id_)}; }                  /**< Forward to @ref Tree::prev_sibling(). Node must be readable. */

    template<class U=Impl>
    C4_ALWAYS_INLINE auto next_sibling() RYML_NOEXCEPT -> _C4_IF_MUTABLE(Impl) { _C4RR(); return {tree__, tree__->next_sibling(id__)}; }  /**< Forward to @ref Tree::next_sibling(). Node must be readable. */
    C4_ALWAYS_INLINE ConstImpl next_sibling() const RYML_NOEXCEPT { _C4RR(); return {tree_, tree_->next_sibling(id_)}; }                  /**< Forward to @ref Tree::next_sibling(). Node must be readable. */

    template<class U=Impl>
    C4_ALWAYS_INLINE auto first_sibling() RYML_NOEXCEPT -> _C4_IF_MUTABLE(Impl) { _C4RR(); return {tree__, tree__->first_sibling(id__)}; }  /**< Forward to @ref Tree::first_sibling(). Node must be readable. */
    C4_ALWAYS_INLINE ConstImpl first_sibling() const RYML_NOEXCEPT { _C4RR(); return {tree_, tree_->first_sibling(id_)}; }                  /**< Forward to @ref Tree::first_sibling(). Node must be readable. */

    template<class U=Impl>
    C4_ALWAYS_INLINE auto last_sibling() RYML_NOEXCEPT -> _C4_IF_MUTABLE(Impl) { _C4RR(); return {tree__, tree__->last_sibling(id__)}; }  /**< Forward to @ref Tree::last_sibling(). Node must be readable. */
    C4_ALWAYS_INLINE ConstImpl last_sibling () const RYML_NOEXCEPT { _C4RR(); return {tree_, tree_->last_sibling(id_)}; }                 /**< Forward to @ref Tree::last_sibling(). Node must be readable. */

    template<class U=Impl>
    C4_ALWAYS_INLINE auto sibling(id_type pos) RYML_NOEXCEPT -> _C4_IF_MUTABLE(Impl) { _C4RR(); return {tree__, tree__->sibling(id__, pos)}; }  /**< Forward to @ref Tree::sibling(). Node must be readable. */
    C4_ALWAYS_INLINE ConstImpl sibling(id_type pos) const RYML_NOEXCEPT { _C4RR(); return {tree_, tree_->sibling(id_, pos)}; }                  /**< Forward to @ref Tree::sibling(). Node must be readable. */

    template<class U=Impl>
    C4_ALWAYS_INLINE auto find_sibling(csubstr name) RYML_NOEXCEPT -> _C4_IF_MUTABLE(Impl) { _C4RR(); return {tree__, tree__->find_sibling(id__, name)}; }  /**< Forward to @ref Tree::find_sibling(). Node must be readable. */
    C4_ALWAYS_INLINE ConstImpl find_sibling(csubstr name) const RYML_NOEXCEPT { _C4RR(); return {tree_, tree_->find_sibling(id_, name)}; }                  /**< Forward to @ref Tree::find_sibling(). Node must be readable. */

    C4_ALWAYS_INLINE id_type num_children() const RYML_NOEXCEPT { _C4RR(); return tree_->num_children(id_); } /**< O(num_children). Forward to @ref Tree::num_children(). */
    C4_ALWAYS_INLINE id_type num_siblings() const RYML_NOEXCEPT { _C4RR(); return tree_->num_siblings(id_); } /**< O(num_children). Forward to @ref Tree::num_siblings(). */
    C4_ALWAYS_INLINE id_type num_other_siblings() const RYML_NOEXCEPT { _C4RR(); return tree_->num_other_siblings(id_); } /**< O(num_siblings). Forward to @ref Tree::num_other_siblings(). */
    C4_ALWAYS_INLINE id_type child_pos(ConstImpl const& n) const RYML_NOEXCEPT { _C4RR(); _RYML_CB_ASSERT(tree_->m_callbacks, n.readable()); return tree_->child_pos(id_, n.m_id); } /**< O(num_children). Forward to @ref Tree::child_pos(). */
    C4_ALWAYS_INLINE id_type sibling_pos(ConstImpl const& n) const RYML_NOEXCEPT { _C4RR(); _RYML_CB_ASSERT(tree_->callbacks(), n.readable()); return tree_->child_pos(tree_->parent(id_), n.m_id); } /**< O(num_siblings). Forward to @ref Tree::sibling_pos(). */

    C4_ALWAYS_INLINE id_type depth_asc() const RYML_NOEXCEPT { _C4RR(); return tree_->depth_asc(id_); } /** O(log(num_nodes)). Forward to Tree::depth_asc(). Node must be readable. */
    C4_ALWAYS_INLINE id_type depth_desc() const RYML_NOEXCEPT { _C4RR(); return tree_->depth_desc(id_); } /** O(num_nodes). Forward to Tree::depth_desc(). Node must be readable. */

    /** @} */

public:

    /** @name square_brackets
     * operator[] */
    /** @{ */

    /** Find child by key; complexity is O(num_children).
     *
     * Returns the requested node, or an object in seed state if no
     * such child is found (see @ref NodeRef for an explanation of
     * what is seed state). When the object is in seed state, using it
     * to read from the tree is UB. The seed node can be used to write
     * to the tree provided that its create() method is called prior
     * to writing, which happens in most modifying methods in
     * NodeRef. It is the caller's responsibility to verify that the
     * returned node is readable before subsequently using it to read
     * from the tree.
     *
     * @warning the calling object must be readable. This precondition
     * is asserted. The assertion is performed only if @ref
     * RYML_USE_ASSERT is set to true. As with the non-const overload,
     * it is UB to call this method if the node is not readable.
     *
     * @see https://github.com/biojppm/rapidyaml/issues/389 */
    template<class U=Impl>
    C4_ALWAYS_INLINE auto operator[] (csubstr key) RYML_NOEXCEPT -> _C4_IF_MUTABLE(Impl)
    {
        _C4RR();
        id_type ch = tree__->find_child(id__, key);
        return ch != NONE ? Impl(tree__, ch) : Impl(tree__, id__, key);
    }

    /** Find child by position; complexity is O(pos).
     *
     * Returns the requested node, or an object in seed state if no
     * such child is found (see @ref NodeRef for an explanation of
     * what is seed state). When the object is in seed state, using it
     * to read from the tree is UB. The seed node can be used to write
     * to the tree provided that its create() method is called prior
     * to writing, which happens in most modifying methods in
     * NodeRef. It is the caller's responsibility to verify that the
     * returned node is readable before subsequently using it to read
     * from the tree.
     *
     * @warning the calling object must be readable. This precondition
     * is asserted. The assertion is performed only if @ref
     * RYML_USE_ASSERT is set to true. As with the non-const overload,
     * it is UB to call this method if the node is not readable.
     *
     * @see https://github.com/biojppm/rapidyaml/issues/389 */
    template<class U=Impl>
    C4_ALWAYS_INLINE auto operator[] (id_type pos) RYML_NOEXCEPT -> _C4_IF_MUTABLE(Impl)
    {
        _C4RR();
        id_type ch = tree__->child(id__, pos);
        return ch != NONE ? Impl(tree__, ch) : Impl(tree__, id__, pos);
    }

    /** Find a child by key; complexity is O(num_children).
     *
     * Behaves similar to the non-const overload, but further asserts
     * that the returned node is readable (because it can never be in
     * a seed state). The assertion is performed only if @ref
     * RYML_USE_ASSERT is set to true. As with the non-const overload,
     * it is UB to use the return value if it is not valid.
     *
     * @see https://github.com/biojppm/rapidyaml/issues/389  */
    C4_ALWAYS_INLINE ConstImpl operator[] (csubstr key) const RYML_NOEXCEPT
    {
        _C4RR();
        id_type ch = tree_->find_child(id_, key);
        _RYML_CB_ASSERT(tree_->m_callbacks, ch != NONE);
        return {tree_, ch};
    }

    /** Find a child by position; complexity is O(pos).
     *
     * Behaves similar to the non-const overload, but further asserts
     * that the returned node is readable (because it can never be in
     * a seed state). This assertion is performed only if @ref
     * RYML_USE_ASSERT is set to true. As with the non-const overload,
     * it is UB to use the return value if it is not valid.
     *
     * @see https://github.com/biojppm/rapidyaml/issues/389  */
    C4_ALWAYS_INLINE ConstImpl operator[] (id_type pos) const RYML_NOEXCEPT
    {
        _C4RR();
        id_type ch = tree_->child(id_, pos);
        _RYML_CB_ASSERT(tree_->m_callbacks, ch != NONE);
        return {tree_, ch};
    }

    /** @} */

public:

    /** @name at
     *
     * These functions are the analogue to operator[], with the
     * difference that they emit an error instead of an
     * assertion. That is, if any of the pre or post conditions is
     * violated, an error is always emitted (resulting in a call to
     * the error callback).
     *
     * @{ */

    /** Find child by key; complexity is O(num_children).
     *
     * Returns the requested node, or an object in seed state if no
     * such child is found (see @ref NodeRef for an explanation of
     * what is seed state). When the object is in seed state, using it
     * to read from the tree is UB. The seed node can be subsequently
     * used to write to the tree provided that its create() method is
     * called prior to writing, which happens inside most mutating
     * methods in NodeRef. It is the caller's responsibility to verify
     * that the returned node is readable before subsequently using it
     * to read from the tree.
     *
     * @warning This method will call the error callback (regardless
     * of build type or of the value of RYML_USE_ASSERT) whenever any
     * of the following preconditions is violated: a) the object is
     * valid (points at a tree and a node), b) the calling object must
     * be readable (must not be in seed state), c) the calling object
     * must be pointing at a MAP node. The preconditions are similar
     * to the non-const operator[](csubstr), but instead of using
     * assertions, this function directly checks those conditions and
     * calls the error callback if any of the checks fail.
     *
     * @note since it is valid behavior for the returned node to be in
     * seed state, the error callback is not invoked when this
     * happens. */
    template<class U=Impl>
    C4_ALWAYS_INLINE auto at(csubstr key) -> _C4_IF_MUTABLE(Impl)
    {
        RYML_CHECK(tree_ != nullptr);
        _RYML_CB_CHECK(tree_->m_callbacks, (id_ >= 0 && id_ < tree_->capacity()));
        _RYML_CB_CHECK(tree_->m_callbacks, ((Impl const*)this)->readable());
        _RYML_CB_CHECK(tree_->m_callbacks, tree_->is_map(id_));
        id_type ch = tree__->find_child(id__, key);
        return ch != NONE ? Impl(tree__, ch) : Impl(tree__, id__, key);
    }

    /** Find child by position; complexity is O(pos).
     *
     * Returns the requested node, or an object in seed state if no
     * such child is found (see @ref NodeRef for an explanation of
     * what is seed state). When the object is in seed state, using it
     * to read from the tree is UB. The seed node can be used to write
     * to the tree provided that its create() method is called prior
     * to writing, which happens in most modifying methods in
     * NodeRef. It is the caller's responsibility to verify that the
     * returned node is readable before subsequently using it to read
     * from the tree.
     *
     * @warning This method will call the error callback (regardless
     * of build type or of the value of RYML_USE_ASSERT) whenever any
     * of the following preconditions is violated: a) the object is
     * valid (points at a tree and a node), b) the calling object must
     * be readable (must not be in seed state), c) the calling object
     * must be pointing at a MAP node. The preconditions are similar
     * to the non-const operator[](id_type), but instead of using
     * assertions, this function directly checks those conditions and
     * calls the error callback if any of the checks fail.
     *
     * @note since it is valid behavior for the returned node to be in
     * seed state, the error callback is not invoked when this
     * happens. */
    template<class U=Impl>
    C4_ALWAYS_INLINE auto at(id_type pos) -> _C4_IF_MUTABLE(Impl)
    {
        RYML_CHECK(tree_ != nullptr);
        const id_type cap = tree_->capacity();
        _RYML_CB_CHECK(tree_->m_callbacks, (id_ >= 0 && id_ < cap));
        _RYML_CB_CHECK(tree_->m_callbacks, (pos >= 0 && pos < cap));
        _RYML_CB_CHECK(tree_->m_callbacks, ((Impl const*)this)->readable());
        _RYML_CB_CHECK(tree_->m_callbacks, tree_->is_container(id_));
        id_type ch = tree__->child(id__, pos);
        return ch != NONE ? Impl(tree__, ch) : Impl(tree__, id__, pos);
    }

    /** Get a child by name, with error checking; complexity is
     * O(num_children).
     *
     * Behaves as operator[](csubstr) const, but always raises an
     * error (even when RYML_USE_ASSERT is set to false) when the
     * returned node does not exist, or when this node is not
     * readable, or when it is not a map. This behaviour is similar to
     * std::vector::at(), but the error consists in calling the error
     * callback instead of directly raising an exception. */
    ConstImpl at(csubstr key) const
    {
        RYML_CHECK(tree_ != nullptr);
        _RYML_CB_CHECK(tree_->m_callbacks, (id_ >= 0 && id_ < tree_->capacity()));
        _RYML_CB_CHECK(tree_->m_callbacks, ((Impl const*)this)->readable());
        _RYML_CB_CHECK(tree_->m_callbacks, tree_->is_map(id_));
        id_type ch = tree_->find_child(id_, key);
        _RYML_CB_CHECK(tree_->m_callbacks, ch != NONE);
        return {tree_, ch};
    }

    /** Get a child by position, with error checking; complexity is
     * O(pos).
     *
     * Behaves as operator[](id_type) const, but always raises an error
     * (even when RYML_USE_ASSERT is set to false) when the returned
     * node does not exist, or when this node is not readable, or when
     * it is not a container. This behaviour is similar to
     * std::vector::at(), but the error consists in calling the error
     * callback instead of directly raising an exception. */
    ConstImpl at(id_type pos) const
    {
        RYML_CHECK(tree_ != nullptr);
        const id_type cap = tree_->capacity();
        _RYML_CB_CHECK(tree_->m_callbacks, (id_ >= 0 && id_ < cap));
        _RYML_CB_CHECK(tree_->m_callbacks, (pos >= 0 && pos < cap));
        _RYML_CB_CHECK(tree_->m_callbacks, ((Impl const*)this)->readable());
        _RYML_CB_CHECK(tree_->m_callbacks, tree_->is_container(id_));
        const id_type ch = tree_->child(id_, pos);
        _RYML_CB_CHECK(tree_->m_callbacks, ch != NONE);
        return {tree_, ch};
    }

    /** @} */

public:

    /** @name locations */
    /** @{ */

    Location location(Parser const& parser) const
    {
        _C4RR();
        return tree_->location(parser, id_);
    }

    /** @} */

public:

    /** @name deserialization */
    /** @{ */

    /** deserialize the node's val to the given variable, forwarding
     * to the user-overrideable @ref read() function. */
    template<class T>
    ConstImpl const& operator>> (T &v) const
    {
        _C4RR();
        if( ! read((ConstImpl const&)*this, &v))
            _RYML_CB_ERR(tree_->m_callbacks, "could not deserialize value");
        return *((ConstImpl const*)this);
    }

    /** deserialize the node's key to the given variable, forwarding
     * to the user-overrideable @ref read() function; use @ref key()
     * to disambiguate; for example: `node >> ryml::key(var)` */
    template<class T>
    ConstImpl const& operator>> (Key<T> v) const
    {
        _C4RR();
        if( ! readkey((ConstImpl const&)*this, &v.k))
            _RYML_CB_ERR(tree_->m_callbacks, "could not deserialize key");
        return *((ConstImpl const*)this);
    }

    /** look for a child by name, if it exists assign to var. return
     * true if the child existed. */
    template<class T>
    bool get_if(csubstr name, T *var) const
    {
        _C4RR();
        ConstImpl ch = find_child(name);
        if(!ch.readable())
            return false;
        ch >> *var;
        return true;
    }

    /** look for a child by name, if it exists assign to var,
     * otherwise default to fallback. return true if the child
     * existed. */
    template<class T>
    bool get_if(csubstr name, T *var, T const& fallback) const
    {
        _C4RR();
        ConstImpl ch = find_child(name);
        if(ch.readable())
        {
            ch >> *var;
            return true;
        }
        else
        {
            *var = fallback;
            return false;
        }
    }

    /** @name deserialization_base64 */
    /** @{ */

    /** deserialize the node's key as base64. lightweight wrapper over @ref deserialize_key() */
    ConstImpl const& operator>> (Key<fmt::base64_wrapper> w) const
    {
        deserialize_key(w.wrapper);
        return *((ConstImpl const*)this);
    }

    /** deserialize the node's val as base64. lightweight wrapper over @ref deserialize_val() */
    ConstImpl const& operator>> (fmt::base64_wrapper w) const
    {
        deserialize_val(w);
        return *((ConstImpl const*)this);
    }

    /** decode the base64-encoded key and assign the
     * decoded blob to the given buffer/
     * @return the size of base64-decoded blob */
    size_t deserialize_key(fmt::base64_wrapper v) const
    {
        _C4RR();
        return from_chars(key(), &v);
    }
    /** decode the base64-encoded key and assign the
     * decoded blob to the given buffer/
     * @return the size of base64-decoded blob */
    size_t deserialize_val(fmt::base64_wrapper v) const
    {
        _C4RR();
        return from_chars(val(), &v);
    };

    /** @} */

    /** @} */

public:

    #if defined(__clang__)
    #   pragma clang diagnostic push
    #   pragma clang diagnostic ignored "-Wnull-dereference"
    #elif defined(__GNUC__)
    #   pragma GCC diagnostic push
    #   if __GNUC__ >= 6
    #       pragma GCC diagnostic ignored "-Wnull-dereference"
    #   endif
    #endif

    /** @name iteration */
    /** @{ */

    using iterator = detail::child_iterator<Impl>;
    using const_iterator = detail::child_iterator<ConstImpl>;
    using children_view = detail::children_view_<Impl>;
    using const_children_view = detail::children_view_<ConstImpl>;

    /** get an iterator to the first child */
    template<class U=Impl>
    C4_ALWAYS_INLINE auto begin() RYML_NOEXCEPT -> _C4_IF_MUTABLE(iterator) { _C4RR(); return iterator(tree__, tree__->first_child(id__)); }
    /** get an iterator to the first child */
    C4_ALWAYS_INLINE const_iterator begin() const RYML_NOEXCEPT { _C4RR(); return const_iterator(tree_, tree_->first_child(id_)); }
    /** get an iterator to the first child */
    C4_ALWAYS_INLINE const_iterator cbegin() const RYML_NOEXCEPT { _C4RR(); return const_iterator(tree_, tree_->first_child(id_)); }

    /** get an iterator to after the last child */
    template<class U=Impl>
    C4_ALWAYS_INLINE auto end() RYML_NOEXCEPT -> _C4_IF_MUTABLE(iterator) { _C4RR(); return iterator(tree__, NONE); }
    /** get an iterator to after the last child */
    C4_ALWAYS_INLINE const_iterator end() const RYML_NOEXCEPT { _C4RR(); return const_iterator(tree_, NONE); }
    /** get an iterator to after the last child */
    C4_ALWAYS_INLINE const_iterator cend() const RYML_NOEXCEPT { _C4RR(); return const_iterator(tree_, tree_->first_child(id_)); }

    /** get an iterable view over children */
    template<class U=Impl>
    C4_ALWAYS_INLINE auto children() RYML_NOEXCEPT -> _C4_IF_MUTABLE(children_view) { _C4RR(); return children_view(begin(), end()); }
    /** get an iterable view over children */
    C4_ALWAYS_INLINE const_children_view children() const RYML_NOEXCEPT { _C4RR(); return const_children_view(begin(), end()); }
    /** get an iterable view over children */
    C4_ALWAYS_INLINE const_children_view cchildren() const RYML_NOEXCEPT { _C4RR(); return const_children_view(begin(), end()); }

    /** get an iterable view over all siblings (including the calling node) */
    template<class U=Impl>
    C4_ALWAYS_INLINE auto siblings() RYML_NOEXCEPT -> _C4_IF_MUTABLE(children_view)
    {
        _C4RR();
        NodeData const *nd = tree__->get(id__);
        return (nd->m_parent != NONE) ? // does it have a parent?
            children_view(iterator(tree__, tree_->get(nd->m_parent)->m_first_child), iterator(tree__, NONE))
            :
            children_view(end(), end());
    }
    /** get an iterable view over all siblings (including the calling node) */
    C4_ALWAYS_INLINE const_children_view siblings() const RYML_NOEXCEPT
    {
        _C4RR();
        NodeData const *nd = tree_->get(id_);
        return (nd->m_parent != NONE) ? // does it have a parent?
            const_children_view(const_iterator(tree_, tree_->get(nd->m_parent)->m_first_child), const_iterator(tree_, NONE))
            :
            const_children_view(end(), end());
    }
    /** get an iterable view over all siblings (including the calling node) */
    C4_ALWAYS_INLINE const_children_view csiblings() const RYML_NOEXCEPT { return siblings(); }

    /** visit every child node calling fn(node) */
    template<class Visitor>
    bool visit(Visitor fn, id_type indentation_level=0, bool skip_root=true) const RYML_NOEXCEPT
    {
        _C4RR();
        return detail::_visit(*(ConstImpl const*)this, fn, indentation_level, skip_root);
    }
    /** visit every child node calling fn(node) */
    template<class Visitor, class U=Impl>
    auto visit(Visitor fn, id_type indentation_level=0, bool skip_root=true) RYML_NOEXCEPT
        -> _C4_IF_MUTABLE(bool)
    {
        _C4RR();
        return detail::_visit(*(Impl*)this, fn, indentation_level, skip_root);
    }

    /** visit every child node calling fn(node, level) */
    template<class Visitor>
    bool visit_stacked(Visitor fn, id_type indentation_level=0, bool skip_root=true) const RYML_NOEXCEPT
    {
        _C4RR();
        return detail::_visit_stacked(*(ConstImpl const*)this, fn, indentation_level, skip_root);
    }
    /** visit every child node calling fn(node, level) */
    template<class Visitor, class U=Impl>
    auto visit_stacked(Visitor fn, id_type indentation_level=0, bool skip_root=true) RYML_NOEXCEPT
        -> _C4_IF_MUTABLE(bool)
    {
        _C4RR();
        return detail::_visit_stacked(*(Impl*)this, fn, indentation_level, skip_root);
    }

    /** @} */

    #if defined(__clang__)
    #   pragma clang diagnostic pop
    #elif defined(__GNUC__)
    #   pragma GCC diagnostic pop
    #endif

    #undef _C4_IF_MUTABLE
    #undef _C4RR
    #undef tree_
    #undef tree__
    #undef id_
    #undef id__

    C4_SUPPRESS_WARNING_GCC_CLANG_POP
};
} // detail


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
/** Holds a pointer to an existing tree, and a node id. It can be used
 * only to read from the tree.
 *
 * @warning The lifetime of the tree must be larger than that of this
 * object. It is up to the user to ensure that this happens. */
class RYML_EXPORT ConstNodeRef : public detail::RoNodeMethods<ConstNodeRef, ConstNodeRef> // NOLINT
{
public:

    using tree_type = Tree const;

public:

    Tree const* C4_RESTRICT m_tree;
    id_type m_id;

    friend NodeRef;
    friend struct detail::RoNodeMethods<ConstNodeRef, ConstNodeRef>;

public:

    /** @name construction */
    /** @{ */

    ConstNodeRef() noexcept : m_tree(nullptr), m_id(NONE) {}
    ConstNodeRef(Tree const &t) noexcept : m_tree(&t), m_id(t .root_id()) {}
    ConstNodeRef(Tree const *t) noexcept : m_tree(t ), m_id(t->root_id()) {}
    ConstNodeRef(Tree const *t, id_type id) noexcept : m_tree(t), m_id(id) {}
    ConstNodeRef(std::nullptr_t) noexcept : m_tree(nullptr), m_id(NONE) {}

    ConstNodeRef(ConstNodeRef const&) noexcept = default;
    ConstNodeRef(ConstNodeRef     &&) noexcept = default;

    inline ConstNodeRef(NodeRef const&) noexcept;
    inline ConstNodeRef(NodeRef     &&) noexcept;

    /** @} */

public:

    /** @name assignment */
    /** @{ */

    ConstNodeRef& operator= (std::nullptr_t) noexcept { m_tree = nullptr; m_id = NONE; return *this; }

    ConstNodeRef& operator= (ConstNodeRef const&) noexcept = default;
    ConstNodeRef& operator= (ConstNodeRef     &&) noexcept = default;

    ConstNodeRef& operator= (NodeRef const&) noexcept;
    ConstNodeRef& operator= (NodeRef     &&) noexcept;

    /** @} */

public:

    /** @name state queries
     *
     * see @ref NodeRef for an explanation on what these states mean */
    /** @{ */

    C4_ALWAYS_INLINE bool invalid() const noexcept { return (!m_tree) || (m_id == NONE); }
    /** because a ConstNodeRef cannot be used to write to the tree,
     * readable() has the same meaning as !invalid() */
    C4_ALWAYS_INLINE bool readable() const noexcept { return m_tree != nullptr && m_id != NONE; }
    /** because a ConstNodeRef cannot be used to write to the tree, it can never be a seed.
     * This method is provided for API equivalence between ConstNodeRef and NodeRef. */
    constexpr static C4_ALWAYS_INLINE bool is_seed() noexcept { return false; }

    RYML_DEPRECATED("use one of readable(), is_seed() or !invalid()") bool valid() const noexcept { return m_tree != nullptr && m_id != NONE; }

    /** @} */

public:

    /** @name member getters */
    /** @{ */

    C4_ALWAYS_INLINE Tree const* tree() const noexcept { return m_tree; }
    C4_ALWAYS_INLINE id_type id() const noexcept { return m_id; }

    /** @} */

public:

    /** @name comparisons */
    /** @{ */

    C4_ALWAYS_INLINE bool operator== (ConstNodeRef const& that) const RYML_NOEXCEPT { return that.m_tree == m_tree && m_id == that.m_id; }
    C4_ALWAYS_INLINE bool operator!= (ConstNodeRef const& that) const RYML_NOEXCEPT { return ! this->operator== (that); }

    /** @cond dev */
    RYML_DEPRECATED("use invalid()")  bool operator== (std::nullptr_t) const noexcept { return m_tree == nullptr || m_id == NONE; }
    RYML_DEPRECATED("use !invalid()") bool operator!= (std::nullptr_t) const noexcept { return !(m_tree == nullptr || m_id == NONE); }

    RYML_DEPRECATED("use (this->val() == s)") bool operator== (csubstr s) const RYML_NOEXCEPT { RYML_ASSERT(m_tree); _RYML_CB_ASSERT(m_tree->m_callbacks, m_id != NONE); return m_tree->val(m_id) == s; }
    RYML_DEPRECATED("use (this->val() != s)") bool operator!= (csubstr s) const RYML_NOEXCEPT { RYML_ASSERT(m_tree); _RYML_CB_ASSERT(m_tree->m_callbacks, m_id != NONE); return m_tree->val(m_id) != s; }
    /** @endcond */

    /** @} */

};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

// NOLINTBEGIN(cppcoreguidelines-c-copy-assignment-signature,misc-unconventional-assign-operator)

/** A reference to a node in an existing yaml tree, offering a more
 * convenient API than the index-based API used in the tree.
 *
 * Unlike its imutable ConstNodeRef peer, a NodeRef can be used to
 * mutate the tree, both by writing to existing nodes and by creating
 * new nodes to subsequently write to. Semantically, a NodeRef
 * object can be in one of three states:
 *
 * ```text
 * invalid  := not pointing at anything
 * readable := points at an existing tree/node
 * seed     := points at an existing tree, and the node
 *             may come to exist, if we write to it.
 * ```
 *
 * So both `readable` and `seed` are states where the node is also `valid`.
 *
 * ```cpp
 * Tree t = parse_in_arena("{a: b}");
 * NodeRef invalid; // not pointing at anything.
 * NodeRef readable = t["a"]; // also valid, because "a" exists
 * NodeRef seed = t["none"]; // also valid, but is seed because "none" is not in the map
 * ```
 *
 * When the object is in seed state, using it to read from the tree is
 * UB. The seed node can be used to write to the tree, provided that
 * its create() method is called prior to writing, which happens in
 * most modifying methods in NodeRef.
 *
 * It is the owners's responsibility to verify that an existing
 * node is readable before subsequently using it to read from the
 * tree.
 *
 * @warning The lifetime of the tree must be larger than that of this
 * object. It is up to the user to ensure that this happens.
 */
class RYML_EXPORT NodeRef : public detail::RoNodeMethods<NodeRef, ConstNodeRef> // NOLINT
{
public:

    using tree_type = Tree;
    using base_type = detail::RoNodeMethods<NodeRef, ConstNodeRef>;

private:

    Tree *C4_RESTRICT m_tree;
    id_type m_id;

    /** This member is used to enable lazy operator[] writing. When a child
     * with a key or index is not found, m_id is set to the id of the parent
     * and the asked-for key or index are stored in this member until a write
     * does happen. Then it is given as key or index for creating the child.
     * When a key is used, the csubstr stores it (so the csubstr's string is
     * non-null and the csubstr's size is different from NONE). When an index is
     * used instead, the csubstr's string is set to null, and only the csubstr's
     * size is set to a value different from NONE. Otherwise, when operator[]
     * does find the child then this member is empty: the string is null and
     * the size is NONE. */
    csubstr m_seed;

    friend ConstNodeRef;
    friend struct detail::RoNodeMethods<NodeRef, ConstNodeRef>;

    // require valid: a helper macro, undefined at the end
    #define _C4RR()                                                         \
        RYML_ASSERT(m_tree != nullptr);                                     \
        _RYML_CB_ASSERT(m_tree->m_callbacks, m_id != NONE && !is_seed())
    // require id: a helper macro, undefined at the end
    #define _C4RID()                                                        \
        RYML_ASSERT(m_tree != nullptr);                                     \
        _RYML_CB_ASSERT(m_tree->m_callbacks, m_id != NONE)

public:

    /** @name construction */
    /** @{ */

    NodeRef() noexcept : m_tree(nullptr), m_id(NONE), m_seed() { _clear_seed(); }
    NodeRef(Tree &t) noexcept : m_tree(&t), m_id(t .root_id()), m_seed() { _clear_seed(); }
    NodeRef(Tree *t) noexcept : m_tree(t ), m_id(t->root_id()), m_seed() { _clear_seed(); }
    NodeRef(Tree *t, id_type id) noexcept : m_tree(t), m_id(id), m_seed() { _clear_seed(); }
    NodeRef(Tree *t, id_type id, id_type seed_pos) noexcept : m_tree(t), m_id(id), m_seed() { m_seed.str = nullptr; m_seed.len = (size_t)seed_pos; }
    NodeRef(Tree *t, id_type id, csubstr  seed_key) noexcept : m_tree(t), m_id(id), m_seed(seed_key) {}
    NodeRef(std::nullptr_t) noexcept : m_tree(nullptr), m_id(NONE), m_seed() {}

    void _clear_seed() noexcept { /*do the following manually or an assert is triggered: */ m_seed.str = nullptr; m_seed.len = npos; }

    /** @} */

public:

    /** @name assignment */
    /** @{ */

    NodeRef(NodeRef const&) noexcept = default;
    NodeRef(NodeRef     &&) noexcept = default;

    NodeRef& operator= (NodeRef const&) noexcept = default;
    NodeRef& operator= (NodeRef     &&) noexcept = default;

    /** @} */

public:

    /** @name state_queries
     * @{ */

    /** true if the object is not referring to any existing or seed node. @see the doc for @ref NodeRef */
    bool invalid() const noexcept { return m_tree == nullptr || m_id == NONE; }
    /** true if the object is not invalid and in seed state. @see the doc for @ref NodeRef */
    bool is_seed() const noexcept { return (m_tree != nullptr && m_id != NONE) && (m_seed.str != nullptr || m_seed.len != (size_t)NONE); }
    /** true if the object is not invalid and not in seed state. @see the doc for @ref NodeRef */
    bool readable() const noexcept { return (m_tree != nullptr && m_id != NONE) && (m_seed.str == nullptr && m_seed.len == (size_t)NONE); }

    RYML_DEPRECATED("use one of readable(), is_seed() or !invalid()") inline bool valid() const { return m_tree != nullptr && m_id != NONE; }

    /** @} */

public:

    /** @name comparisons */
    /** @{ */

    bool operator== (NodeRef const& that) const
    {
        if(m_tree == that.m_tree && m_id == that.m_id)
        {
            bool seed = is_seed();
            if(seed == that.is_seed())
            {
                if(seed)
                {
                    return (m_seed.len == that.m_seed.len)
                        && (m_seed.str == that.m_seed.str
                            || m_seed == that.m_seed); // do strcmp only in the last resort
                }
                return true;
            }
        }
        return false;
    }
    bool operator!= (NodeRef const& that) const { return ! this->operator==(that); }

    bool operator== (ConstNodeRef const& that) const { return m_tree == that.m_tree && m_id == that.m_id && !is_seed(); }
    bool operator!= (ConstNodeRef const& that) const { return ! this->operator==(that); }

    /** @cond dev */
    RYML_DEPRECATED("use !readable()") bool operator== (std::nullptr_t) const { return m_tree == nullptr || m_id == NONE || is_seed(); }
    RYML_DEPRECATED("use readable()")  bool operator!= (std::nullptr_t) const { return !(m_tree == nullptr || m_id == NONE || is_seed()); }

    RYML_DEPRECATED("use `this->val() == s`") bool operator== (csubstr s) const { _C4RR(); _RYML_CB_ASSERT(m_tree->m_callbacks, has_val()); return m_tree->val(m_id) == s; }
    RYML_DEPRECATED("use `this->val() != s`") bool operator!= (csubstr s) const { _C4RR(); _RYML_CB_ASSERT(m_tree->m_callbacks, has_val()); return m_tree->val(m_id) != s; }
    /** @endcond */

public:

    /** @name node_property_getters
     * @{ */

    C4_ALWAYS_INLINE Tree * tree() noexcept { return m_tree; }
    C4_ALWAYS_INLINE Tree const* tree() const noexcept { return m_tree; }

    C4_ALWAYS_INLINE id_type id() const noexcept { return m_id; }

    /** @} */

public:

    /** @name node_modifiers */
    /** @{ */

    void create() { _apply_seed(); }

    void change_type(NodeType t) { _C4RR(); m_tree->change_type(m_id, t); }

    void set_type(NodeType t) { _apply_seed(); m_tree->_set_flags(m_id, t); }
    void set_key(csubstr key) { _apply_seed(); m_tree->_set_key(m_id, key); }
    void set_val(csubstr val) { _apply_seed(); m_tree->_set_val(m_id, val); }
    void set_key_tag(csubstr key_tag) { _apply_seed(); m_tree->set_key_tag(m_id, key_tag); }
    void set_val_tag(csubstr val_tag) { _apply_seed(); m_tree->set_val_tag(m_id, val_tag); }
    void set_key_anchor(csubstr key_anchor) { _apply_seed(); m_tree->set_key_anchor(m_id, key_anchor); }
    void set_val_anchor(csubstr val_anchor) { _apply_seed(); m_tree->set_val_anchor(m_id, val_anchor); }
    void set_key_ref(csubstr key_ref) { _apply_seed(); m_tree->set_key_ref(m_id, key_ref); }
    void set_val_ref(csubstr val_ref) { _apply_seed(); m_tree->set_val_ref(m_id, val_ref); }

    void set_container_style(NodeType_e style) { _C4RR(); m_tree->set_container_style(m_id, style); }
    void set_key_style(NodeType_e style) { _C4RR(); m_tree->set_key_style(m_id, style); }
    void set_val_style(NodeType_e style) { _C4RR(); m_tree->set_val_style(m_id, style); }
    void clear_style(bool recurse=false) { _C4RR(); m_tree->clear_style(m_id, recurse); }
    void set_style_conditionally(NodeType type_mask,
                                 NodeType rem_style_flags,
                                 NodeType add_style_flags,
                                 bool recurse=false)
    {
        _C4RR(); m_tree->set_style_conditionally(m_id, type_mask, rem_style_flags, add_style_flags, recurse);
    }

public:

    void clear()
    {
        if(is_seed())
            return;
        m_tree->remove_children(m_id);
        m_tree->_clear(m_id);
    }

    void clear_key()
    {
        if(is_seed())
            return;
        m_tree->_clear_key(m_id);
    }

    void clear_val()
    {
        if(is_seed())
            return;
        m_tree->_clear_val(m_id);
    }

    void clear_children()
    {
        if(is_seed())
            return;
        m_tree->remove_children(m_id);
    }

    void operator= (NodeType_e t)
    {
        _apply_seed();
        m_tree->_add_flags(m_id, t);
    }

    void operator|= (NodeType_e t)
    {
        _apply_seed();
        m_tree->_add_flags(m_id, t);
    }

    void operator= (NodeInit const& v)
    {
        _apply_seed();
        _apply(v);
    }

    void operator= (NodeScalar const& v)
    {
        _apply_seed();
        _apply(v);
    }

    void operator= (std::nullptr_t)
    {
        _apply_seed();
        _apply(csubstr{});
    }

    void operator= (csubstr v)
    {
        _apply_seed();
        _apply(v);
    }

    template<size_t N>
    void operator= (const char (&v)[N])
    {
        _apply_seed();
        csubstr sv;
        sv.assign<N>(v);
        _apply(sv);
    }

    /** @} */

public:

    /** @name serialization */
    /** @{ */

    /** serialize a variable to the arena */
    template<class T>
    csubstr to_arena(T const& C4_RESTRICT s)
    {
        RYML_ASSERT(m_tree); // no need for valid or readable
        return m_tree->to_arena(s);
    }

    template<class T>
    size_t set_key_serialized(T const& C4_RESTRICT k)
    {
        _apply_seed();
        csubstr s = m_tree->to_arena(k);
        m_tree->_set_key(m_id, s);
        return s.len;
    }
    size_t set_key_serialized(std::nullptr_t)
    {
        _apply_seed();
        m_tree->_set_key(m_id, csubstr{});
        return 0;
    }

    template<class T>
    size_t set_val_serialized(T const& C4_RESTRICT v)
    {
        _apply_seed();
        csubstr s = m_tree->to_arena(v);
        m_tree->_set_val(m_id, s);
        return s.len;
    }
    size_t set_val_serialized(std::nullptr_t)
    {
        _apply_seed();
        m_tree->_set_val(m_id, csubstr{});
        return 0;
    }

    /** encode a blob as base64 into the tree's arena, then assign the
     * result to the node's key
     * @return the size of base64-encoded blob */
    size_t set_key_serialized(fmt::const_base64_wrapper w);
    /** encode a blob as base64 into the tree's arena, then assign the
     * result to the node's val
     * @return the size of base64-encoded blob */
    size_t set_val_serialized(fmt::const_base64_wrapper w);

    /** serialize a variable, then assign the result to the node's val */
    NodeRef& operator<< (csubstr s)
    {
        // this overload is needed to prevent ambiguity (there's also
        // operator<< for writing a substr to a stream)
        _apply_seed();
        write(this, s);
        _RYML_CB_ASSERT(m_tree->m_callbacks, val() == s);
        return *this;
    }

    template<class T>
    NodeRef& operator<< (T const& C4_RESTRICT v)
    {
        _apply_seed();
        write(this, v);
        return *this;
    }

    /** serialize a variable, then assign the result to the node's key */
    template<class T>
    NodeRef& operator<< (Key<const T> const& C4_RESTRICT v)
    {
        _apply_seed();
        set_key_serialized(v.k);
        return *this;
    }

    /** serialize a variable, then assign the result to the node's key */
    template<class T>
    NodeRef& operator<< (Key<T> const& C4_RESTRICT v)
    {
        _apply_seed();
        set_key_serialized(v.k);
        return *this;
    }

    NodeRef& operator<< (Key<fmt::const_base64_wrapper> w)
    {
        set_key_serialized(w.wrapper);
        return *this;
    }

    NodeRef& operator<< (fmt::const_base64_wrapper w)
    {
        set_val_serialized(w);
        return *this;
    }

    /** @} */

private:

    void _apply_seed()
    {
        _C4RID();
        if(m_seed.str) // we have a seed key: use it to create the new child
        {
            m_id = m_tree->append_child(m_id);
            m_tree->_set_key(m_id, m_seed);
            m_seed.str = nullptr;
            m_seed.len = (size_t)NONE;
        }
        else if(m_seed.len != (size_t)NONE) // we have a seed index: create a child at that position
        {
            _RYML_CB_ASSERT(m_tree->m_callbacks, (size_t)m_tree->num_children(m_id) == m_seed.len);
            m_id = m_tree->append_child(m_id);
            m_seed.str = nullptr;
            m_seed.len = (size_t)NONE;
        }
        else
        {
            _RYML_CB_ASSERT(m_tree->m_callbacks, readable());
        }
    }

    void _apply(csubstr v)
    {
        m_tree->_set_val(m_id, v);
    }

    void _apply(NodeScalar const& v)
    {
        m_tree->_set_val(m_id, v);
    }

    void _apply(NodeInit const& i)
    {
        m_tree->_set(m_id, i);
    }

public:

    /** @name modification of hierarchy */
    /** @{ */

    NodeRef insert_child(NodeRef after)
    {
        _C4RR();
        _RYML_CB_ASSERT(m_tree->m_callbacks, after.m_tree == m_tree);
        NodeRef r(m_tree, m_tree->insert_child(m_id, after.m_id));
        return r;
    }

    NodeRef insert_child(NodeInit const& i, NodeRef after)
    {
        _C4RR();
        _RYML_CB_ASSERT(m_tree->m_callbacks, after.m_tree == m_tree);
        NodeRef r(m_tree, m_tree->insert_child(m_id, after.m_id));
        r._apply(i);
        return r;
    }

    NodeRef prepend_child()
    {
        _C4RR();
        NodeRef r(m_tree, m_tree->insert_child(m_id, NONE));
        return r;
    }

    NodeRef prepend_child(NodeInit const& i)
    {
        _C4RR();
        NodeRef r(m_tree, m_tree->insert_child(m_id, NONE));
        r._apply(i);
        return r;
    }

    NodeRef append_child()
    {
        _C4RR();
        NodeRef r(m_tree, m_tree->append_child(m_id));
        return r;
    }

    NodeRef append_child(NodeInit const& i)
    {
        _C4RR();
        NodeRef r(m_tree, m_tree->append_child(m_id));
        r._apply(i);
        return r;
    }

    NodeRef insert_sibling(ConstNodeRef const& after)
    {
        _C4RR();
        _RYML_CB_ASSERT(m_tree->m_callbacks, after.m_tree == m_tree);
        NodeRef r(m_tree, m_tree->insert_sibling(m_id, after.m_id));
        return r;
    }

    NodeRef insert_sibling(NodeInit const& i, ConstNodeRef const& after)
    {
        _C4RR();
        _RYML_CB_ASSERT(m_tree->m_callbacks, after.m_tree == m_tree);
        NodeRef r(m_tree, m_tree->insert_sibling(m_id, after.m_id));
        r._apply(i);
        return r;
    }

    NodeRef prepend_sibling()
    {
        _C4RR();
        NodeRef r(m_tree, m_tree->prepend_sibling(m_id));
        return r;
    }

    NodeRef prepend_sibling(NodeInit const& i)
    {
        _C4RR();
        NodeRef r(m_tree, m_tree->prepend_sibling(m_id));
        r._apply(i);
        return r;
    }

    NodeRef append_sibling()
    {
        _C4RR();
        NodeRef r(m_tree, m_tree->append_sibling(m_id));
        return r;
    }

    NodeRef append_sibling(NodeInit const& i)
    {
        _C4RR();
        NodeRef r(m_tree, m_tree->append_sibling(m_id));
        r._apply(i);
        return r;
    }

public:

    void remove_child(NodeRef & child)
    {
        _C4RR();
        _RYML_CB_ASSERT(m_tree->m_callbacks, has_child(child));
        _RYML_CB_ASSERT(m_tree->m_callbacks, child.parent().id() == id());
        m_tree->remove(child.id());
        child.clear();
    }

    //! remove the nth child of this node
    void remove_child(id_type pos)
    {
        _C4RR();
        _RYML_CB_ASSERT(m_tree->m_callbacks, pos >= 0 && pos < num_children());
        id_type child = m_tree->child(m_id, pos);
        _RYML_CB_ASSERT(m_tree->m_callbacks, child != NONE);
        m_tree->remove(child);
    }

    //! remove a child by name
    void remove_child(csubstr key)
    {
        _C4RR();
        id_type child = m_tree->find_child(m_id, key);
        _RYML_CB_ASSERT(m_tree->m_callbacks, child != NONE);
        m_tree->remove(child);
    }

public:

    /** change the node's position within its parent, placing it after
     * @p after. To move to the first position in the parent, simply
     * pass an empty or default-constructed reference like this:
     * `n.move({})`. */
    void move(ConstNodeRef const& after)
    {
        _C4RR();
        m_tree->move(m_id, after.m_id);
    }

    /** move the node to a different @p parent (which may belong to a
     * different tree), placing it after @p after. When the
     * destination parent is in a new tree, then this node's tree
     * pointer is reset to the tree of the parent node. */
    void move(NodeRef const& parent, ConstNodeRef const& after)
    {
        _C4RR();
        if(parent.m_tree == m_tree)
        {
            m_tree->move(m_id, parent.m_id, after.m_id);
        }
        else
        {
            parent.m_tree->move(m_tree, m_id, parent.m_id, after.m_id);
            m_tree = parent.m_tree;
        }
    }

    /** duplicate the current node somewhere within its parent, and
     * place it after the node @p after. To place into the first
     * position of the parent, simply pass an empty or
     * default-constructed reference like this: `n.move({})`. */
    NodeRef duplicate(ConstNodeRef const& after) const
    {
        _C4RR();
        _RYML_CB_ASSERT(m_tree->m_callbacks, m_tree == after.m_tree || after.m_id == NONE);
        id_type dup = m_tree->duplicate(m_id, m_tree->parent(m_id), after.m_id);
        NodeRef r(m_tree, dup);
        return r;
    }

    /** duplicate the current node somewhere into a different @p parent
     * (possibly from a different tree), and place it after the node
     * @p after. To place into the first position of the parent,
     * simply pass an empty or default-constructed reference like
     * this: `n.move({})`. */
    NodeRef duplicate(NodeRef const& parent, ConstNodeRef const& after) const
    {
        _C4RR();
        _RYML_CB_ASSERT(m_tree->m_callbacks, parent.m_tree == after.m_tree || after.m_id == NONE);
        if(parent.m_tree == m_tree)
        {
            id_type dup = m_tree->duplicate(m_id, parent.m_id, after.m_id);
            NodeRef r(m_tree, dup);
            return r;
        }
        else
        {
            id_type dup = parent.m_tree->duplicate(m_tree, m_id, parent.m_id, after.m_id);
            NodeRef r(parent.m_tree, dup);
            return r;
        }
    }

    void duplicate_children(NodeRef const& parent, ConstNodeRef const& after) const
    {
        _C4RR();
        _RYML_CB_ASSERT(m_tree->m_callbacks, parent.m_tree == after.m_tree);
        if(parent.m_tree == m_tree)
        {
            m_tree->duplicate_children(m_id, parent.m_id, after.m_id);
        }
        else
        {
            parent.m_tree->duplicate_children(m_tree, m_id, parent.m_id, after.m_id);
        }
    }

    /** @} */

#undef _C4RR
#undef _C4RID
};

// NOLINTEND(cppcoreguidelines-c-copy-assignment-signature,misc-unconventional-assign-operator)


//-----------------------------------------------------------------------------

inline ConstNodeRef::ConstNodeRef(NodeRef const& that) noexcept
    : m_tree(that.m_tree)
    , m_id(!that.is_seed() ? that.id() : (id_type)NONE)
{
}

inline ConstNodeRef::ConstNodeRef(NodeRef && that) noexcept // NOLINT
    : m_tree(that.m_tree)
    , m_id(!that.is_seed() ? that.id() : (id_type)NONE)
{
}


inline ConstNodeRef& ConstNodeRef::operator= (NodeRef const& that) noexcept
{
    m_tree = (that.m_tree);
    m_id = (!that.is_seed() ? that.id() : (id_type)NONE);
    return *this;
}

inline ConstNodeRef& ConstNodeRef::operator= (NodeRef && that) noexcept // NOLINT
{
    m_tree = (that.m_tree);
    m_id = (!that.is_seed() ? that.id() : (id_type)NONE);
    return *this;
}


//-----------------------------------------------------------------------------

/** @addtogroup doc_serialization_helpers
 *
 * @{
 */

template<class T>
C4_ALWAYS_INLINE void write(NodeRef *n, T const& v)
{
    n->set_val_serialized(v);
}

template<class T>
C4_ALWAYS_INLINE bool read(ConstNodeRef const& C4_RESTRICT n, T *v)
{
    return read(n.m_tree, n.m_id, v);
}

template<class T>
C4_ALWAYS_INLINE bool read(NodeRef const& C4_RESTRICT n, T *v)
{
    return read(n.tree(), n.id(), v);
}

template<class T>
C4_ALWAYS_INLINE bool readkey(ConstNodeRef const& C4_RESTRICT n, T *v)
{
    return readkey(n.m_tree, n.m_id, v);
}

template<class T>
C4_ALWAYS_INLINE bool readkey(NodeRef const& C4_RESTRICT n, T *v)
{
    return readkey(n.tree(), n.id(), v);
}

/** @} */

/** @} */


} // namespace yml
} // namespace c4



#ifdef __clang__
#   pragma clang diagnostic pop
#elif defined(__GNUC__)
#   pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#   pragma warning(pop)
#endif

#endif /* _C4_YML_NODE_HPP_ */
