#ifndef _C4_YML_EMIT_HPP_
#define _C4_YML_EMIT_HPP_

/** @file emit.hpp Utilities to emit YAML and JSON. */

#ifndef _C4_YML_WRITER_HPP_
#include "./writer.hpp"
#endif

#ifndef _C4_YML_TREE_HPP_
#include "./tree.hpp"
#endif

#ifndef _C4_YML_NODE_HPP_
#include "./node.hpp"
#endif


C4_SUPPRESS_WARNING_GCC_CLANG_WITH_PUSH("-Wold-style-cast")


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

namespace c4 {
namespace yml {

/** @addtogroup doc_emit
 *
 * @{
 */

// fwd declarations
template<class Writer> class Emitter;
template<class OStream>
using EmitterOStream = Emitter<WriterOStream<OStream>>;
using EmitterFile = Emitter<WriterFile>;
using EmitterBuf  = Emitter<WriterBuf>;

namespace detail {
inline bool is_set_(ConstNodeRef n) { return n.tree() && (n.id() != NONE); }
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/** Specifies the type of content to emit */
typedef enum {
    EMIT_YAML = 0, ///< emit YAML
    EMIT_JSON = 1  ///< emit JSON
} EmitType_e;


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/** A lightweight object containing options to be used when emitting. */
struct EmitOptions
{
    typedef enum : uint32_t {
        DEFAULT_FLAGS = 0u,
        JSON_ERR_ON_TAG = 1u << 0u,
        JSON_ERR_ON_ANCHOR = 1u << 1u,
        _JSON_ERR_MASK = JSON_ERR_ON_TAG|JSON_ERR_ON_ANCHOR,
    } EmitOptionFlags_e;

public:

    /** @name option flags
     *
     * @{ */
    C4_ALWAYS_INLINE EmitOptionFlags_e json_error_flags() const noexcept { return m_option_flags; }
    EmitOptions& json_error_flags(EmitOptionFlags_e d) noexcept { m_option_flags = (EmitOptionFlags_e)(d & _JSON_ERR_MASK); return *this; }
    /** @} */

public:

    /** @name max depth for the emitted tree
     *
     * This makes the emitter fail when emitting trees exceeding the
     * max_depth.
     *
     * @{ */
    C4_ALWAYS_INLINE id_type max_depth() const noexcept { return m_max_depth; }
    EmitOptions& max_depth(id_type d) noexcept { m_max_depth = d; return *this; }
    static constexpr const id_type max_depth_default = 64;
    /** @} */

public:

    bool operator== (const EmitOptions& that) const noexcept
    {
        return m_max_depth == that.m_max_depth &&
            m_option_flags == that.m_option_flags;
    }

private:

    /** @cond dev */
    id_type m_max_depth{max_depth_default};
    EmitOptionFlags_e m_option_flags{DEFAULT_FLAGS};
    /** @endcond */
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/** A stateful emitter, for use with a writer such as @ref WriterBuf,
 * @ref WriterFile, or @ref WriterOStream */
template<class Writer>
class Emitter : public Writer
{
public:

    /** Construct the emitter and its internal Writer state, using default emit options.
     * @param args arguments to be forwarded to the constructor of the writer.
     * */
    template<class ...Args>
    Emitter(Args &&...args) : Writer(std::forward<Args>(args)...), m_tree(), m_opts(), m_flow(false) {}

    /** Construct the emitter and its internal Writer state.
     *
     * @param opts EmitOptions
     * @param args arguments to be forwarded to the constructor of the writer.
     * */
    template<class ...Args>
    Emitter(EmitOptions const& opts, Args &&...args) : Writer(std::forward<Args>(args)...), m_tree(), m_opts(opts), m_flow(false) {}

    /** emit!
     *
     * When writing to a buffer, returns a substr of the emitted YAML.
     * If the given buffer has insufficient space, the returned substr
     * will be null and its size will be the needed space. Whatever
     * the size of the buffer, it is guaranteed that no writes are
     * done past its end.
     *
     * When writing to a file, the returned substr will be null, but its
     * length will be set to the number of bytes written.
     *
     * @param type specify what to emit
     * @param t the tree to emit
     * @param id the id of the node to emit
     * @param error_on_excess when true, an error is raised when the
     *        output buffer is too small for the emitted YAML/JSON
     * */
    substr emit_as(EmitType_e type, Tree const& t, id_type id, bool error_on_excess);
    /** emit starting at the root node */
    substr emit_as(EmitType_e type, Tree const& t, bool error_on_excess=true)
    {
        if(t.empty())
            return {};
        return this->emit_as(type, t, t.root_id(), error_on_excess);
    }
    /** emit starting at the given node */
    substr emit_as(EmitType_e type, ConstNodeRef const& n, bool error_on_excess=true)
    {
        if(!detail::is_set_(n))
            return {};
        _RYML_CB_CHECK(n.tree()->callbacks(), n.readable());
        return this->emit_as(type, *n.tree(), n.id(), error_on_excess);
    }

public:

    /** get the emit options for this object */
    EmitOptions const& options() const noexcept { return m_opts; }

    /** set the max depth for emitted trees (to prevent a stack overflow) */
    void max_depth(id_type max_depth) noexcept { m_opts.max_depth(max_depth); }
    /** get the max depth for emitted trees (to prevent a stack overflow) */
    id_type max_depth() const noexcept { return m_opts.max_depth(); }

private:

    Tree const* C4_RESTRICT m_tree;
    EmitOptions m_opts;
    bool m_flow;

private:

    void _emit_yaml(id_type id);
    void _do_visit_flow_sl(id_type id, id_type depth, id_type ilevel=0);
    void _do_visit_flow_ml(id_type id, id_type depth, id_type ilevel=0, id_type do_indent=1);
    void _do_visit_block(id_type id, id_type depth, id_type ilevel=0, id_type do_indent=1);
    void _do_visit_block_container(id_type id, id_type depth, id_type next_level, bool do_indent);
    void _do_visit_json(id_type id, id_type depth);

private:

    void _write(NodeScalar const& C4_RESTRICT sc, NodeType flags, id_type level);
    void _write_json(NodeScalar const& C4_RESTRICT sc, NodeType flags);

    void _write_doc(id_type id);
    void _write_scalar_json_dquo(csubstr s);
    void _write_scalar_literal(csubstr s, id_type level, bool as_key);
    void _write_scalar_folded(csubstr s, id_type level, bool as_key);
    void _write_scalar_squo(csubstr s, id_type level);
    void _write_scalar_dquo(csubstr s, id_type level);
    void _write_scalar_plain(csubstr s, id_type level);

    size_t _write_escaped_newlines(csubstr s, size_t i);
    size_t _write_indented_block(csubstr s, size_t i, id_type level);

    void _write_tag(csubstr tag)
    {
        if(!tag.begins_with('!'))
            this->Writer::_do_write('!');
        this->Writer::_do_write(tag);
    }

    enum : type_bits {
        _keysc =  (KEY|KEYREF|KEYANCH|KEYQUO|KEY_STYLE) | ~(VAL|VALREF|VALANCH|VALQUO|VAL_STYLE) | CONTAINER_STYLE,
        _valsc = ~(KEY|KEYREF|KEYANCH|KEYQUO|KEY_STYLE) |  (VAL|VALREF|VALANCH|VALQUO|VAL_STYLE) | CONTAINER_STYLE,
        _keysc_json =  (KEY)  | ~(VAL),
        _valsc_json = ~(KEY)  |  (VAL),
    };

    C4_ALWAYS_INLINE void _writek(id_type id, id_type level) { _write(m_tree->keysc(id), (m_tree->_p(id)->m_type.type & ~_valsc), level); }
    C4_ALWAYS_INLINE void _writev(id_type id, id_type level) { _write(m_tree->valsc(id), (m_tree->_p(id)->m_type.type & ~_keysc), level); }

    C4_ALWAYS_INLINE void _writek_json(id_type id) { _write_json(m_tree->keysc(id), m_tree->_p(id)->m_type.type & ~(VAL)); }
    C4_ALWAYS_INLINE void _writev_json(id_type id) { _write_json(m_tree->valsc(id), m_tree->_p(id)->m_type.type & ~(KEY)); }

    void _indent(id_type level, bool enabled)
    {
        if(enabled)
            this->Writer::_do_write(' ', 2u * (size_t)level);
    }
    void _indent(id_type level)
    {
        if(!m_flow)
            this->Writer::_do_write(' ', 2u * (size_t)level);
    }
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/** @defgroup doc_emit_to_file Emit to file
 *
 * @{
 */


// emit from tree and node id -----------------------

/** (1) emit YAML to the given file, starting at the given node. A null
 * file defaults to stdout. Return the number of bytes written. */
inline size_t emit_yaml(Tree const& t, id_type id, EmitOptions const& opts, FILE *f)
{
    EmitterFile em(opts, f);
    return em.emit_as(EMIT_YAML, t, id, /*error_on_excess*/true).len;
}
/** (2) like (1), but use default emit options */
inline size_t emit_yaml(Tree const& t, id_type id, FILE *f)
{
    EmitterFile em(f);
    return em.emit_as(EMIT_YAML, t, id, /*error_on_excess*/true).len;
}
/** (1) emit JSON to the given file, starting at the given node. A null
 * file defaults to stdout.  Return the number of bytes written. */
inline size_t emit_json(Tree const& t, id_type id, EmitOptions const& opts, FILE *f)
{
    EmitterFile em(opts, f);
    return em.emit_as(EMIT_JSON, t, id, /*error_on_excess*/true).len;
}
/** (2) like (1), but use default emit options */
inline size_t emit_json(Tree const& t, id_type id, FILE *f)
{
    EmitterFile em(f);
    return em.emit_as(EMIT_JSON, t, id, /*error_on_excess*/true).len;
}


// emit from root -------------------------

/** (1) emit YAML to the given file, starting at the root node. A null file defaults to stdout.
 * Return the number of bytes written. */
inline size_t emit_yaml(Tree const& t, EmitOptions const& opts, FILE *f=nullptr)
{
    EmitterFile em(opts, f);
    return em.emit_as(EMIT_YAML, t, /*error_on_excess*/true).len;
}
/** (2) like (1), but use default emit options */
inline size_t emit_yaml(Tree const& t, FILE *f=nullptr)
{
    EmitterFile em(f);
    return em.emit_as(EMIT_YAML, t, /*error_on_excess*/true).len;
}
/** (1) emit JSON to the given file. A null file defaults to stdout.
 * Return the number of bytes written. */
inline size_t emit_json(Tree const& t, EmitOptions const& opts, FILE *f=nullptr)
{
    EmitterFile em(opts, f);
    return em.emit_as(EMIT_JSON, t, /*error_on_excess*/true).len;
}
/** (2) like (1), but use default emit options */
inline size_t emit_json(Tree const& t, FILE *f=nullptr)
{
    EmitterFile em(f);
    return em.emit_as(EMIT_JSON, t, /*error_on_excess*/true).len;
}


// emit from ConstNodeRef ------------------------

/** (1) emit YAML to the given file. A null file defaults to stdout.
 * Return the number of bytes written. */
inline size_t emit_yaml(ConstNodeRef const& r, EmitOptions const& opts, FILE *f=nullptr)
{
    if(!detail::is_set_(r))
        return {};
    EmitterFile em(opts, f);
    return em.emit_as(EMIT_YAML, r, /*error_on_excess*/true).len;
}
/** (2) like (1), but use default emit options */
inline size_t emit_yaml(ConstNodeRef const& r, FILE *f=nullptr)
{
    if(!detail::is_set_(r))
        return {};
    EmitterFile em(f);
    return em.emit_as(EMIT_YAML, r, /*error_on_excess*/true).len;
}
/** (1) emit JSON to the given file. A null file defaults to stdout.
 * Return the number of bytes written. */
inline size_t emit_json(ConstNodeRef const& r, EmitOptions const& opts, FILE *f=nullptr)
{
    if(!detail::is_set_(r))
        return {};
    EmitterFile em(opts, f);
    return em.emit_as(EMIT_JSON, r, /*error_on_excess*/true).len;
}
/** (2) like (1), but use default emit options */
inline size_t emit_json(ConstNodeRef const& r, FILE *f=nullptr)
{
    if(!detail::is_set_(r))
        return {};
    EmitterFile em(f);
    return em.emit_as(EMIT_JSON, r, /*error_on_excess*/true).len;
}

/** @} */


//-----------------------------------------------------------------------------

/** @defgroup doc_emit_to_ostream Emit to an STL-like ostream
 *
 * @{
 */

/** emit YAML to an STL-like ostream */
template<class OStream>
inline OStream& operator<< (OStream& s, Tree const& t)
{
    EmitterOStream<OStream> em(s);
    em.emit_as(EMIT_YAML, t);
    return s;
}

/** emit YAML to an STL-like ostream
 * @overload */
template<class OStream>
inline OStream& operator<< (OStream& s, ConstNodeRef const& n)
{
    if(!detail::is_set_(n))
        return s;
    EmitterOStream<OStream> em(s);
    em.emit_as(EMIT_YAML, n);
    return s;
}

/** mark a tree or node to be emitted as yaml when using @ref
 * operator<<, with options. For example:
 *
 * ```cpp
 * Tree t = parse_in_arena("{foo: bar}");
 * std::cout << t; // emits YAML
 * std::cout << as_yaml(t); // emits YAML, same as above
 * std::cout << as_yaml(t, EmitOptions().max_depth(10)); // emits JSON with a max tree depth
 * ```
 *
 * @see @ref operator<< */
struct as_json
{
    Tree const* tree;
    size_t node;
    EmitOptions options;
    as_json(Tree const& t, EmitOptions const& opts={}) : tree(&t), node(t.empty() ? NONE : t.root_id()), options(opts)  {}
    as_json(Tree const& t, size_t id, EmitOptions const& opts={}) : tree(&t), node(id), options(opts)  {}
    as_json(ConstNodeRef const& n, EmitOptions const& opts={}) : tree(n.tree()), node(n.id()), options(opts) {}
};

/** mark a tree or node to be emitted as yaml when using @ref
 * operator<< . For example:
 *
 * ```cpp
 * Tree t = parse_in_arena("{foo: bar}");
 * std::cout << t; // emits YAML
 * std::cout << as_json(t); // emits JSON
 * std::cout << as_json(t, EmitOptions().max_depth(10)); // emits JSON with a max tree depth
 * ```
 *
 * @see @ref operator<< */
struct as_yaml
{
    Tree const* tree;
    size_t node;
    EmitOptions options;
    as_yaml(Tree const& t, EmitOptions const& opts={}) : tree(&t), node(t.empty() ? NONE : t.root_id()), options(opts)  {}
    as_yaml(Tree const& t, size_t id, EmitOptions const& opts={}) : tree(&t), node(id), options(opts)  {}
    as_yaml(ConstNodeRef const& n, EmitOptions const& opts={}) : tree(n.tree()), node(n.id()), options(opts) {}
};

/** emit json to an STL-like stream */
template<class OStream>
inline OStream& operator<< (OStream& s, as_json const& j)
{
    if(!j.tree || j.node == NONE)
        return s;
    EmitterOStream<OStream> em(j.options, s);
    em.emit_as(EMIT_JSON, *j.tree, j.node, true);
    return s;
}

/** emit yaml to an STL-like stream */
template<class OStream>
inline OStream& operator<< (OStream& s, as_yaml const& y)
{
    if(!y.tree || y.node == NONE)
        return s;
    EmitterOStream<OStream> em(y.options, s);
    em.emit_as(EMIT_YAML, *y.tree, y.node, true);
    return s;
}

/** @} */


//-----------------------------------------------------------------------------

/** @defgroup doc_emit_to_buffer Emit to memory buffer
 *
 * @{
 */

// emit from tree and node id -----------------------

/** (1) emit YAML to the given buffer. Return a substr trimmed to the emitted YAML.
 * @param t the tree to emit.
 * @param id the node where to start emitting.
 * @param opts emit options.
 * @param buf the output buffer.
 * @param error_on_excess Raise an error if the space in the buffer is insufficient.
 * @return a substr trimmed to the result in the output buffer. If the buffer is
 * insufficient (when error_on_excess is false), the string pointer of the
 * result will be set to null, and the length will report the required buffer size. */
inline substr emit_yaml(Tree const& t, id_type id, EmitOptions const& opts, substr buf, bool error_on_excess=true)
{
    EmitterBuf em(opts, buf);
    return em.emit_as(EMIT_YAML, t, id, error_on_excess);
}
/** (2) like (1), but use default emit options */
inline substr emit_yaml(Tree const& t, id_type id, substr buf, bool error_on_excess=true)
{
    EmitterBuf em(buf);
    return em.emit_as(EMIT_YAML, t, id, error_on_excess);
}
/** (1) emit JSON to the given buffer. Return a substr trimmed to the emitted JSON.
 * @param t the tree to emit.
 * @param id the node where to start emitting.
 * @param opts emit options.
 * @param buf the output buffer.
 * @param error_on_excess Raise an error if the space in the buffer is insufficient.
 * @return a substr trimmed to the result in the output buffer. If the buffer is
 * insufficient (when error_on_excess is false), the string pointer of the
 * result will be set to null, and the length will report the required buffer size. */
inline substr emit_json(Tree const& t, id_type id, EmitOptions const& opts, substr buf, bool error_on_excess=true)
{
    EmitterBuf em(opts, buf);
    return em.emit_as(EMIT_JSON, t, id, error_on_excess);
}
/** (2) like (1), but use default emit options */
inline substr emit_json(Tree const& t, id_type id, substr buf, bool error_on_excess=true)
{
    EmitterBuf em(buf);
    return em.emit_as(EMIT_JSON, t, id, error_on_excess);
}


// emit from root -------------------------

/** (1) emit YAML to the given buffer. Return a substr trimmed to the emitted YAML.
 * @param t the tree; will be emitted from the root node.
 * @param opts emit options.
 * @param buf the output buffer.
 * @param error_on_excess Raise an error if the space in the buffer is insufficient.
 * @return a substr trimmed to the result in the output buffer. If the buffer is
 * insufficient (when error_on_excess is false), the string pointer of the
 * result will be set to null, and the length will report the required buffer size. */
inline substr emit_yaml(Tree const& t, EmitOptions const& opts, substr buf, bool error_on_excess=true)
{
    EmitterBuf em(opts, buf);
    return em.emit_as(EMIT_YAML, t, error_on_excess);
}
/** (2) like (1), but use default emit options */
inline substr emit_yaml(Tree const& t, substr buf, bool error_on_excess=true)
{
    EmitterBuf em(buf);
    return em.emit_as(EMIT_YAML, t, error_on_excess);
}
/** (1) emit JSON to the given buffer. Return a substr trimmed to the emitted JSON.
 * @param t the tree; will be emitted from the root node.
 * @param opts emit options.
 * @param buf the output buffer.
 * @param error_on_excess Raise an error if the space in the buffer is insufficient.
 * @return a substr trimmed to the result in the output buffer. If the buffer is
 * insufficient (when error_on_excess is false), the string pointer of the
 * result will be set to null, and the length will report the required buffer size. */
inline substr emit_json(Tree const& t, EmitOptions const& opts, substr buf, bool error_on_excess=true)
{
    EmitterBuf em(opts, buf);
    return em.emit_as(EMIT_JSON, t, error_on_excess);
}
/** (2) like (1), but use default emit options */
inline substr emit_json(Tree const& t, substr buf, bool error_on_excess=true)
{
    EmitterBuf em(buf);
    return em.emit_as(EMIT_JSON, t, error_on_excess);
}


// emit from ConstNodeRef ------------------------

/** (1) emit YAML to the given buffer. Return a substr trimmed to the emitted YAML.
 * @param r the starting node.
 * @param buf the output buffer.
 * @param opts emit options.
 * @param error_on_excess Raise an error if the space in the buffer is insufficient.
 * @return a substr trimmed to the result in the output buffer. If the buffer is
 * insufficient (when error_on_excess is false), the string pointer of the
 * result will be set to null, and the length will report the required buffer size. */
inline substr emit_yaml(ConstNodeRef const& r, EmitOptions const& opts, substr buf, bool error_on_excess=true)
{
    if(!detail::is_set_(r))
        return {};
    EmitterBuf em(opts, buf);
    return em.emit_as(EMIT_YAML, r, error_on_excess);
}
/** (2) like (1), but use default emit options */
inline substr emit_yaml(ConstNodeRef const& r, substr buf, bool error_on_excess=true)
{
    if(!detail::is_set_(r))
        return {};
    EmitterBuf em(buf);
    return em.emit_as(EMIT_YAML, r, error_on_excess);
}
/** (1) emit JSON to the given buffer. Return a substr trimmed to the emitted JSON.
 * @param r the starting node.
 * @param buf the output buffer.
 * @param opts emit options.
 * @param error_on_excess Raise an error if the space in the buffer is insufficient.
 * @return a substr trimmed to the result in the output buffer. If the buffer is
 * insufficient (when error_on_excess is false), the string pointer of the
 * result will be set to null, and the length will report the required buffer size. */
inline substr emit_json(ConstNodeRef const& r, EmitOptions const& opts, substr buf, bool error_on_excess=true)
{
    if(!detail::is_set_(r))
        return {};
    EmitterBuf em(opts, buf);
    return em.emit_as(EMIT_JSON, r, error_on_excess);
}
/** (2) like (1), but use default emit options */
inline substr emit_json(ConstNodeRef const& r, substr buf, bool error_on_excess=true)
{
    if(!detail::is_set_(r))
        return {};
    EmitterBuf em(buf);
    return em.emit_as(EMIT_JSON, r, error_on_excess);
}


//-----------------------------------------------------------------------------

/** @defgroup doc_emit_to_container Emit to resizeable container
 *
 * @{
 */

// emit from tree and node id ---------------------------

/** (1) emit+resize: emit YAML to the given `std::string`/`std::vector`-like
 * container, resizing it as needed to fit the emitted YAML. If @p append is
 * set to true, the emitted YAML is appended at the end of the container.
 *
 * @return a substr trimmed to the emitted YAML (excluding the initial contents, when appending) */
template<class CharOwningContainer>
substr emitrs_yaml(Tree const& t, id_type id, EmitOptions const& opts, CharOwningContainer * cont, bool append=false)
{
    size_t startpos = append ? cont->size() : 0u;
    cont->resize(cont->capacity()); // otherwise the first emit would be certain to fail
    substr buf = to_substr(*cont).sub(startpos);
    substr ret = emit_yaml(t, id, opts, buf, /*error_on_excess*/false);
    if(ret.str == nullptr && ret.len > 0)
    {
        cont->resize(startpos + ret.len);
        buf = to_substr(*cont).sub(startpos);
        ret = emit_yaml(t, id, opts, buf, /*error_on_excess*/true);
    }
    else
    {
        cont->resize(startpos + ret.len);
    }
    return ret;
}
/** (2) like (1), but use default emit options */
template<class CharOwningContainer>
substr emitrs_yaml(Tree const& t, id_type id, CharOwningContainer * cont, bool append=false)
{
    return emitrs_yaml(t, id, EmitOptions{}, cont, append);
}
/** (1) emit+resize: emit JSON to the given `std::string`/`std::vector`-like
 * container, resizing it as needed to fit the emitted JSON. If @p append is
 * set to true, the emitted YAML is appended at the end of the container.
 *
 * @return a substr trimmed to the emitted JSON (excluding the initial contents, when appending) */
template<class CharOwningContainer>
substr emitrs_json(Tree const& t, id_type id, EmitOptions const& opts, CharOwningContainer * cont, bool append=false)
{
    const size_t startpos = append ? cont->size() : 0u;
    cont->resize(cont->capacity()); // otherwise the first emit would be certain to fail
    substr buf = to_substr(*cont).sub(startpos);
    EmitterBuf em(opts, buf);
    substr ret = emit_json(t, id, opts, buf, /*error_on_excess*/false);
    if(ret.str == nullptr && ret.len > 0)
    {
        cont->resize(startpos + ret.len);
        buf = to_substr(*cont).sub(startpos);
        ret = emit_json(t, id, opts, buf, /*error_on_excess*/true);
    }
    else
    {
        cont->resize(startpos + ret.len);
    }
    return ret;
}
/** (2) like (1), but use default emit options */
template<class CharOwningContainer>
substr emitrs_json(Tree const& t, id_type id, CharOwningContainer * cont, bool append=false)
{
    return emitrs_json(t, id, EmitOptions{}, cont, append);
}


/** (3) emit+resize: YAML to a newly-created `std::string`/`std::vector`-like container. */
template<class CharOwningContainer>
CharOwningContainer emitrs_yaml(Tree const& t, id_type id, EmitOptions const& opts={})
{
    CharOwningContainer c;
    emitrs_yaml(t, id, opts, &c);
    return c;
}
/** (3) emit+resize: JSON to a newly-created `std::string`/`std::vector`-like container. */
template<class CharOwningContainer>
CharOwningContainer emitrs_json(Tree const& t, id_type id, EmitOptions const& opts={})
{
    CharOwningContainer c;
    emitrs_json(t, id, opts, &c);
    return c;
}


// emit from root -------------------------

/** (1) emit+resize: YAML to the given `std::string`/`std::vector`-like
 * container, resizing it as needed to fit the emitted YAML.
 * @return a substr trimmed to the new emitted contents. */
template<class CharOwningContainer>
substr emitrs_yaml(Tree const& t, EmitOptions const& opts, CharOwningContainer * cont, bool append=false)
{
    if(t.empty())
        return {};
    return emitrs_yaml(t, t.root_id(), opts, cont, append);
}
/** (2) like (1), but use default emit options */
template<class CharOwningContainer>
substr emitrs_yaml(Tree const& t, CharOwningContainer * cont, bool append=false)
{
    if(t.empty())
        return {};
    return emitrs_yaml(t, t.root_id(), EmitOptions{}, cont, append);
}
/** (1) emit+resize: JSON to the given `std::string`/`std::vector`-like
 * container, resizing it as needed to fit the emitted JSON.
 * @return a substr trimmed to the new emitted contents. */
template<class CharOwningContainer>
substr emitrs_json(Tree const& t, EmitOptions const& opts, CharOwningContainer * cont, bool append=false)
{
    if(t.empty())
        return {};
    return emitrs_json(t, t.root_id(), opts, cont, append);
}
/** (2) like (1), but use default emit options */
template<class CharOwningContainer>
substr emitrs_json(Tree const& t, CharOwningContainer * cont, bool append=false)
{
    if(t.empty())
        return {};
    return emitrs_json(t, t.root_id(), EmitOptions{}, cont, append);
}


/** (3) emit+resize: YAML to a newly-created `std::string`/`std::vector`-like container. */
template<class CharOwningContainer>
CharOwningContainer emitrs_yaml(Tree const& t, EmitOptions const& opts={})
{
    CharOwningContainer c;
    if(t.empty())
        return c;
    emitrs_yaml(t, t.root_id(), opts, &c);
    return c;
}
/** (3) emit+resize: JSON to a newly-created `std::string`/`std::vector`-like container. */
template<class CharOwningContainer>
CharOwningContainer emitrs_json(Tree const& t, EmitOptions const& opts={})
{
    CharOwningContainer c;
    if(t.empty())
        return c;
    emitrs_json(t, t.root_id(), opts, &c);
    return c;
}


// emit from ConstNodeRef ------------------------


/** (1) emit+resize: YAML to the given `std::string`/`std::vector`-like container,
 * resizing it as needed to fit the emitted YAML.
 * @return a substr trimmed to the new emitted contents */
template<class CharOwningContainer>
substr emitrs_yaml(ConstNodeRef const& n, EmitOptions const& opts, CharOwningContainer * cont, bool append=false)
{
    if(!detail::is_set_(n))
        return {};
    _RYML_CB_CHECK(n.tree()->callbacks(), n.readable());
    return emitrs_yaml(*n.tree(), n.id(), opts, cont, append);
}
/** (2) like (1), but use default emit options */
template<class CharOwningContainer>
substr emitrs_yaml(ConstNodeRef const& n, CharOwningContainer * cont, bool append=false)
{
    if(!detail::is_set_(n))
        return {};
    _RYML_CB_CHECK(n.tree()->callbacks(), n.readable());
    return emitrs_yaml(*n.tree(), n.id(), EmitOptions{}, cont, append);
}
/** (1) emit+resize: JSON to the given `std::string`/`std::vector`-like container,
 * resizing it as needed to fit the emitted JSON.
 * @return a substr trimmed to the new emitted contents */
template<class CharOwningContainer>
substr emitrs_json(ConstNodeRef const& n, EmitOptions const& opts, CharOwningContainer * cont, bool append=false)
{
    if(!detail::is_set_(n))
        return {};
    _RYML_CB_CHECK(n.tree()->callbacks(), n.readable());
    return emitrs_json(*n.tree(), n.id(), opts, cont, append);
}
/** (2) like (1), but use default emit options */
template<class CharOwningContainer>
substr emitrs_json(ConstNodeRef const& n, CharOwningContainer * cont, bool append=false)
{
    if(!detail::is_set_(n))
        return {};
    _RYML_CB_CHECK(n.tree()->callbacks(), n.readable());
    return emitrs_json(*n.tree(), n.id(), EmitOptions{}, cont, append);
}


/** (3) emit+resize: YAML to a newly-created `std::string`/`std::vector`-like container. */
template<class CharOwningContainer>
CharOwningContainer emitrs_yaml(ConstNodeRef const& n, EmitOptions const& opts={})
{
    if(!detail::is_set_(n))
        return {};
    _RYML_CB_CHECK(n.tree()->callbacks(), n.readable());
    CharOwningContainer c;
    emitrs_yaml(*n.tree(), n.id(), opts, &c);
    return c;
}
/** (3) emit+resize: JSON to a newly-created `std::string`/`std::vector`-like container. */
template<class CharOwningContainer>
CharOwningContainer emitrs_json(ConstNodeRef const& n, EmitOptions const& opts={})
{
    if(!detail::is_set_(n))
        return {};
    _RYML_CB_CHECK(n.tree()->callbacks(), n.readable());
    CharOwningContainer c;
    emitrs_json(*n.tree(), n.id(), opts, &c);
    return c;
}


/** @} */


//-----------------------------------------------------------------------------

/** @cond dev */

#define RYML_DEPRECATE_EMIT                                             \
    RYML_DEPRECATED("use emit_yaml() instead. "                         \
                    "See https://github.com/biojppm/rapidyaml/issues/120")
#define RYML_DEPRECATE_EMITRS                                           \
    RYML_DEPRECATED("use emitrs_yaml() instead. "                       \
                    "See https://github.com/biojppm/rapidyaml/issues/120")

// workaround for Qt emit which is a macro;
// see https://github.com/biojppm/rapidyaml/issues/120.
// emit is defined in qobjectdefs.h (as an empty define).
#ifdef emit
#define RYML_TMP_EMIT_
#undef emit
#endif

RYML_DEPRECATE_EMIT inline size_t emit(Tree const& t, id_type id, FILE *f)
{
    return emit_yaml(t, id, f);
}
RYML_DEPRECATE_EMIT inline size_t emit(Tree const& t, FILE *f=nullptr)
{
    return emit_yaml(t, f);
}
RYML_DEPRECATE_EMIT inline size_t emit(ConstNodeRef const& r, FILE *f=nullptr)
{
    return emit_yaml(r, f);
}

RYML_DEPRECATE_EMIT inline substr emit(Tree const& t, id_type id, substr buf, bool error_on_excess=true)
{
    return emit_yaml(t, id, buf, error_on_excess);
}
RYML_DEPRECATE_EMIT inline substr emit(Tree const& t, substr buf, bool error_on_excess=true)
{
    return emit_yaml(t, buf, error_on_excess);
}
RYML_DEPRECATE_EMIT inline substr emit(ConstNodeRef const& r, substr buf, bool error_on_excess=true)
{
    return emit_yaml(r, buf, error_on_excess);
}

#ifdef RYML_TMP_EMIT_
#define emit
#undef RYML_TMP_EMIT_
#endif

template<class CharOwningContainer>
RYML_DEPRECATE_EMITRS substr emitrs(Tree const& t, id_type id, CharOwningContainer * cont)
{
    return emitrs_yaml(t, id, cont);
}
template<class CharOwningContainer>
RYML_DEPRECATE_EMITRS CharOwningContainer emitrs(Tree const& t, id_type id)
{
    return emitrs_yaml<CharOwningContainer>(t, id);
}
template<class CharOwningContainer>
RYML_DEPRECATE_EMITRS substr emitrs(Tree const& t, CharOwningContainer * cont)
{
    return emitrs_yaml(t, cont);
}
template<class CharOwningContainer>
RYML_DEPRECATE_EMITRS CharOwningContainer emitrs(Tree const& t)
{
    return emitrs_yaml<CharOwningContainer>(t);
}
template<class CharOwningContainer>
RYML_DEPRECATE_EMITRS substr emitrs(ConstNodeRef const& n, CharOwningContainer * cont)
{
    return emitrs_yaml(n, cont);
}
template<class CharOwningContainer>
RYML_DEPRECATE_EMITRS CharOwningContainer emitrs(ConstNodeRef const& n)
{
    return emitrs_yaml<CharOwningContainer>(n);
}

/** @endcond */


} // namespace yml
} // namespace c4

C4_SUPPRESS_WARNING_GCC_CLANG_POP

#undef RYML_DEPRECATE_EMIT
#undef RYML_DEPRECATE_EMITRS

#include "c4/yml/emit.def.hpp" // NOLINT

#endif /* _C4_YML_EMIT_HPP_ */
