#ifndef _C4_YML_EMIT_DEF_HPP_
#define _C4_YML_EMIT_DEF_HPP_

#ifndef _C4_YML_EMIT_HPP_
#include "c4/yml/emit.hpp"
#endif

/** @file emit.def.hpp Definitions for emit functions. */
#ifndef _C4_YML_DETAIL_DBGPRINT_HPP_
#include "c4/yml/detail/dbgprint.hpp"
#endif

namespace c4 {
namespace yml {

template<class Writer>
substr Emitter<Writer>::emit_as(EmitType_e type, Tree const& t, id_type id, bool error_on_excess)
{
    if(t.empty())
    {
        _RYML_CB_ASSERT(t.callbacks(), id == NONE);
        return {};
    }
    if(id == NONE)
        id = t.root_id();
    _RYML_CB_CHECK(t.callbacks(), id < t.capacity());
    m_tree = &t;
    m_flow = false;
    if(type == EMIT_YAML)
        _emit_yaml(id);
    else if(type == EMIT_JSON)
        _do_visit_json(id, 0);
    else
        _RYML_CB_ERR(m_tree->callbacks(), "unknown emit type");
    m_tree = nullptr;
    return this->Writer::_get(error_on_excess);
}


//-----------------------------------------------------------------------------

template<class Writer>
void Emitter<Writer>::_emit_yaml(id_type id)
{
    // save branches in the visitor by doing the initial stream/doc
    // logic here, sparing the need to check stream/val/keyval inside
    // the visitor functions
    auto dispatch = [this](id_type node){
        NodeType ty = m_tree->type(node);
        if(ty.is_flow_sl())
            _do_visit_flow_sl(node, 0);
        else if(ty.is_flow_ml())
            _do_visit_flow_ml(node, 0);
        else
        {
            _do_visit_block(node, 0);
        }
    };
    if(!m_tree->is_root(id))
    {
        if(m_tree->is_container(id) && !m_tree->type(id).is_flow())
        {
            id_type ilevel = 0;
            if(m_tree->has_key(id))
            {
                this->Writer::_do_write(m_tree->key(id));
                this->Writer::_do_write(":\n");
                ++ilevel;
            }
            _do_visit_block_container(id, 0, ilevel, ilevel);
            return;
        }
    }

    TagDirectiveRange tagds = m_tree->tag_directives();
    auto write_tag_directives = [&tagds, this](const id_type next_node){
        TagDirective const* C4_RESTRICT end = tagds.b;
        while(end < tagds.e)
        {
            if(end->next_node_id > next_node)
                break;
            ++end;
        }
        const id_type parent = m_tree->parent(next_node);
        for( ; tagds.b != end; ++tagds.b)
        {
            if(next_node != m_tree->first_child(parent))
                this->Writer::_do_write("...\n");
            this->Writer::_do_write("%TAG ");
            this->Writer::_do_write(tagds.b->handle);
            this->Writer::_do_write(' ');
            this->Writer::_do_write(tagds.b->prefix);
            this->Writer::_do_write('\n');
        }
    };
    if(m_tree->is_stream(id))
    {
        const id_type first_child = m_tree->first_child(id);
        if(first_child != NONE)
            write_tag_directives(first_child);
        for(id_type child = first_child; child != NONE; child = m_tree->next_sibling(child))
        {
            dispatch(child);
            if(m_tree->is_doc(child) && m_tree->type(child).is_flow_sl())
                this->Writer::_do_write('\n');
            if(m_tree->next_sibling(child) != NONE)
                write_tag_directives(m_tree->next_sibling(child));
        }
    }
    else if(m_tree->is_container(id))
    {
        dispatch(id);
    }
    else if(m_tree->is_doc(id))
    {
        _RYML_CB_ASSERT(m_tree->callbacks(), !m_tree->is_container(id)); // checked above
        _RYML_CB_ASSERT(m_tree->callbacks(), m_tree->is_val(id)); // so it must be a val
        _write_doc(id);
    }
    else if(m_tree->is_keyval(id))
    {
        _writek(id, 0);
        this->Writer::_do_write(": ");
        _writev(id, 0);
        if(!m_tree->type(id).is_flow())
            this->Writer::_do_write('\n');
    }
    else if(m_tree->is_val(id))
    {
        //this->Writer::_do_write("- ");
        _writev(id, 0);
        if(!m_tree->type(id).is_flow())
            this->Writer::_do_write('\n');
    }
    else if(m_tree->type(id) == NOTYPE)
    {
        ;
    }
    else
    {
        _RYML_CB_ERR(m_tree->callbacks(), "unknown type");
    }
}

#define _rymlindent_nextline() this->_indent(ilevel + 1);

template<class Writer>
void Emitter<Writer>::_write_doc(id_type id)
{
    const NodeType ty = m_tree->type(id);
    RYML_ASSERT(ty.is_doc());
    RYML_ASSERT(!ty.has_key());
    if(!m_tree->is_root(id))
    {
        RYML_ASSERT(m_tree->is_stream(m_tree->parent(id)));
        this->Writer::_do_write("---");
    }
    //
    if(!ty.has_val()) // this is more frequent
    {
        const bool tag = ty.has_val_tag();
        const bool anchor = ty.has_val_anchor();
        if(!tag && !anchor)
        {
            ;
        }
        else if(!tag && anchor)
        {
            if(!m_tree->is_root(id))
                this->Writer::_do_write(' ');
            this->Writer::_do_write('&');
            this->Writer::_do_write(m_tree->val_anchor(id));
            #ifdef RYML_NO_COVERAGE__TO_BE_DELETED
            if(m_tree->has_children(id) && m_tree->is_root(id))
                this->Writer::_do_write('\n');
            #endif
        }
        else if(tag && !anchor)
        {
            if(!m_tree->is_root(id))
                this->Writer::_do_write(' ');
            _write_tag(m_tree->val_tag(id));
            #ifdef RYML_NO_COVERAGE__TO_BE_DELETED
            if(m_tree->has_children(id) && m_tree->is_root(id))
                this->Writer::_do_write('\n');
            #endif
        }
        else // tag && anchor
        {
            if(!m_tree->is_root(id))
                this->Writer::_do_write(' ');
            _write_tag(m_tree->val_tag(id));
            this->Writer::_do_write(" &");
            this->Writer::_do_write(m_tree->val_anchor(id));
            #ifdef RYML_NO_COVERAGE__TO_BE_DELETED
            if(m_tree->has_children(id) && m_tree->is_root(id))
                this->Writer::_do_write('\n');
            #endif
        }
    }
    else // docval
    {
        _RYML_CB_ASSERT(m_tree->callbacks(), ty.has_val());
        // some plain scalars such as '...' and '---' must not
        // appear at 0-indentation
        const csubstr val = m_tree->val(id);
        const bool preceded_by_3_dashes = !m_tree->is_root(id);
        const type_bits style_marks = ty & VAL_STYLE;
        const bool is_plain = ty.is_val_plain();
        const bool is_ambiguous = (is_plain || !style_marks)
            && ((val.begins_with("...") || val.begins_with("---"))
                ||
                (val.find('\n') != npos));
        if(preceded_by_3_dashes)
        {
            if(is_plain && val.len == 0 && !ty.has_val_anchor() && !ty.has_val_tag())
            {
                this->Writer::_do_write('\n');
                return;
            }
            else if(val.len && is_ambiguous)
            {
                this->Writer::_do_write('\n');
            }
            else
            {
                this->Writer::_do_write(' ');
            }
        }
        id_type ilevel = 0u;
        if(is_ambiguous)
        {
            _rymlindent_nextline();
            ++ilevel;
        }
        _writev(id, ilevel);
        if(val.len && m_tree->is_root(id))
            this->Writer::_do_write('\n');
    }
    if(!m_tree->is_root(id))
        this->Writer::_do_write('\n');
}

template<class Writer>
void Emitter<Writer>::_do_visit_flow_sl(id_type node, id_type depth, id_type ilevel)
{
    const bool prev_flow = m_flow;
    m_flow = true;
    _RYML_CB_ASSERT(m_tree->callbacks(), !m_tree->is_stream(node));
    _RYML_CB_ASSERT(m_tree->callbacks(), m_tree->is_container(node) || m_tree->is_doc(node));
    _RYML_CB_ASSERT(m_tree->callbacks(), m_tree->is_root(node) || (m_tree->parent_is_map(node) || m_tree->parent_is_seq(node)));
    if(C4_UNLIKELY(depth > m_opts.max_depth()))
        _RYML_CB_ERR(m_tree->callbacks(), "max depth exceeded");

    if(m_tree->is_doc(node))
    {
        _write_doc(node);
        #ifdef RYML_NO_COVERAGE__TO_BE_DELETED
        if(!m_tree->has_children(node))
            return;
        else
        #endif
        {
            if(m_tree->is_map(node))
            {
                this->Writer::_do_write('{');
            }
            else
            {
                _RYML_CB_ASSERT(m_tree->callbacks(), m_tree->is_seq(node));
                this->Writer::_do_write('[');
            }
        }
    }
    else if(m_tree->is_container(node))
    {
        _RYML_CB_ASSERT(m_tree->callbacks(), m_tree->is_map(node) || m_tree->is_seq(node));

        bool spc = false; // write a space

        if(m_tree->has_key(node))
        {
            _writek(node, ilevel);
            this->Writer::_do_write(':');
            spc = true;
        }

        if(m_tree->has_val_tag(node))
        {
            if(spc)
                this->Writer::_do_write(' ');
            _write_tag(m_tree->val_tag(node));
            spc = true;
        }

        if(m_tree->has_val_anchor(node))
        {
            if(spc)
                this->Writer::_do_write(' ');
            this->Writer::_do_write('&');
            this->Writer::_do_write(m_tree->val_anchor(node));
            spc = true;
        }

        if(spc)
            this->Writer::_do_write(' ');

        if(m_tree->is_map(node))
        {
            this->Writer::_do_write('{');
        }
        else
        {
            _RYML_CB_ASSERT(m_tree->callbacks(), m_tree->is_seq(node));
            this->Writer::_do_write('[');
        }
    } // container

    for(id_type child = m_tree->first_child(node), count = 0; child != NONE; child = m_tree->next_sibling(child))
    {
        if(count++)
            this->Writer::_do_write(',');
        if(m_tree->is_keyval(child))
        {
            _writek(child, ilevel);
            this->Writer::_do_write(": ");
            _writev(child, ilevel);
        }
        else if(m_tree->is_val(child))
        {
            _writev(child, ilevel);
        }
        else
        {
            // with single-line flow, we can never go back to block
            _do_visit_flow_sl(child, depth + 1, ilevel + 1);
        }
    }

    if(m_tree->is_map(node))
    {
        this->Writer::_do_write('}');
    }
    else if(m_tree->is_seq(node))
    {
        this->Writer::_do_write(']');
    }
    m_flow = prev_flow;
}

C4_SUPPRESS_WARNING_MSVC_WITH_PUSH(4702) // unreachable error, triggered by flow_ml not implemented

template<class Writer>
void Emitter<Writer>::_do_visit_flow_ml(id_type id, id_type depth, id_type ilevel, id_type do_indent)
{
    C4_UNUSED(id);
    C4_UNUSED(depth);
    C4_UNUSED(ilevel);
    C4_UNUSED(do_indent);
    c4::yml::error("not implemented");
    #ifdef THIS_IS_A_WORK_IN_PROGRESS
    if(C4_UNLIKELY(depth > m_opts.max_depth()))
        _RYML_CB_ERR(m_tree->callbacks(), "max depth exceeded");
    const bool prev_flow = m_flow;
    m_flow = true;
    // do it...
    m_flow = prev_flow;
    #endif
}

template<class Writer>
void Emitter<Writer>::_do_visit_block_container(id_type node, id_type depth, id_type level, bool do_indent)
{
    if(m_tree->is_seq(node))
    {
        for(id_type child = m_tree->first_child(node); child != NONE; child = m_tree->next_sibling(child))
        {
            _RYML_CB_ASSERT(m_tree->callbacks(), !m_tree->has_key(child));
            if(m_tree->is_val(child))
            {
                _indent(level, do_indent);
                this->Writer::_do_write("- ");
                _writev(child, level);
                this->Writer::_do_write('\n');
            }
            else
            {
                _RYML_CB_ASSERT(m_tree->callbacks(), m_tree->is_container(child));
                NodeType ty = m_tree->type(child);
                if(ty.is_flow_sl())
                {
                    _indent(level, do_indent);
                    this->Writer::_do_write("- ");
                    _do_visit_flow_sl(child, depth+1, 0u);
                    this->Writer::_do_write('\n');
                }
                else if(ty.is_flow_ml())
                {
                    _indent(level, do_indent);
                    this->Writer::_do_write("- ");
                    _do_visit_flow_ml(child, depth+1, 0u, do_indent);
                    this->Writer::_do_write('\n');
                }
                else
                {
                    _do_visit_block(child, depth+1, level, do_indent); // same indentation level
                }
            }
            do_indent = true;
        }
    }
    else // map
    {
        _RYML_CB_ASSERT(m_tree->callbacks(), m_tree->is_map(node));
        for(id_type ich = m_tree->first_child(node); ich != NONE; ich = m_tree->next_sibling(ich))
        {
            _RYML_CB_ASSERT(m_tree->callbacks(), m_tree->has_key(ich));
            if(m_tree->is_keyval(ich))
            {
                _indent(level, do_indent);
                _writek(ich, level);
                this->Writer::_do_write(": ");
                _writev(ich, level);
                this->Writer::_do_write('\n');
            }
            else
            {
                _RYML_CB_ASSERT(m_tree->callbacks(), m_tree->is_container(ich));
                NodeType ty = m_tree->type(ich);
                if(ty.is_flow_sl())
                {
                    _indent(level, do_indent);
                    _do_visit_flow_sl(ich, depth+1, 0u);
                    this->Writer::_do_write('\n');
                }
                else if(ty.is_flow_ml())
                {
                    _indent(level, do_indent);
                    _do_visit_flow_ml(ich, depth+1, 0u);
                    this->Writer::_do_write('\n');
                }
                else
                {
                    _do_visit_block(ich, depth+1, level, do_indent); // same level!
                }
            } // keyval vs container
            do_indent = true;
        } // for children
    } // seq vs map
}

template<class Writer>
void Emitter<Writer>::_do_visit_block(id_type node, id_type depth, id_type ilevel, id_type do_indent)
{
    _RYML_CB_ASSERT(m_tree->callbacks(), !m_tree->is_stream(node));
    _RYML_CB_ASSERT(m_tree->callbacks(), m_tree->is_container(node) || m_tree->is_doc(node));
    _RYML_CB_ASSERT(m_tree->callbacks(), m_tree->is_root(node) || (m_tree->parent_is_map(node) || m_tree->parent_is_seq(node)));
    if(C4_UNLIKELY(depth > m_opts.max_depth()))
        _RYML_CB_ERR(m_tree->callbacks(), "max depth exceeded");
    if(m_tree->is_doc(node))
    {
        _write_doc(node);
        if(!m_tree->has_children(node))
            return;
    }
    else if(m_tree->is_container(node))
    {
        _RYML_CB_ASSERT(m_tree->callbacks(), m_tree->is_map(node) || m_tree->is_seq(node));
        bool spc = false; // write a space
        bool nl = false;  // write a newline
        if(m_tree->has_key(node))
        {
            _indent(ilevel, do_indent);
            _writek(node, ilevel);
            this->Writer::_do_write(':');
            spc = true;
        }
        else if(!m_tree->is_root(node))
        {
            _indent(ilevel, do_indent);
            this->Writer::_do_write('-');
            spc = true;
        }

        if(m_tree->has_val_tag(node))
        {
            if(spc)
                this->Writer::_do_write(' ');
            _write_tag(m_tree->val_tag(node));
            spc = true;
            nl = true;
        }

        if(m_tree->has_val_anchor(node))
        {
            if(spc)
                this->Writer::_do_write(' ');
            this->Writer::_do_write('&');
            this->Writer::_do_write(m_tree->val_anchor(node));
            spc = true;
            nl = true;
        }

        if(m_tree->has_children(node))
        {
            if(m_tree->has_key(node))
                nl = true;
            else
                if(!m_tree->is_root(node) && !nl)
                    spc = true;
        }
        else
        {
            if(m_tree->is_seq(node))
                this->Writer::_do_write(" []\n");
            else if(m_tree->is_map(node))
                this->Writer::_do_write(" {}\n");
            return;
        }

        if(spc && !nl)
            this->Writer::_do_write(' ');

        do_indent = 0;
        if(nl)
        {
            this->Writer::_do_write('\n');
            do_indent = 1;
        }
    } // container

    id_type next_level = ilevel + 1;
    if(m_tree->is_root(node) || m_tree->is_doc(node))
        next_level = ilevel; // do not indent at top level

    _do_visit_block_container(node, depth, next_level, do_indent);
}

C4_SUPPRESS_WARNING_MSVC_POP


template<class Writer>
void Emitter<Writer>::_do_visit_json(id_type id, id_type depth)
{
    _RYML_CB_CHECK(m_tree->callbacks(), !m_tree->is_stream(id)); // JSON does not have streams
    if(C4_UNLIKELY(depth > m_opts.max_depth()))
        _RYML_CB_ERR(m_tree->callbacks(), "max depth exceeded");
    if(m_tree->is_keyval(id))
    {
        _writek_json(id);
        this->Writer::_do_write(": ");
        _writev_json(id);
    }
    else if(m_tree->is_val(id))
    {
        _writev_json(id);
    }
    else if(m_tree->is_container(id))
    {
        if(m_tree->has_key(id))
        {
            _writek_json(id);
            this->Writer::_do_write(": ");
        }
        if(m_tree->is_seq(id))
            this->Writer::_do_write('[');
        else if(m_tree->is_map(id))
            this->Writer::_do_write('{');
    }  // container

    for(id_type ich = m_tree->first_child(id); ich != NONE; ich = m_tree->next_sibling(ich))
    {
        if(ich != m_tree->first_child(id))
            this->Writer::_do_write(',');
        _do_visit_json(ich, depth+1);
    }

    if(m_tree->is_seq(id))
        this->Writer::_do_write(']');
    else if(m_tree->is_map(id))
        this->Writer::_do_write('}');
}

template<class Writer>
void Emitter<Writer>::_write(NodeScalar const& C4_RESTRICT sc, NodeType flags, id_type ilevel)
{
    if( ! sc.tag.empty())
    {
        _write_tag(sc.tag);
        this->Writer::_do_write(' ');
    }
    if(flags.has_anchor())
    {
        RYML_ASSERT(flags.is_ref() != flags.has_anchor());
        RYML_ASSERT( ! sc.anchor.empty());
        this->Writer::_do_write('&');
        this->Writer::_do_write(sc.anchor);
        this->Writer::_do_write(' ');
    }
    else if(flags.is_ref())
    {
        if(sc.anchor != "<<")
            this->Writer::_do_write('*');
        this->Writer::_do_write(sc.anchor);
        if(flags.is_key_ref())
            this->Writer::_do_write(' ');
        return;
    }

    // ensure the style flags only have one of KEY or VAL
    _RYML_CB_ASSERT(m_tree->callbacks(), ((flags & SCALAR_STYLE) == 0) || (((flags & KEY_STYLE) == 0) != ((flags & VAL_STYLE) == 0)));
    type_bits style_marks = flags & SCALAR_STYLE;
    if(!style_marks)
        style_marks = scalar_style_choose(sc.scalar);
    if(style_marks & (KEY_LITERAL|VAL_LITERAL))
    {
        _write_scalar_literal(sc.scalar, ilevel, flags.has_key());
    }
    else if(style_marks & (KEY_FOLDED|VAL_FOLDED))
    {
        _write_scalar_folded(sc.scalar, ilevel, flags.has_key());
    }
    else if(style_marks & (KEY_SQUO|VAL_SQUO))
    {
        _write_scalar_squo(sc.scalar, ilevel);
    }
    else if(style_marks & (KEY_DQUO|VAL_DQUO))
    {
        _write_scalar_dquo(sc.scalar, ilevel);
    }
    else if(style_marks & (KEY_PLAIN|VAL_PLAIN))
    {
        if(C4_LIKELY(!(sc.scalar.begins_with(": ") || sc.scalar.begins_with(":\t"))))
            _write_scalar_plain(sc.scalar, ilevel);
        else
            _write_scalar_squo(sc.scalar, ilevel);
    }
    else
    {
        _RYML_CB_ERR(m_tree->callbacks(), "not implemented");
    }
}

template<class Writer>
void Emitter<Writer>::_write_json(NodeScalar const& C4_RESTRICT sc, NodeType flags)
{
    if(flags & (KEYTAG|VALTAG))
        if(m_opts.json_error_flags() & EmitOptions::JSON_ERR_ON_TAG)
            _RYML_CB_ERR(m_tree->callbacks(), "JSON does not have tags");
    if(C4_UNLIKELY(flags.has_anchor()))
        if(m_opts.json_error_flags() & EmitOptions::JSON_ERR_ON_ANCHOR)
            _RYML_CB_ERR(m_tree->callbacks(), "JSON does not have anchors");
    if(sc.scalar.len)
    {
        // use double quoted style...
        // if it is a key (mandatory in JSON)
        // if the style is marked quoted
        bool dquoted = ((flags & (KEY|VALQUO))
                        || (scalar_style_json_choose(sc.scalar) & SCALAR_DQUO)); // choose the style
        if(dquoted)
            _write_scalar_json_dquo(sc.scalar);
        else
            this->Writer::_do_write(sc.scalar);
    }
    else
    {
        if(sc.scalar.str || (flags & (KEY|VALQUO|KEYTAG|VALTAG)))
            this->Writer::_do_write("\"\"");
        else
            this->Writer::_do_write("null");
    }
}

template<class Writer>
size_t Emitter<Writer>::_write_escaped_newlines(csubstr s, size_t i)
{
    RYML_ASSERT(s.len > i);
    RYML_ASSERT(s.str[i] == '\n');
    //_c4dbgpf("nl@i={} rem=[{}]~~~{}~~~", i, s.sub(i).len, s.sub(i));
    // add an extra newline for each sequence of consecutive
    // newline/whitespace
    this->Writer::_do_write('\n');
    do
    {
        this->Writer::_do_write('\n'); // write the newline again
        ++i; // increase the outer loop counter!
    } while(i < s.len && s.str[i] == '\n');
    _RYML_CB_ASSERT(m_tree->callbacks(), i > 0);
    --i;
    _RYML_CB_ASSERT(m_tree->callbacks(), s.str[i] == '\n');
    return i;
}

inline bool _is_indented_block(csubstr s, size_t prev, size_t i) noexcept
{
    if(prev == 0 && s.begins_with_any(" \t"))
        return true;
    const size_t pos = s.first_not_of('\n', i);
    return (pos != npos) && (s.str[pos] == ' ' || s.str[pos] == '\t');
}

template<class Writer>
size_t Emitter<Writer>::_write_indented_block(csubstr s, size_t i, id_type ilevel)
{
    //_c4dbgpf("indblock@i={} rem=[{}]~~~\n{}~~~", i, s.sub(i).len, s.sub(i));
    _RYML_CB_ASSERT(m_tree->callbacks(), i > 0);
    _RYML_CB_ASSERT(m_tree->callbacks(), s.str[i-1] == '\n');
    _RYML_CB_ASSERT(m_tree->callbacks(), i < s.len);
    _RYML_CB_ASSERT(m_tree->callbacks(), s.str[i] == ' ' || s.str[i] == '\t' || s.str[i] == '\n');
again:
    size_t pos = s.find("\n ", i);
    if(pos == npos)
        pos = s.find("\n\t", i);
    if(pos != npos)
    {
        ++pos;
        //_c4dbgpf("indblock line@i={} rem=[{}]~~~\n{}~~~", i, s.range(i, pos).len, s.range(i, pos));
        _rymlindent_nextline();
        this->Writer::_do_write(s.range(i, pos));
        i = pos;
        goto again; // NOLINT
    }
    // consume the newlines after the indented block
    // to prevent them from being escaped
    pos = s.find('\n', i);
    if(pos != npos)
    {
        const size_t pos2 = s.first_not_of('\n', pos);
        pos = (pos2 != npos) ? pos2 : pos;
        //_c4dbgpf("indblock line@i={} rem=[{}]~~~\n{}~~~", i, s.range(i, pos).len, s.range(i, pos));
        _rymlindent_nextline();
        this->Writer::_do_write(s.range(i, pos));
        i = pos;
    }
    return i;
}

template<class Writer>
void Emitter<Writer>::_write_scalar_literal(csubstr s, id_type ilevel, bool explicit_key)
{
    _RYML_CB_ASSERT(m_tree->callbacks(), s.find("\r") == csubstr::npos);
    if(explicit_key)
        this->Writer::_do_write("? ");
    csubstr trimmed = s.trimr('\n');
    const size_t numnewlines_at_end = s.len - trimmed.len;
    const bool is_newline_only = (trimmed.len == 0 && (s.len > 0));
    const bool explicit_indentation = s.triml("\n\r").begins_with_any(" \t");
    //
    this->Writer::_do_write('|');
    if(explicit_indentation)
        this->Writer::_do_write('2');
    //
    if(numnewlines_at_end > 1 || is_newline_only)
        this->Writer::_do_write('+');
    else if(numnewlines_at_end == 0)
        this->Writer::_do_write('-');
    //
    if(trimmed.len)
    {
        this->Writer::_do_write('\n');
        size_t pos = 0; // tracks the last character that was already written
        for(size_t i = 0; i < trimmed.len; ++i)
        {
            if(trimmed[i] != '\n')
                continue;
            // write everything up to this point
            csubstr since_pos = trimmed.range(pos, i+1); // include the newline
            _rymlindent_nextline()
            this->Writer::_do_write(since_pos);
            pos = i+1; // already written
        }
        if(pos < trimmed.len)
        {
            _rymlindent_nextline()
            this->Writer::_do_write(trimmed.sub(pos));
        }
    }
    for(size_t i = !is_newline_only; i < numnewlines_at_end; ++i)
        this->Writer::_do_write('\n');
    if(explicit_key)
    {
        this->Writer::_do_write('\n');
        this->_indent(ilevel);
    }
}

template<class Writer>
void Emitter<Writer>::_write_scalar_folded(csubstr s, id_type ilevel, bool explicit_key)
{
    if(explicit_key)
        this->Writer::_do_write("? ");
    _RYML_CB_ASSERT(m_tree->callbacks(), s.find("\r") == csubstr::npos);
    csubstr trimmed = s.trimr('\n');
    const size_t numnewlines_at_end = s.len - trimmed.len;
    const bool is_newline_only = (trimmed.len == 0 && (s.len > 0));
    const bool explicit_indentation = s.triml("\n\r").begins_with_any(" \t");
    //
    this->Writer::_do_write('>');
    if(explicit_indentation)
        this->Writer::_do_write('2');
    //
    if(numnewlines_at_end == 0)
        this->Writer::_do_write('-');
    else if(numnewlines_at_end > 1 || is_newline_only)
        this->Writer::_do_write('+');
    //
    if(trimmed.len)
    {
        this->Writer::_do_write('\n');
        size_t pos = 0; // tracks the last character that was already written
        for(size_t i = 0; i < trimmed.len; ++i)
        {
            if(trimmed[i] != '\n')
                continue;
            // escape newline sequences
            if( ! _is_indented_block(s, pos, i))
            {
                if(pos < i)
                {
                    _rymlindent_nextline()
                    this->Writer::_do_write(s.range(pos, i));
                    i = _write_escaped_newlines(s, i);
                    pos = i+1;
                }
                else
                {
                    if(i+1 < s.len)
                    {
                        if(s.str[i+1] == '\n')
                        {
                            ++i;
                            i = _write_escaped_newlines(s, i);
                            pos = i+1;
                        }
                        else
                        {
                            this->Writer::_do_write('\n');
                            pos = i+1;
                        }
                    }
                }
            }
            else // do not escape newlines in indented blocks
            {
                ++i;
                _rymlindent_nextline()
                this->Writer::_do_write(s.range(pos, i));
                if(pos > 0 || !s.begins_with_any(" \t"))
                    i = _write_indented_block(s, i, ilevel);
                pos = i;
            }
        }
        if(pos < trimmed.len)
        {
            _rymlindent_nextline()
            this->Writer::_do_write(trimmed.sub(pos));
        }
    }
    for(size_t i = !is_newline_only; i < numnewlines_at_end; ++i)
        this->Writer::_do_write('\n');
    if(explicit_key)
    {
        this->Writer::_do_write('\n');
        this->_indent(ilevel);
    }
}

template<class Writer>
void Emitter<Writer>::_write_scalar_squo(csubstr s, id_type ilevel)
{
    size_t pos = 0; // tracks the last character that was already written
    this->Writer::_do_write('\'');
    for(size_t i = 0; i < s.len; ++i)
    {
        if(s[i] == '\n')
        {
            this->Writer::_do_write(s.range(pos, i));  // write everything up to (excluding) this char
            //_c4dbgpf("newline at {}. writing ~~~{}~~~", i, s.range(pos, i));
            i = _write_escaped_newlines(s, i);
            //_c4dbgpf("newline --> {}", i);
            if(i < s.len)
                _rymlindent_nextline()
            pos = i+1;
        }
        else if(s[i] == '\'')
        {
            csubstr sub = s.range(pos, i+1);
            //_c4dbgpf("squote at {}. writing ~~~{}~~~", i, sub);
            this->Writer::_do_write(sub); // write everything up to (including) this squote
            this->Writer::_do_write('\''); // write the squote again
            pos = i+1;
        }
    }
    // write missing characters at the end of the string
    if(pos < s.len)
        this->Writer::_do_write(s.sub(pos));
    this->Writer::_do_write('\'');
}

template<class Writer>
void Emitter<Writer>::_write_scalar_dquo(csubstr s, id_type ilevel)
{
    size_t pos = 0; // tracks the last character that was already written
    this->Writer::_do_write('"');
    for(size_t i = 0; i < s.len; ++i)
    {
        const char curr = s.str[i];
        switch(curr) // NOLINT
        {
        case '"':
        case '\\':
        {
            csubstr sub = s.range(pos, i);
            this->Writer::_do_write(sub);  // write everything up to (excluding) this char
            this->Writer::_do_write('\\'); // write the escape
            this->Writer::_do_write(curr); // write the char
            pos = i+1;
            break;
        }
#ifndef prefer_writing_newlines_as_double_newlines
        case '\n':
        {
            csubstr sub = s.range(pos, i);
            this->Writer::_do_write(sub);   // write everything up to (excluding) this char
            this->Writer::_do_write("\\n"); // write the escape
            pos = i+1;
            (void)ilevel;
            break;
        }
#else
        case '\n':
        {
            // write everything up to (excluding) this newline
            //_c4dbgpf("nl@i={} rem=[{}]~~~{}~~~", i, s.sub(i).len, s.sub(i));
            this->Writer::_do_write(s.range(pos, i));
            i = _write_escaped_newlines(s, i);
            ++i;
            pos = i;
            // as for the next line...
            if(i < s.len)
            {
                _rymlindent_nextline() // indent the next line
                // escape leading whitespace, and flush it
                size_t first = s.first_not_of(" \t", i);
                _c4dbgpf("@i={} first={} rem=[{}]~~~{}~~~", i, first, s.sub(i).len, s.sub(i));
                if(first > i)
                {
                    if(first == npos)
                        first = s.len;
                    this->Writer::_do_write('\\');
                    this->Writer::_do_write(s.range(i, first));
                    this->Writer::_do_write('\\');
                    i = first-1;
                    pos = first;
                }
            }
            break;
        }
        // escape trailing whitespace before a newline
        case ' ':
        case '\t':
        {
            const size_t next = s.first_not_of(" \t\r", i);
            if(next != npos && s.str[next] == '\n')
            {
                csubstr sub = s.range(pos, i);
                this->Writer::_do_write(sub);  // write everything up to (excluding) this char
                this->Writer::_do_write('\\'); // escape the whitespace
                pos = i;
            }
            break;
        }
#endif
        case '\r':
        {
            csubstr sub = s.range(pos, i);
            this->Writer::_do_write(sub);  // write everything up to (excluding) this char
            this->Writer::_do_write("\\r"); // write the escaped char
            pos = i+1;
            break;
        }
        case '\b':
        {
            csubstr sub = s.range(pos, i);
            this->Writer::_do_write(sub);  // write everything up to (excluding) this char
            this->Writer::_do_write("\\b"); // write the escaped char
            pos = i+1;
            break;
        }
        }
    }
    // write missing characters at the end of the string
    if(pos < s.len)
        this->Writer::_do_write(s.sub(pos));
    this->Writer::_do_write('"');
}

template<class Writer>
void Emitter<Writer>::_write_scalar_plain(csubstr s, id_type ilevel)
{
    if(C4_UNLIKELY(ilevel == 0 && (s.begins_with("...") || s.begins_with("---"))))
    {
        _rymlindent_nextline()     // indent the next line
        ++ilevel;
    }
    size_t pos = 0; // tracks the last character that was already written
    for(size_t i = 0; i < s.len; ++i)
    {
        const char curr = s.str[i];
        if(curr == '\n')
        {
            csubstr sub = s.range(pos, i);
            this->Writer::_do_write(sub);  // write everything up to (including) this newline
            i = _write_escaped_newlines(s, i);
            pos = i+1;
            if(pos < s.len)
                _rymlindent_nextline()     // indent the next line
        }
    }
    // write missing characters at the end of the string
    if(pos < s.len)
        this->Writer::_do_write(s.sub(pos));
}

#undef _rymlindent_nextline

template<class Writer>
void Emitter<Writer>::_write_scalar_json_dquo(csubstr s)
{
    size_t pos = 0;
    this->Writer::_do_write('"');
    for(size_t i = 0; i < s.len; ++i)
    {
        switch(s.str[i])
        {
        case '"':
            this->Writer ::_do_write(s.range(pos, i));
            this->Writer ::_do_write("\\\"");
            pos = i + 1;
            break;
        case '\n':
            this->Writer ::_do_write(s.range(pos, i));
            this->Writer ::_do_write("\\n");
            pos = i + 1;
            break;
        case '\t':
            this->Writer ::_do_write(s.range(pos, i));
            this->Writer ::_do_write("\\t");
            pos = i + 1;
            break;
        case '\\':
            this->Writer ::_do_write(s.range(pos, i));
            this->Writer ::_do_write("\\\\");
            pos = i + 1;
            break;
        case '\r':
            this->Writer ::_do_write(s.range(pos, i));
            this->Writer ::_do_write("\\r");
            pos = i + 1;
            break;
        case '\b':
            this->Writer ::_do_write(s.range(pos, i));
            this->Writer ::_do_write("\\b");
            pos = i + 1;
            break;
        case '\f':
            this->Writer ::_do_write(s.range(pos, i));
            this->Writer ::_do_write("\\f");
            pos = i + 1;
            break;
        }
    }
    if(pos < s.len)
    {
        csubstr sub = s.sub(pos);
        this->Writer::_do_write(sub);
    }
    this->Writer::_do_write('"');
}

} // namespace yml
} // namespace c4

#endif /* _C4_YML_EMIT_DEF_HPP_ */
