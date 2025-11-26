#ifndef _C4_YML_EVENT_HANDLER_STACK_HPP_
#define _C4_YML_EVENT_HANDLER_STACK_HPP_

#ifndef _C4_YML_DETAIL_STACK_HPP_
#include "c4/yml/detail/stack.hpp"
#endif

#ifndef _C4_YML_NODE_TYPE_HPP_
#include "c4/yml/node_type.hpp"
#endif

#ifndef _C4_YML_DETAIL_DBGPRINT_HPP_
#include "c4/yml/detail/dbgprint.hpp"
#endif

#ifndef _C4_YML_PARSER_STATE_HPP_
#include "c4/yml/parser_state.hpp"
#endif

#ifdef RYML_DBG
#ifndef _C4_YML_DETAIL_PRINT_HPP_
#include "c4/yml/detail/print.hpp"
#endif
#endif

// NOLINTBEGIN(hicpp-signed-bitwise)

namespace c4 {
namespace yml {

/** @addtogroup doc_event_handlers
 * @{ */

namespace detail {
using pfn_relocate_arena = void (*)(void*, csubstr prev_arena, substr next_arena);
} // detail

/** Use this class a base of implementations of event handler to
 * simplify the stack logic. */
template<class HandlerImpl, class HandlerState>
struct EventHandlerStack
{
    static_assert(std::is_base_of<ParserState, HandlerState>::value,
                  "ParserState must be a base of HandlerState");

    using state = HandlerState;
    using pfn_relocate_arena = detail::pfn_relocate_arena;

public:

    detail::stack<state> m_stack;
    state *C4_RESTRICT   m_curr;    ///< current stack level: top of the stack. cached here for easier access.
    state *C4_RESTRICT   m_parent;  ///< parent of the current stack level.
    pfn_relocate_arena   m_relocate_arena; ///< callback when the arena gets relocated
    void *               m_relocate_arena_data;

protected:

    EventHandlerStack() : m_stack(), m_curr(), m_parent(), m_relocate_arena(), m_relocate_arena_data() {}
    EventHandlerStack(Callbacks const& cb) : m_stack(cb), m_curr(), m_parent(), m_relocate_arena(), m_relocate_arena_data() {}

protected:

    void _stack_start_parse(const char *filename, pfn_relocate_arena relocate_arena, void *relocate_arena_data)
    {
        _RYML_CB_ASSERT(m_stack.m_callbacks, m_curr != nullptr);
        _RYML_CB_ASSERT(m_stack.m_callbacks, relocate_arena != nullptr);
        _RYML_CB_ASSERT(m_stack.m_callbacks, relocate_arena_data != nullptr);
        m_curr->start_parse(filename, m_curr->node_id);
        m_relocate_arena = relocate_arena;
        m_relocate_arena_data = relocate_arena_data;
    }

    void _stack_finish_parse()
    {
    }

protected:

    void _stack_reset_root()
    {
        m_stack.clear();
        m_stack.push({});
        m_parent = nullptr;
        m_curr = &m_stack.top();
    }

    void _stack_reset_non_root()
    {
        m_stack.clear();
        m_stack.push({}); // parent
        m_stack.push({}); // node
        m_parent = &m_stack.top(1);
        m_curr = &m_stack.top();
    }

    void _stack_push()
    {
        m_stack.push_top();
        m_parent = &m_stack.top(1); // don't use m_curr. watch out for relocations inside the prev push
        m_curr = &m_stack.top();
        m_curr->reset_after_push();
    }

    void _stack_pop()
    {
        _RYML_CB_ASSERT(m_stack.m_callbacks, m_parent);
        _RYML_CB_ASSERT(m_stack.m_callbacks, m_stack.size() > 1);
        m_parent->reset_before_pop(*m_curr);
        m_stack.pop();
        m_parent = m_stack.size() > 1 ? &m_stack.top(1) : nullptr;
        m_curr = &m_stack.top();
        #ifdef RYML_DBG
        if(m_parent)
            _c4dbgpf("popped! top is now node={} (parent={})", m_curr->node_id, m_parent->node_id);
        else
            _c4dbgpf("popped! top is now node={} @ ROOT", m_curr->node_id);
        #endif
    }

protected:

    // undefined at the end
    #define _has_any_(bits) (static_cast<HandlerImpl const* C4_RESTRICT>(this)->template _has_any__<bits>())

    bool _stack_should_push_on_begin_doc() const
    {
        const bool is_root = (m_stack.size() == 1u);
        return is_root && (_has_any_(DOC|VAL|MAP|SEQ) || m_curr->has_children);
    }

    bool _stack_should_pop_on_end_doc() const
    {
        const bool is_root = (m_stack.size() == 1u);
        return !is_root && _has_any_(DOC);
    }

protected:

    void _stack_relocate_to_new_arena(csubstr prev, substr curr)
    {
        for(state &st : m_stack)
        {
            if(st.line_contents.rem.is_sub(prev))
                st.line_contents.rem = _stack_relocate_to_new_arena(st.line_contents.rem, prev, curr);
            if(st.line_contents.full.is_sub(prev))
                st.line_contents.full = _stack_relocate_to_new_arena(st.line_contents.full, prev, curr);
            if(st.line_contents.stripped.is_sub(prev))
                st.line_contents.stripped = _stack_relocate_to_new_arena(st.line_contents.stripped, prev, curr);
        }
        _RYML_CB_ASSERT(m_stack.m_callbacks, m_relocate_arena != nullptr);
        _RYML_CB_ASSERT(m_stack.m_callbacks, m_relocate_arena_data != nullptr);
        m_relocate_arena(m_relocate_arena_data, prev, curr);
    }

    substr _stack_relocate_to_new_arena(csubstr s, csubstr prev, substr curr)
    {
        _RYML_CB_ASSERT(m_stack.m_callbacks, prev.is_super(s));
        auto pos = s.str - prev.str;
        substr out = {curr.str + pos, s.len};
        _RYML_CB_ASSERT(m_stack.m_callbacks, curr.is_super(out));
        return out;
    }

public:

    /** Check whether the current parse tokens are trailing on the
     * previous doc, and raise an error if they are. This function is
     * called by the parse engine (not the event handler) before a doc
     * is started. */
    void check_trailing_doc_token() const
    {
        const bool is_root = (m_stack.size() == 1u);
        const bool isndoc = (m_curr->flags & NDOC) != 0;
        const bool suspicious = _has_any_(MAP|SEQ|VAL);
        _c4dbgpf("target={} isroot={} suspicious={} ndoc={}", m_curr->node_id, is_root, suspicious, isndoc);
        if((is_root || _has_any_(DOC)) && suspicious && !isndoc)
            _RYML_CB_ERR_(m_stack.m_callbacks, "parse error", m_curr->pos);
    }

protected:

    #undef _has_any_

};

/** @} */

} // namespace yml
} // namespace c4

// NOLINTEND(hicpp-signed-bitwise)

#endif /* _C4_YML_EVENT_HANDLER_STACK_HPP_ */
