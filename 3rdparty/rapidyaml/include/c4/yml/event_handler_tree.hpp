#ifndef _C4_YML_EVENT_HANDLER_TREE_HPP_
#define _C4_YML_EVENT_HANDLER_TREE_HPP_

#ifndef _C4_YML_TREE_HPP_
#include "c4/yml/tree.hpp"
#endif

#ifndef _C4_YML_EVENT_HANDLER_STACK_HPP_
#include "c4/yml/event_handler_stack.hpp"
#endif

C4_SUPPRESS_WARNING_MSVC_WITH_PUSH(4702) // unreachable code
// NOLINTBEGIN(hicpp-signed-bitwise)

namespace c4 {
namespace yml {

/** @addtogroup doc_event_handlers
 * @{ */


/** @cond dev */
struct EventHandlerTreeState : public ParserState
{
    NodeData *tr_data;
};
/** @endcond */


/** The event handler to create a ryml @ref Tree. See the
 * documentation for @ref doc_event_handlers, which has important
 * notes about the event model used by rapidyaml. */
struct EventHandlerTree : public EventHandlerStack<EventHandlerTree, EventHandlerTreeState>
{

    /** @name types
     * @{ */

    using state = EventHandlerTreeState;

    /** @} */

public:

    /** @cond dev */
    Tree *C4_RESTRICT m_tree;
    id_type m_id;
    size_t m_num_directives;
    bool m_yaml_directive;

    #ifdef RYML_DBG
    #define _enable_(bits) _enable__<bits>(); _c4dbgpf("node[{}]: enable {}", m_curr->node_id, #bits)
    #define _disable_(bits) _disable__<bits>(); _c4dbgpf("node[{}]: disable {}", m_curr->node_id, #bits)
    #else
    #define _enable_(bits) _enable__<bits>()
    #define _disable_(bits) _disable__<bits>()
    #endif
    #define _has_any_(bits) _has_any__<bits>()
    /** @endcond */

public:

    /** @name construction and resetting
     * @{ */

    EventHandlerTree() : EventHandlerStack(), m_tree(), m_id(NONE), m_num_directives(), m_yaml_directive() {}
    EventHandlerTree(Callbacks const& cb) : EventHandlerStack(cb), m_tree(), m_id(NONE), m_num_directives(), m_yaml_directive() {}
    EventHandlerTree(Tree *tree, id_type id) : EventHandlerStack(tree->callbacks()), m_tree(tree), m_id(id), m_num_directives(), m_yaml_directive()
    {
        reset(tree, id);
    }

    void reset(Tree *tree, id_type id)
    {
        if(C4_UNLIKELY(!tree))
            _RYML_CB_ERR(m_stack.m_callbacks, "null tree");
        if(C4_UNLIKELY(id >= tree->capacity()))
            _RYML_CB_ERR(tree->callbacks(), "invalid node");
        if(C4_UNLIKELY(!tree->is_root(id)))
            if(C4_UNLIKELY(tree->is_map(tree->parent(id))))
                if(C4_UNLIKELY(!tree->has_key(id)))
                    _RYML_CB_ERR(tree->callbacks(), "destination node belongs to a map and has no key");
        m_tree = tree;
        m_id = id;
        if(m_tree->is_root(id))
        {
            _stack_reset_root();
            _reset_parser_state(m_curr, id, m_tree->root_id());
        }
        else
        {
            _stack_reset_non_root();
            _reset_parser_state(m_parent, id, m_tree->parent(id));
            _reset_parser_state(m_curr, id, id);
        }
        m_num_directives = 0;
        m_yaml_directive = false;
    }

    /** @} */

public:

    /** @name parse events
     * @{ */

    void start_parse(const char* filename, detail::pfn_relocate_arena relocate_arena, void *relocate_arena_data)
    {
        _RYML_CB_ASSERT(m_stack.m_callbacks, m_tree != nullptr);
        this->_stack_start_parse(filename, relocate_arena, relocate_arena_data);
    }

    void finish_parse()
    {
        _RYML_CB_ASSERT(m_stack.m_callbacks, m_tree != nullptr);
        if(m_num_directives && !m_tree->is_stream(m_tree->root_id()))
            _RYML_CB_ERR_(m_stack.m_callbacks, "directives cannot be used without a document", {});
        this->_stack_finish_parse();
        /* This pointer is temporary. Remember that:
         *
         * - this handler object may be held by the user
         * - it may be used with a temporary tree inside the parse function
         * - when the parse function returns the temporary tree, its address
         *   will change
         *
         * As a result, the user could try to read the tree from m_tree, and
         * end up reading the stale temporary object.
         *
         * So it is better to clear it here; then the user will get an obvious
         * segfault if reading from m_tree. */
        m_tree = nullptr;
    }

    void cancel_parse()
    {
        m_tree = nullptr;
    }

    /** @} */

public:

    /** @name YAML stream events */
    /** @{ */

    C4_ALWAYS_INLINE void begin_stream() const noexcept { /*nothing to do*/ }

    C4_ALWAYS_INLINE void end_stream() const noexcept { /*nothing to do*/ }

    /** @} */

public:

    /** @name YAML document events */
    /** @{ */

    /** implicit doc start (without ---) */
    void begin_doc()
    {
        _c4dbgp("begin_doc");
        if(_stack_should_push_on_begin_doc())
        {
            _c4dbgp("push!");
            _set_root_as_stream();
            _push();
            _enable_(DOC);
        }
    }
    /** implicit doc end (without ...) */
    void end_doc()
    {
        _c4dbgp("end_doc");
        if(_stack_should_pop_on_end_doc())
        {
            _remove_speculative();
            _c4dbgp("pop!");
            _pop();
        }
    }

    /** explicit doc start, with --- */
    void begin_doc_expl()
    {
        _c4dbgp("begin_doc_expl");
        _RYML_CB_ASSERT(m_stack.m_callbacks, m_tree->root_id() == m_curr->node_id);
        if(!m_tree->is_stream(m_tree->root_id())) //if(_should_push_on_begin_doc())
        {
            _c4dbgp("ensure stream");
            _set_root_as_stream();
            id_type first = m_tree->first_child(m_tree->root_id());
            _RYML_CB_ASSERT(m_stack.m_callbacks, m_tree->is_stream(m_tree->root_id()));
            _RYML_CB_ASSERT(m_stack.m_callbacks, m_tree->num_children(m_tree->root_id()) == 1u);
            if(m_tree->has_children(first) || m_tree->is_val(first))
            {
                _c4dbgp("push!");
                _push();
            }
            else
            {
                _c4dbgp("tweak");
                _push();
                _remove_speculative();
                m_curr->node_id = m_tree->last_child(m_tree->root_id());
                m_curr->tr_data = m_tree->_p(m_curr->node_id);
            }
        }
        else
        {
            _c4dbgp("push!");
            _push();
        }
        _enable_(DOC);
    }
    /** explicit doc end, with ... */
    void end_doc_expl()
    {
        _c4dbgp("end_doc_expl");
        _remove_speculative();
        if(_stack_should_pop_on_end_doc())
        {
            _c4dbgp("pop!");
            _pop();
        }
        m_yaml_directive = false;
    }

    /** @} */

public:

    /** @name YAML map events */
    /** @{ */

    void begin_map_key_flow()
    {
        _RYML_CB_ERR_(m_stack.m_callbacks, "ryml trees cannot handle containers as keys", m_curr->pos);
    }
    void begin_map_key_block()
    {
        _RYML_CB_ERR_(m_stack.m_callbacks, "ryml trees cannot handle containers as keys", m_curr->pos);
    }

    void begin_map_val_flow()
    {
        _c4dbgpf("node[{}]: begin_map_val_flow", m_curr->node_id);
        _RYML_CB_CHECK(m_stack.m_callbacks, !_has_any_(VAL));
        _enable_(MAP|FLOW_SL);
        _save_loc();
        _push();
    }
    void begin_map_val_block()
    {
        _c4dbgpf("node[{}]: begin_map_val_block", m_curr->node_id);
        _RYML_CB_CHECK(m_stack.m_callbacks, !_has_any_(VAL));
        _enable_(MAP|BLOCK);
        _save_loc();
        _push();
    }

    void end_map()
    {
        _pop();
        _c4dbgpf("node[{}]: end_map_val", m_curr->node_id);
    }

    /** @} */

public:

    /** @name YAML seq events */
    /** @{ */

    void begin_seq_key_flow()
    {
        _RYML_CB_ERR_(m_stack.m_callbacks, "ryml trees cannot handle containers as keys", m_curr->pos);
    }
    void begin_seq_key_block()
    {
        _RYML_CB_ERR_(m_stack.m_callbacks, "ryml trees cannot handle containers as keys", m_curr->pos);
    }

    void begin_seq_val_flow()
    {
        _c4dbgpf("node[{}]: begin_seq_val_flow", m_curr->node_id);
        _RYML_CB_CHECK(m_stack.m_callbacks, !_has_any_(VAL));
        _enable_(SEQ|FLOW_SL);
        _save_loc();
        _push();
    }
    void begin_seq_val_block()
    {
        _c4dbgpf("node[{}]: begin_seq_val_block", m_curr->node_id);
        _RYML_CB_CHECK(m_stack.m_callbacks, !_has_any_(VAL));
        _enable_(SEQ|BLOCK);
        _save_loc();
        _push();
    }

    void end_seq()
    {
        _pop();
        _c4dbgpf("node[{}]: end_seq_val", m_curr->node_id);
    }

    /** @} */

public:

    /** @name YAML structure events */
    /** @{ */

    void add_sibling()
    {
        _RYML_CB_ASSERT(m_stack.m_callbacks, m_tree);
        _RYML_CB_ASSERT(m_stack.m_callbacks, m_parent);
        _RYML_CB_ASSERT(m_stack.m_callbacks, m_tree->has_children(m_parent->node_id));
        NodeData const* prev = m_tree->m_buf; // watchout against relocation of the tree nodes
        _set_state_(m_curr, m_tree->_append_child__unprotected(m_parent->node_id));
        if(prev != m_tree->m_buf)
            _refresh_after_relocation();
        _c4dbgpf("node[{}]: added sibling={} prev={}", m_parent->node_id, m_curr->node_id, m_tree->prev_sibling(m_curr->node_id));
    }

    /** set the previous val as the first key of a new map, with flow style.
     *
     * See the documentation for @ref doc_event_handlers, which has
     * important notes about this event.
     */
    void actually_val_is_first_key_of_new_map_flow()
    {
        if(C4_UNLIKELY(m_tree->is_container(m_curr->node_id)))
            _RYML_CB_ERR_(m_stack.m_callbacks, "ryml trees cannot handle containers as keys", m_curr->pos);
        _RYML_CB_ASSERT(m_stack.m_callbacks, m_parent);
        _RYML_CB_ASSERT(m_stack.m_callbacks, m_tree->is_seq(m_parent->node_id));
        _RYML_CB_ASSERT(m_stack.m_callbacks, !m_tree->is_container(m_curr->node_id));
        _RYML_CB_ASSERT(m_stack.m_callbacks, !m_tree->has_key(m_curr->node_id));
        const NodeData tmp = _val2key_(*m_curr->tr_data);
        _disable_(_VALMASK|VAL_STYLE);
        m_curr->tr_data->m_val = {};
        begin_map_val_flow();
        m_curr->tr_data->m_type = tmp.m_type;
        m_curr->tr_data->m_key = tmp.m_key;
    }

    /** like its flow counterpart, but this function can only be
     * called after the end of a flow-val at root or doc level.
     *
     * See the documentation for @ref doc_event_handlers, which has
     * important notes about this event.
     */
    void actually_val_is_first_key_of_new_map_block()
    {
        _RYML_CB_ERR_(m_stack.m_callbacks, "ryml trees cannot handle containers as keys", m_curr->pos);
    }

    /** @} */

public:

    /** @name YAML scalar events */
    /** @{ */


    C4_ALWAYS_INLINE void set_key_scalar_plain_empty() noexcept
    {
        _c4dbgpf("node[{}]: set key scalar plain as empty", m_curr->node_id);
        m_curr->tr_data->m_key.scalar = {};
        _enable_(KEY|KEY_PLAIN|KEYNIL);
    }
    C4_ALWAYS_INLINE void set_val_scalar_plain_empty() noexcept
    {
        _c4dbgpf("node[{}]: set val scalar plain as empty", m_curr->node_id);
        m_curr->tr_data->m_val.scalar = {};
        _enable_(VAL|VAL_PLAIN|VALNIL);
    }

    C4_ALWAYS_INLINE void set_key_scalar_plain(csubstr scalar) noexcept
    {
        _c4dbgpf("node[{}]: set key scalar plain: [{}]~~~{}~~~", m_curr->node_id, scalar.len, scalar);
        m_curr->tr_data->m_key.scalar = scalar;
        _enable_(KEY|KEY_PLAIN);
    }
    C4_ALWAYS_INLINE void set_val_scalar_plain(csubstr scalar) noexcept
    {
        _c4dbgpf("node[{}]: set val scalar plain: [{}]~~~{}~~~", m_curr->node_id, scalar.len, scalar);
        m_curr->tr_data->m_val.scalar = scalar;
        _enable_(VAL|VAL_PLAIN);
    }


    C4_ALWAYS_INLINE void set_key_scalar_dquoted(csubstr scalar) noexcept
    {
        _c4dbgpf("node[{}]: set key scalar dquot: [{}]~~~{}~~~", m_curr->node_id, scalar.len, scalar);
        m_curr->tr_data->m_key.scalar = scalar;
        _enable_(KEY|KEY_DQUO);
    }
    C4_ALWAYS_INLINE void set_val_scalar_dquoted(csubstr scalar) noexcept
    {
        _c4dbgpf("node[{}]: set val scalar dquot: [{}]~~~{}~~~", m_curr->node_id, scalar.len, scalar);
        m_curr->tr_data->m_val.scalar = scalar;
        _enable_(VAL|VAL_DQUO);
    }


    C4_ALWAYS_INLINE void set_key_scalar_squoted(csubstr scalar) noexcept
    {
        _c4dbgpf("node[{}]: set key scalar squot: [{}]~~~{}~~~", m_curr->node_id, scalar.len, scalar);
        m_curr->tr_data->m_key.scalar = scalar;
        _enable_(KEY|KEY_SQUO);
    }
    C4_ALWAYS_INLINE void set_val_scalar_squoted(csubstr scalar) noexcept
    {
        _c4dbgpf("node[{}]: set val scalar squot: [{}]~~~{}~~~", m_curr->node_id, scalar.len, scalar);
        m_curr->tr_data->m_val.scalar = scalar;
        _enable_(VAL|VAL_SQUO);
    }


    C4_ALWAYS_INLINE void set_key_scalar_literal(csubstr scalar) noexcept
    {
        _c4dbgpf("node[{}]: set key scalar literal: [{}]~~~{}~~~", m_curr->node_id, scalar.len, scalar);
        m_curr->tr_data->m_key.scalar = scalar;
        _enable_(KEY|KEY_LITERAL);
    }
    C4_ALWAYS_INLINE void set_val_scalar_literal(csubstr scalar) noexcept
    {
        _c4dbgpf("node[{}]: set val scalar literal: [{}]~~~{}~~~", m_curr->node_id, scalar.len, scalar);
        m_curr->tr_data->m_val.scalar = scalar;
        _enable_(VAL|VAL_LITERAL);
    }


    C4_ALWAYS_INLINE void set_key_scalar_folded(csubstr scalar) noexcept
    {
        _c4dbgpf("node[{}]: set key scalar folded: [{}]~~~{}~~~", m_curr->node_id, scalar.len, scalar);
        m_curr->tr_data->m_key.scalar = scalar;
        _enable_(KEY|KEY_FOLDED);
    }
    C4_ALWAYS_INLINE void set_val_scalar_folded(csubstr scalar) noexcept
    {
        _c4dbgpf("node[{}]: set val scalar folded: [{}]~~~{}~~~", m_curr->node_id, scalar.len, scalar);
        m_curr->tr_data->m_val.scalar = scalar;
        _enable_(VAL|VAL_FOLDED);
    }


    C4_ALWAYS_INLINE void mark_key_scalar_unfiltered() noexcept
    {
        _enable_(KEY_UNFILT);
    }
    C4_ALWAYS_INLINE void mark_val_scalar_unfiltered() noexcept
    {
        _enable_(VAL_UNFILT);
    }

    /** @} */

public:

    /** @name YAML anchor/reference events */
    /** @{ */

    void set_key_anchor(csubstr anchor)
    {
        _c4dbgpf("node[{}]: set key anchor: [{}]~~~{}~~~", m_curr->node_id, anchor.len, anchor);
        _RYML_CB_ASSERT(m_stack.m_callbacks, m_tree);
        _RYML_CB_ASSERT(m_stack.m_callbacks, !_has_any_(KEYREF));
        _RYML_CB_ASSERT(m_stack.m_callbacks, !anchor.begins_with('&'));
        _enable_(KEYANCH);
        m_curr->tr_data->m_key.anchor = anchor;
    }
    void set_val_anchor(csubstr anchor)
    {
        _c4dbgpf("node[{}]: set val anchor: [{}]~~~{}~~~", m_curr->node_id, anchor.len, anchor);
        _RYML_CB_ASSERT(m_stack.m_callbacks, m_tree);
        _RYML_CB_ASSERT(m_stack.m_callbacks, !_has_any_(VALREF));
        _RYML_CB_ASSERT(m_stack.m_callbacks, !anchor.begins_with('&'));
        _enable_(VALANCH);
        m_curr->tr_data->m_val.anchor = anchor;
    }

    void set_key_ref(csubstr ref)
    {
        _c4dbgpf("node[{}]: set key ref: [{}]~~~{}~~~", m_curr->node_id, ref.len, ref);
        _RYML_CB_ASSERT(m_stack.m_callbacks, m_tree);
        if(C4_UNLIKELY(_has_any_(KEYANCH)))
            _RYML_CB_ERR_(m_tree->callbacks(), "key cannot have both anchor and ref", m_curr->pos);
        _RYML_CB_ASSERT(m_tree->callbacks(), ref.begins_with('*'));
        _enable_(KEY|KEYREF);
        m_curr->tr_data->m_key.anchor = ref.sub(1);
        m_curr->tr_data->m_key.scalar = ref;
    }
    void set_val_ref(csubstr ref)
    {
        _c4dbgpf("node[{}]: set val ref: [{}]~~~{}~~~", m_curr->node_id, ref.len, ref);
        _RYML_CB_ASSERT(m_stack.m_callbacks, m_tree);
        if(C4_UNLIKELY(_has_any_(VALANCH)))
            _RYML_CB_ERR_(m_tree->callbacks(), "val cannot have both anchor and ref", m_curr->pos);
        _RYML_CB_ASSERT(m_tree->callbacks(), ref.begins_with('*'));
        _enable_(VAL|VALREF);
        m_curr->tr_data->m_val.anchor = ref.sub(1);
        m_curr->tr_data->m_val.scalar = ref;
    }

    /** @} */

public:

    /** @name YAML tag events */
    /** @{ */

    void set_key_tag(csubstr tag) noexcept
    {
        _c4dbgpf("node[{}]: set key tag: [{}]~~~{}~~~", m_curr->node_id, tag.len, tag);
        _enable_(KEYTAG);
        m_curr->tr_data->m_key.tag = tag;
    }
    void set_val_tag(csubstr tag) noexcept
    {
        _c4dbgpf("node[{}]: set val tag: [{}]~~~{}~~~", m_curr->node_id, tag.len, tag);
        _enable_(VALTAG);
        m_curr->tr_data->m_val.tag = tag;
    }

    /** @} */

public:

    /** @name YAML directive events */
    /** @{ */

    C4_NO_INLINE void add_directive(csubstr directive)
    {
        _c4dbgpf("% directive! {}", directive);
        _RYML_CB_ASSERT(m_tree->callbacks(), directive.begins_with('%'));
        if(directive.begins_with("%TAG"))
        {
            if(C4_UNLIKELY(!m_tree->add_tag_directive(directive)))
                _RYML_CB_ERR_(m_stack.m_callbacks, "failed to add directive", m_curr->pos);
        }
        else if(directive.begins_with("%YAML"))
        {
            _c4dbgpf("%YAML directive! ignoring...: {}", directive);
            if(C4_UNLIKELY(m_yaml_directive))
                _RYML_CB_ERR_(m_stack.m_callbacks, "multiple yaml directives", m_curr->pos);
            m_yaml_directive = true;
        }
        else
        {
            _c4dbgpf("unknown directive! ignoring... {}", directive);
        }
        ++m_num_directives;
    }

    /** @} */

public:

    /** @name arena functions */
    /** @{ */

    substr alloc_arena(size_t len)
    {
        _RYML_CB_ASSERT(m_stack.m_callbacks, m_tree);
        csubstr prev = m_tree->arena();
        substr out = m_tree->alloc_arena(len);
        substr curr = m_tree->arena();
        if(curr.str != prev.str)
            _stack_relocate_to_new_arena(prev, curr);
        return out;
    }

    substr alloc_arena(size_t len, substr *relocated)
    {
        _RYML_CB_ASSERT(m_stack.m_callbacks, m_tree);
        csubstr prev = m_tree->arena();
        if(!prev.is_super(*relocated))
            return alloc_arena(len);
        substr out = alloc_arena(len);
        substr curr = m_tree->arena();
        if(curr.str != prev.str)
            *relocated = _stack_relocate_to_new_arena(*relocated, prev, curr);
        return out;
    }

    /** @} */

public:

    /** @cond dev */
    void _reset_parser_state(state* st, id_type parse_root, id_type node)
    {
        _RYML_CB_ASSERT(m_stack.m_callbacks, m_tree);
        _set_state_(st, node);
        const NodeType type = m_tree->type(node);
        #ifdef RYML_DBG
        char flagbuf[80];
        _c4dbgpf("resetting state: initial flags={}", detail::_parser_flags_to_str(flagbuf, st->flags));
        #endif
        if(type == NOTYPE)
        {
            _c4dbgpf("node[{}] is notype", node);
            if(m_tree->is_root(parse_root))
            {
                _c4dbgpf("node[{}] is root", node);
                st->flags |= RUNK|RTOP;
            }
            else
            {
                _c4dbgpf("node[{}] is not root. setting USTY", node);
                st->flags |= USTY;
            }
        }
        else if(type.is_map())
        {
            _c4dbgpf("node[{}] is map", node);
            st->flags |= RMAP|USTY;
        }
        else if(type.is_seq())
        {
            _c4dbgpf("node[{}] is map", node);
            st->flags |= RSEQ|USTY;
        }
        else if(type.has_key())
        {
            _c4dbgpf("node[{}] has key. setting USTY", node);
            st->flags |= USTY;
        }
        else
        {
            _RYML_CB_ERR(m_tree->callbacks(), "cannot append to node");
        }
        if(type.is_doc())
        {
            _c4dbgpf("node[{}] is doc", node);
            st->flags |= RDOC;
        }
        #ifdef RYML_DBG
        _c4dbgpf("resetting state: final flags={}", detail::_parser_flags_to_str(flagbuf, st->flags));
        #endif
    }

    /** push a new parent, add a child to the new parent, and set the
     * child as the current node */
    void _push()
    {
        _stack_push();
        NodeData const* prev = m_tree->m_buf; // watch out against relocation of the tree nodes
        m_curr->node_id = m_tree->_append_child__unprotected(m_parent->node_id);
        m_curr->tr_data = m_tree->_p(m_curr->node_id);
        if(prev != m_tree->m_buf)
            _refresh_after_relocation();
        _c4dbgpf("pushed! level={}. top is now node={} (parent={})", m_curr->level, m_curr->node_id, m_parent ? m_parent->node_id : NONE);
    }
    /** end the current scope */
    void _pop()
    {
        _remove_speculative_with_parent();
        _stack_pop();
    }

public:

    template<type_bits bits> C4_HOT C4_ALWAYS_INLINE void _enable__() noexcept
    {
        m_curr->tr_data->m_type.type = static_cast<NodeType_e>(m_curr->tr_data->m_type.type | bits);
    }
    template<type_bits bits> C4_HOT C4_ALWAYS_INLINE void _disable__() noexcept
    {
        m_curr->tr_data->m_type.type = static_cast<NodeType_e>(m_curr->tr_data->m_type.type & (~bits));
    }
    template<type_bits bits> C4_HOT C4_ALWAYS_INLINE bool _has_any__() const noexcept
    {
        return (m_curr->tr_data->m_type.type & bits) != 0;
    }

public:

    C4_ALWAYS_INLINE void _set_state_(state *C4_RESTRICT s, id_type id) const noexcept
    {
        s->node_id = id;
        s->tr_data = m_tree->_p(id);
    }
    void _refresh_after_relocation()
    {
        _c4dbgp("tree: refreshing stack data after tree data relocation");
        for(auto &st : m_stack)
            st.tr_data = m_tree->_p(st.node_id);
    }

    void _set_root_as_stream()
    {
        _c4dbgp("set root as stream");
        _RYML_CB_ASSERT(m_tree->callbacks(), m_tree->root_id() == 0u);
        _RYML_CB_ASSERT(m_tree->callbacks(), m_curr->node_id == 0u);
        const bool hack = !m_tree->has_children(m_curr->node_id) && !m_tree->is_val(m_curr->node_id);
        if(hack)
            m_tree->_p(m_tree->root_id())->m_type.add(VAL);
        m_tree->set_root_as_stream();
        _RYML_CB_ASSERT(m_tree->callbacks(), m_tree->is_stream(m_tree->root_id()));
        _RYML_CB_ASSERT(m_tree->callbacks(), m_tree->has_children(m_tree->root_id()));
        _RYML_CB_ASSERT(m_tree->callbacks(), m_tree->is_doc(m_tree->first_child(m_tree->root_id())));
        if(hack)
            m_tree->_p(m_tree->first_child(m_tree->root_id()))->m_type.rem(VAL);
        _set_state_(m_curr, m_tree->root_id());
    }

    static NodeData _val2key_(NodeData const& C4_RESTRICT d) noexcept
    {
        NodeData r = d;
        r.m_key = d.m_val;
        r.m_val = {};
        r.m_type = d.m_type;
        static_assert((_VALMASK >> 1u) == _KEYMASK, "required for this function to work");
        static_assert((VAL_STYLE >> 1u) == KEY_STYLE, "required for this function to work");
        r.m_type.type = ((d.m_type.type & (_VALMASK|VAL_STYLE)) >> 1u);
        r.m_type.type = (r.m_type.type & ~(_VALMASK|VAL_STYLE));
        r.m_type.type = (r.m_type.type | KEY);
        return r;
    }

    void _remove_speculative()
    {
        _c4dbgp("remove speculative node");
        _RYML_CB_ASSERT(m_stack.m_callbacks, m_tree);
        _RYML_CB_ASSERT(m_tree->callbacks(), !m_tree->empty());
        const id_type last_added = m_tree->size() - 1;
        if(m_tree->has_parent(last_added))
            if(m_tree->_p(last_added)->m_type == NOTYPE)
                m_tree->remove(last_added);
    }

    void _remove_speculative_with_parent()
    {
        _RYML_CB_ASSERT(m_stack.m_callbacks, m_tree);
        _RYML_CB_ASSERT(m_tree->callbacks(), !m_tree->empty());
        const id_type last_added = m_tree->size() - 1;
        _RYML_CB_ASSERT(m_tree->callbacks(), m_tree->has_parent(last_added));
        if(m_tree->_p(last_added)->m_type == NOTYPE)
        {
            _c4dbgpf("remove speculative node with parent. parent={} node={} parent(node)={}", m_parent->node_id, last_added, m_tree->parent(last_added));
            m_tree->remove(last_added);
        }
    }

    C4_ALWAYS_INLINE void _save_loc()
    {
        _RYML_CB_ASSERT(m_stack.m_callbacks, m_tree);
        _RYML_CB_ASSERT(m_tree->callbacks(), m_tree->_p(m_curr->node_id)->m_val.scalar.len == 0);
        m_tree->_p(m_curr->node_id)->m_val.scalar.str = m_curr->line_contents.rem.str;
    }

#undef _enable_
#undef _disable_
#undef _has_any_

    /** @endcond */
};

/** @} */

} // namespace yml
} // namespace c4

// NOLINTEND(hicpp-signed-bitwise)
C4_SUPPRESS_WARNING_MSVC_POP

#endif /* _C4_YML_EVENT_HANDLER_TREE_HPP_ */
