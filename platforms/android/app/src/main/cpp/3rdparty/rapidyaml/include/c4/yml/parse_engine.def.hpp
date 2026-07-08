#ifndef _C4_YML_PARSE_ENGINE_DEF_HPP_
#define _C4_YML_PARSE_ENGINE_DEF_HPP_

#include "c4/yml/parse_engine.hpp"
#include "c4/error.hpp"
#include "c4/charconv.hpp"
#include "c4/utf.hpp"

#include <ctype.h>

#include "c4/yml/detail/dbgprint.hpp"
#include "c4/yml/filter_processor.hpp"
#ifdef RYML_DBG
#include <c4/dump.hpp>
#include "c4/yml/detail/print.hpp"
#define _c4err_(fmt, ...) do { RYML_DEBUG_BREAK(); this->_err("ERROR:\n" "{}:{}: " fmt, __FILE__, __LINE__, __VA_ARGS__); } while(0)
#define _c4err(fmt) do { RYML_DEBUG_BREAK(); this->_err("ERROR:\n" "{}:{}: " fmt, __FILE__, __LINE__); } while(0)
#else
#define _c4err_(fmt, ...) this->_err("ERROR: " fmt, __VA_ARGS__)
#define _c4err(fmt) this->_err("ERROR: {}", fmt)
#endif


#if defined(RYML_WITH_TAB_TOKENS)
#define _RYML_WITH_TAB_TOKENS(...) __VA_ARGS__
#define _RYML_WITHOUT_TAB_TOKENS(...)
#define _RYML_WITH_OR_WITHOUT_TAB_TOKENS(with, without) with
#else
#define _RYML_WITH_TAB_TOKENS(...)
#define _RYML_WITHOUT_TAB_TOKENS(...) __VA_ARGS__
#define _RYML_WITH_OR_WITHOUT_TAB_TOKENS(with, without) without
#endif


// scaffold:
#define _c4dbgnextline()                           \
    do {                                           \
       _c4dbgq("\n-----------");                   \
       _c4dbgt("handling line={}, offset={}B",     \
               m_evt_handler->m_curr->pos.line,    \
               m_evt_handler->m_curr->pos.offset); \
    } while(0)


#if defined(_MSC_VER)
#   pragma warning(push)
#   pragma warning(disable: 4296/*expression is always 'boolean_value'*/)
#   pragma warning(disable: 4702/*unreachable code*/)
#elif defined(__clang__)
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wtype-limits" // to remove a warning on an assertion that a size_t >= 0. Later on, this size_t will turn into a template argument, and then it can become < 0.
#   pragma clang diagnostic ignored "-Wformat-nonliteral"
#   pragma clang diagnostic ignored "-Wold-style-cast"
#elif defined(__GNUC__)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wtype-limits" // to remove a warning on an assertion that a size_t >= 0. Later on, this size_t will turn into a template argument, and then it can become < 0.
#   pragma GCC diagnostic ignored "-Wformat-nonliteral"
#   pragma GCC diagnostic ignored "-Wold-style-cast"
#   if __GNUC__ >= 7
#       pragma GCC diagnostic ignored "-Wduplicated-branches"
#   endif
#endif

// NOLINTBEGIN(hicpp-signed-bitwise,cppcoreguidelines-avoid-goto,hicpp-avoid-goto,hicpp-multiway-paths-covered)

namespace c4 {
namespace yml {

namespace { // NOLINT

C4_HOT C4_ALWAYS_INLINE bool _is_blck_token(csubstr s) noexcept
{
    RYML_ASSERT(s.len > 0);
    RYML_ASSERT(s.str[0] == '-' || s.str[0] == ':' || s.str[0] == '?');
    return ((s.len == 1) || ((s.str[1] == ' ') _RYML_WITH_TAB_TOKENS( || (s.str[1] == '\t'))));
}

inline bool _is_doc_begin_token(csubstr s)
{
    RYML_ASSERT(s.begins_with('-'));
    RYML_ASSERT(!s.ends_with("\n"));
    RYML_ASSERT(!s.ends_with("\r"));
    return (s.len >= 3 && s.str[1] == '-' && s.str[2] == '-')
        && (s.len == 3 || (s.str[3] == ' ' _RYML_WITH_TAB_TOKENS(|| s.str[3] == '\t')));
}

inline bool _is_doc_end_token(csubstr s)
{
    RYML_ASSERT(s.begins_with('.'));
    RYML_ASSERT(!s.ends_with("\n"));
    RYML_ASSERT(!s.ends_with("\r"));
    return (s.len >= 3 && s.str[1] == '.' && s.str[2] == '.')
        && (s.len == 3 || (s.str[3] == ' ' _RYML_WITH_TAB_TOKENS(|| s.str[3] == '\t')));
}

inline bool _is_doc_token(csubstr s) noexcept
{
    //
    // NOTE: this function was failing under some scenarios when
    // compiled with gcc -O2 (but not -O3 or -O1 or -O0), likely
    // related to optimizer assumptions on the input string and
    // possibly caused from UB around assignment to that string (the
    // call site was in _scan_block()). For more details see:
    //
    // https://github.com/biojppm/rapidyaml/issues/440
    //
    // The current version does not suffer this problem, but it may
    // appear again.
    //
    //
    // UPDATE. The problem appeared again in gcc12 and gcc13 with -Os
    // (but not any other optimization level, nor any other compiler
    // or version), because the assignment to s is being hoisted out
    // of the loop which calls this function. Then the length doesn't
    // enter the s.len >= 3 when it should. Adding a
    // C4_DONT_OPTIMIZE(var) makes the problem go away.
    //
    if(s.len >= 3)
    {
        switch(s.str[0])
        {
        case '-':
            //return _is_doc_begin_token(s); // this was failing with gcc -O2
            return (s.str[1] == '-' && s.str[2] == '-')
                && (s.len == 3 || (s.str[3] == ' ' _RYML_WITH_TAB_TOKENS(|| s.str[3] == '\t')));
        case '.':
            //return _is_doc_end_token(s); // this was failing with gcc -O2
            return (s.str[1] == '.' && s.str[2] == '.')
                && (s.len == 3 || (s.str[3] == ' ' _RYML_WITH_TAB_TOKENS(|| s.str[3] == '\t')));
        }
    }
    return false;
}

inline size_t _is_special_json_scalar(csubstr s)
{
    RYML_ASSERT(s.len);
    switch(s.str[0])
    {
    case 'f':
        if(s.len >= 5 && s.begins_with("false"))
            return 5u;
        break;
    case 't':
        if(s.len >= 4 && s.begins_with("true"))
            return 4u;
        break;
    case 'n':
        if(s.len >= 4 && s.begins_with("null"))
            return 4u;
        break;
    }
    return 0u;
}


//-----------------------------------------------------------------------------

C4_ALWAYS_INLINE size_t _extend_from_combined_newline(char nl, char following)
{
    return (nl == '\n' && following == '\r') || (nl == '\r' && following == '\n');
}

//! look for the next newline chars, and jump to the right of those
inline substr from_next_line(substr rem)
{
    size_t nlpos = rem.first_of("\r\n");
    if(nlpos == csubstr::npos)
        return {};
    const char nl = rem[nlpos];
    rem = rem.right_of(nlpos);
    if(rem.empty())
        return {};
    if(_extend_from_combined_newline(nl, rem.front()))
        rem = rem.sub(1);
    return rem;
}


//-----------------------------------------------------------------------------

inline size_t _count_following_newlines(csubstr r, size_t *C4_RESTRICT i)
{
    RYML_ASSERT(r[*i] == '\n');
    size_t numnl_following = 0;
    ++(*i);
    for( ; *i < r.len; ++(*i))
    {
        if(r.str[*i] == '\n')
            ++numnl_following;
        // skip leading whitespace
        else if(r.str[*i] == ' ' || r.str[*i] == '\t' || r.str[*i] == '\r')
            ;
        else
            break;
    }
    return numnl_following;
}

/** @p i is set to the first non whitespace character after the line
 * @return the number of empty lines after the initial position */
inline size_t _count_following_newlines(csubstr r, size_t *C4_RESTRICT i, size_t indentation)
{
    RYML_ASSERT(r[*i] == '\n');
    size_t numnl_following = 0;
    ++(*i);
    if(indentation == 0)
    {
        for( ; *i < r.len; ++(*i))
        {
            if(r.str[*i] == '\n')
                ++numnl_following;
            // skip leading whitespace
            else if(r.str[*i] == ' ' || r.str[*i] == '\t' || r.str[*i] == '\r')
                ;
            else
                break;
        }
    }
    else
    {
        for( ; *i < r.len; ++(*i))
        {
            if(r.str[*i] == '\n')
            {
                ++numnl_following;
                // skip the indentation after the newline
                size_t stop = *i + indentation;
                for( ; *i < r.len; ++(*i))
                {
                    if(r.str[*i] != ' ' && r.str[*i] != '\r')
                        break;
                    RYML_ASSERT(*i < stop);
                }
                C4_UNUSED(stop);
            }
            // skip leading whitespace
            else if(r.str[*i] == ' ' || r.str[*i] == '\t' || r.str[*i] == '\r')
                ;
            else
                break;
        }
    }
    return numnl_following;
}

} // anon namespace


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

template<class EventHandler>
ParseEngine<EventHandler>::~ParseEngine()
{
    _free();
    _clr();
}

template<class EventHandler>
ParseEngine<EventHandler>::ParseEngine(EventHandler *evt_handler, ParserOptions opts)
    : m_options(opts)
    , m_file()
    , m_buf()
    , m_evt_handler(evt_handler)
    , m_pending_anchors()
    , m_pending_tags()
    , m_was_inside_qmrk(false)
    , m_doc_empty(false)
    , m_prev_colon(npos)
    , m_encoding(NOBOM)
    , m_newline_offsets()
    , m_newline_offsets_size(0)
    , m_newline_offsets_capacity(0)
    , m_newline_offsets_buf()
{
    RYML_CHECK(evt_handler);
}

template<class EventHandler>
ParseEngine<EventHandler>::ParseEngine(ParseEngine &&that) noexcept
    : m_options(that.m_options)
    , m_file(that.m_file)
    , m_buf(that.m_buf)
    , m_evt_handler(that.m_evt_handler)
    , m_pending_anchors(that.m_pending_anchors)
    , m_pending_tags(that.m_pending_tags)
    , m_was_inside_qmrk(false)
    , m_doc_empty(false)
    , m_prev_colon(npos)
    , m_encoding(NOBOM)
    , m_newline_offsets(that.m_newline_offsets)
    , m_newline_offsets_size(that.m_newline_offsets_size)
    , m_newline_offsets_capacity(that.m_newline_offsets_capacity)
    , m_newline_offsets_buf(that.m_newline_offsets_buf)
{
    that._clr();
}

template<class EventHandler>
ParseEngine<EventHandler>::ParseEngine(ParseEngine const& that)
    : m_options(that.m_options)
    , m_file(that.m_file)
    , m_buf(that.m_buf)
    , m_evt_handler(that.m_evt_handler)
    , m_pending_anchors(that.m_pending_anchors)
    , m_pending_tags(that.m_pending_tags)
    , m_was_inside_qmrk(false)
    , m_doc_empty(false)
    , m_prev_colon(npos)
    , m_encoding(NOBOM)
    , m_newline_offsets()
    , m_newline_offsets_size()
    , m_newline_offsets_capacity()
    , m_newline_offsets_buf()
{
    if(that.m_newline_offsets_capacity)
    {
        _resize_locations(that.m_newline_offsets_capacity);
        _RYML_CB_CHECK(m_evt_handler->m_stack.m_callbacks, m_newline_offsets_capacity == that.m_newline_offsets_capacity);
        memcpy(m_newline_offsets, that.m_newline_offsets, that.m_newline_offsets_size * sizeof(size_t));
        m_newline_offsets_size = that.m_newline_offsets_size;
    }
}

template<class EventHandler>
ParseEngine<EventHandler>& ParseEngine<EventHandler>::operator=(ParseEngine &&that) noexcept
{
    _free();
    m_options = (that.m_options);
    m_file = (that.m_file);
    m_buf = (that.m_buf);
    m_evt_handler = that.m_evt_handler;
    m_pending_anchors = that.m_pending_anchors;
    m_pending_tags = that.m_pending_tags;
    m_was_inside_qmrk = that.m_was_inside_qmrk;
    m_doc_empty = that.m_doc_empty;
    m_prev_colon = that.m_prev_colon;
    m_encoding = that.m_encoding;
    m_newline_offsets = (that.m_newline_offsets);
    m_newline_offsets_size = (that.m_newline_offsets_size);
    m_newline_offsets_capacity = (that.m_newline_offsets_capacity);
    m_newline_offsets_buf = (that.m_newline_offsets_buf);
    that._clr();
    return *this;
}

template<class EventHandler>
ParseEngine<EventHandler>& ParseEngine<EventHandler>::operator=(ParseEngine const& that)
{
    if(&that != this)
    {
        _free();
        m_options = (that.m_options);
        m_file = (that.m_file);
        m_buf = (that.m_buf);
        m_evt_handler = that.m_evt_handler;
        m_pending_anchors = that.m_pending_anchors;
        m_pending_tags = that.m_pending_tags;
        m_was_inside_qmrk = that.m_was_inside_qmrk;
        m_doc_empty = that.m_doc_empty;
        m_prev_colon = that.m_prev_colon;
        m_encoding = that.m_encoding;
        if(that.m_newline_offsets_capacity > m_newline_offsets_capacity)
            _resize_locations(that.m_newline_offsets_capacity);
        _RYML_CB_CHECK(m_evt_handler->m_stack.m_callbacks, m_newline_offsets_capacity >= that.m_newline_offsets_capacity);
        _RYML_CB_CHECK(m_evt_handler->m_stack.m_callbacks, m_newline_offsets_capacity >= that.m_newline_offsets_size);
        memcpy(m_newline_offsets, that.m_newline_offsets, that.m_newline_offsets_size * sizeof(size_t));
        m_newline_offsets_size = that.m_newline_offsets_size;
        m_newline_offsets_buf = that.m_newline_offsets_buf;
    }
    return *this;
}

template<class EventHandler>
void ParseEngine<EventHandler>::_clr()
{
    m_options = {};
    m_file = {};
    m_buf = {};
    m_evt_handler = {};
    m_pending_anchors = {};
    m_pending_tags = {};
    m_was_inside_qmrk = false;
    m_doc_empty = true;
    m_prev_colon = npos;
    m_encoding = NOBOM;
    m_newline_offsets = {};
    m_newline_offsets_size = {};
    m_newline_offsets_capacity = {};
    m_newline_offsets_buf = {};
}

template<class EventHandler>
void ParseEngine<EventHandler>::_free()
{
    if(m_newline_offsets)
    {
        _RYML_CB_FREE(m_evt_handler->m_stack.m_callbacks, m_newline_offsets, size_t, m_newline_offsets_capacity);
        m_newline_offsets = nullptr;
        m_newline_offsets_size = 0u;
        m_newline_offsets_capacity = 0u;
        m_newline_offsets_buf = nullptr;
    }
}


//-----------------------------------------------------------------------------

template<class EventHandler>
void ParseEngine<EventHandler>::_reset()
{
    m_pending_anchors = {};
    m_pending_tags = {};
    m_doc_empty = true;
    m_was_inside_qmrk = false;
    m_prev_colon = npos;
    m_encoding = NOBOM;
    if(m_options.locations())
    {
        _prepare_locations();
    }
}


//-----------------------------------------------------------------------------

template<class EventHandler>
void ParseEngine<EventHandler>::_relocate_arena(csubstr prev_arena, substr next_arena)
{
    #define _ryml_relocate(s)                                   \
    if((s).is_sub(prev_arena))                                  \
    {                                                           \
        (s).str = next_arena.str + ((s).str - prev_arena.str);  \
    }
    _ryml_relocate(m_buf);
    _ryml_relocate(m_newline_offsets_buf);
    for(size_t i = 0; i < m_pending_tags.num_entries; ++i)
        _ryml_relocate(m_pending_tags.annotations[i].str);
    for(size_t i = 0; i < m_pending_anchors.num_entries; ++i)
        _ryml_relocate(m_pending_anchors.annotations[i].str);
    #undef _ryml_relocate
}

template<class EventHandler>
void ParseEngine<EventHandler>::_s_relocate_arena(void* data, csubstr prev_arena, substr next_arena)
{
    ((ParseEngine*)data)->_relocate_arena(prev_arena, next_arena);
}


//-----------------------------------------------------------------------------

template<class EventHandler>
template<class DumpFn>
void ParseEngine<EventHandler>::_fmt_msg(DumpFn &&dumpfn) const
{
    auto const *const C4_RESTRICT st = m_evt_handler->m_curr;
    auto const& lc = st->line_contents;
    csubstr contents = lc.stripped;
    if(contents.len)
    {
        // print the yaml src line
        size_t offs = 3u + to_chars(substr{}, st->pos.line) + to_chars(substr{}, st->pos.col);
        if(m_file.len)
        {
            detail::_dump(std::forward<DumpFn>(dumpfn), "{}:", m_file);
            offs += m_file.len + 1;
        }
        detail::_dump(std::forward<DumpFn>(dumpfn), "{}:{}: ", st->pos.line, st->pos.col);
        csubstr maybe_full_content = (contents.len < 80u ? contents : contents.first(80u));
        csubstr maybe_ellipsis = (contents.len < 80u ? csubstr{} : csubstr("..."));
        detail::_dump(std::forward<DumpFn>(dumpfn), "{}{}  (size={})\n", maybe_full_content, maybe_ellipsis, contents.len);
        // highlight the remaining portion of the previous line
        size_t firstcol = (size_t)(lc.rem.begin() - lc.full.begin());
        size_t lastcol = firstcol + lc.rem.len;
        for(size_t i = 0; i < offs + firstcol; ++i)
            std::forward<DumpFn>(dumpfn)(" ");
        std::forward<DumpFn>(dumpfn)("^");
        for(size_t i = 1, e = (lc.rem.len < 80u ? lc.rem.len : 80u); i < e; ++i)
            std::forward<DumpFn>(dumpfn)("~");
        detail::_dump(std::forward<DumpFn>(dumpfn), "{}  (cols {}-{})\n", maybe_ellipsis, firstcol+1, lastcol+1);
    }
    else
    {
        std::forward<DumpFn>(dumpfn)("\n");
    }

#ifdef RYML_DBG
    // next line: print the state flags
    {
        char flagbuf_[128];
        detail::_dump(std::forward<DumpFn>(dumpfn), "top state: {}\n", detail::_parser_flags_to_str(flagbuf_, m_evt_handler->m_curr->flags));
    }
#endif
}


//-----------------------------------------------------------------------------

template<class EventHandler>
template<class ...Args>
void ParseEngine<EventHandler>::_err(csubstr fmt, Args const& C4_RESTRICT ...args) const
{
    char errmsg[RYML_ERRMSG_SIZE];
    detail::_SubstrWriter writer(errmsg);
    auto dumpfn = [&writer](csubstr s){ writer.append(s); };
    detail::_dump(dumpfn, fmt, args...);
    writer.append('\n');
    _fmt_msg(dumpfn);
    size_t len = writer.pos < RYML_ERRMSG_SIZE ? writer.pos : RYML_ERRMSG_SIZE;
    m_evt_handler->cancel_parse();
    m_evt_handler->m_stack.m_callbacks.m_error(errmsg, len, m_evt_handler->m_curr->pos, m_evt_handler->m_stack.m_callbacks.m_user_data);
}


//-----------------------------------------------------------------------------
#ifdef RYML_DBG
template<class EventHandler>
template<class ...Args>
void ParseEngine<EventHandler>::_dbg(csubstr fmt, Args const& C4_RESTRICT ...args) const
{
    if(_dbg_enabled())
    {
        auto dumpfn = [](csubstr s){ if(s.str) fwrite(s.str, 1, s.len, stdout); };
        detail::_dump(dumpfn, fmt, args...);
        dumpfn("\n");
        _fmt_msg(dumpfn);
    }
}
#endif


//-----------------------------------------------------------------------------
template<class EventHandler>
bool ParseEngine<EventHandler>::_finished_file() const
{
    bool ret = m_evt_handler->m_curr->pos.offset >= m_buf.len;
    if(ret)
    {
        _c4dbgp("finished file!!!");
    }
    return ret;
}

template<class EventHandler>
C4_HOT C4_ALWAYS_INLINE bool ParseEngine<EventHandler>::_finished_line() const
{
    return m_evt_handler->m_curr->line_contents.rem.empty();
}


//-----------------------------------------------------------------------------

template<class EventHandler>
void ParseEngine<EventHandler>::_maybe_skip_whitespace_tokens()
{
    csubstr rem = m_evt_handler->m_curr->line_contents.rem;
    if(rem.len && (rem.str[0] == ' ' _RYML_WITH_TAB_TOKENS(|| rem.str[0] == '\t')))
    {
        size_t pos = rem.first_not_of(_RYML_WITH_OR_WITHOUT_TAB_TOKENS(" \t", ' '));
        if(pos == npos)
            pos = rem.len; // maybe the line is just all whitespace
        _c4dbgpf("skip {} whitespace characters", pos);
        _line_progressed(pos);
    }
}

template<class EventHandler>
void ParseEngine<EventHandler>::_maybe_skipchars(char c)
{
    csubstr rem = m_evt_handler->m_curr->line_contents.rem;
    if(rem.len && rem.str[0] == c)
    {
        size_t pos = rem.first_not_of(c);
        if(pos == npos)
            pos = rem.len; // maybe the line is just all c
        _c4dbgpf("skip {}x'{}'", pos, c);
        _line_progressed(pos);
    }
}

#ifdef RYML_NO_COVERAGE__TO_BE_DELETED
template<class EventHandler>
void ParseEngine<EventHandler>::_maybe_skipchars_up_to(char c, size_t max_to_skip)
{
    csubstr rem = m_evt_handler->m_curr->line_contents.rem;
    if(rem.len && rem.str[0] == c)
    {
        size_t pos = rem.first_not_of(c);
        if(pos == npos)
            pos = rem.len; // maybe the line is just all c
        if(pos > max_to_skip)
            pos = max_to_skip;
        _c4dbgpf("skip {}x'{}'", pos, c);
        _line_progressed(pos);
    }
}
#endif

template<class EventHandler>
template<size_t N>
void ParseEngine<EventHandler>::_skipchars(const char (&chars)[N])
{
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->line_contents.rem.begins_with_any(chars));
    size_t pos = m_evt_handler->m_curr->line_contents.rem.first_not_of(chars);
    if(pos == npos)
        pos = m_evt_handler->m_curr->line_contents.rem.len; // maybe the line is just whitespace
    _c4dbgpf("skip {} characters", pos);
    _line_progressed(pos);
}

template<class EventHandler>
void ParseEngine<EventHandler>::_skip_comment()
{
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->line_contents.rem.begins_with('#'));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->line_contents.rem.is_sub(m_evt_handler->m_curr->line_contents.full));
    csubstr rem = m_evt_handler->m_curr->line_contents.rem;
    csubstr full = m_evt_handler->m_curr->line_contents.full;
    // raise an error if the comment is not preceded by whitespace
    if(!full.begins_with('#'))
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, rem.str > full.str);
        const char c = full[(size_t)(rem.str - full.str - 1)];
        if(C4_UNLIKELY(c != ' ' && c != '\t'))
            _RYML_CB_ERR(m_evt_handler->m_stack.m_callbacks, "comment not preceded by whitespace");
    }
    else
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, rem.str == full.str);
    }
    _c4dbgpf("comment was '{}'", rem);
    _line_progressed(rem.len);
}

template<class EventHandler>
void ParseEngine<EventHandler>::_maybe_skip_comment()
{
    csubstr s = m_evt_handler->m_curr->line_contents.rem.triml(' ');
    if(s.begins_with('#'))
    {
        _line_progressed((size_t)(s.str - m_evt_handler->m_curr->line_contents.rem.str));
        _skip_comment();
    }
}

template<class EventHandler>
bool ParseEngine<EventHandler>::_maybe_scan_following_colon() noexcept
{
    if(m_evt_handler->m_curr->line_contents.rem.len)
    {
        if(m_evt_handler->m_curr->line_contents.rem.str[0] == ' ' || m_evt_handler->m_curr->line_contents.rem.str[0] == '\t')
        {
            size_t pos = m_evt_handler->m_curr->line_contents.rem.first_not_of(" \t");
            if(pos == npos)
                pos = m_evt_handler->m_curr->line_contents.rem.len; // maybe the line has only spaces
            _c4dbgpf("skip {}x'{}'", pos, ' ');
            _line_progressed(pos);
        }
        if(m_evt_handler->m_curr->line_contents.rem.len && (m_evt_handler->m_curr->line_contents.rem.str[0] == ':'))
        {
            _c4dbgp("found ':' colon next");
            _line_progressed(1);
            return true;
        }
    }
    return false;
}

template<class EventHandler>
bool ParseEngine<EventHandler>::_maybe_scan_following_comma() noexcept
{
    if(m_evt_handler->m_curr->line_contents.rem.len)
    {
        if(m_evt_handler->m_curr->line_contents.rem.str[0] == ' ' || m_evt_handler->m_curr->line_contents.rem.str[0] == '\t')
        {
            size_t pos = m_evt_handler->m_curr->line_contents.rem.first_not_of(" \t");
            if(pos == npos)
                pos = m_evt_handler->m_curr->line_contents.rem.len; // maybe the line has only spaces
            _c4dbgpf("skip {}x'{}'", pos, ' ');
            _line_progressed(pos);
        }
        if(m_evt_handler->m_curr->line_contents.rem.len && (m_evt_handler->m_curr->line_contents.rem.str[0] == ','))
        {
            _c4dbgp("found ',' comma next");
            _line_progressed(1);
            return true;
        }
    }
    return false;
}


//-----------------------------------------------------------------------------

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_scan_anchor()
{
    csubstr s = m_evt_handler->m_curr->line_contents.rem;
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, s.begins_with('&'));
    csubstr anchor = s.range(1, s.first_of(' '));
    _line_progressed(1u + anchor.len);
    _maybe_skipchars(' ');
    return anchor;
}

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_scan_ref_seq()
{
    csubstr s = m_evt_handler->m_curr->line_contents.rem;
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, s.begins_with('*'));
    csubstr ref = s.first(s.first_of(",] :"));
    _line_progressed(ref.len);
    return ref;
}

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_scan_ref_map()
{
    csubstr s = m_evt_handler->m_curr->line_contents.rem;
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, s.begins_with('*'));
    csubstr ref = s.first(s.first_of(",} "));
    _line_progressed(ref.len);
    return ref;
}

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_scan_tag()
{
    csubstr rem = m_evt_handler->m_curr->line_contents.rem.triml(' ');
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, rem.begins_with('!'));
    csubstr t;
    if(rem.begins_with("!!"))
    {
        _c4dbgp("begins with '!!'");
        if(has_any(FLOW))
            t = rem.left_of(rem.first_of(" ,"));
        else
            t = rem.left_of(rem.first_of(' '));
    }
    else if(rem.begins_with("!<"))
    {
        _c4dbgp("begins with '!<'");
        t = rem.left_of(rem.first_of('>'), true);
    }
    #ifdef RYML_NO_COVERAGE__TO_BE_DELETED
    else if(rem.begins_with("!h!"))
    {
        _c4dbgp("begins with '!h!'");
        t = rem.left_of(rem.first_of(' '));
    }
    #endif
    else
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, rem.begins_with('!'));
        _c4dbgp("begins with '!'");
        if(has_any(FLOW))
            t = rem.left_of(rem.first_of(" ,"));
        else
            t = rem.left_of(rem.first_of(' '));
    }
    _line_progressed(t.len);
    _maybe_skip_whitespace_tokens();
    return t;
}


//-----------------------------------------------------------------------------

template<class EventHandler>
bool ParseEngine<EventHandler>::_is_valid_start_scalar_plain_flow(csubstr s)
{
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, !s.empty());

    // it's not a scalar if it starts with any of these characters:
    switch(s.str[0])
    {
    // these are all legal tokens which mean no scalar is starting:
    case '[':
    case ']':
    case '{':
    case '}':
    case '!':
    case '&':
    case '*':
    case '|':
    case '>':
    case '#':
        _c4dbgpf("not a scalar: found non-scalar token '{}'", _c4prc(s.str[0]));
        return false;
    // '-' and ':' are illegal at the beginning if not followed by a scalar character
    case '-':
    case ':':
        if(s.len > 1)
        {
            switch(s.str[1])
            {
            case '\n':
            case '\r':
            case '{':
            case '[':
            //_RYML_WITHOUT_TAB_TOKENS(case '\t'):
                _c4err_("invalid token \":{}\"", _c4prc(s.str[1]));
                break;
            case ' ':
            case '}':
            case ']':
                if(s.str[0] == ':')
                {
                    _c4dbgpf("not a scalar: found non-scalar token '{}{}'", s.str[0], s.str[1]);
                    return false;
                }
                break;
            default:
                break;
            }
        }
        else
        {
            return false;
        }
        break;
    case '?':
        if(s.len > 1)
        {
            switch(s.str[1])
            {
            case ' ':
            case '\n':
            case '\r':
            _RYML_WITHOUT_TAB_TOKENS(case '\t':)
                _c4dbgpf("not a scalar: found non-scalar token '?{}'", _c4prc(s.str[1]));
                return false;
            case '{':
            case '}':
            case '[':
            case ']':
                _c4err_("invalid token \"?{}\"", _c4prc(s.str[1]));
                break;
            default:
                break;
            }
        }
        else
        {
            return false;
        }
        break;
    // everything else is a legal starting character
    default:
        break;
    }

    return true;
}

template<class EventHandler>
bool ParseEngine<EventHandler>::_scan_scalar_plain_seq_flow(ScannedScalar *C4_RESTRICT sc)
{
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RMAP));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(BLCK));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(RSEQ|RSEQIMAP));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(FLOW));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(RVAL));

    substr s = m_evt_handler->m_curr->line_contents.rem;
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, !s.begins_with(' '));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, !s.begins_with('\n'));

    if(!s.len)
        return false;

    if(!_is_valid_start_scalar_plain_flow(s))
        return false;

    _c4dbgp("scanning seqflow scalar...");

    const size_t start_offset = m_evt_handler->m_curr->pos.offset;
    bool needs_filter = false;
    while(true)
    {
        _c4dbgpf("scanning scalar: curr line=[{}]~~~{}~~~", s.len, s);
        for(size_t i = 0; i < s.len; ++i)
        {
            const char c = s.str[i];
            switch(c)
            {
            case ',':
                _c4dbgpf("found terminating character at {}: '{}'", i, c);
                _line_progressed(i);
                if(m_evt_handler->m_curr->pos.offset + i > start_offset)
                {
                    goto ended_scalar;
                }
                else
                {
                    _c4dbgp("at the beginning. no scalar here.");
                    return false;
                }
                break;
            case ']':
                _c4dbgpf("found terminating character at {}: '{}'", i, c);
                _line_progressed(i);
                goto ended_scalar;
                break;
            case '#':
                _c4dbgp("found suspicious '#'");
                if(!i || (s.str[i-1] == ' ' _RYML_WITH_TAB_TOKENS(|| s.str[i-1] == '\t')))
                {
                    _c4dbgpf("found terminating character at {}: '{}'", i, c);
                    _line_progressed(i);
                    goto ended_scalar;
                }
                break;
            case ':':
                _c4dbgp("found suspicious ':'");
                if(s.len > i+1)
                {
                    const char next = s.str[i+1];
                    _c4dbgpf("next char is '{}'", _c4prc(next));
                    if(next == ' ' || next == ',' _RYML_WITH_TAB_TOKENS(|| next == '\t'))
                    {
                        _c4dbgp("map starting!");
                        if(m_evt_handler->m_curr->pos.offset + i > start_offset)
                        {
                            _c4dbgp("scalar finished!");
                            _line_progressed(i);
                            goto ended_scalar;
                        }
                        else
                        {
                            _c4dbgp("at the beginning. no scalar here.");
                            return false;
                        }
                    }
                    else
                    {
                        _c4dbgp("it's a scalar indeed.");
                        ++i; // skip the next char
                    }
                }
                else if(s.len == i+1)
                {
                    _c4dbgp("':' at line end. map starting!");
                    return false;
                }
                break;
            case '[':
            case '{':
            case '}':
                _line_progressed(i);
                _c4err_("invalid character: '{}'", c); // noreturn
            default:
                ;
            }
        }
        _line_progressed(s.len);
        if(!_finished_file())
        {
            _c4dbgp("next line!");
            _line_ended();
            _scan_line();
        }
        else
        {
            _c4dbgp("file finished!");
            goto ended_scalar;
        }
        s = m_evt_handler->m_curr->line_contents.rem;
        needs_filter = true;
    }

ended_scalar:

    sc->scalar = m_buf.range(start_offset, m_evt_handler->m_curr->pos.offset).trimr(_RYML_WITH_OR_WITHOUT_TAB_TOKENS(" \t", ' '));
    sc->needs_filter = needs_filter;

    _c4prscalar("scanned plain scalar", sc->scalar, /*keep_newlines*/true);

    return true;
}

template<class EventHandler>
bool ParseEngine<EventHandler>::_scan_scalar_plain_map_flow(ScannedScalar *C4_RESTRICT sc)
{
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RSEQ) || has_any(RSEQIMAP));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(BLCK));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(RMAP|RSEQIMAP));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(FLOW));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(RKEY|RVAL|QMRK));

    substr s = m_evt_handler->m_curr->line_contents.rem;
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, !s.begins_with(' '));

    if(!s.len)
        return false;

    if(!_is_valid_start_scalar_plain_flow(s))
        return false;

    _c4dbgp("scanning scalar...");

    const size_t start_offset = m_evt_handler->m_curr->pos.offset;
    bool needs_filter = false;
    while(true)
    {
        for(size_t i = 0; i < s.len; ++i)
        {
            const char c = s.str[i];
            switch(c)
            {
            case ',':
            case '}':
                _line_progressed(i);
                _c4dbgpf("found terminating character: '{}'", c);
                goto ended_scalar;
            case ':':
                if(s.len == i+1 || s.str[i+1] == ' ' || s.str[i+1] == ',' || s.str[i+1] == '}' _RYML_WITH_TAB_TOKENS(|| s.str[i+1] == '\t'))
                {
                    _line_progressed(i);
                    _c4dbgpf("found terminating character: '{}'", c);
                    goto ended_scalar;
                }
                break;
            case '{':
            case '[':
                _line_progressed(i);
                _c4err_("invalid character: '{}'", c); // noreturn
                break;
            case ']':
                _line_progressed(i);
                if(has_any(RSEQIMAP))
                    goto ended_scalar;
                else
                    _c4err_("invalid character: '{}'", c); // noreturn
                break;
            case '#':
                if(!i || s.str[i-1] == ' ' _RYML_WITH_TAB_TOKENS(|| s.str[i-1] == '\t'))
                {
                    _line_progressed(i);
                    _c4dbgpf("found terminating character: '{}'", c);
                    goto ended_scalar;
                }
                break;
            default:
                ;
            }
        }
        _c4dbgp("next line!");
        _line_progressed(s.len);
        if(!_finished_file())
        {
            _c4dbgp("next line!");
            _line_ended();
            _scan_line();
        }
        else
        {
            _c4dbgp("file finished!");
            goto ended_scalar;
        }
        s = m_evt_handler->m_curr->line_contents.rem;
        needs_filter = true;
    }

ended_scalar:

    sc->scalar = m_buf.range(start_offset, m_evt_handler->m_curr->pos.offset).trimr(_RYML_WITH_OR_WITHOUT_TAB_TOKENS(" \n\t\r", " \n\r"));
    sc->needs_filter = needs_filter;

    _c4dbgpf("scalar was [{}]~~~{}~~~", sc->scalar.len, sc->scalar);

    return sc->scalar.len > 0u;
}

template<class EventHandler>
bool ParseEngine<EventHandler>::_scan_scalar_seq_json(ScannedScalar *C4_RESTRICT sc)
{
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RMAP));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(BLCK));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(RSEQ));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(FLOW));

    substr s = m_evt_handler->m_curr->line_contents.rem;
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, !s.begins_with(' '));

    if(!s.len)
        return false;

    _c4dbgp("scanning scalar...");

    switch(s.str[0])
    {
    case ']':
    case '{':
    case ',':
        _c4dbgp("not a scalar.");
        return false;
    }

    {
        const size_t len = _is_special_json_scalar(s);
        if(len)
        {
            sc->scalar = s.first(len);
            sc->needs_filter = false;
            _c4dbgpf("special json scalar: '{}'", sc->scalar);
            _line_progressed(len);
            return true;
        }
    }

    // must be a number
    size_t i = 0;
    for( ; i < s.len; ++i)
    {
        const char c = s.str[i];
        switch(c)
        {
        case ',':
        case ']':
        case ' ':
        case '\t':
            _c4dbgpf("found terminating character: '{}'", c);
            goto ended_scalar;
        case '#':
            if(!i || s.str[i-1] == ' ')
            {
                _c4dbgpf("found terminating character: '{}'", c);
                goto ended_scalar;
            }
            break;
        default:
            ;
        }
    }

ended_scalar:

    if(C4_LIKELY(i > 0))
    {
        _line_progressed(i);
        sc->scalar = s.first(i);
        sc->needs_filter = false;
        _c4dbgpf("scalar was [{}]~~~{}~~~", sc->scalar.len, sc->scalar);
        return true;
    }

    return false;
}

template<class EventHandler>
bool ParseEngine<EventHandler>::_scan_scalar_map_json(ScannedScalar *C4_RESTRICT sc)
{
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RSEQ));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(BLCK));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(RMAP));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(FLOW));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(RKEY|RVAL));

    substr s = m_evt_handler->m_curr->line_contents.rem;
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, !s.begins_with(' '));

    if(!s.len)
        return false;

    _c4dbgp("scanning scalar...");

    {
        const size_t len = _is_special_json_scalar(s);
        if(len)
        {
            sc->scalar = s.first(len);
            sc->needs_filter = false;
            _c4dbgpf("special json scalar: '{}'", sc->scalar);
            _line_progressed(len);
            return true;
        }
    }

    // must be a number
    size_t i = 0;
    for( ; i < s.len; ++i)
    {
        const char c = s.str[i];
        switch(c)
        {
        case ',':
        case '}':
        case ' ':
        case '\t':
            _c4dbgpf("found terminating character: '{}'", c);
            goto ended_scalar;
        case '#':
            if(!i || s.str[i-1] == ' ')
            {
                _c4dbgpf("found terminating character: '{}'", c);
                goto ended_scalar;
            }
            break;
        default:
            ;
        }
    }

ended_scalar:

    if(C4_LIKELY(i > 0))
    {
        _line_progressed(i);
        sc->scalar = s.first(i);
        sc->needs_filter = false;
        _c4dbgpf("scalar was [{}]~~~{}~~~", sc->scalar.len, sc->scalar);
        return true;
    }

    return false;
}

template<class EventHandler>
bool ParseEngine<EventHandler>::_is_doc_begin(csubstr s)
{
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, s[0] == '-');
    return (m_evt_handler->m_curr->line_contents.indentation == 0u && _at_line_begin() && _is_doc_begin_token(s));
}

template<class EventHandler>
bool ParseEngine<EventHandler>::_is_doc_end(csubstr s)
{
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, s[0] == '.');
    return (m_evt_handler->m_curr->line_contents.indentation == 0u && _at_line_begin() && _is_doc_end_token(s));
}

template<class EventHandler>
bool ParseEngine<EventHandler>::_scan_scalar_plain_blck(ScannedScalar *C4_RESTRICT sc, size_t indentation)
{
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(FLOW));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RSEQIMAP));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(BLCK|RUNK|USTY));

    substr s = m_evt_handler->m_curr->line_contents.rem;
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, !s.begins_with(' '));

    if(!s.len)
        return false;

    switch(s.str[0])
    {
    case '-':
        if(_is_blck_token(s))
        {
            return false;
        }
        else if(_is_doc_begin(s))
        {
            _c4dbgp("token is doc start");
            return false;
        }
        break;
    case ':':
    case '?':
        if(_is_blck_token(s))
            return false;
        break;
    case '[':
    case '{':
    case '&':
    case '*':
    case '!':
    _RYML_WITH_TAB_TOKENS(case '\t':)
        return false;
    case '.':
        if(_is_doc_end(s))
        {
            _c4dbgp("token is doc end");
            return false;
        }
        break;
    }

    _c4dbgpf("plain scalar! indentation={}", indentation);

    const size_t start_offset = m_evt_handler->m_curr->pos.offset;
    const size_t start_line = m_evt_handler->m_curr->pos.line;

    bool needs_filter = false;
    while(true)
    {
        _c4dbgpf("plain scalar line: [{}]~~~{}~~~", s.len, s);
        for(size_t i = 0; i < s.len; ++i)
        {
            const char curr = s.str[i];
            //_c4dbgpf("[{}]='{}'", i, _c4prc(curr));
            switch(curr)
            {
            case ':':
                _c4dbgpf("[{}]: got suspicious ':'", i);
                // are there more characters?
                if((i + 1 == s.len) || ((s.str[i+1] == ' ') _RYML_WITH_TAB_TOKENS( || (s.str[i+1] == '\t'))))
                {
                    _c4dbgpf("followed by '{}'", i+1 == s.len ? csubstr("\\n") : _c4prc(s.str[i+1]));
                    _line_progressed(i);
                    // ': ' is accepted only on the first line
                    if(C4_LIKELY(m_evt_handler->m_curr->pos.line == start_line))
                    {
                        _c4dbgp("start line. scalar ends here");
                        goto ended_scalar;
                    }
                    else
                    {
                        _c4err("parse error");
                    }
                }
                else
                {
                    size_t j = i;
                    while(j + 1 < s.len && s.str[j+1] == ':')
                    {
                        _c4dbgp("skip colon");
                        ++j;
                    }
                    i = j > i ? j-1 : i;
                    _c4dbgp("nothing to see here");
                }
                break;
            case '#':
                _c4dbgp("got suspicious '#'");
                if(!i || (s.str[i-1] == ' ' || s.str[i-1] == '\t'))
                {
                    _c4dbgp("comment! scalar ends here");
                    _line_progressed(i);
                    goto ended_scalar;
                }
                else
                {
                    _c4dbgp("nothing to see here");
                }
                break;
            }
        }
        _line_progressed(s.len);
        csubstr next_peeked = _peek_next_line(m_evt_handler->m_curr->pos.offset);
        next_peeked = next_peeked.trimr("\n\r");
        const size_t next_indentation = next_peeked.first_not_of(' ');
        _c4dbgpf("indentation curr={} next={}", indentation, next_indentation);
        if(next_indentation < indentation)
        {
            _c4dbgp("smaller indentation! scalar ended");
            goto ended_scalar;
        }
        else if(next_indentation == 0 && next_peeked.len > 0)
        {
            const char first = next_peeked.str[0];
            switch(first)
            {
            case '-':
                next_peeked = next_peeked.trimr("\n\r");
                _c4dbgpf("doc begin? peeked=[{}]~~~{}{}~~~", next_peeked.len, next_peeked.len >= 3 ? next_peeked.first(3) : next_peeked, next_peeked.len > 3 ? "..." : "");
                if(_is_doc_begin_token(next_peeked))
                {
                    _c4dbgp("doc begin! scalar ended");
                    goto ended_scalar;
                }
                break;
            case '.':
                next_peeked = next_peeked.trimr("\n\r");
                _c4dbgpf("doc end? peeked=[{}]~~~{}{}~~~", next_peeked.len, next_peeked.len >= 3 ? next_peeked.first(3) : next_peeked, next_peeked.len > 3 ? "..." : "");
                if(_is_doc_end_token(next_peeked))
                {
                    _c4dbgp("doc end! scalar ended");
                    goto ended_scalar;
                }
                break;
            }
        }
        // load with next line
        _c4dbgp("next line!");
        if(!_finished_file())
        {
            _c4dbgp("next line!");
            _line_ended();
            _scan_line();
        }
        else
        {
            _c4dbgp("file finished!");
            goto ended_scalar;
        }
        s = m_evt_handler->m_curr->line_contents.rem;
        needs_filter = true;
    }

ended_scalar:

    sc->scalar = m_buf.range(start_offset, m_evt_handler->m_curr->pos.offset).trimr(" \n\r\t");
    sc->needs_filter = needs_filter;

    _c4dbgpf("scalar was [{}]~~~{}~~~", sc->scalar.len, sc->scalar);

    return true;
}

template<class EventHandler>
bool ParseEngine<EventHandler>::_scan_scalar_plain_seq_blck(ScannedScalar *C4_RESTRICT sc)
{
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RMAP));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(FLOW));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RSEQIMAP));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(RSEQ));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(BLCK));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(RVAL));
    return _scan_scalar_plain_blck(sc, m_evt_handler->m_curr->indref + 1u);
}

template<class EventHandler>
bool ParseEngine<EventHandler>::_scan_scalar_plain_map_blck(ScannedScalar *C4_RESTRICT sc)
{
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RSEQ));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(FLOW));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(RMAP));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(BLCK));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(RKEY|RVAL|QMRK));
    return _scan_scalar_plain_blck(sc, m_evt_handler->m_curr->indref + 1u);
}

template<class EventHandler>
bool ParseEngine<EventHandler>::_scan_scalar_plain_unk(ScannedScalar *C4_RESTRICT sc)
{
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks,  has_any(RUNK|USTY));
    return _scan_scalar_plain_blck(sc, m_evt_handler->m_curr->indref);
}


//-----------------------------------------------------------------------------

template<class EventHandler>
substr ParseEngine<EventHandler>::_peek_next_line(size_t pos) const
{
    substr rem{}; // declare here because of the goto
    size_t nlpos{}; // declare here because of the goto
    pos = pos == npos ? m_evt_handler->m_curr->pos.offset : pos;
    if(pos >= m_buf.len)
        goto next_is_empty;

    // look for the next newline chars, and jump to the right of those
    rem = from_next_line(m_buf.sub(pos));
    if(rem.empty())
        goto next_is_empty;

    // now get everything up to and including the following newline chars
    nlpos = rem.first_of("\r\n");
    if((nlpos != csubstr::npos) && (nlpos + 1 < rem.len))
        nlpos += _extend_from_combined_newline(rem[nlpos], rem[nlpos+1]);
    rem = rem.left_of(nlpos, /*include_pos*/true);

    _c4dbgpf("peek next line @ {}: (len={})'{}'", pos, rem.len, rem.trimr("\r\n"));
    return rem;

next_is_empty:
    _c4dbgpf("peek next line @ {}: (len=0)''", pos);
    return {};
}

//-----------------------------------------------------------------------------

template<class EventHandler>
void ParseEngine<EventHandler>::_scan_line()
{
    if(C4_LIKELY(m_evt_handler->m_curr->pos.offset < m_buf.len))
        m_evt_handler->m_curr->line_contents.reset_with_next_line(m_buf, m_evt_handler->m_curr->pos.offset);
    else
        m_evt_handler->m_curr->line_contents.reset(m_buf.last(0), m_buf.last(0));
}

template<class EventHandler>
void ParseEngine<EventHandler>::_line_progressed(size_t ahead)
{
    _c4dbgpf("line[{}] ({} cols) progressed by {}:  col {}-->{}   offset {}-->{}", m_evt_handler->m_curr->pos.line, m_evt_handler->m_curr->line_contents.full.len, ahead, m_evt_handler->m_curr->pos.col, m_evt_handler->m_curr->pos.col+ahead, m_evt_handler->m_curr->pos.offset, m_evt_handler->m_curr->pos.offset+ahead);
    m_evt_handler->m_curr->pos.offset += ahead;
    m_evt_handler->m_curr->pos.col += ahead;
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->pos.col <= m_evt_handler->m_curr->line_contents.stripped.len+1);
    m_evt_handler->m_curr->line_contents.rem = m_evt_handler->m_curr->line_contents.rem.sub(ahead);
}

template<class EventHandler>
void ParseEngine<EventHandler>::_line_ended()
{
    _c4dbgpf("line[{}] ({} cols) ended! offset {}-->{} / col {}-->{}",
             m_evt_handler->m_curr->pos.line,
             m_evt_handler->m_curr->line_contents.full.len,
             m_evt_handler->m_curr->pos.offset, m_evt_handler->m_curr->pos.offset + m_evt_handler->m_curr->line_contents.full.len - m_evt_handler->m_curr->line_contents.stripped.len,
             m_evt_handler->m_curr->pos.col, 1);
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->pos.col == m_evt_handler->m_curr->line_contents.stripped.len + 1);
    m_evt_handler->m_curr->pos.offset += m_evt_handler->m_curr->line_contents.full.len - m_evt_handler->m_curr->line_contents.stripped.len;
    ++m_evt_handler->m_curr->pos.line;
    m_evt_handler->m_curr->pos.col = 1;
}

template<class EventHandler>
void ParseEngine<EventHandler>::_line_ended_undo()
{
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->pos.col == 1u);
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->pos.line > 0u);
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->pos.offset >= m_evt_handler->m_curr->line_contents.full.len - m_evt_handler->m_curr->line_contents.stripped.len);
    const size_t delta = m_evt_handler->m_curr->line_contents.full.len - m_evt_handler->m_curr->line_contents.stripped.len;
    _c4dbgpf("line[{}] undo ended! line {}-->{}, offset {}-->{}", m_evt_handler->m_curr->pos.line, m_evt_handler->m_curr->pos.line, m_evt_handler->m_curr->pos.line - 1, m_evt_handler->m_curr->pos.offset, m_evt_handler->m_curr->pos.offset - delta);
    m_evt_handler->m_curr->pos.offset -= delta;
    --m_evt_handler->m_curr->pos.line;
    m_evt_handler->m_curr->pos.col = m_evt_handler->m_curr->line_contents.stripped.len + 1u;
    // don't forget to undo also the changes to the remainder of the line
    //_RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->pos.offset >= m_buf.len || m_buf[m_evt_handler->m_curr->pos.offset] == '\n' || m_buf[m_evt_handler->m_curr->pos.offset] == '\r');
    m_evt_handler->m_curr->line_contents.rem = m_buf.sub(m_evt_handler->m_curr->pos.offset, 0);
}


//-----------------------------------------------------------------------------
template<class EventHandler>
void ParseEngine<EventHandler>::_set_indentation(size_t indentation)
{
    m_evt_handler->m_curr->indref = indentation;
    _c4dbgpf("state[{}]: saving indentation: {}", m_evt_handler->m_curr->level, m_evt_handler->m_curr->indref);
}

template<class EventHandler>
void ParseEngine<EventHandler>::_save_indentation()
{
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->line_contents.rem.begin() >= m_evt_handler->m_curr->line_contents.full.begin());
    m_evt_handler->m_curr->indref = m_evt_handler->m_curr->line_contents.current_col();
    _c4dbgpf("state[{}]: saving indentation: {}", m_evt_handler->m_curr->level, m_evt_handler->m_curr->indref);
}


//-----------------------------------------------------------------------------

template<class EventHandler>
void ParseEngine<EventHandler>::_end_map_blck()
{
    _c4dbgp("mapblck: end");
    if(has_any(RKCL|RVAL))
    {
        _c4dbgp("mapblck: set missing val");
        _handle_annotations_before_blck_val_scalar();
        m_evt_handler->set_val_scalar_plain_empty();
    }
    else if(has_any(QMRK))
    {
        _c4dbgp("mapblck: set missing keyval");
        _handle_annotations_before_blck_key_scalar();
        m_evt_handler->set_key_scalar_plain_empty();
        _handle_annotations_before_blck_val_scalar();
        m_evt_handler->set_val_scalar_plain_empty();
    }
    m_evt_handler->end_map();
}

template<class EventHandler>
void ParseEngine<EventHandler>::_end_seq_blck()
{
    if(has_any(RVAL))
    {
        _c4dbgp("seqblck: set missing val");
        _handle_annotations_before_blck_val_scalar();
        m_evt_handler->set_val_scalar_plain_empty();
    }
    m_evt_handler->end_seq();
}

template<class EventHandler>
void ParseEngine<EventHandler>::_end2_map()
{
    _c4dbgp("map: end");
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(RMAP));
    if(has_any(BLCK))
    {
        _end_map_blck();
    }
    else
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(FLOW));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(USTY));
        m_evt_handler->_pop();
    }
}

template<class EventHandler>
void ParseEngine<EventHandler>::_end2_seq()
{
    _c4dbgp("seq: end");
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(RSEQ));
    if(has_any(BLCK))
    {
        _end_seq_blck();
    }
    else
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(FLOW));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(USTY));
        m_evt_handler->_pop();
    }
}

template<class EventHandler>
void ParseEngine<EventHandler>::_begin2_doc()
{
    m_doc_empty = true;
    add_flags(RDOC);
    m_evt_handler->begin_doc();
    m_evt_handler->m_curr->indref = 0; // ?
}

template<class EventHandler>
void ParseEngine<EventHandler>::_begin2_doc_expl()
{
    m_doc_empty = true;
    add_flags(RDOC);
    m_evt_handler->begin_doc_expl();
    m_evt_handler->m_curr->indref = 0; // ?
}

template<class EventHandler>
void ParseEngine<EventHandler>::_end2_doc()
{
    _c4dbgp("doc: end");
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(RDOC));
    if(m_doc_empty || (m_pending_tags.num_entries || m_pending_anchors.num_entries))
    {
        _c4dbgp("doc was empty; add empty val");
        _handle_annotations_before_blck_val_scalar();
        m_evt_handler->set_val_scalar_plain_empty();
    }
    m_evt_handler->end_doc();
}

template<class EventHandler>
void ParseEngine<EventHandler>::_end2_doc_expl()
{
    _c4dbgp("doc: end");
    if(m_doc_empty || (m_pending_tags.num_entries || m_pending_anchors.num_entries))
    {
        _c4dbgp("doc: no children; add empty val");
        _handle_annotations_before_blck_val_scalar();
        m_evt_handler->set_val_scalar_plain_empty();
    }
    m_evt_handler->end_doc_expl();
}

template<class EventHandler>
void ParseEngine<EventHandler>::_maybe_begin_doc()
{
    if(has_none(RDOC))
    {
        _c4dbgp("doc must be started");
        _begin2_doc();
    }
}
template<class EventHandler>
void ParseEngine<EventHandler>::_maybe_end_doc()
{
    if(has_any(RDOC))
    {
        _c4dbgp("doc must be finished");
        _end2_doc();
    }
    else if(m_doc_empty && (m_pending_tags.num_entries || m_pending_anchors.num_entries))
    {
        _c4dbgp("no doc to finish, but pending annotations");
        m_evt_handler->begin_doc();
        _handle_annotations_before_blck_val_scalar();
        m_evt_handler->set_val_scalar_plain_empty();
        m_evt_handler->end_doc();
    }
}

template<class EventHandler>
void ParseEngine<EventHandler>::_end_doc_suddenly__pop()
{
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_stack.size() >= 1);
    if(m_evt_handler->m_stack[0].flags & RDOC)
    {
        _c4dbgp("root is RDOC");
        if(m_evt_handler->m_curr->level != 0)
            _handle_indentation_pop(&m_evt_handler->m_stack[0]);
    }
    else if((m_evt_handler->m_stack.size() > 1) && (m_evt_handler->m_stack[1].flags & RDOC))
    {
        _c4dbgp("root is STREAM");
        if(m_evt_handler->m_curr->level != 1)
            _handle_indentation_pop(&m_evt_handler->m_stack[1]);
    }
    else
    {
        _c4err("internal error");
    }
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(RDOC));
}

template<class EventHandler>
void ParseEngine<EventHandler>::_end_doc_suddenly()
{
    _c4dbgp("end doc suddenly");
    _end_doc_suddenly__pop();
    _end2_doc_expl();
    addrem_flags(RUNK|RTOP|NDOC, RMAP|RSEQ|RDOC);
}

template<class EventHandler>
void ParseEngine<EventHandler>::_start_doc_suddenly()
{
    _c4dbgp("start doc suddenly");
    _end_doc_suddenly__pop();
    _end2_doc();
    _begin2_doc_expl();
}

template<class EventHandler>
void ParseEngine<EventHandler>::_end_stream()
{
    _c4dbgpf("end_stream, level={} node_id={}", m_evt_handler->m_curr->level, m_evt_handler->m_curr->node_id);
    if(has_all(RSEQ|FLOW))
        _c4err("missing terminating ]");
    else if(has_all(RMAP|FLOW))
        _c4err("missing terminating }");
    if(m_evt_handler->m_stack.size() > 1)
        _handle_indentation_pop(m_evt_handler->m_stack.begin());
    if(has_all(RDOC))
    {
        _end2_doc();
    }
    else if(has_all(RTOP|RUNK))
    {
        if(m_pending_anchors.num_entries || m_pending_tags.num_entries)
        {
            if(m_doc_empty)
            {
                m_evt_handler->begin_doc();
                _handle_annotations_before_blck_val_scalar();
                m_evt_handler->set_val_scalar_plain_empty();
                m_evt_handler->end_doc();
            }
        }
    }
    m_evt_handler->end_stream();
}


template<class EventHandler>
void ParseEngine<EventHandler>::_handle_indentation_pop(ParserState const* popto)
{
    _c4dbgpf("popping {} level{}: from level {}(@ind={}) to level {}(@ind={})", m_evt_handler->m_curr->level - popto->level, (((m_evt_handler->m_curr->level - popto->level) > 1) ? "s" : ""), m_evt_handler->m_curr->level, m_evt_handler->m_curr->indref, popto->level, popto->indref);
    while(m_evt_handler->m_curr != popto)
    {
        if(has_any(RSEQ))
        {
            _c4dbgpf("popping seq at level {} (indentation={},addr={})", m_evt_handler->m_curr->level, m_evt_handler->m_curr->indref, m_evt_handler->m_curr);
            _end2_seq();
        }
        else if(has_any(RMAP))
        {
            _c4dbgpf("popping map at level {} (indentation={},addr={})", m_evt_handler->m_curr->level, m_evt_handler->m_curr->indref, m_evt_handler->m_curr);
            _end2_map();
        }
        else
        {
            break;
        }
    }
    _c4dbgpf("current level is {} (indentation={})", m_evt_handler->m_curr->level, m_evt_handler->m_curr->indref);
}

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_indentation_pop_from_block_seq()
{
    // search the stack frame to jump to based on its indentation
    using state_type = typename EventHandler::state;
    state_type const* popto = nullptr;
    auto &stack = m_evt_handler->m_stack;
    _RYML_CB_ASSERT(stack.m_callbacks, stack.is_contiguous()); // this search relies on the stack being contiguous
    _RYML_CB_ASSERT(stack.m_callbacks, m_evt_handler->m_curr >= stack.begin() && m_evt_handler->m_curr < stack.end());
    const size_t ind = m_evt_handler->m_curr->line_contents.indentation;
    #ifdef RYML_DBG
    if(_dbg_enabled())
    {
        char flagbuf_[128];
        for(state_type const& s : stack)
            _dbg_printf("state[{}]: ind={} node={} flags={}\n", s.level, s.indref, s.node_id, detail::_parser_flags_to_str(flagbuf_, s.flags));
    }
    #endif
    for(state_type const* s = m_evt_handler->m_curr-1; s >= stack.begin(); --s)
    {
        _c4dbgpf("searching for state with indentation {}. curr={} (level={},node={})", ind, s->indref, s->level, s->node_id);
        if(s->indref == ind)
        {
            _c4dbgpf("gotit!!! level={} node={}", s->level, s->node_id);
            popto = s;
            break;
        }
    }
    if(!popto || popto >= m_evt_handler->m_curr || popto->level >= m_evt_handler->m_curr->level)
    {
        _c4err("parse error: incorrect indentation?");
    }
    _handle_indentation_pop(popto);
}

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_indentation_pop_from_block_map()
{
    // search the stack frame to jump to based on its indentation
    using state_type = typename EventHandler::state;
    auto &stack = m_evt_handler->m_stack;
    _RYML_CB_ASSERT(stack.m_callbacks, stack.is_contiguous()); // this search relies on the stack being contiguous
    _RYML_CB_ASSERT(stack.m_callbacks, m_evt_handler->m_curr >= stack.begin() && m_evt_handler->m_curr < stack.end());
    const size_t ind = m_evt_handler->m_curr->line_contents.indentation;
    state_type const* popto = nullptr;
    #ifdef RYML_DBG
    char flagbuf_[128];
    if(_dbg_enabled())
    {
        for(state_type const& s : stack)
            _dbg_printf("state[{}]: ind={} node={} flags={}\n", s.level, s.indref, s.node_id, detail::_parser_flags_to_str(flagbuf_, s.flags));
    }
    #endif
    for(state_type const* s = m_evt_handler->m_curr-1; s > stack.begin(); --s) // never go to the stack bottom. that's the root
    {
        _c4dbgpf("searching for state with indentation {}. current: ind={},level={},node={},flags={}", ind, s->indref, s->level, s->node_id, detail::_parser_flags_to_str(flagbuf_, s->flags));
        if(s->indref < ind)
        {
            break;
        }
        else if(s->indref == ind)
        {
            _c4dbgpf("same indentation!!! level={} node={}", s->level, s->node_id);
            if(popto && has_any(RTOP, s) && has_none(RMAP|RSEQ, s))
            {
                break;
            }
            popto = s;
            if(has_all(RSEQ|BLCK, s))
            {
                csubstr rem = m_evt_handler->m_curr->line_contents.rem;
                const size_t first = rem.first_not_of(' ');
                _RYML_CB_ASSERT(stack.m_callbacks, first == ind || first == npos);
                rem = rem.right_of(first, true);
                _c4dbgpf("indentless? rem='{}' first={}", rem, first);
                if(rem.begins_with('-') && _is_blck_token(rem))
                {
                    _c4dbgp("parent was indentless seq");
                    break;
                }
            }
        }
    }
    if(!popto || popto >= m_evt_handler->m_curr || popto->level >= m_evt_handler->m_curr->level)
    {
        _c4err("parse error: incorrect indentation?");
    }
    _handle_indentation_pop(popto);
}


//-----------------------------------------------------------------------------
template<class EventHandler>
typename ParseEngine<EventHandler>::ScannedScalar ParseEngine<EventHandler>::_scan_scalar_squot()
{
    // quoted scalars can spread over multiple lines!
    // nice explanation here: http://yaml-multiline.info/

    // a span to the end of the file
    size_t b = m_evt_handler->m_curr->pos.offset;
    substr s = m_buf.sub(b);
    if(s.begins_with(' '))
    {
        s = s.triml(' ');
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, m_buf.sub(b).is_super(s));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, s.begin() >= m_buf.sub(b).begin());
        _line_progressed((size_t)(s.begin() - m_buf.sub(b).begin()));
    }
    b = m_evt_handler->m_curr->pos.offset; // take this into account
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, s.begins_with('\''));

    // skip the opening quote
    _line_progressed(1);
    s = s.sub(1);

    bool needs_filter = false;

    size_t numlines = 1; // we already have one line
    size_t pos = npos; // find the pos of the matching quote
    while( ! _finished_file())
    {
        const csubstr line = m_evt_handler->m_curr->line_contents.rem;
        bool line_is_blank = true;
        _c4dbgpf("scanning single quoted scalar @ line[{}]: ~~~{}~~~", m_evt_handler->m_curr->pos.line, line);
        for(size_t i = 0; i < line.len; ++i)
        {
            const char curr = line.str[i];
            if(curr == '\'') // single quotes are escaped with two single quotes
            {
                const char next = i+1 < line.len ? line.str[i+1] : '~';
                if(next != '\'') // so just look for the first quote
                {                // without another after it
                    pos = i;
                    break;
                }
                else
                {
                    needs_filter = true; // needs filter to remove escaped quotes
                    ++i; // skip the escaped quote
                }
            }
            else if(curr != ' ')
            {
                line_is_blank = false;
            }
        }

        // leading whitespace also needs filtering
        needs_filter = needs_filter
            || (numlines > 1)
            || line_is_blank
            || (_at_line_begin() && line.begins_with(' '));

        if(pos == npos)
        {
            _line_progressed(line.len);
            ++numlines;
        }
        else
        {
            _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, pos >= 0 && pos < m_buf.len);
            _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, m_buf[m_evt_handler->m_curr->pos.offset + pos] == '\'');
            _line_progressed(pos + 1); // progress beyond the quote
            pos = m_evt_handler->m_curr->pos.offset - b - 1; // but we stop before it
            break;
        }

        _line_ended();
        _scan_line();
    }

    if(pos == npos)
    {
        _c4err("reached end of file while looking for closing quote");
    }
    else
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, pos > 0);
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, s.end() >= m_buf.begin() && s.end() <= m_buf.end());
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, s.end() == m_buf.end() || *s.end() == '\'');
        s = s.sub(0, pos-1);
    }

    _c4prscalar("scanned squoted scalar", s, /*keep_newlines*/true);

    return ScannedScalar { s, needs_filter };
}


//-----------------------------------------------------------------------------
template<class EventHandler>
typename ParseEngine<EventHandler>::ScannedScalar ParseEngine<EventHandler>::_scan_scalar_dquot()
{
    // quoted scalars can spread over multiple lines!
    // nice explanation here: http://yaml-multiline.info/

    // a span to the end of the file
    size_t b = m_evt_handler->m_curr->pos.offset;
    substr s = m_buf.sub(b);
    if(s.begins_with(' '))
    {
        s = s.triml(' ');
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, m_buf.sub(b).is_super(s));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, s.begin() >= m_buf.sub(b).begin());
        _line_progressed((size_t)(s.begin() - m_buf.sub(b).begin()));
    }
    b = m_evt_handler->m_curr->pos.offset; // take this into account
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, s.begins_with('"'));

    // skip the opening quote
    _line_progressed(1);
    s = s.sub(1);

    bool needs_filter = false;

    size_t numlines = 1; // we already have one line
    size_t pos = npos; // find the pos of the matching quote
    while( ! _finished_file())
    {
        const csubstr line = m_evt_handler->m_curr->line_contents.rem;
        #if defined(__GNUC__) && __GNUC__ == 11
        C4_DONT_OPTIMIZE(line); // prevent erroneous hoist of the assignment out of the loop
        #endif
        bool line_is_blank = true;
        _c4dbgpf("scanning double quoted scalar @ line[{}]:  line='{}'", m_evt_handler->m_curr->pos.line, line);
        for(size_t i = 0; i < line.len; ++i)
        {
            const char curr = line.str[i];
            if(curr != ' ')
                line_is_blank = false;
            // every \ is an escape
            if(curr == '\\')
            {
                const char next = i+1 < line.len ? line.str[i+1] : '~';
                needs_filter = true;
                if(next == '"' || next == '\\')
                    ++i;
            }
            else if(curr == '"')
            {
                pos = i;
                break;
            }
        }

        // leading whitespace also needs filtering
        needs_filter = needs_filter
            || (numlines > 1)
            || line_is_blank
            || (_at_line_begin() && line.begins_with(' '));

        if(pos == npos)
        {
            _line_progressed(line.len);
            ++numlines;
        }
        else
        {
            _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, pos >= 0 && pos < m_buf.len);
            _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, m_buf[m_evt_handler->m_curr->pos.offset + pos] == '"');
            _line_progressed(pos + 1); // progress beyond the quote
            pos = m_evt_handler->m_curr->pos.offset - b - 1; // but we stop before it
            break;
        }

        _line_ended();
        _scan_line();
    }

    if(pos == npos)
    {
        _c4err("reached end of file looking for closing quote");
    }
    else
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, pos > 0);
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, s.end() == m_buf.end() || *s.end() == '"');
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, s.end() >= m_buf.begin() && s.end() <= m_buf.end());
        s = s.sub(0, pos-1);
    }

    _c4prscalar("scanned dquoted scalar", s, /*keep_newlines*/true);

    return ScannedScalar { s, needs_filter };
}


//-----------------------------------------------------------------------------
template<class EventHandler>
void ParseEngine<EventHandler>::_scan_block(ScannedBlock *C4_RESTRICT sb, size_t indref)
{
    _c4dbgpf("blck: indref={}", indref);
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, indref != npos);

    // nice explanation here: http://yaml-multiline.info/
    csubstr s = m_evt_handler->m_curr->line_contents.rem;
    csubstr trimmed = s.triml(' ');
    if(trimmed.str > s.str)
    {
        _c4dbgp("skipping whitespace");
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, trimmed.str >= s.str);
        _line_progressed(static_cast<size_t>(trimmed.str - s.str));
        s = trimmed;
    }
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, s.begins_with('|') || s.begins_with('>'));

    _c4dbgpf("blck: specs=[{}]~~~{}~~~", s.len, s);

    // parse the spec
    BlockChomp_e chomp = CHOMP_CLIP; // default to clip unless + or - are used
    size_t indentation = npos; // have to find out if no spec is given
    csubstr digits;
    if(s.len > 1)
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, s.begins_with_any("|>"));
        csubstr t = s.sub(1);
        _c4dbgpf("blck: spec is multichar: '{}'", t);
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, t.len >= 1);
        size_t pos = t.first_of("-+");
        _c4dbgpf("blck: spec chomp char at {}", pos);
        if(pos != npos)
        {
            if(t[pos] == '-')
                chomp = CHOMP_STRIP;
            else if(t[pos] == '+')
                chomp = CHOMP_KEEP;
            if(pos == 0)
                t = t.sub(1);
            else
                t = t.first(pos);
        }
        // from here to the end, only digits are considered
        digits = t.left_of(t.first_not_of("0123456789"));
        if( ! digits.empty())
        {
            if(C4_UNLIKELY(digits.len > 1))
                _c4err("parse error: invalid indentation");
            _c4dbgpf("blck: parse indentation digits: [{}]~~~{}~~~", digits.len, digits);
            if(C4_UNLIKELY( ! c4::atou(digits, &indentation)))
                _c4err("parse error: could not read indentation as decimal");
            if(C4_UNLIKELY( ! indentation))
                _c4err("parse error: null indentation");
            _c4dbgpf("blck: indentation specified: {}. add {} from curr state -> {}", indentation, m_evt_handler->m_curr->indref, indentation+indref);
            indentation += m_evt_handler->m_curr->indref;
        }
    }

    _c4dbgpf("blck: style={}  chomp={}  indentation={}", s.begins_with('>') ? "fold" : "literal", chomp==CHOMP_CLIP ? "clip" : (chomp==CHOMP_STRIP ? "strip" : "keep"), indentation);

    // finish the current line
    _line_progressed(s.len);
    _line_ended();
    _scan_line();

    // start with a zero-length block, already pointing at the right place
    substr raw_block(m_buf.data() + m_evt_handler->m_curr->pos.offset, size_t(0));// m_evt_handler->m_curr->line_contents.full.sub(0, 0);
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, raw_block.begin() == m_evt_handler->m_curr->line_contents.full.begin());

    // read every full line into a raw block,
    // from which newlines are to be stripped as needed.
    //
    // If no explicit indentation was given, pick it from the first
    // non-empty line. See
    // https://yaml.org/spec/1.2.2/#8111-block-indentation-indicator
    size_t num_lines = 0;
    size_t first = m_evt_handler->m_curr->pos.line;
    size_t provisional_indentation = npos;
    LineContents lc;
    while(( ! _finished_file()))
    {
        // peek next line, but do not advance immediately
        lc.reset_with_next_line(m_buf, m_evt_handler->m_curr->pos.offset);
        #if defined(__GNUC__) && (__GNUC__ == 12 || __GNUC__ == 13)
        C4_DONT_OPTIMIZE(lc.rem);
        #endif
        _c4dbgpf("blck: peeking at [{}]~~~{}~~~", lc.stripped.len, lc.stripped);
        // evaluate termination conditions
        if(indentation != npos)
        {
            _c4dbgpf("blck: indentation={}", indentation);
            // stop when the line is deindented and not empty
            if(lc.indentation < indentation && ( ! lc.rem.trim(" \t").empty()))
            {
                if(raw_block.len)
                {
                    _c4dbgpf("blck: indentation decreased ref={} thisline={}", indentation, lc.indentation);
                }
                else
                {
                    _c4err("indentation decreased without any scalar");
                }
                break;
            }
            else if(indentation == 0)
            {
                _c4dbgpf("blck: noindent. lc.rem=[{}]~~~{}~~~", lc.rem.len, lc.rem);
                if(_is_doc_token(lc.rem))
                {
                    _c4dbgp("blck: stop. indentation=0 and doc ended");
                    break;
                }
            }
        }
        else
        {
            const size_t fns = lc.stripped.first_not_of(' ');
            _c4dbgpf("blck: indentation ref not set. firstnonws={}", fns);
            if(fns != npos) // non-empty line
            {
                _RYML_WITH_TAB_TOKENS(
                    if(C4_UNLIKELY(lc.stripped.begins_with('\t')))
                        _c4err("parse error");
                )
                _c4dbgpf("blck: line not empty. indref={} indprov={} indentation={}", indref, provisional_indentation, lc.indentation);
                if(provisional_indentation == npos)
                {
                    if(lc.indentation < indref)
                    {
                        _c4dbgpf("blck: block terminated indentation={} < indref={}", lc.indentation, indref);
                        if(raw_block.len == 0)
                        {
                            _c4dbgp("blck: was empty, undo next line");
                            _line_ended_undo();
                        }
                        break;
                    }
                    else if(lc.indentation == m_evt_handler->m_curr->indref)
                    {
                        if(has_any(RSEQ|RMAP))
                        {
                            _c4dbgpf("blck: block terminated. reading container and indentation={}==indref={}", lc.indentation, m_evt_handler->m_curr->indref);
                            break;
                        }
                    }
                    _c4dbgpf("blck: set indentation ref from this line: ref={}", lc.indentation);
                    indentation = lc.indentation;
                }
                else
                {
                    if(lc.indentation >= provisional_indentation)
                    {
                        _c4dbgpf("blck: set indentation ref from provisional indentation: provisional_ref={}, thisline={}", provisional_indentation, lc.indentation);
                        //indentation = provisional_indentation ? provisional_indentation : lc.indentation;
                        indentation = lc.indentation;
                    }
                    else
                    {
                        break;
                        //_c4err("parse error: first non-empty block line should have at least the original indentation");
                    }
                }
            }
            else // empty line
            {
                _c4dbgpf("blck: line empty or {} spaces. line_indentation={} prov_indentation={}", lc.stripped.len, lc.indentation, provisional_indentation);
                if(provisional_indentation != npos)
                {
                    if(lc.stripped.len >= provisional_indentation)
                    {
                        _c4dbgpf("blck: increase provisional_ref {} -> {}", provisional_indentation, lc.stripped.len);
                        provisional_indentation = lc.stripped.len;
                    }
                    #ifdef RYML_NO_COVERAGE__TO_BE_DELETED
                    else if(lc.indentation >= provisional_indentation && lc.indentation != npos)
                    {
                        _c4dbgpf("blck: increase provisional_ref {} -> {}", provisional_indentation, lc.indentation);
                        provisional_indentation = lc.indentation;
                    }
                    #endif
                }
                else
                {
                    provisional_indentation = lc.indentation ? lc.indentation : has_any(RSEQ|RVAL);
                    _c4dbgpf("blck: initialize provisional_ref={}", provisional_indentation);
                    if(provisional_indentation == npos)
                    {
                        provisional_indentation = lc.stripped.len ? lc.stripped.len : has_any(RSEQ|RVAL);
                        _c4dbgpf("blck: initialize provisional_ref={}", provisional_indentation);
                    }
                    if(provisional_indentation < indref)
                    {
                        provisional_indentation = indref;
                        _c4dbgpf("blck: initialize provisional_ref={}", provisional_indentation);
                    }
                }
            }
        }
        // advance now that we know the folded scalar continues
        m_evt_handler->m_curr->line_contents = lc;
        _c4dbgpf("blck: append '{}'", m_evt_handler->m_curr->line_contents.rem);
        raw_block.len += m_evt_handler->m_curr->line_contents.full.len;
        _line_progressed(m_evt_handler->m_curr->line_contents.rem.len);
        _line_ended();
        ++num_lines;
    }
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->pos.line == (first + num_lines) || (raw_block.len == 0));
    C4_UNUSED(num_lines);
    C4_UNUSED(first);

    if(indentation == npos)
    {
        _c4dbgpf("blck: set indentation from provisional: {}", provisional_indentation);
        indentation = provisional_indentation;
    }

    if(num_lines)
        _line_ended_undo();

    _c4prscalar("scanned block", raw_block, /*keep_newlines*/true);

    sb->scalar = raw_block;
    sb->indentation = indentation;
    sb->chomp = chomp;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
/** @cond dev */

// a debugging scaffold:
#if 0
#define _c4dbgfws(fmt, ...) _c4dbgpf("filt_ws[{}->{}]: " fmt, proc.rpos, proc.wpos, __VA_ARGS__)
#else
#define _c4dbgfws(...)
#endif

template<class EventHandler>
template<class FilterProcessor>
bool ParseEngine<EventHandler>::_filter_ws_handle_to_first_non_space(FilterProcessor &proc)
{
    _c4dbgfws("found whitespace '{}'", _c4prc(proc.curr()));
    _RYML_CB_ASSERT(this->callbacks(), proc.curr() == ' ' || proc.curr() == '\t');

    const size_t first_pos = proc.rpos > 0 ? proc.src.first_not_of(" \t", proc.rpos) : proc.src.first_not_of(' ', proc.rpos);
    if(first_pos != npos)
    {
        const char first_char = proc.src[first_pos];
        _c4dbgfws("firstnonws='{}'@{}", _c4prc(first_char), first_pos);
        if(first_char == '\n' || first_char == '\r') // skip trailing whitespace
        {
            _c4dbgfws("whitespace is trailing on line", "");
            proc.skip(first_pos - proc.rpos);
        }
        else // a legit whitespace
        {
            proc.copy();
            _c4dbgfws("legit whitespace. sofar=[{}]~~~{}~~~", proc.wpos, proc.sofar());
        }
        return true;
    }
    _c4dbgfws("whitespace is trailing on line", "");
    return false;
}

template<class EventHandler>
template<class FilterProcessor>
void ParseEngine<EventHandler>::_filter_ws_copy_trailing(FilterProcessor &proc)
{
    if(!_filter_ws_handle_to_first_non_space(proc))
    {
        _c4dbgfws("... everything else is trailing whitespace - copy {} chars", proc.src.len - proc.rpos);
        proc.copy(proc.src.len - proc.rpos);
    }
}

template<class EventHandler>
template<class FilterProcessor>
void ParseEngine<EventHandler>::_filter_ws_skip_trailing(FilterProcessor &proc)
{
    if(!_filter_ws_handle_to_first_non_space(proc))
    {
        _c4dbgfws("... everything else is trailing whitespace - skip {} chars", proc.src.len - proc.rpos);
        proc.skip(proc.src.len - proc.rpos);
    }
}

#undef _c4dbgfws


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
/* plain scalars */

// a debugging scaffold:
#if 0
#define _c4dbgfps(fmt, ...) _c4dbgpf("filt_plain[{}->{}]: " fmt, proc.rpos, proc.wpos, __VA_ARGS__)
#else
#define _c4dbgfps(fmt, ...)
#endif

template<class EventHandler>
template<class FilterProcessor>
void ParseEngine<EventHandler>::_filter_nl_plain(FilterProcessor &C4_RESTRICT proc, size_t indentation)
{
    _RYML_CB_ASSERT(this->callbacks(), proc.curr() == '\n');

    _c4dbgfps("found newline. sofar=[{}]~~~{}~~~", proc.wpos, proc.sofar());
    size_t ii = proc.rpos;
    const size_t numnl_following = _count_following_newlines(proc.src, &ii, indentation);
    if(numnl_following)
    {
        proc.set('\n', numnl_following);
        _c4dbgfps("{} consecutive (empty) lines {}. totalws={}", 1+numnl_following, ii < proc.src.len ? "in the middle" : "at the end", proc.rpos-ii);
    }
    else
    {
        const size_t ret = proc.src.first_not_of(" \t", proc.rpos+1);
        if(ret != npos)
        {
            proc.set(' ');
             _c4dbgfps("single newline. convert to space. ret={}/{}. sofar=[{}]~~~{}~~~", ii, proc.src.len, proc.wpos, proc.sofar());
        }
        else
        {
            _c4dbgfps("last newline, everything else is whitespace. ii={}/{}", ii, proc.src.len);
            ii = proc.src.len;
        }
    }
    proc.rpos = ii;
}

template<class EventHandler>
template<class FilterProcessor>
auto ParseEngine<EventHandler>::_filter_plain(FilterProcessor &C4_RESTRICT proc, size_t indentation) -> decltype(proc.result())
{
    _RYML_CB_ASSERT(this->callbacks(), indentation != npos);
    _c4dbgfps("before=[{}]~~~{}~~~", proc.src.len, proc.src);

    while(proc.has_more_chars())
    {
        const char curr = proc.curr();
        _c4dbgfps("'{}', sofar=[{}]~~~{}~~~", _c4prc(curr), proc.wpos, proc.sofar());
        switch(curr)
        {
        case ' ':
        _RYML_WITH_TAB_TOKENS(case '\t':)
            _c4dbgfps("whitespace", curr);
            _filter_ws_skip_trailing(proc);
            break;
        case '\n':
            _c4dbgfps("newline", curr);
            _filter_nl_plain(proc, /*indentation*/indentation);
            break;
        case '\r':  // skip \r --- https://stackoverflow.com/questions/1885900
            _c4dbgfps("carriage return, ignore", curr);
            proc.skip();
            break;
        default:
            proc.copy();
            break;
        }
    }

    _c4dbgfps("after[{}]=~~~{}~~~", proc.wpos, proc.sofar());

    return proc.result();
}

#undef _c4dbgfps


template<class EventHandler>
FilterResult ParseEngine<EventHandler>::filter_scalar_plain(csubstr scalar, substr dst, size_t indentation)
{
    FilterProcessorSrcDst proc(scalar, dst);
    return _filter_plain(proc, indentation);
}

template<class EventHandler>
FilterResult ParseEngine<EventHandler>::filter_scalar_plain_in_place(substr dst, size_t cap, size_t indentation)
{
    FilterProcessorInplaceEndExtending proc(dst, cap);
    return _filter_plain(proc, indentation);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
/* single quoted */

// a debugging scaffold:
#if 0
#define _c4dbgfsq(fmt, ...) _c4dbgpf("filt_squo[{}->{}]: " fmt, proc.rpos, proc.wpos, __VA_ARGS__)
#else
#define _c4dbgfsq(fmt, ...)
#endif

template<class EventHandler>
template<class FilterProcessor>
void ParseEngine<EventHandler>::_filter_nl_squoted(FilterProcessor &C4_RESTRICT proc)
{
    _RYML_CB_ASSERT(this->callbacks(), proc.curr() == '\n');

    _c4dbgfsq("found newline. sofar=[{}]~~~{}~~~", proc.wpos, proc.sofar());
    size_t ii = proc.rpos;
    const size_t numnl_following = _count_following_newlines(proc.src, &ii);
    if(numnl_following)
    {
        proc.set('\n', numnl_following);
        _c4dbgfsq("{} consecutive (empty) lines {}. totalws={}", 1+numnl_following, ii < proc.src.len ? "in the middle" : "at the end", proc.rpos-ii);
    }
    else
    {
        const size_t ret = proc.src.first_not_of(" \t", proc.rpos+1);
        if(ret != npos)
        {
            proc.set(' ');
            _c4dbgfsq("single newline. convert to space. ret={}/{}. sofar=[{}]~~~{}~~~", ii, proc.src.len, proc.wpos, proc.sofar());
        }
        else
        {
            proc.set(' ');
            _c4dbgfsq("single newline. convert to space. ii={}/{}. sofar=[{}]~~~{}~~~", ii, proc.src.len, proc.wpos, proc.sofar());
        }
    }
    proc.rpos = ii;
}

template<class EventHandler>
template<class FilterProcessor>
auto ParseEngine<EventHandler>::_filter_squoted(FilterProcessor &C4_RESTRICT proc) -> decltype(proc.result())
{
    _c4dbgfsq("before=[{}]~~~{}~~~", proc.src.len, proc.src);

    // from the YAML spec for double-quoted scalars:
    // https://yaml.org/spec/1.2-old/spec.html#style/flow/single-quoted
    while(proc.has_more_chars())
    {
        const char curr = proc.curr();
        _c4dbgfsq("'{}', sofar=[{}]~~~{}~~~", _c4prc(curr), proc.wpos, proc.sofar());
        switch(curr)
        {
        case ' ':
        case '\t':
            _c4dbgfsq("whitespace", curr);
            _filter_ws_copy_trailing(proc);
            break;
        case '\n':
            _c4dbgfsq("newline", curr);
            _filter_nl_squoted(proc);
            break;
        case '\r':  // skip \r --- https://stackoverflow.com/questions/1885900
            _c4dbgfsq("skip cr", curr);
            proc.skip();
            break;
        case '\'':
            _c4dbgfsq("squote", curr);
            if(proc.next() == '\'')
            {
                _c4dbgfsq("two consecutive squotes", curr);
                proc.skip();
                proc.copy();
            }
            else
            {
                _c4err("filter error");
            }
            break;
        default:
            proc.copy();
            break;
        }
    }

    _c4dbgfsq(": #filteredchars={} after=~~~[{}]{}~~~", proc.src.len-proc.sofar().len, proc.sofar().len, proc.sofar());

    return proc.result();
}

#undef _c4dbgfsq

template<class EventHandler>
FilterResult ParseEngine<EventHandler>::filter_scalar_squoted(csubstr scalar, substr dst)
{
    FilterProcessorSrcDst proc(scalar, dst);
    return _filter_squoted(proc);
}

template<class EventHandler>
FilterResult ParseEngine<EventHandler>::filter_scalar_squoted_in_place(substr dst, size_t cap)
{
    FilterProcessorInplaceEndExtending proc(dst, cap);
    return _filter_squoted(proc);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
/* double quoted */

// a debugging scaffold:
#if 0
#define _c4dbgfdq(fmt, ...) _c4dbgpf("filt_dquo[{}->{}]: " fmt, proc.rpos, proc.wpos, __VA_ARGS__)
#else
#define _c4dbgfdq(...)
#endif

template<class EventHandler>
template<class FilterProcessor>
void ParseEngine<EventHandler>::_filter_nl_dquoted(FilterProcessor &C4_RESTRICT proc)
{
    _RYML_CB_ASSERT(this->callbacks(), proc.curr() == '\n');

    _c4dbgfdq("found newline. sofar=[{}]~~~{}~~~", proc.wpos, proc.sofar());
    size_t ii = proc.rpos;
    const size_t numnl_following = _count_following_newlines(proc.src, &ii);
    if(numnl_following)
    {
        proc.set('\n', numnl_following);
        _c4dbgfdq("{} consecutive (empty) lines {}. totalws={}", 1+numnl_following, ii < proc.src.len ? "in the middle" : "at the end", proc.rpos-ii);
    }
    else
    {
        const size_t ret = proc.src.first_not_of(" \t", proc.rpos+1);
        if(ret != npos)
        {
            proc.set(' ');
            _c4dbgfdq("single newline. convert to space. ret={}/{}. sofar=[{}]~~~{}~~~", ii, proc.src.len, proc.wpos, proc.sofar());
        }
        else
        {
            proc.set(' ');
            _c4dbgfdq("single newline. convert to space. ii={}/{}. sofar=[{}]~~~{}~~~", ii, proc.src.len, proc.wpos, proc.sofar());
        }
        if(ii < proc.src.len && proc.src.str[ii] == '\\')
        {
            _c4dbgfdq("backslash at [{}]", ii);
            const char next = ii+1 < proc.src.len ? proc.src.str[ii+1] : '\0';
            if(next == ' ' || next == '\t')
            {
                _c4dbgfdq("extend skip to backslash", "");
                ++ii;
            }
        }
    }
    proc.rpos = ii;
}

template<class EventHandler>
template<class FilterProcessor>
void ParseEngine<EventHandler>::_filter_dquoted_backslash(FilterProcessor &C4_RESTRICT proc)
{
    char next = proc.next();
    _c4dbgfdq("backslash, next='{}'", _c4prc(next));
    if(next == '\r')
    {
        if(proc.rpos+2 < proc.src.len && proc.src.str[proc.rpos+2] == '\n')
        {
            proc.skip(); // newline escaped with \ -- skip both (add only one as i is loop-incremented)
            next = '\n';
            _c4dbgfdq("[{}]: was \\r\\n, now next='\\n'", proc.rpos);
        }
    }

    if(next == '\n')
    {
        size_t ii = proc.rpos + 2;
        for( ; ii < proc.src.len; ++ii)
        {
            // skip leading whitespace
            if(proc.src.str[ii] == ' ' || proc.src.str[ii] == '\t')
                ;
            else
                break;
        }
        proc.skip(ii - proc.rpos);
    }
    else if(next == '"' || next == '/' || next == ' ' || next == '\t')
    {
        // escapes for json compatibility
        proc.translate_esc(next);
        _c4dbgfdq("here, used '{}'", _c4prc(next));
    }
    else if(next == '\r')
    {
        proc.skip();
    }
    else if(next == 'n')
    {
        proc.translate_esc('\n');
    }
    else if(next == 'r')
    {
        proc.translate_esc('\r');
    }
    else if(next == 't')
    {
        proc.translate_esc('\t');
    }
    else if(next == '\\')
    {
        proc.translate_esc('\\');
    }
    else if(next == 'x') // 2-digit Unicode escape (\xXX), code point 0x00–0xFF
    {
        if(C4_UNLIKELY(proc.rpos + 1u + 2u >= proc.src.len))
            _c4err_("\\x requires 2 hex digits. scalar pos={}", proc.rpos);
        char readbuf[8];
        csubstr codepoint = proc.src.sub(proc.rpos + 2u, 2u);
        _c4dbgfdq("utf8 ~~~{}~~~ rpos={} rem=~~~{}~~~", codepoint, proc.rpos, proc.src.sub(proc.rpos));
        uint32_t codepoint_val = {};
        if(C4_UNLIKELY(!read_hex(codepoint, &codepoint_val)))
            _c4err_("failed to read \\x codepoint. scalar pos={}", proc.rpos);
        const size_t numbytes = decode_code_point((uint8_t*)readbuf, sizeof(readbuf), codepoint_val);
        if(C4_UNLIKELY(numbytes == 0))
            _c4err_("failed to decode code point={}", proc.rpos);
        _RYML_CB_ASSERT(callbacks(), numbytes <= 4);
        proc.translate_esc_bulk(readbuf, numbytes, /*nread*/3u);
        _c4dbgfdq("utf8 after rpos={} rem=~~~{}~~~", proc.rpos, proc.src.sub(proc.rpos));
    }
    else if(next == 'u') // 4-digit Unicode escape (\uXXXX), code point 0x0000–0xFFFF
    {
        if(C4_UNLIKELY(proc.rpos + 1u + 4u >= proc.src.len))
            _c4err_("\\u requires 4 hex digits. scalar pos={}", proc.rpos);
        char readbuf[8];
        csubstr codepoint = proc.src.sub(proc.rpos + 2u, 4u);
        uint32_t codepoint_val = {};
        if(C4_UNLIKELY(!read_hex(codepoint, &codepoint_val)))
            _c4err_("failed to parse \\u codepoint. scalar pos={}", proc.rpos);
        const size_t numbytes = decode_code_point((uint8_t*)readbuf, sizeof(readbuf), codepoint_val);
        if(C4_UNLIKELY(numbytes == 0))
            _c4err_("failed to decode code point={}", proc.rpos);
        _RYML_CB_ASSERT(callbacks(), numbytes <= 4);
        proc.translate_esc_bulk(readbuf, numbytes, /*nread*/5u);
    }
    else if(next == 'U') // 8-digit Unicode escape (\UXXXXXXXX), full 32-bit code point
    {
        if(C4_UNLIKELY(proc.rpos + 1u + 8u >= proc.src.len))
            _c4err_("\\U requires 8 hex digits. scalar pos={}", proc.rpos);
        char readbuf[8];
        csubstr codepoint = proc.src.sub(proc.rpos + 2u, 8u);
        uint32_t codepoint_val = {};
        if(C4_UNLIKELY(!read_hex(codepoint, &codepoint_val)))
            _c4err_("failed to parse \\U codepoint. scalar pos={}", proc.rpos);
        const size_t numbytes = decode_code_point((uint8_t*)readbuf, sizeof(readbuf), codepoint_val);
        if(C4_UNLIKELY(numbytes == 0))
            _c4err_("failed to decode code point={}", proc.rpos);
        _RYML_CB_ASSERT(callbacks(), numbytes <= 4);
        proc.translate_esc_bulk(readbuf, numbytes, /*nread*/9u);
    }
    // https://yaml.org/spec/1.2.2/#rule-c-ns-esc-char
    else if(next == '0')
    {
        proc.translate_esc('\0');
    }
    else if(next == 'b') // backspace
    {
        proc.translate_esc('\b');
    }
    else if(next == 'f') // form feed
    {
        proc.translate_esc('\f');
    }
    else if(next == 'a') // bell character
    {
        proc.translate_esc('\a');
    }
    else if(next == 'v') // vertical tab
    {
        proc.translate_esc('\v');
    }
    else if(next == 'e') // escape character
    {
        proc.translate_esc('\x1b');
    }
    else if(next == '_') // unicode non breaking space \u00a0
    {
        // https://www.compart.com/en/unicode/U+00a0
        const char payload[] = {
            _RYML_CHCONST(-0x3e, 0xc2),
            _RYML_CHCONST(-0x60, 0xa0),
        };
        proc.translate_esc_bulk(payload, /*nwrite*/2, /*nread*/1);
    }
    else if(next == 'N') // unicode next line \u0085
    {
        // https://www.compart.com/en/unicode/U+0085
        const char payload[] = {
            _RYML_CHCONST(-0x3e, 0xc2),
            _RYML_CHCONST(-0x7b, 0x85),
        };
        proc.translate_esc_bulk(payload, /*nwrite*/2, /*nread*/1);
    }
    else if(next == 'L') // unicode line separator \u2028
    {
        // https://www.utf8-chartable.de/unicode-utf8-table.pl?start=8192&number=1024&names=-&utf8=0x&unicodeinhtml=hex
        const char payload[] = {
            _RYML_CHCONST(-0x1e, 0xe2),
            _RYML_CHCONST(-0x80, 0x80),
            _RYML_CHCONST(-0x58, 0xa8),
        };
        proc.translate_esc_extending(payload, /*nwrite*/3, /*nread*/1);
    }
    else if(next == 'P') // unicode paragraph separator \u2029
    {
        // https://www.utf8-chartable.de/unicode-utf8-table.pl?start=8192&number=1024&names=-&utf8=0x&unicodeinhtml=hex
        const char payload[] = {
            _RYML_CHCONST(-0x1e, 0xe2),
            _RYML_CHCONST(-0x80, 0x80),
            _RYML_CHCONST(-0x57, 0xa9),
        };
        proc.translate_esc_extending(payload, /*nwrite*/3, /*nread*/1);
    }
    else if(next == '\0')
    {
        proc.skip();
    }
    else
    {
        _c4err_("unknown character '{}' after '\\' pos={}", _c4prc(next), proc.rpos);
    }
    _c4dbgfdq("backslash...sofar=[{}]~~~{}~~~", proc.wpos, proc.sofar());
}


template<class EventHandler>
template<class FilterProcessor>
auto ParseEngine<EventHandler>::_filter_dquoted(FilterProcessor &C4_RESTRICT proc) -> decltype(proc.result())
{
    _c4dbgfdq("before=[{}]~~~{}~~~", proc.src.len, proc.src);
    // from the YAML spec for double-quoted scalars:
    // https://yaml.org/spec/1.2-old/spec.html#style/flow/double-quoted
    while(proc.has_more_chars())
    {
        const char curr = proc.curr();
        _c4dbgfdq("'{}' sofar=[{}]~~~{}~~~", _c4prc(curr), proc.wpos, proc.sofar());
        switch(curr)
        {
        case ' ':
        case '\t':
        {
            _c4dbgfdq("whitespace", curr);
            _filter_ws_copy_trailing(proc);
            break;
        }
        case '\n':
        {
            _c4dbgfdq("newline", curr);
            _filter_nl_dquoted(proc);
            break;
        }
        case '\r':  // skip \r --- https://stackoverflow.com/questions/1885900
        {
            _c4dbgfdq("carriage return, ignore", curr);
            proc.skip();
            break;
        }
        case '\\':
        {
            _filter_dquoted_backslash(proc);
            break;
        }
        default:
        {
            proc.copy();
            break;
        }
        }
    }
    _c4dbgfdq("after[{}]=~~~{}~~~", proc.wpos, proc.sofar());
    return proc.result();
}

#undef _c4dbgfdq


template<class EventHandler>
FilterResult ParseEngine<EventHandler>::filter_scalar_dquoted(csubstr scalar, substr dst)
{
    FilterProcessorSrcDst proc(scalar, dst);
    return _filter_dquoted(proc);
}

template<class EventHandler>
FilterResultExtending ParseEngine<EventHandler>::filter_scalar_dquoted_in_place(substr dst, size_t cap)
{
    FilterProcessorInplaceMidExtending proc(dst, cap);
    return _filter_dquoted(proc);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// block filtering helpers

C4_NO_INLINE inline size_t _find_last_newline_and_larger_indentation(csubstr s, size_t indentation) noexcept
{
    if(indentation + 1 > s.len)
        return npos;
    for(size_t i = s.len-indentation-1; i != size_t(-1); --i)
    {
        if(s.str[i] == '\n')
        {
            csubstr rem = s.sub(i + 1);
            size_t first = rem.first_not_of(' ');
            first = (first != npos) ? first : rem.len;
            if(first > indentation)
                return i;
        }
    }
    return npos;
}

template<class EventHandler>
template<class FilterProcessor>
void ParseEngine<EventHandler>::_filter_chomp(FilterProcessor &C4_RESTRICT proc, BlockChomp_e chomp, size_t indentation)
{
    _RYML_CB_ASSERT(this->callbacks(), chomp == CHOMP_CLIP || chomp == CHOMP_KEEP || chomp == CHOMP_STRIP);
    _RYML_CB_ASSERT(this->callbacks(), proc.rem().first_not_of(" \n\r") == npos);

    // a debugging scaffold:
    #if 0
    #define _c4dbgchomp(fmt, ...) _c4dbgpf("chomp[{}->{}]: " fmt, proc.rpos, proc.wpos, __VA_ARGS__)
    #else
    #define _c4dbgchomp(...)
    #endif

    // advance to the last line having spaces beyond the indentation
    {
        size_t last = _find_last_newline_and_larger_indentation(proc.rem(), indentation);
        if(last != npos)
        {
            _c4dbgchomp("found newline and larger indentation. last={}", last);
            last = proc.rpos + last + size_t(1) + indentation;  // last started at to-be-read.
            _RYML_CB_ASSERT(this->callbacks(), last <= proc.src.len);
            // remove indentation spaces, copy the rest
            while((proc.rpos < last) && proc.has_more_chars())
            {
                const char curr = proc.curr();
                _c4dbgchomp("curr='{}'", _c4prc(curr));
                switch(curr)
                {
                case '\n':
                    {
                        _c4dbgchomp("newline! remlen={}", proc.rem().len);
                        proc.copy();
                        // are there spaces after the newline?
                        csubstr at_next_line = proc.rem();
                        if(at_next_line.begins_with(' '))
                        {
                            _c4dbgchomp("next line begins with spaces. indentation={}", indentation);
                            // there are spaces.
                            size_t first_non_space = at_next_line.first_not_of(' ');
                            _c4dbgchomp("first_non_space={}", first_non_space);
                            if(first_non_space == npos)
                            {
                                _c4dbgchomp("{} spaces, to the end", at_next_line.len);
                                first_non_space = at_next_line.len;
                            }
                            if(first_non_space <= indentation)
                            {
                                _c4dbgchomp("skip spaces={}<=indentation={}", first_non_space, indentation);
                                proc.skip(first_non_space);
                            }
                            else
                            {
                                _c4dbgchomp("skip indentation={}<spaces={}", indentation, first_non_space);
                                proc.skip(indentation);
                                // copy the spaces after the indentation
                                _c4dbgchomp("copy {}={}-{} spaces", first_non_space - indentation, first_non_space, indentation);
                                proc.copy(first_non_space - indentation);
                            }
                        }
                        break;
                    }
                case '\r':
                    proc.skip();
                    break;
                default:
                    _c4err("parse error");
                    break;
                }
            }
        }
    }

    // from now on, we only have line ends (or indentation spaces)
    switch(chomp)
    {
    case CHOMP_CLIP:
    {
        bool had_one = false;
        while(proc.has_more_chars())
        {
            const char curr = proc.curr();
            _c4dbgchomp("CLIP: '{}'", _c4prc(curr));
            switch(curr)
            {
            case '\n':
            {
                _c4dbgchomp("copy newline!", curr);
                proc.copy();
                proc.set_at_end();
                had_one = true;
                break;
            }
            case ' ':
            case '\r':
                _c4dbgchomp("skip!", curr);
                proc.skip();
                break;
            }
        }
        if(!had_one) // there were no newline characters. add one.
        {
            _c4dbgchomp("chomp=CLIP: add missing newline @{}", proc.wpos);
            proc.set('\n');
        }
        break;
    }
    case CHOMP_KEEP:
    {
        _c4dbgchomp("chomp=KEEP: copy all remaining new lines of {} characters", proc.rem().len);
        while(proc.has_more_chars())
        {
            const char curr = proc.curr();
            _c4dbgchomp("KEEP: '{}'", _c4prc(curr));
            switch(curr)
            {
            case '\n':
                _c4dbgchomp("copy newline!", curr);
                proc.copy();
                break;
            case ' ':
            case '\r':
                _c4dbgchomp("skip!", curr);
                proc.skip();
                break;
            }
        }
        break;
    }
    case CHOMP_STRIP:
    {
        _c4dbgchomp("chomp=STRIP: strip {} characters", proc.rem().len);
        // nothing to do!
        break;
    }
    }

    #undef _c4dbgchomp
}


// a debugging scaffold:
#if 0
#define _c4dbgfb(fmt, ...) _c4dbgpf("filt_block[{}->{}]: " fmt, proc.rpos, proc.wpos, __VA_ARGS__)
#else
#define _c4dbgfb(...)
#endif

template<class EventHandler>
template<class FilterProcessor>
void ParseEngine<EventHandler>::_filter_block_indentation(FilterProcessor &C4_RESTRICT proc, size_t indentation)
{
    csubstr rem = proc.rem(); // remaining
    if(rem.len)
    {
        size_t first = rem.first_not_of(' ');
        if(first != npos)
        {
            _c4dbgfb("{} spaces follow before next nonws character", first);
            if(first < indentation)
            {
                _c4dbgfb("skip {}<{} spaces from indentation", first, indentation);
                proc.skip(first);
            }
            else
            {
                _c4dbgfb("skip {} spaces from indentation", indentation);
                proc.skip(indentation);
            }
        }
        #ifdef RYML_NO_COVERAGE__TO_BE_DELETED
        else
        {
            _c4dbgfb("all spaces to the end: {} spaces", first);
            first = rem.len;
            if(first)
            {
                if(first < indentation)
                {
                    _c4dbgfb("skip everything", first);
                    proc.skip(proc.src.len - proc.rpos);
                }
                else
                {
                    _c4dbgfb("skip {} spaces from indentation", indentation);
                    proc.skip(indentation);
                }
            }
        }
        #endif
    }
}

template<class EventHandler>
template<class FilterProcessor>
size_t ParseEngine<EventHandler>::_handle_all_whitespace(FilterProcessor &C4_RESTRICT proc, BlockChomp_e chomp)
{
    csubstr contents = proc.src.trimr(" \n\r");
    _c4dbgfb("ws: contents_len={} wslen={}", contents.len, proc.src.len-contents.len);
    if(!contents.len)
    {
        _c4dbgfb("ws: all whitespace: len={}", proc.src.len);
        if(chomp == CHOMP_KEEP && proc.src.len)
        {
            _c4dbgfb("ws: chomp=KEEP all {} newlines", proc.src.count('\n'));
            while(proc.has_more_chars())
            {
                const char curr = proc.curr();
                if(curr == '\n')
                    proc.copy();
                else
                    proc.skip();
            }
            if(!proc.wpos)
            {
                proc.set('\n');
            }
        }
    }
    return contents.len;
}

template<class EventHandler>
template<class FilterProcessor>
size_t ParseEngine<EventHandler>::_extend_to_chomp(FilterProcessor &C4_RESTRICT proc, size_t contents_len)
{
    _c4dbgfb("contents_len={}", contents_len);

    _RYML_CB_ASSERT(this->callbacks(), contents_len > 0u);

    // extend contents to just before the first newline at the end,
    // in case it is preceded by spaces
    size_t firstnewl = proc.src.first_of('\n', contents_len);
    if(firstnewl != npos)
    {
        contents_len = firstnewl;
        _c4dbgfb("contents_len={}  <--- firstnewl={}", contents_len, firstnewl);
    }
    else
    {
        contents_len = proc.src.len;
        _c4dbgfb("contents_len={}  <--- src.len={}", contents_len, proc.src.len);
    }

    return contents_len;
}

#undef _c4dbgfb


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

// a debugging scaffold:
#if 0
#define _c4dbgfbl(fmt, ...) _c4dbgpf("filt_block_lit[{}->{}]: " fmt, proc.rpos, proc.wpos, __VA_ARGS__)
#else
#define _c4dbgfbl(...)
#endif

template<class EventHandler>
template<class FilterProcessor>
auto ParseEngine<EventHandler>::_filter_block_literal(FilterProcessor &C4_RESTRICT proc, size_t indentation, BlockChomp_e chomp) -> decltype(proc.result())
{
    _c4dbgfbl("indentation={} before=[{}]~~~{}~~~", indentation, proc.src.len, proc.src);

    size_t contents_len = _handle_all_whitespace(proc, chomp);
    if(!contents_len)
        return proc.result();

    contents_len = _extend_to_chomp(proc, contents_len);

    _c4dbgfbl("to filter=[{}]~~~{}~~~", contents_len, proc.src.first(contents_len));

    _filter_block_indentation(proc, indentation);

    // now filter the bulk
    while(proc.has_more_chars(/*maxpos*/contents_len))
    {
        const char curr = proc.curr();
        _c4dbgfbl("'{}' sofar=[{}]~~~{}~~~",  _c4prc(curr), proc.wpos, proc.sofar());
        switch(curr)
        {
        case '\n':
        {
            _c4dbgfbl("found newline. skip indentation on the next line", curr);
            proc.copy();  // copy the newline
            _filter_block_indentation(proc, indentation);
            break;
        }
        case '\r':
            proc.skip();
            break;
        default:
            proc.copy();
            break;
        }
    }

    _c4dbgfbl("before chomp: #tochomp={}   sofar=[{}]~~~{}~~~", proc.rem().len, proc.sofar().len, proc.sofar());

    _filter_chomp(proc, chomp, indentation);

    _c4dbgfbl("final=[{}]~~~{}~~~", proc.sofar().len, proc.sofar());

    return proc.result();
}

#undef _c4dbgfbl

template<class EventHandler>
FilterResult ParseEngine<EventHandler>::filter_scalar_block_literal(csubstr scalar, substr dst, size_t indentation, BlockChomp_e chomp)
{
    FilterProcessorSrcDst proc(scalar, dst);
    return _filter_block_literal(proc, indentation, chomp);
}

template<class EventHandler>
FilterResult ParseEngine<EventHandler>::filter_scalar_block_literal_in_place(substr scalar, size_t cap, size_t indentation, BlockChomp_e chomp)
{
    FilterProcessorInplaceEndExtending proc(scalar, cap);
    return _filter_block_literal(proc, indentation, chomp);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

// a debugging scaffold:
#if 0
#define _c4dbgfbf(fmt, ...) _c4dbgpf("filt_block_folded[{}->{}]: " fmt, proc.rpos, proc.wpos, __VA_ARGS__)
#else
#define _c4dbgfbf(...)
#endif


template<class EventHandler>
template<class FilterProcessor>
void ParseEngine<EventHandler>::_filter_block_folded_newlines_leading(FilterProcessor &C4_RESTRICT proc, size_t indentation, size_t len)
{
    _filter_block_indentation(proc, indentation);
    while(proc.has_more_chars(len))
    {
        const char curr = proc.curr();
        _c4dbgfbf("'{}' sofar=[{}]~~~{}~~~",  _c4prc(curr), proc.wpos, proc.sofar());
        switch(curr)
        {
        case '\n':
            _c4dbgfbf("newline.", curr);
            proc.copy();
            _filter_block_indentation(proc, indentation);
            break;
        case '\r':
            proc.skip();
            break;
        case ' ':
        case '\t':
        {
            size_t first = proc.rem().first_not_of(" \t");
            _c4dbgfbf("space. first={}", first);
            if(first == npos)
                first = proc.rem().len;
            _c4dbgfbf("... indentation increased to {}",  first);
            _filter_block_folded_indented_block(proc, indentation, len, first);
            break;
        }
        default:
            _c4dbgfbf("newl leading: not space, not newline. stop.", 0);
            return;
        }
    }
}

template<class EventHandler>
template<class FilterProcessor>
size_t ParseEngine<EventHandler>::_filter_block_folded_newlines_compress(FilterProcessor &C4_RESTRICT proc, size_t num_newl, size_t wpos_at_first_newl)
{
    switch(num_newl)
    {
    case 1u:
        _c4dbgfbf("... this is the first newline. turn into space. wpos={}", proc.wpos);
        wpos_at_first_newl = proc.wpos;
        proc.skip();
        proc.set(' ');
        break;
    case 2u:
        _c4dbgfbf("... this is the second newline. prev space (at wpos={}) must be newline", wpos_at_first_newl);
        _RYML_CB_ASSERT(this->callbacks(), wpos_at_first_newl != npos);
        _RYML_CB_ASSERT(this->callbacks(), proc.sofar()[wpos_at_first_newl] == ' ');
        _RYML_CB_ASSERT(this->callbacks(), wpos_at_first_newl + 1u == proc.wpos);
        proc.skip();
        proc.set_at(wpos_at_first_newl, '\n');
        _RYML_CB_ASSERT(this->callbacks(), proc.sofar()[wpos_at_first_newl] == '\n');
        break;
    default:
        _c4dbgfbf("... subsequent newline (num_newl={}). copy", num_newl);
        proc.copy();
        break;
    }
    return wpos_at_first_newl;
}

template<class EventHandler>
template<class FilterProcessor>
void ParseEngine<EventHandler>::_filter_block_folded_newlines(FilterProcessor &C4_RESTRICT proc, size_t indentation, size_t len)
{
    _RYML_CB_ASSERT(this->callbacks(), proc.curr() == '\n');
    size_t num_newl = 0;
    size_t wpos_at_first_newl = npos;
    while(proc.has_more_chars(len))
    {
        const char curr = proc.curr();
        _c4dbgfbf("'{}' sofar=[{}]~~~{}~~~",  _c4prc(curr), proc.wpos, proc.sofar());
        switch(curr)
        {
        case '\n':
        {
            _c4dbgfbf("newline. sofar={}", num_newl);
            // NOTE: vs2022-32bit-release builds were giving wrong
            // results in this block, if it was written as either
            // as a  switch(num_newl) or its equivalent if-form.
            //
            // For this reason, we're using a dedicated function
            // (**_compress), which seems to work around the issue.
            //
            // The manifested problem was that somewhere between the
            // assignment to curr and this point, proc.wpos (the
            // write-position of the processor) jumped to npos, which
            // made the write wrap-around! To make things worse,
            // enabling prints via _c4dbgpf() and _c4dbgfbf() made the
            // problem go away!
            //
            // The only way to make the problem appear with prints
            // enabled was by disabling all prints in this function
            // (including in the block which was moved to the compress
            // function) and then selectively enabling only some of
            // those prints.
            //
            // This may be due to some bug in the cl-x86 optimizer; or
            // it may be triggered by some UB which may be
            // inadvertedly present in this function or in the filter
            // processor. This is despite our best efforts to weed out
            // any such UB problem: neither clang-tidy nor none of the
            // sanitizers, or gcc's -fanalyzer pointed to any problems
            // in this code.
            //
            // In the end, moving this block to a separate function
            // was the only way to bury the problem. But it may
            // resurface again, as The Undead, rising to from the
            // grave to haunt us with his terrible presence.
            //
            // We may have to revisit this. With a stake, and lots of
            // garlic.
            wpos_at_first_newl = _filter_block_folded_newlines_compress(proc, ++num_newl, wpos_at_first_newl);
            _filter_block_indentation(proc, indentation);
            break;
        }
        case ' ':
        case '\t':
            {
                size_t first = proc.rem().first_not_of(" \t");
                _c4dbgfbf("space. first={}", first);
                if(first == npos)
                    first = proc.rem().len;
                _c4dbgfbf("... indentation increased to {}",  first);
                if(num_newl)
                {
                    _c4dbgfbf("... prev space (at wpos={}) must be newline", wpos_at_first_newl);
                    proc.set_at(wpos_at_first_newl, '\n');
                }
                if(num_newl > 1u)
                {
                    _c4dbgfbf("... add missing newline", wpos_at_first_newl);
                    proc.set('\n');
                }
                _filter_block_folded_indented_block(proc, indentation, len, first);
                num_newl = 0;
                wpos_at_first_newl = npos;
                break;
            }
        case '\r':
            proc.skip();
            break;
        default:
            _c4dbgfbf("not space, not newline. stop.", 0);
            return;
        }
    }
}


template<class EventHandler>
template<class FilterProcessor>
void ParseEngine<EventHandler>::_filter_block_folded_indented_block(FilterProcessor &C4_RESTRICT proc, size_t indentation, size_t len, size_t curr_indentation) noexcept
{
    _RYML_CB_ASSERT(this->callbacks(), (proc.rem().first_not_of(" \t") == curr_indentation) || (proc.rem().first_not_of(" \t") == npos));
    if(curr_indentation)
        proc.copy(curr_indentation);
    while(proc.has_more_chars(len))
    {
        const char curr = proc.curr();
        _c4dbgfbf("'{}' sofar=[{}]~~~{}~~~",  _c4prc(curr), proc.wpos, proc.sofar());
        switch(curr)
        {
        case '\n':
            {
                proc.copy();
                _filter_block_indentation(proc, indentation);
                csubstr rem = proc.rem();
                const size_t first = rem.first_not_of(' ');
                _c4dbgfbf("newline. firstns={}",  first);
                if(first == 0)
                {
                    const char c = rem[first];
                    _c4dbgfbf("firstns={}='{}'", first, _c4prc(c));
                    if(c == '\n' || c == '\r')
                    {
                        ;
                    }
                    else
                    {
                        _c4dbgfbf("done with indented block",  first);
                        goto endloop;
                    }
                }
                else if(first != npos)
                {
                    proc.copy(first);
                    _c4dbgfbf("copy all {} spaces",  first);
                }
                break;
            }
            break;
        case '\r':
            proc.skip();
            break;
        default:
            proc.copy();
            break;
        }
    }
 endloop:
    return;
}


template<class EventHandler>
template<class FilterProcessor>
auto ParseEngine<EventHandler>::_filter_block_folded(FilterProcessor &C4_RESTRICT proc, size_t indentation, BlockChomp_e chomp) -> decltype(proc.result())
{
    _c4dbgfbf("indentation={} before=[{}]~~~{}~~~", indentation, proc.src.len, proc.src);

    size_t contents_len = _handle_all_whitespace(proc, chomp);
    if(!contents_len)
        return proc.result();

    contents_len = _extend_to_chomp(proc, contents_len);

    _c4dbgfbf("to filter=[{}]~~~{}~~~", contents_len, proc.src.first(contents_len));

    _filter_block_folded_newlines_leading(proc, indentation, contents_len);

    // now filter the bulk
    while(proc.has_more_chars(/*maxpos*/contents_len))
    {
        const char curr = proc.curr();
        _c4dbgfbf("'{}' sofar=[{}]~~~{}~~~",  _c4prc(curr), proc.wpos, proc.sofar());
        switch(curr)
        {
        case '\n':
        {
            _c4dbgfbf("found newline", curr);
            _filter_block_folded_newlines(proc, indentation, contents_len);
            break;
        }
        case '\r':
            proc.skip();
            break;
        default:
            proc.copy();
            break;
        }
    }

    _c4dbgfbf("before chomp: #tochomp={}   sofar=[{}]~~~{}~~~", proc.rem().len, proc.sofar().len, proc.sofar());

    _filter_chomp(proc, chomp, indentation);

    _c4dbgfbf("final=[{}]~~~{}~~~", proc.sofar().len, proc.sofar());

    return proc.result();
}

#undef _c4dbgfbf

template<class EventHandler>
FilterResult ParseEngine<EventHandler>::filter_scalar_block_folded(csubstr scalar, substr dst, size_t indentation, BlockChomp_e chomp)
{
    FilterProcessorSrcDst proc(scalar, dst);
    return _filter_block_folded(proc, indentation, chomp);
}

template<class EventHandler>
FilterResult ParseEngine<EventHandler>::filter_scalar_block_folded_in_place(substr scalar, size_t cap, size_t indentation, BlockChomp_e chomp)
{
    FilterProcessorInplaceEndExtending proc(scalar, cap);
    return _filter_block_folded(proc, indentation, chomp);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_filter_scalar_plain(substr s, size_t indentation)
{
    _c4dbgpf("filtering plain scalar: s=[{}]~~~{}~~~", s.len, s);
    FilterResult r = this->filter_scalar_plain_in_place(s, s.len, indentation);
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, r.valid());
    _c4dbgpf("filtering plain scalar: success! s=[{}]~~~{}~~~", r.get().len, r.get());
    return r.get();
}

//-----------------------------------------------------------------------------

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_filter_scalar_squot(substr s)
{
    _c4dbgpf("filtering squo scalar: s=[{}]~~~{}~~~", s.len, s);
    FilterResult r = this->filter_scalar_squoted_in_place(s, s.len);
    _RYML_CB_ASSERT(this->callbacks(), r.valid());
    _c4dbgpf("filtering squo scalar: success! s=[{}]~~~{}~~~", r.get().len, r.get());
    return r.get();
}


//-----------------------------------------------------------------------------

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_filter_scalar_dquot(substr s)
{
    _c4dbgpf("filtering dquo scalar: s=[{}]~~~{}~~~", s.len, s);
    FilterResultExtending r = this->filter_scalar_dquoted_in_place(s, s.len);
    if(C4_LIKELY(r.valid()))
    {
        _c4dbgpf("filtering dquo scalar: success! s=[{}]~~~{}~~~", r.get().len, r.get());
        return r.get();
    }
    else
    {
        const size_t len = r.required_len();
        _c4dbgpf("filtering dquo scalar: not enough space: needs {}, have {}", len, s.len);
        substr dst = m_evt_handler->alloc_arena(len, &s);
        _c4dbgpf("filtering dquo scalar: dst.len={}", dst.len);
        if(dst.str)
        {
            _RYML_CB_ASSERT(this->callbacks(), dst.len == len);
            FilterResult rsd = this->filter_scalar_dquoted(s, dst);
            _c4dbgpf("filtering dquo scalar: ... result now needs {} was {}", rsd.required_len(), len);
            _RYML_CB_ASSERT(this->callbacks(), rsd.required_len() <= len); // may be smaller!
            _RYML_CB_CHECK(m_evt_handler->m_stack.m_callbacks, rsd.valid());
            _c4dbgpf("filtering dquo scalar: success! s=[{}]~~~{}~~~", rsd.get().len, rsd.get());
            return rsd.get();
        }
        return dst;
    }
}


//-----------------------------------------------------------------------------

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_move_scalar_left_and_add_newline(substr s)
{
    if(s.is_sub(m_buf))
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, s.str > m_buf.str);
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, s.str-1 >= m_buf.str);
        if(s.len)
            memmove(s.str - 1, s.str, s.len);
        --s.str;
        s.str[s.len] = '\n';
        ++s.len;
        return s;
    }
    else
    {
        substr dst = m_evt_handler->alloc_arena(s.len + 1);
        if(s.len)
            memcpy(dst.str, s.str, s.len);
        dst[s.len] = '\n';
        return dst;
    }
}

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_filter_scalar_literal(substr s, size_t indentation, BlockChomp_e chomp)
{
    _c4dbgpf("filtering block literal scalar: s=[{}]~~~{}~~~", s.len, s);
    FilterResult r = this->filter_scalar_block_literal_in_place(s, s.len, indentation, chomp);
    csubstr result;
    if(C4_LIKELY(r.valid()))
    {
        result = r.get();
    }
    else
    {
        _c4dbgpf("filtering block literal scalar: not enough space: needs {}, have {}", r.required_len(), s.len);
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, r.required_len() == s.len + 1);
        // this can only happen when adding a single newline in clip mode.
        // so we shift left the scalar by one place
        result = _move_scalar_left_and_add_newline(s);
    }
    _c4dbgpf("filtering block literal scalar: success! s=[{}]~~~{}~~~", result.len, result);
    return result;
}


//-----------------------------------------------------------------------------
template<class EventHandler>
csubstr ParseEngine<EventHandler>::_filter_scalar_folded(substr s, size_t indentation, BlockChomp_e chomp)
{
    _c4dbgpf("filtering block folded scalar: s=[{}]~~~{}~~~", s.len, s);
    FilterResult r = this->filter_scalar_block_folded_in_place(s, s.len, indentation, chomp);
    csubstr result;
    if(C4_LIKELY(r.valid()))
    {
        result = r.get();
    }
    else
    {
        _c4dbgpf("filtering block folded scalar: not enough space: needs {}, have {}", r.required_len(), s.len);
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, r.required_len() == s.len + 1);
        // this can only happen when adding a single newline in clip mode.
        // so we shift left the scalar by one place
        result = _move_scalar_left_and_add_newline(s);
    }
    _c4dbgpf("filtering block folded scalar: success! s=[{}]~~~{}~~~", result.len, result);
    return result;
}


//-----------------------------------------------------------------------------

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_maybe_filter_key_scalar_plain(ScannedScalar const& C4_RESTRICT sc, size_t indentation)
{
    if(sc.needs_filter)
    {
        if(m_options.scalar_filtering())
        {
            return _filter_scalar_plain(sc.scalar, indentation);
        }
        else
        {
            _c4dbgp("plain scalar left unfiltered");
            m_evt_handler->mark_key_scalar_unfiltered();
        }
    }
    else
    {
        _c4dbgp("plain scalar doesn't need filtering");
    }
    return sc.scalar;
}

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_maybe_filter_val_scalar_plain(ScannedScalar const& C4_RESTRICT sc, size_t indentation)
{
    if(sc.needs_filter)
    {
        if(m_options.scalar_filtering())
        {
            return _filter_scalar_plain(sc.scalar, indentation);
        }
        else
        {
            _c4dbgp("plain scalar left unfiltered");
            m_evt_handler->mark_val_scalar_unfiltered();
        }
    }
    else
    {
        _c4dbgp("plain scalar doesn't need filtering");
    }
    return sc.scalar;
}


//-----------------------------------------------------------------------------

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_maybe_filter_key_scalar_squot(ScannedScalar const& C4_RESTRICT sc)
{
    if(sc.needs_filter)
    {
        if(m_options.scalar_filtering())
        {
            return _filter_scalar_squot(sc.scalar);
        }
        else
        {
            _c4dbgp("squo key scalar left unfiltered");
            m_evt_handler->mark_key_scalar_unfiltered();
        }
    }
    else
    {
        _c4dbgp("squo key scalar doesn't need filtering");
    }
    return sc.scalar;
}

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_maybe_filter_val_scalar_squot(ScannedScalar const& C4_RESTRICT sc)
{
    if(sc.needs_filter)
    {
        if(m_options.scalar_filtering())
        {
            return _filter_scalar_squot(sc.scalar);
        }
        else
        {
            _c4dbgp("squo val scalar left unfiltered");
            m_evt_handler->mark_val_scalar_unfiltered();
        }
    }
    else
    {
        _c4dbgp("squo val scalar doesn't need filtering");
    }
    return sc.scalar;
}


//-----------------------------------------------------------------------------

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_maybe_filter_key_scalar_dquot(ScannedScalar const& C4_RESTRICT sc)
{
    if(sc.needs_filter)
    {
        if(m_options.scalar_filtering())
        {
            return _filter_scalar_dquot(sc.scalar);
        }
        else
        {
            _c4dbgp("dquo scalar left unfiltered");
            m_evt_handler->mark_key_scalar_unfiltered();
        }
    }
    else
    {
        _c4dbgp("dquo scalar doesn't need filtering");
    }
    return sc.scalar;
}

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_maybe_filter_val_scalar_dquot(ScannedScalar const& C4_RESTRICT sc)
{
    if(sc.needs_filter)
    {
        if(m_options.scalar_filtering())
        {
            return _filter_scalar_dquot(sc.scalar);
        }
        else
        {
            _c4dbgp("dquo scalar left unfiltered");
            m_evt_handler->mark_val_scalar_unfiltered();
        }
    }
    else
    {
        _c4dbgp("dquo scalar doesn't need filtering");
    }
    return sc.scalar;
}


//-----------------------------------------------------------------------------

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_maybe_filter_key_scalar_literal(ScannedBlock const& C4_RESTRICT sb)
{
    if(m_options.scalar_filtering())
    {
        return _filter_scalar_literal(sb.scalar, sb.indentation, sb.chomp);
    }
    else
    {
        _c4dbgp("literal scalar left unfiltered");
        m_evt_handler->mark_key_scalar_unfiltered();
    }
    return sb.scalar;
}

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_maybe_filter_val_scalar_literal(ScannedBlock const& C4_RESTRICT sb)
{
    if(m_options.scalar_filtering())
    {
        return _filter_scalar_literal(sb.scalar, sb.indentation, sb.chomp);
    }
    else
    {
        _c4dbgp("literal scalar left unfiltered");
        m_evt_handler->mark_val_scalar_unfiltered();
    }
    return sb.scalar;
}


//-----------------------------------------------------------------------------

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_maybe_filter_key_scalar_folded(ScannedBlock const& C4_RESTRICT sb)
{
    if(m_options.scalar_filtering())
    {
        return _filter_scalar_folded(sb.scalar, sb.indentation, sb.chomp);
    }
    else
    {
        _c4dbgp("folded scalar left unfiltered");
        m_evt_handler->mark_key_scalar_unfiltered();
    }
    return sb.scalar;
}

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_maybe_filter_val_scalar_folded(ScannedBlock const& C4_RESTRICT sb)
{
    if(m_options.scalar_filtering())
    {
        return _filter_scalar_folded(sb.scalar, sb.indentation, sb.chomp);
    }
    else
    {
        _c4dbgp("folded scalar left unfiltered");
        m_evt_handler->mark_val_scalar_unfiltered();
    }
    return sb.scalar;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

#ifdef RYML_DBG  //   !!! <----------------------------------

template<class EventHandler>
void ParseEngine<EventHandler>::add_flags(ParserFlag_t on, ParserState * s)
{
    char buf1_[64], buf2_[64], buf3_[64];
    csubstr buf1 = detail::_parser_flags_to_str(buf1_, on);
    csubstr buf2 = detail::_parser_flags_to_str(buf2_, s->flags);
    csubstr buf3 = detail::_parser_flags_to_str(buf3_, s->flags|on);
    _c4dbgpf("state[{}]: add {}: before={} after={}", s->level, buf1, buf2, buf3);
    s->flags |= on;
}

template<class EventHandler>
void ParseEngine<EventHandler>::addrem_flags(ParserFlag_t on, ParserFlag_t off, ParserState * s)
{
    char buf1_[64], buf2_[64], buf3_[64], buf4_[64];
    csubstr buf1 = detail::_parser_flags_to_str(buf1_, on);
    csubstr buf2 = detail::_parser_flags_to_str(buf2_, off);
    csubstr buf3 = detail::_parser_flags_to_str(buf3_, s->flags);
    csubstr buf4 = detail::_parser_flags_to_str(buf4_, ((s->flags|on)&(~off)));
    _c4dbgpf("state[{}]: add {} / rem {}: before={} after={}", s->level, buf1, buf2, buf3, buf4);
    s->flags |= on;
    s->flags &= ~off;
}

template<class EventHandler>
void ParseEngine<EventHandler>::rem_flags(ParserFlag_t off, ParserState * s)
{
    char buf1_[64], buf2_[64], buf3_[64];
    csubstr buf1 = detail::_parser_flags_to_str(buf1_, off);
    csubstr buf2 = detail::_parser_flags_to_str(buf2_, s->flags);
    csubstr buf3 = detail::_parser_flags_to_str(buf3_, s->flags&(~off));
    _c4dbgpf("state[{}]: rem {}: before={} after={}", s->level, buf1, buf2, buf3);
    s->flags &= ~off;
}

inline C4_NO_INLINE csubstr detail::_parser_flags_to_str(substr buf, ParserFlag_t flags)
{
    size_t pos = 0;
    bool gotone = false;

    #define _prflag(fl)                                     \
    if((flags & fl) == (fl))                                \
    {                                                       \
        if(gotone)                                          \
        {                                                   \
            if(pos + 1 < buf.len)                           \
                buf[pos] = '|';                             \
            ++pos;                                          \
        }                                                   \
        csubstr fltxt = #fl;                                \
        if(pos + fltxt.len <= buf.len)                      \
            memcpy(buf.str + pos, fltxt.str, fltxt.len);    \
        pos += fltxt.len;                                   \
        gotone = true;                                      \
    }

    _prflag(RTOP);
    _prflag(RUNK);
    _prflag(RMAP);
    _prflag(RSEQ);
    _prflag(FLOW);
    _prflag(BLCK);
    _prflag(QMRK);
    _prflag(RKEY);
    _prflag(RVAL);
    _prflag(RKCL);
    _prflag(RNXT);
    _prflag(SSCL);
    _prflag(QSCL);
    _prflag(RSET);
    _prflag(RDOC);
    _prflag(NDOC);
    _prflag(USTY);
    _prflag(RSEQIMAP);

    #undef _prflag

    if(pos == 0)
        if(buf.len > 0)
            buf[pos++] = '0';

    RYML_CHECK(pos <= buf.len);

    return buf.first(pos);
}

#endif // RYML_DBG   !!! <----------------------------------


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

template<class EventHandler>
csubstr ParseEngine<EventHandler>::location_contents(Location const& loc) const
{
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, loc.offset < m_buf.len);
    return m_buf.sub(loc.offset);
}

template<class EventHandler>
Location ParseEngine<EventHandler>::val_location(const char *val) const
{
    if(C4_UNLIKELY(val == nullptr))
        return {m_file, 0, 0, 0};
    _RYML_CB_CHECK(m_evt_handler->m_stack.m_callbacks, m_options.locations());
    // NOTE: if any of these checks fails, the parser needs to be
    // instantiated with locations enabled.
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, m_buf.str == m_newline_offsets_buf.str);
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, m_buf.len == m_newline_offsets_buf.len);
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, m_options.locations());
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, !_locations_dirty());
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, m_newline_offsets != nullptr);
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, m_newline_offsets_size > 0);
    // NOTE: the pointer needs to belong to the buffer that was used to parse.
    csubstr src = m_buf;
    _RYML_CB_CHECK(m_evt_handler->m_stack.m_callbacks, val != nullptr || src.str == nullptr);
    _RYML_CB_CHECK(m_evt_handler->m_stack.m_callbacks, (val >= src.begin() && val <= src.end()) || (src.str == nullptr && val == nullptr));
    // ok. search the first stored newline after the given ptr
    using lineptr_type = size_t const* C4_RESTRICT;
    lineptr_type lineptr = nullptr;
    size_t offset = (size_t)(val - src.begin());
    if(m_newline_offsets_size < RYML_LOCATIONS_SMALL_THRESHOLD)
    {
        // just do a linear search if the size is small.
        for(lineptr_type curr = m_newline_offsets, last = m_newline_offsets + m_newline_offsets_size; curr < last; ++curr)
        {
            if(*curr > offset)
            {
                lineptr = curr;
                break;
            }
        }
    }
    else
    {
        // do a bisection search if the size is not small.
        //
        // We could use std::lower_bound but this is simple enough and
        // spares the costly include of <algorithm>.
        size_t count = m_newline_offsets_size;
        size_t step;
        lineptr_type it;
        lineptr = m_newline_offsets;
        while(count)
        {
            step = count >> 1;
            it = lineptr + step;
            if(*it < offset)
            {
                lineptr = ++it;
                count -= step + 1;
            }
            else
            {
                count = step;
            }
        }
    }
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, lineptr >= m_newline_offsets);
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, lineptr <= m_newline_offsets + m_newline_offsets_size);
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, *lineptr > offset);
    Location loc;
    loc.name = m_file;
    loc.offset = offset;
    loc.line = (size_t)(lineptr - m_newline_offsets);
    if(lineptr > m_newline_offsets)
        loc.col = (offset - *(lineptr-1) - 1u);
    else
        loc.col = offset;
    return loc;
}

template<class EventHandler>
void ParseEngine<EventHandler>::_prepare_locations()
{
    m_newline_offsets_buf = m_buf;
    size_t numnewlines = 1u + m_buf.count('\n');
    _resize_locations(numnewlines);
    m_newline_offsets_size = 0;
    for(size_t i = 0; i < m_buf.len; i++)
        if(m_buf[i] == '\n')
            m_newline_offsets[m_newline_offsets_size++] = i;
    m_newline_offsets[m_newline_offsets_size++] = m_buf.len;
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, m_newline_offsets_size == numnewlines);
}

template<class EventHandler>
void ParseEngine<EventHandler>::_resize_locations(size_t numnewlines)
{
    if(numnewlines > m_newline_offsets_capacity)
    {
        if(m_newline_offsets)
            _RYML_CB_FREE(m_evt_handler->m_stack.m_callbacks, m_newline_offsets, size_t, m_newline_offsets_capacity);
        m_newline_offsets = _RYML_CB_ALLOC_HINT(m_evt_handler->m_stack.m_callbacks, size_t, numnewlines, m_newline_offsets);
        m_newline_offsets_capacity = numnewlines;
    }
}

template<class EventHandler>
bool ParseEngine<EventHandler>::_locations_dirty() const
{
    return !m_newline_offsets_size;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_flow_skip_whitespace()
{
    // don't assign to csubstr rem: otherwise, gcc12,13,14 -O3 -m32 misbuilds
    if(m_evt_handler->m_curr->line_contents.rem.len > 0)
    {
        if(m_evt_handler->m_curr->line_contents.rem.str[0] == ' ' || m_evt_handler->m_curr->line_contents.rem.str[0] == '\t')
        {
            _c4dbgpf("starts with whitespace: '{}'", _c4prc(m_evt_handler->m_curr->line_contents.rem.str[0]));
            _skipchars(" \t");
        }
        // comments
        if(m_evt_handler->m_curr->line_contents.rem.begins_with('#'))
        {
            _c4dbgpf("it's a comment: {}", m_evt_handler->m_curr->line_contents.rem);
            _line_progressed(m_evt_handler->m_curr->line_contents.rem.len);
        }
    }
}


//-----------------------------------------------------------------------------


template<class EventHandler>
void ParseEngine<EventHandler>::_handle_colon()
{
    size_t curr = m_evt_handler->m_curr->pos.line;
    if(m_prev_colon != npos)
    {
        if(curr == m_prev_colon)
            _c4err("two colons on same line");
    }
    m_prev_colon = curr;
}

template<class EventHandler>
void ParseEngine<EventHandler>::_add_annotation(Annotation *C4_RESTRICT dst, csubstr str, size_t indentation, size_t line)
{
    _c4dbgpf("store annotation[{}]: '{}' indentation={} line={}", dst->num_entries, str, indentation, line);
    if(C4_UNLIKELY(dst->num_entries >= C4_COUNTOF(dst->annotations))) // NOLINT(bugprone-sizeof-expression)
        _c4err("too many annotations");
    dst->annotations[dst->num_entries].str = str;
    dst->annotations[dst->num_entries].indentation = indentation;
    dst->annotations[dst->num_entries].line = line;
    ++dst->num_entries;
}

template<class EventHandler>
void ParseEngine<EventHandler>::_clear_annotations(Annotation *C4_RESTRICT dst)
{
    dst->num_entries = 0;
}

#ifdef RYML_NO_COVERAGE__TO_BE_DELETED
template<class EventHandler>
bool ParseEngine<EventHandler>::_handle_indentation_from_annotations()
{
    if(m_pending_anchors.num_entries == 1u || m_pending_tags.num_entries == 1u)
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, m_pending_anchors.num_entries < 2u && m_pending_tags.num_entries < 2u);
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, m_pending_anchors.annotations[0].line < m_evt_handler->m_curr->pos.line);
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, m_pending_tags.annotations[1].line < m_evt_handler->m_curr->pos.line);
        size_t to_skip = m_evt_handler->m_curr->indref;
        if(m_pending_anchors.num_entries)
            to_skip = m_pending_anchors.annotations[0].indentation > to_skip ? m_pending_anchors.annotations[0].indentation : to_skip;
        if(m_pending_tags.num_entries)
            to_skip = m_pending_tags.annotations[0].indentation > to_skip ? m_pending_tags.annotations[0].indentation : to_skip;
        _c4dbgpf("annotations pending, skip indentation up to {}!", to_skip);
        _maybe_skipchars_up_to(' ', to_skip);
        return true;
    }
    return false;
}
#endif

template<class EventHandler>
bool ParseEngine<EventHandler>::_annotations_require_key_container() const
{
    return m_pending_tags.num_entries > 1 || m_pending_anchors.num_entries > 1;
}

template<class EventHandler>
void ParseEngine<EventHandler>::_check_tag(csubstr tag)
{
    if(!tag.begins_with("!<"))
    {
        if(C4_UNLIKELY(tag.first_of("[]{},") != npos))
            _RYML_CB_ERR_(m_evt_handler->m_stack.m_callbacks, "tags must not contain any of '[]{},'", m_evt_handler->m_curr->pos);
    }
    else
    {
        if(C4_UNLIKELY(!tag.ends_with('>')))
            _RYML_CB_ERR_(m_evt_handler->m_stack.m_callbacks, "malformed tag", m_evt_handler->m_curr->pos);
    }
}

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_annotations_before_blck_key_scalar()
{
    _c4dbgpf("annotations_before_blck_key_scalar, node={}", m_evt_handler->m_curr->node_id);
    if(m_pending_tags.num_entries)
    {
        _c4dbgpf("annotations_before_blck_key_scalar, #tags={}", m_pending_tags.num_entries);
        if(C4_LIKELY(m_pending_tags.num_entries == 1))
        {
            _check_tag(m_pending_tags.annotations[0].str);
            m_evt_handler->set_key_tag(m_pending_tags.annotations[0].str);
            _clear_annotations(&m_pending_tags);
        }
        else
        {
            _c4err("too many tags");
        }
    }
    if(m_pending_anchors.num_entries)
    {
        _c4dbgpf("annotations_before_blck_key_scalar, #anchors={}", m_pending_anchors.num_entries);
        if(C4_LIKELY(m_pending_anchors.num_entries == 1))
        {
            m_evt_handler->set_key_anchor(m_pending_anchors.annotations[0].str);
            _clear_annotations(&m_pending_anchors);
        }
        else
        {
            _c4err("too many anchors");
        }
    }
}

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_annotations_before_blck_val_scalar()
{
    _c4dbgpf("annotations_before_blck_val_scalar, node={}", m_evt_handler->m_curr->node_id);
    if(m_pending_tags.num_entries)
    {
        _c4dbgpf("annotations_before_blck_val_scalar, #tags={}", m_pending_tags.num_entries);
        if(C4_LIKELY(m_pending_tags.num_entries == 1))
        {
            _check_tag(m_pending_tags.annotations[0].str);
            m_evt_handler->set_val_tag(m_pending_tags.annotations[0].str);
            _clear_annotations(&m_pending_tags);
        }
        else
        {
            _c4err("too many tags");
        }
    }
    if(m_pending_anchors.num_entries)
    {
        _c4dbgpf("annotations_before_blck_val_scalar, #anchors={}", m_pending_anchors.num_entries);
        if(C4_LIKELY(m_pending_anchors.num_entries == 1))
        {
            m_evt_handler->set_val_anchor(m_pending_anchors.annotations[0].str);
            _clear_annotations(&m_pending_anchors);
        }
        else
        {
            _c4err("too many anchors");
        }
    }
}

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_annotations_before_start_mapblck(size_t current_line)
{
    _c4dbgpf("annotations_before_start_mapblck, current_line={}", current_line);
    if(m_pending_tags.num_entries == 2)
    {
        _c4dbgp("2 tags, setting entry 0");
        _check_tag(m_pending_tags.annotations[0].str);
        m_evt_handler->set_val_tag(m_pending_tags.annotations[0].str);
    }
    else if(m_pending_tags.num_entries == 1)
    {
        _c4dbgpf("1 tag. line={}, curr={}", m_pending_tags.annotations[0].line);
        if(m_pending_tags.annotations[0].line < current_line)
        {
            _c4dbgp("...tag is for the map. setting it.");
            _check_tag(m_pending_tags.annotations[0].str);
            m_evt_handler->set_val_tag(m_pending_tags.annotations[0].str);
            _clear_annotations(&m_pending_tags);
        }
    }
    //
    if(m_pending_anchors.num_entries == 2)
    {
        _c4dbgp("2 anchors, setting entry 0");
        m_evt_handler->set_val_anchor(m_pending_anchors.annotations[0].str);
    }
    else if(m_pending_anchors.num_entries == 1)
    {
        _c4dbgpf("1 anchor. line={}, curr={}", m_pending_anchors.annotations[0].line);
        if(m_pending_anchors.annotations[0].line < current_line)
        {
            _c4dbgp("...anchor is for the map. setting it.");
            m_evt_handler->set_val_anchor(m_pending_anchors.annotations[0].str);
            _clear_annotations(&m_pending_anchors);
        }
    }
}

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_annotations_before_start_mapblck_as_key()
{
    _c4dbgp("annotations_before_start_mapblck_as_key");
    if(m_pending_tags.num_entries == 2)
    {
        _check_tag(m_pending_tags.annotations[0].str);
        m_evt_handler->set_key_tag(m_pending_tags.annotations[0].str);
    }
    if(m_pending_anchors.num_entries == 2)
    {
        m_evt_handler->set_key_anchor(m_pending_anchors.annotations[0].str);
    }
}

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_annotations_and_indentation_after_start_mapblck(size_t key_indentation, size_t key_line)
{
    _c4dbgp("annotations_after_start_mapblck");
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, m_pending_tags.num_entries <= 2);
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, m_pending_anchors.num_entries <= 2);
    if(m_pending_anchors.num_entries || m_pending_tags.num_entries)
    {
        key_indentation = _select_indentation_from_annotations(key_indentation, key_line);
        switch(m_pending_tags.num_entries)
        {
        case 1u:
            _check_tag(m_pending_tags.annotations[0].str);
            m_evt_handler->set_key_tag(m_pending_tags.annotations[0].str);
            _clear_annotations(&m_pending_tags);
            break;
        case 2u:
            _check_tag(m_pending_tags.annotations[1].str);
            m_evt_handler->set_key_tag(m_pending_tags.annotations[1].str);
            _clear_annotations(&m_pending_tags);
            break;
        }
        switch(m_pending_anchors.num_entries)
        {
        case 1u:
            m_evt_handler->set_key_anchor(m_pending_anchors.annotations[0].str);
            _clear_annotations(&m_pending_anchors);
            break;
        case 2u:
            m_evt_handler->set_key_anchor(m_pending_anchors.annotations[1].str);
            _clear_annotations(&m_pending_anchors);
            break;
        }
    }
    _set_indentation(key_indentation);
}

template<class EventHandler>
size_t ParseEngine<EventHandler>::_select_indentation_from_annotations(size_t val_indentation, size_t val_line)
{
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, m_pending_tags.num_entries || m_pending_anchors.num_entries);
    // select the left-most annotation on the max line
    auto const *C4_RESTRICT curr = m_pending_anchors.num_entries ? &m_pending_anchors.annotations[0] : &m_pending_tags.annotations[0];
    for(size_t i = 0; i < m_pending_anchors.num_entries; ++i)
    {
        auto const& C4_RESTRICT ann = m_pending_anchors.annotations[i];
        if(ann.line > curr->line)
            curr = &ann;
        else if(ann.indentation < curr->indentation)
            curr = &ann;
    }
    for(size_t j = 0; j < m_pending_tags.num_entries; ++j)
    {
        auto const& C4_RESTRICT ann = m_pending_tags.annotations[j];
        if(ann.line > curr->line)
            curr = &ann;
        else if(ann.indentation < curr->indentation)
            curr = &ann;
    }
    return curr->line < val_line ? val_indentation : curr->indentation;
}

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_directive(csubstr rem)
{
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, rem.is_sub(m_evt_handler->m_curr->line_contents.rem));
    const size_t pos = rem.find('#');
    _c4dbgpf("handle_directive: pos={} rem={}", pos, rem);
    if(pos == npos) // no comments
    {
        m_evt_handler->add_directive(rem);
        _line_progressed(rem.len);
    }
    else
    {
        csubstr to_comment = rem.first(pos);
        csubstr trimmed = to_comment.trimr(" \t");
        m_evt_handler->add_directive(trimmed);
        _line_progressed(pos);
        _skip_comment();
    }
}

template<class EventHandler>
bool ParseEngine<EventHandler>::_handle_bom()
{
    const csubstr rem = m_evt_handler->m_curr->line_contents.rem;
    if(rem.len)
    {
        const csubstr rest = rem.sub(1);
        // https://yaml.org/spec/1.2.2/#52-character-encodings
        #define _rymlisascii(c) ((c) > '\0' && (c) <= '\x7f') // is the character ASCII?
        if(rem.begins_with({"\x00\x00\xfe\xff", 4}) || (rem.begins_with({"\x00\x00\x00", 3}) && rem.len >= 4u && _rymlisascii(rem.str[3])))
        {
            _c4dbgp("byte order mark: UTF32BE");
            _handle_bom(UTF32BE);
            _line_progressed(4);
            return true;
        }
        else if(rem.begins_with("\xff\xfe\x00\x00") || (rest.begins_with({"\x00\x00\x00", 3}) && rem.len >= 4u && _rymlisascii(rem.str[0])))
        {
            _c4dbgp("byte order mark: UTF32LE");
            _handle_bom(UTF32LE);
            _line_progressed(4);
            return true;
        }
        else if(rem.begins_with("\xfe\xff") || (rem.begins_with('\x00') && rem.len >= 2u && _rymlisascii(rem.str[1])))
        {
            _c4dbgp("byte order mark: UTF16BE");
            _handle_bom(UTF16BE);
            _line_progressed(2);
            return true;
        }
        else if(rem.begins_with("\xff\xfe") || (rest.begins_with('\x00') && rem.len >= 2u && _rymlisascii(rem.str[0])))
        {
            _c4dbgp("byte order mark: UTF16LE");
            _handle_bom(UTF16LE);
            _line_progressed(2);
            return true;
        }
        else if(rem.begins_with("\xef\xbb\xbf"))
        {
            _c4dbgp("byte order mark: UTF8");
            _handle_bom(UTF8);
            _line_progressed(3);
            return true;
        }
        #undef _rymlisascii
    }
    return false;
}

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_bom(Encoding_e enc)
{
    if(m_encoding == NOBOM)
    {
        const bool is_beginning_of_file = m_evt_handler->m_curr->line_contents.rem.str == m_buf.str;
        if(enc == UTF8 || is_beginning_of_file)
            m_encoding = enc;
        else
            _c4err("non-UTF8 byte order mark can appear only at the beginning of the file");
    }
    else if(enc != m_encoding)
    {
        _c4err("byte order mark can only be set once");
    }
}


//-----------------------------------------------------------------------------

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_seq_json()
{
seqjson_start:
    _c4dbgpf("handle2_seq_json: node_id={} level={} indentation={}", m_evt_handler->m_curr->node_id, m_evt_handler->m_curr->level, m_evt_handler->m_curr->indref);

    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RKEY));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_all(RSEQ));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_all(FLOW));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(RVAL|RNXT));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_all(RVAL) != has_all(RNXT));

    _handle_flow_skip_whitespace();
    csubstr rem = m_evt_handler->m_curr->line_contents.rem;
    if(!rem.len)
        goto seqjson_again;

    if(has_any(RVAL))
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RNXT));
        const char first = rem.str[0];
        _c4dbgpf("mapjson[RVAL]: '{}'", first);
        switch(first)
        {
        case '"':
        {
            _c4dbgp("seqjson[RVAL]: scanning double-quoted scalar");
            ScannedScalar sc = _scan_scalar_dquot();
            csubstr maybe_filtered = _maybe_filter_val_scalar_dquot(sc);
            m_evt_handler->set_val_scalar_dquoted(maybe_filtered);
            addrem_flags(RNXT, RVAL);
            break;
        }
        case '[':
        {
            _c4dbgp("seqjson[RVAL]: start child seqjson");
            addrem_flags(RNXT, RVAL);
            m_evt_handler->begin_seq_val_flow();
            addrem_flags(RVAL, RNXT);
            _line_progressed(1);
            break;
        }
        case '{':
        {
            _c4dbgp("seqjson[RVAL]: start child mapjson");
            addrem_flags(RNXT, RVAL);
            m_evt_handler->begin_map_val_flow();
            addrem_flags(RMAP|RKEY, RSEQ|RVAL|RNXT);
            _line_progressed(1);
            goto seqjson_finish;
        }
        case ']': // this happens on a trailing comma like ", ]"
        {
            _c4dbgp("seqjson[RVAL]: end!");
            rem_flags(RSEQ);
            m_evt_handler->end_seq();
            _line_progressed(1);
            if(!has_all(RSEQ|FLOW))
                goto seqjson_finish;
            break;
        }
        default:
        {
            ScannedScalar sc;
            if(_scan_scalar_seq_json(&sc))
            {
                _c4dbgp("seqjson[RVAL]: it's a plain scalar.");
                csubstr maybe_filtered = _maybe_filter_val_scalar_plain(sc, m_evt_handler->m_curr->indref);
                m_evt_handler->set_val_scalar_plain(maybe_filtered);
                addrem_flags(RNXT, RVAL);
            }
            else
            {
                _c4err("parse error");
            }
        }
        }
    }
    else // RNXT
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(RNXT));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RVAL));
        const char first = rem.str[0];
        _c4dbgpf("mapjson[RNXT]: '{}'", first);
        switch(first)
        {
        case ',':
        {
            _c4dbgp("seqjson[RNXT]: expect next val");
            addrem_flags(RVAL, RNXT);
            m_evt_handler->add_sibling();
            _line_progressed(1);
            break;
        }
        case ']':
        {
            _c4dbgp("seqjson[RNXT]: end!");
            m_evt_handler->end_seq();
            _line_progressed(1);
            goto seqjson_finish;
        }
        default:
            _c4err("parse error");
        }
    }

 seqjson_again:
    _c4dbgt("seqjson: go again", 0);
    if(_finished_line())
    {
        if(C4_LIKELY(!_finished_file()))
        {
            _line_ended();
            _scan_line();
            _c4dbgnextline();
        }
        else
        {
            _c4err("missing terminating ]");
        }
    }
    goto seqjson_start;

 seqjson_finish:
    _c4dbgp("seqjson: finish");
}


//-----------------------------------------------------------------------------

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_map_json()
{
mapjson_start:
    _c4dbgpf("handle2_map_json: node_id={} level={} indentation={}", m_evt_handler->m_curr->node_id, m_evt_handler->m_curr->level, m_evt_handler->m_curr->indref);

    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_all(RMAP));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_all(FLOW));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(QMRK));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(RKEY|RKCL|RVAL|RNXT));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, 1 == (has_any(RKEY) + has_any(RKCL) + has_any(RVAL) + has_any(RNXT)));

    _handle_flow_skip_whitespace();
    csubstr rem = m_evt_handler->m_curr->line_contents.rem;
    if(!rem.len)
        goto mapjson_again;

    if(has_any(RKEY))
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RKCL));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RVAL));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RNXT));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(QMRK));
        const char first = rem.str[0];
        _c4dbgpf("mapjson[RKEY]: '{}'", first);
        switch(first)
        {
        case '"':
        {
            _c4dbgp("mapjson[RKEY]: scanning double-quoted scalar");
            ScannedScalar sc = _scan_scalar_dquot();
            csubstr maybe_filtered = _maybe_filter_key_scalar_dquot(sc);
            m_evt_handler->set_key_scalar_dquoted(maybe_filtered);
            addrem_flags(RKCL, RKEY);
            break;
        }
        case '}': // this happens on a trailing comma like ", }"
        {
            _c4dbgp("mapjson[RKEY]: end!");
            m_evt_handler->end_map();
            _line_progressed(1);
            goto mapjson_finish;
        }
        default:
            _c4err("parse error");
        }
    }
    else if(has_any(RVAL))
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RKEY));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RKCL));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RNXT));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(QMRK));
        const char first = rem.str[0];
        _c4dbgpf("mapjson[RVAL]: '{}'", first);
        switch(first)
        {
        case '"':
        {
            _c4dbgp("mapjson[RVAL]: scanning double-quoted scalar");
            ScannedScalar sc = _scan_scalar_dquot();
            csubstr maybe_filtered = _maybe_filter_val_scalar_dquot(sc);
            m_evt_handler->set_val_scalar_dquoted(maybe_filtered);
            addrem_flags(RNXT, RVAL);
            break;
        }
        case '[':
        {
            _c4dbgp("mapjson[RVAL]: start val seqjson");
            addrem_flags(RNXT, RVAL);
            m_evt_handler->begin_seq_val_flow();
            _set_indentation(m_evt_handler->m_parent->indref);
            addrem_flags(RSEQ|RVAL, RMAP|RNXT);
            _line_progressed(1);
            goto mapjson_finish;
        }
        case '{':
        {
            _c4dbgp("mapjson[RVAL]: start val mapjson");
            addrem_flags(RNXT, RVAL);
            m_evt_handler->begin_map_val_flow();
            _set_indentation(m_evt_handler->m_parent->indref);
            addrem_flags(RKEY, RNXT);
            _line_progressed(1);
            // keep going in this function
            break;
        }
        default:
        {
            ScannedScalar sc;
            if(_scan_scalar_map_json(&sc))
            {
                _c4dbgp("mapjson[RVAL]: plain scalar.");
                csubstr maybe_filtered = _maybe_filter_val_scalar_plain(sc, m_evt_handler->m_curr->indref);
                m_evt_handler->set_val_scalar_plain(maybe_filtered);
                addrem_flags(RNXT, RVAL);
            }
            else
            {
                _c4err("parse error");
            }
            break;
        }
        }
    }
    else if(has_any(RKCL)) // read the key colon
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RKEY));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RVAL));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RNXT));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(QMRK));
        const char first = rem.str[0];
        _c4dbgpf("mapjson[RKCL]: '{}'", first);
        if(first == ':')
        {
            _c4dbgp("mapjson[RKCL]: found the colon");
            addrem_flags(RVAL, RKCL);
            _line_progressed(1);
        }
        else
        {
            _c4err("parse error");
        }
    }
    else if(has_any(RNXT))
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RKEY));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RKCL));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RVAL));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(QMRK));
        _c4dbgpf("mapjson[RNXT]: '{}'", rem.str[0]);
        if(rem.begins_with(','))
        {
            _c4dbgp("mapjson[RNXT]: expect next keyval");
            m_evt_handler->add_sibling();
            addrem_flags(RKEY, RNXT);
            _line_progressed(1);
        }
        else if(rem.begins_with('}'))
        {
            _c4dbgp("mapjson[RNXT]: end!");
            m_evt_handler->end_map();
            _line_progressed(1);
            goto mapjson_finish;
        }
        else
        {
            _c4err("parse error");
        }
    }

 mapjson_again:
    _c4dbgt("mapjson: go again", 0);
    if(_finished_line())
    {
        if(C4_LIKELY(!_finished_file()))
        {
            _line_ended();
            _scan_line();
            _c4dbgnextline();
        }
        else
        {
            _c4err("missing terminating }");
        }
    }
    goto mapjson_start;

 mapjson_finish:
    _c4dbgp("mapjson: finish");
}


//-----------------------------------------------------------------------------

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_seq_imap()
{
seqimap_start:
    _c4dbgpf("handle2_seq_imap: node_id={} level={} indref={}", m_evt_handler->m_curr->node_id, m_evt_handler->m_curr->level, m_evt_handler->m_curr->indref);

    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_all(RSEQIMAP));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RKEY));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(RVAL|RNXT|QMRK|RKCL));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, 1 == has_all(RVAL) + has_all(RNXT) + has_all(QMRK) + has_all(RKCL));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_stack.size() >= 3);

    _handle_flow_skip_whitespace();
    csubstr rem = m_evt_handler->m_curr->line_contents.rem;
    if(!rem.len)
        goto seqimap_again;

    if(has_any(RVAL))
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(RVAL));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RNXT));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(QMRK));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RKCL));
        const char first = rem.str[0];
        _c4dbgpf("seqimap[RVAL]: '{}'", _c4prc(first));
        ScannedScalar sc;
        if(first == '\'')
        {
            _c4dbgp("seqimap[RVAL]: scanning single-quoted scalar");
            sc = _scan_scalar_squot();
            csubstr maybe_filtered = _maybe_filter_val_scalar_squot(sc);
            m_evt_handler->set_val_scalar_squoted(maybe_filtered);
            m_evt_handler->end_map();
            goto seqimap_finish;
        }
        else if(first == '"')
        {
            _c4dbgp("seqimap[RVAL]: scanning double-quoted scalar");
            sc = _scan_scalar_dquot();
            csubstr maybe_filtered = _maybe_filter_val_scalar_dquot(sc);
            m_evt_handler->set_val_scalar_dquoted(maybe_filtered);
            m_evt_handler->end_map();
            goto seqimap_finish;
        }
        // block scalars (ie | and >) cannot appear in flow containers
        else if(_scan_scalar_plain_map_flow(&sc))
        {
            _c4dbgp("seqimap[RVAL]: it's a scalar.");
            csubstr maybe_filtered = _maybe_filter_val_scalar_plain(sc, m_evt_handler->m_curr->indref);
            m_evt_handler->set_val_scalar_plain(maybe_filtered);
            m_evt_handler->end_map();
            goto seqimap_finish;
        }
        else if(first == '[')
        {
            _c4dbgp("seqimap[RVAL]: start child seqflow");
            addrem_flags(RNXT, RVAL);
            m_evt_handler->begin_seq_val_flow();
            addrem_flags(RVAL, RNXT|RSEQIMAP);
            _set_indentation(m_evt_handler->m_parent->indref);
            _line_progressed(1);
            goto seqimap_finish;
        }
        else if(first == '{')
        {
            _c4dbgp("seqimap[RVAL]: start child mapflow");
            addrem_flags(RNXT, RVAL);
            m_evt_handler->begin_map_val_flow();
            addrem_flags(RMAP|RKEY, RSEQ|RVAL|RSEQIMAP|RNXT);
            _set_indentation(m_evt_handler->m_parent->indref);
            _line_progressed(1);
            goto seqimap_finish;
        }
        else if(first == ',' || first == ']')
        {
            _c4dbgp("seqimap[RVAL]: finish without val.");
            m_evt_handler->set_val_scalar_plain_empty();
            m_evt_handler->end_map();
            goto seqimap_finish;
        }
        else if(first == '&')
        {
            csubstr anchor = _scan_anchor();
            _c4dbgp("seqimap[RVAL]: anchor!");
            m_evt_handler->set_val_anchor(anchor);
        }
        else if(first == '*')
        {
            csubstr ref = _scan_ref_seq();
            _c4dbgp("seqimap[RVAL]: ref!");
            m_evt_handler->set_val_ref(ref);
            addrem_flags(RNXT, RVAL);
        }
        else
        {
            _c4err("parse error");
        }
    }
    else if(has_any(RNXT))
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(RNXT));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RVAL));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(QMRK));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RKCL));
        const char first = rem.str[0];
        _c4dbgpf("seqimap[RNXT]: '{}'", _c4prc(first));
        if(first == ',' || first == ']')
        {
            // we may get here because a map or a seq started and we
            // return later
            _c4dbgp("seqimap: done");
            m_evt_handler->end_map();
            goto seqimap_finish;
        }
        else
        {
            _c4err("parse error");
        }
    }
    else if(has_any(QMRK))
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(QMRK));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RVAL));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RNXT));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RKCL));
        const char first = rem.str[0];
        _c4dbgpf("seqimap[QMRK]: '{}'", _c4prc(first));
        ScannedScalar sc;
        if(first == '\'')
        {
            _c4dbgp("seqimap[QMRK]: scanning single-quoted scalar");
            sc = _scan_scalar_squot();
            csubstr maybe_filtered = _maybe_filter_key_scalar_squot(sc);
            m_evt_handler->set_key_scalar_squoted(maybe_filtered);
            addrem_flags(RKCL, QMRK);
            goto seqimap_again;
        }
        else if(first == '"')
        {
            _c4dbgp("seqimap[QMRK]: scanning double-quoted scalar");
            sc = _scan_scalar_dquot();
            csubstr maybe_filtered = _maybe_filter_key_scalar_dquot(sc);
            m_evt_handler->set_key_scalar_dquoted(maybe_filtered);
            addrem_flags(RKCL, QMRK);
            goto seqimap_again;
        }
        // block scalars (ie | and >) cannot appear in flow containers
        else if(_scan_scalar_plain_map_flow(&sc))
        {
            _c4dbgp("seqimap[QMRK]: it's a scalar.");
            csubstr maybe_filtered = _maybe_filter_key_scalar_plain(sc, m_evt_handler->m_curr->indref);
            m_evt_handler->set_key_scalar_plain(maybe_filtered);
            addrem_flags(RKCL, QMRK);
            goto seqimap_again;
        }
        else if(first == '[')
        {
            _c4dbgp("seqimap[QMRK]: start child seqflow");
            addrem_flags(RKCL, QMRK);
            m_evt_handler->begin_seq_key_flow();
            addrem_flags(RSEQ|RVAL, RKCL|RSEQIMAP);
            _set_indentation(m_evt_handler->m_parent->indref);
            _line_progressed(1);
            goto seqimap_finish;
        }
        else if(first == '{')
        {
            _c4dbgp("seqimap[QMRK]: start child mapflow");
            addrem_flags(RKCL, QMRK);
            m_evt_handler->begin_map_key_flow();
            addrem_flags(RMAP|RKEY, RSEQ|RKCL|RSEQIMAP);
            _set_indentation(m_evt_handler->m_parent->indref);
            _line_progressed(1);
            goto seqimap_finish;
        }
        else if(first == ',' || first == ']')
        {
            _c4dbgp("seqimap[QMRK]: finish without key.");
            m_evt_handler->set_key_scalar_plain_empty();
            m_evt_handler->set_val_scalar_plain_empty();
            m_evt_handler->end_map();
            goto seqimap_finish;
        }
        else if(first == '&')
        {
            csubstr anchor = _scan_anchor();
            _c4dbgp("seqimap[QMRK]: anchor!");
            m_evt_handler->set_key_anchor(anchor);
        }
        else if(first == '*')
        {
            csubstr ref = _scan_ref_seq();
            _c4dbgp("seqimap[QMRK]: ref!");
            m_evt_handler->set_key_ref(ref);
            addrem_flags(RKCL, QMRK);
        }
        else
        {
            _c4err("parse error");
        }
    }
    else if(has_any(RKCL))
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RVAL));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RNXT));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(QMRK));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(RKCL));
        const char first = rem.str[0];
        _c4dbgpf("seqimap[RKCL]: '{}'", _c4prc(first));
        if(first == ':')
        {
            _c4dbgp("seqimap[RKCL]: found ':'");
            addrem_flags(RVAL, RKCL);
            _line_progressed(1);
            goto seqimap_again;
        }
        else if(first == ',' || first == ']')
        {
            _c4dbgp("seqimap[RKCL]: found ','. finish without val");
            m_evt_handler->set_val_scalar_plain_empty();
            m_evt_handler->end_map();
            goto seqimap_finish;
        }
        else
        {
            _c4err("parse error");
        }
    }

 seqimap_again:
    _c4dbgt("seqimap: go again", 0);
    if(_finished_line())
    {
        if(C4_LIKELY(!_finished_file()))
        {
            _line_ended();
            _scan_line();
            _c4dbgnextline();
        }
        else
        {
            _c4err("parse error");
        }
    }
    goto seqimap_start;

 seqimap_finish:
    _c4dbgp("seqimap: finish");
}


//-----------------------------------------------------------------------------

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_seq_flow()
{
seqflow_start:
    _c4dbgpf("handle2_seq_flow: node_id={} level={} indentation={}", m_evt_handler->m_curr->node_id, m_evt_handler->m_curr->level, m_evt_handler->m_curr->indref);

    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RKEY));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_all(RSEQ));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_all(FLOW));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(RVAL|RNXT));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_all(RVAL) != has_all(RNXT));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->indref != npos);

    _handle_flow_skip_whitespace();
    // don't assign to csubstr rem: otherwise, gcc12,13,14 -O3 -m32 misbuilds
    if(!m_evt_handler->m_curr->line_contents.rem.len)
        goto seqflow_again;

    if(has_any(RVAL))
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RNXT));
        const char first = m_evt_handler->m_curr->line_contents.rem.str[0];
        ScannedScalar sc;
        if(first == '\'')
        {
            _c4dbgp("seqflow[RVAL]: scanning single-quoted scalar");
            sc = _scan_scalar_squot();
            csubstr maybe_filtered = _maybe_filter_val_scalar_squot(sc);
            m_evt_handler->set_val_scalar_squoted(maybe_filtered);
            addrem_flags(RNXT, RVAL);
        }
        else if(first == '"')
        {
            _c4dbgp("seqflow[RVAL]: scanning double-quoted scalar");
            sc = _scan_scalar_dquot();
            csubstr maybe_filtered = _maybe_filter_val_scalar_dquot(sc);
            m_evt_handler->set_val_scalar_dquoted(maybe_filtered);
            addrem_flags(RNXT, RVAL);
        }
        // block scalars (ie | and >) cannot appear in flow containers
        else if(_scan_scalar_plain_seq_flow(&sc))
        {
            _c4dbgp("seqflow[RVAL]: it's a scalar.");
            csubstr maybe_filtered = _maybe_filter_val_scalar_plain(sc, m_evt_handler->m_curr->indref);
            m_evt_handler->set_val_scalar_plain(maybe_filtered);
            addrem_flags(RNXT, RVAL);
        }
        else if(first == '[')
        {
            _c4dbgp("seqflow[RVAL]: start child seqflow");
            addrem_flags(RNXT, RVAL);
            m_evt_handler->begin_seq_val_flow();
            _set_indentation(m_evt_handler->m_parent->indref);
            addrem_flags(RVAL, RNXT);
            _line_progressed(1);
        }
        else if(first == '{')
        {
            _c4dbgp("seqflow[RVAL]: start child mapflow");
            addrem_flags(RNXT, RVAL);
            m_evt_handler->begin_map_val_flow();
            _set_indentation(m_evt_handler->m_parent->indref);
            addrem_flags(RMAP|RKEY, RSEQ|RVAL|RNXT);
            _line_progressed(1);
            goto seqflow_finish;
        }
        else if(first == ']') // this happens on a trailing comma like ", ]"
        {
            _c4dbgp("seqflow[RVAL]: end!");
            _line_progressed(1);
            m_evt_handler->end_seq();
            goto seqflow_finish;
        }
        else if(first == '*')
        {
            csubstr ref = _scan_ref_seq();
            _c4dbgpf("seqflow[RVAL]: ref! [{}]~~~{}~~~", ref.len, ref);
            m_evt_handler->set_val_ref(ref);
            addrem_flags(RNXT, RVAL);
        }
        else if(first == '&')
        {
            csubstr anchor = _scan_anchor();
            _c4dbgpf("seqflow[RVAL]: anchor! [{}]~~~{}~~~", anchor.len, anchor);
            m_evt_handler->set_val_anchor(anchor);
            if(_maybe_scan_following_comma())
            {
                _c4dbgp("seqflow[RVAL]: empty scalar!");
                m_evt_handler->set_val_scalar_plain_empty();
                m_evt_handler->add_sibling();
            }
        }
        else if(first == '!')
        {
            csubstr tag = _scan_tag();
            _c4dbgpf("seqflow[RVAL]: tag! [{}]~~~{}~~~", tag.len, tag);
            _check_tag(tag);
            m_evt_handler->set_val_tag(tag);
            if(_maybe_scan_following_comma())
            {
                _c4dbgp("seqflow[RVAL]: empty scalar!");
                m_evt_handler->set_val_scalar_plain_empty();
                m_evt_handler->add_sibling();
            }
        }
        else if(first == ':')
        {
            _c4dbgpf("seqflow[RVAL]: actually seqimap at node[{}], with empty key", m_evt_handler->m_curr->node_id);
            addrem_flags(RNXT, RVAL);
            m_evt_handler->begin_map_val_flow();
            _set_indentation(m_evt_handler->m_parent->indref);
            m_evt_handler->set_key_scalar_plain_empty();
            addrem_flags(RSEQIMAP|RVAL, RSEQ|RNXT);
            _line_progressed(1);
            goto seqflow_finish;
        }
        else if(first == '?')
        {
            _c4dbgp("seqflow[RVAL]: start child mapflow, explicit key");
            addrem_flags(RNXT, RVAL);
            m_was_inside_qmrk = true;
            m_evt_handler->begin_map_val_flow();
            _set_indentation(m_evt_handler->m_parent->indref);
            addrem_flags(RSEQIMAP|QMRK, RSEQ|RNXT);
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
            goto seqflow_finish;
        }
        else
        {
            _c4err("parse error");
        }
    }
    else // RNXT
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(RNXT));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RVAL));
        const char first = m_evt_handler->m_curr->line_contents.rem.str[0];
        if(first == ',')
        {
            _c4dbgp("seqflow[RNXT]: expect next val");
            addrem_flags(RVAL, RNXT);
            m_evt_handler->add_sibling();
            _line_progressed(1);
        }
        else if(first == ']')
        {
            _c4dbgp("seqflow[RNXT]: end!");
            m_evt_handler->end_seq();
            _line_progressed(1);
            goto seqflow_finish;
        }
        else if(first == ':')
        {
            _c4dbgpf("seqflow[RNXT]: actually seqimap at node[{}]", m_evt_handler->m_curr->node_id);
            m_evt_handler->actually_val_is_first_key_of_new_map_flow();
            _set_indentation(m_evt_handler->m_parent->indref);
            _line_progressed(1);
            addrem_flags(RSEQIMAP|RVAL, RNXT);
            goto seqflow_finish;
        }
        else
        {
            _c4err("parse error");
        }
    }

 seqflow_again:
    _c4dbgt("seqflow: go again", 0);
    if(_finished_line())
    {
        if(C4_LIKELY(!_finished_file()))
        {
            _line_ended();
            _scan_line();
            _c4dbgnextline();
        }
        else
        {
            _c4err("missing terminating ]");
        }
    }
    goto seqflow_start;

 seqflow_finish:
    _c4dbgp("seqflow: finish");
}


//-----------------------------------------------------------------------------

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_map_flow()
{
mapflow_start:
    _c4dbgpf("handle2_map_flow: node_id={} level={} indentation={}", m_evt_handler->m_curr->node_id, m_evt_handler->m_curr->level, m_evt_handler->m_curr->indref);

    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_all(RMAP));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_all(FLOW));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(RKEY|RKCL|RVAL|RNXT|QMRK));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, 1 == (has_any(RKEY) + has_any(RKCL) + has_any(RVAL) + has_any(RNXT) + has_any(QMRK)));

    _handle_flow_skip_whitespace();
    csubstr rem = m_evt_handler->m_curr->line_contents.rem;
    if(!rem.len)
        goto mapflow_again;

    if(has_any(RKEY))
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RKCL));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RVAL));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RNXT));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(QMRK));
        const char first = rem.str[0];
        _c4dbgpf("mapflow[RKEY]: '{}'", first);
        ScannedScalar sc;
        if(first == '\'')
        {
            _c4dbgp("mapflow[RKEY]: scanning single-quoted scalar");
            sc = _scan_scalar_squot();
            csubstr maybe_filtered = _maybe_filter_key_scalar_squot(sc);
            m_evt_handler->set_key_scalar_squoted(maybe_filtered);
            addrem_flags(RKCL, RKEY|QMRK);
        }
        else if(first == '"')
        {
            _c4dbgp("mapflow[RKEY]: scanning double-quoted scalar");
            sc = _scan_scalar_dquot();
            csubstr maybe_filtered = _maybe_filter_key_scalar_dquot(sc);
            m_evt_handler->set_key_scalar_dquoted(maybe_filtered);
            addrem_flags(RKCL, RKEY|QMRK);
        }
        // block scalars (ie | and >) cannot appear in flow containers
        else if(_scan_scalar_plain_map_flow(&sc))
        {
            _c4dbgp("mapflow[RKEY]: plain scalar");
            csubstr maybe_filtered = _maybe_filter_key_scalar_plain(sc, m_evt_handler->m_curr->indref);
            m_evt_handler->set_key_scalar_plain(maybe_filtered);
            addrem_flags(RKCL, RKEY|QMRK);
        }
        else if(first == '?')
        {
            _c4dbgp("mapflow[RKEY]: explicit key");
            _line_progressed(1);
            addrem_flags(QMRK, RKEY);
            _maybe_skip_whitespace_tokens();
        }
        else if(first == ':')
        {
            _c4dbgp("mapflow[RKEY]: setting empty key");
            m_evt_handler->set_key_scalar_plain_empty();
            addrem_flags(RVAL, RKEY|QMRK);
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
        }
        else if(first == ',')
        {
            _c4dbgp("mapflow[RKEY]: empty key+val!");
            m_evt_handler->set_key_scalar_plain_empty();
            m_evt_handler->set_val_scalar_plain_empty();
            addrem_flags(RNXT, RKEY|QMRK);
            // keep going in this function
        }
        else if(first == '}') // this happens on a trailing comma like ", }"
        {
            _c4dbgp("mapflow[RKEY]: end!");
            m_evt_handler->end_map();
            _line_progressed(1);
            goto mapflow_finish;
        }
        else if(first == '&')
        {
            csubstr anchor = _scan_anchor();
            _c4dbgpf("mapflow[RKEY]: key anchor! [{}]~~~{}~~~", anchor.len, anchor);
            m_evt_handler->set_key_anchor(anchor);
        }
        else if(first == '*')
        {
            csubstr ref = _scan_ref_map();
            _c4dbgpf("mapflow[RKEY]: key ref! [{}]~~~{}~~~", ref.len, ref);
            m_evt_handler->set_key_ref(ref);
            addrem_flags(RKCL, RKEY);
        }
        else if(first == '[')
        {
            // RYML's tree cannot store container keys, but that's
            // handled inside the tree sink. Other sink types may be
            // able to handle it.
            _c4dbgp("mapflow[RKEY]: start child seqflow (!)");
            addrem_flags(RKCL, RKEY);
            m_evt_handler->begin_seq_key_flow();
            addrem_flags(RSEQ|RVAL, RMAP|RKCL);
            _set_indentation(m_evt_handler->m_parent->indref);
            _line_progressed(1);
            goto mapflow_finish;
        }
        else if(first == '{')
        {
            // RYML's tree cannot store container keys, but that's
            // handled inside the tree sink. Other sink types may be
            // able to handle it.
            _c4dbgp("mapflow[RKEY]: start child mapflow (!)");
            addrem_flags(RKCL, RKEY);
            m_evt_handler->begin_map_key_flow();
            addrem_flags(RKEY, RVAL|RKCL);
            _set_indentation(m_evt_handler->m_parent->indref);
            _line_progressed(1);
            // keep going in this function
        }
        else if(first == '!')
        {
            csubstr tag = _scan_tag();
            _c4dbgpf("mapflow[RKEY]: tag! [{}]~~~{}~~~", tag.len, tag);
            _check_tag(tag);
            m_evt_handler->set_key_tag(tag);
        }
        else
        {
            _c4err("parse error");
        }
    }
    else if(has_any(RKCL)) // read the key colon
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RKEY));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RVAL));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RNXT));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(QMRK));
        const char first = rem.str[0];
        _c4dbgpf("mapflow[RKCL]: '{}'", first);
        if(first == ':')
        {
            _c4dbgp("mapflow[RKCL]: found the colon");
            addrem_flags(RVAL, RKCL);
            _line_progressed(1);
        }
        else if(first == '}')
        {
            _c4dbgp("mapflow[RKCL]: end with missing val!");
            addrem_flags(RVAL, RKCL);
            m_evt_handler->set_val_scalar_plain_empty();
            m_evt_handler->end_map();
            _line_progressed(1);
            goto mapflow_finish;
        }
        else if(first == ',')
        {
            _c4dbgp("mapflow[RKCL]: got comma. val is missing");
            m_evt_handler->set_val_scalar_plain_empty();
            m_evt_handler->add_sibling();
            addrem_flags(RKEY, RKCL);
            _line_progressed(1);
        }
        else
        {
            _c4err("parse error");
        }
    }
    else if(has_any(RVAL))
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RKEY));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RKCL));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RNXT));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(QMRK));
        const char first = rem.str[0];
        _c4dbgpf("mapflow[RVAL]: '{}'", first);
        ScannedScalar sc;
        if(first == '\'')
        {
            _c4dbgp("mapflow[RVAL]: scanning single-quoted scalar");
            sc = _scan_scalar_squot();
            csubstr maybe_filtered = _maybe_filter_val_scalar_squot(sc);
            m_evt_handler->set_val_scalar_squoted(maybe_filtered);
            addrem_flags(RNXT, RVAL);
        }
        else if(first == '"')
        {
            _c4dbgp("mapflow[RVAL]: scanning double-quoted scalar");
            sc = _scan_scalar_dquot();
            csubstr maybe_filtered = _maybe_filter_val_scalar_dquot(sc);
            m_evt_handler->set_val_scalar_dquoted(maybe_filtered);
            addrem_flags(RNXT, RVAL);
        }
        // block scalars (ie | and >) cannot appear in flow containers
        else if(_scan_scalar_plain_map_flow(&sc))
        {
            _c4dbgp("mapflow[RVAL]: plain scalar.");
            csubstr maybe_filtered = _maybe_filter_val_scalar_plain(sc, m_evt_handler->m_curr->indref);
            m_evt_handler->set_val_scalar_plain(maybe_filtered);
            addrem_flags(RNXT, RVAL);
        }
        else if(first == '[')
        {
            _c4dbgp("mapflow[RVAL]: start val seqflow");
            addrem_flags(RNXT, RVAL);
            m_evt_handler->begin_seq_val_flow();
            _set_indentation(m_evt_handler->m_parent->indref);
            addrem_flags(RSEQ|RVAL, RMAP|RNXT);
            _line_progressed(1);
            goto mapflow_finish;
        }
        else if(first == '{')
        {
            _c4dbgp("mapflow[RVAL]: start val mapflow");
            addrem_flags(RNXT, RVAL);
            m_evt_handler->begin_map_val_flow();
            _set_indentation(m_evt_handler->m_parent->indref);
            addrem_flags(RKEY, RNXT);
            _line_progressed(1);
            // keep going in this function
        }
        else if(first == '}')
        {
            _c4dbgp("mapflow[RVAL]: end!");
            m_evt_handler->set_val_scalar_plain_empty();
            m_evt_handler->end_map();
            _line_progressed(1);
            goto mapflow_finish;
        }
        else if(first == ',')
        {
            _c4dbgp("mapflow[RVAL]: empty val!");
            m_evt_handler->set_val_scalar_plain_empty();
            addrem_flags(RNXT, RVAL);
            // keep going in this function
        }
        else if(first == '*')
        {
            csubstr ref = _scan_ref_map();
            _c4dbgpf("mapflow[RVAL]: key ref! [{}]~~~{}~~~", ref.len, ref);
            m_evt_handler->set_val_ref(ref);
            addrem_flags(RNXT, RVAL);
        }
        else if(first == '&')
        {
            csubstr anchor = _scan_anchor();
            _c4dbgpf("mapflow[RVAL]: key anchor! [{}]~~~{}~~~", anchor.len, anchor);
            m_evt_handler->set_val_anchor(anchor);
        }
        else if(first == '!')
        {
            csubstr tag = _scan_tag();
            _c4dbgpf("mapflow[RVAL]: tag! [{}]~~~{}~~~", tag.len, tag);
            _check_tag(tag);
            m_evt_handler->set_val_tag(tag);
        }
        else
        {
            _c4err("parse error");
        }
    }
    else if(has_any(RNXT))
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RKEY));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RKCL));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RVAL));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(QMRK));
        _c4dbgpf("mapflow[RNXT]: '{}'", rem.str[0]);
        if(rem.begins_with(','))
        {
            _c4dbgp("mapflow[RNXT]: expect next keyval");
            m_evt_handler->add_sibling();
            addrem_flags(RKEY, RNXT);
            _line_progressed(1);
        }
        else if(rem.begins_with('}'))
        {
            _c4dbgp("mapflow[RNXT]: end!");
            m_evt_handler->end_map();
            _line_progressed(1);
            goto mapflow_finish;
        }
        else
        {
            _c4err("parse error");
        }
    }
    else if(has_any(QMRK))
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RKEY));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RKCL));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RVAL));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RNXT));
        const char first = rem.str[0];
        _c4dbgpf("mapflow[QMRK]: '{}'", first);
        ScannedScalar sc;
        if(first == '\'')
        {
            _c4dbgp("mapflow[QMRK]: scanning single-quoted scalar");
            sc = _scan_scalar_squot();
            csubstr maybe_filtered = _maybe_filter_key_scalar_squot(sc);
            m_evt_handler->set_key_scalar_squoted(maybe_filtered);
            addrem_flags(RKCL, QMRK);
        }
        else if(first == '"')
        {
            _c4dbgp("mapflow[QMRK]: scanning double-quoted scalar");
            sc = _scan_scalar_dquot();
            csubstr maybe_filtered = _maybe_filter_key_scalar_dquot(sc);
            m_evt_handler->set_key_scalar_dquoted(maybe_filtered);
            addrem_flags(RKCL, QMRK);
        }
        // block scalars (ie | and >) cannot appear in flow containers
        else if(_scan_scalar_plain_map_flow(&sc))
        {
            _c4dbgp("mapflow[QMRK]: plain scalar");
            csubstr maybe_filtered = _maybe_filter_key_scalar_plain(sc, m_evt_handler->m_curr->indref);
            m_evt_handler->set_key_scalar_plain(maybe_filtered);
            addrem_flags(RKCL, QMRK);
        }
        else if(first == ':')
        {
            _c4dbgp("mapflow[QMRK]: setting empty key");
            m_evt_handler->set_key_scalar_plain_empty();
            addrem_flags(RVAL, QMRK);
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
        }
        else if(first == '}') // this happens on a trailing comma like ", }"
        {
            _c4dbgp("mapflow[QMRK]: end!");
            m_evt_handler->set_key_scalar_plain_empty();
            m_evt_handler->set_val_scalar_plain_empty();
            m_evt_handler->end_map();
            _line_progressed(1);
            goto mapflow_finish;
        }
        else if(first == ',')
        {
            _c4dbgp("mapflow[QMRK]: empty key+val!");
            m_evt_handler->set_key_scalar_plain_empty();
            m_evt_handler->set_val_scalar_plain_empty();
            addrem_flags(RNXT, QMRK);
        }
        else if(first == '&')
        {
            csubstr anchor = _scan_anchor();
            _c4dbgpf("mapflow[QMRK]: key anchor! [{}]~~~{}~~~", anchor.len, anchor);
            m_evt_handler->set_key_anchor(anchor);
        }
        else if(first == '*')
        {
            csubstr ref = _scan_ref_map();
            _c4dbgpf("mapflow[QMRK]: key ref! [{}]~~~{}~~~", ref.len, ref);
            m_evt_handler->set_key_ref(ref);
            addrem_flags(RKCL, QMRK);
        }
        else if(first == '[')
        {
            // RYML's tree cannot store container keys, but that's
            // handled inside the tree sink. Other sink types may be
            // able to handle it.
            _c4dbgp("mapflow[QMRK]: start child seqflow (!)");
            addrem_flags(RKCL, QMRK);
            m_evt_handler->begin_seq_key_flow();
            addrem_flags(RSEQ|RVAL, RMAP|RKCL);
            _set_indentation(m_evt_handler->m_parent->indref);
            _line_progressed(1);
            goto mapflow_finish;
        }
        else if(first == '{')
        {
            // RYML's tree cannot store container keys, but that's
            // handled inside the tree sink. Other sink types may be
            // able to handle it.
            _c4dbgp("mapflow[QMRK]: start child mapflow (!)");
            addrem_flags(RKCL, QMRK);
            m_evt_handler->begin_map_key_flow();
            _set_indentation(m_evt_handler->m_parent->indref);
            addrem_flags(RKEY, RKCL);
            _line_progressed(1);
            // keep going in this function
        }
        else if(first == '!')
        {
            csubstr tag = _scan_tag();
            _c4dbgpf("mapflow[QMRK]: tag! [{}]~~~{}~~~", tag.len, tag);
            _check_tag(tag);
            m_evt_handler->set_key_tag(tag);
        }
        else
        {
            _c4err("parse error");
        }
    }

 mapflow_again:
    _c4dbgt("mapflow: go again", 0);
    if(_finished_line())
    {
        if(C4_LIKELY(!_finished_file()))
        {
            _line_ended();
            _scan_line();
            _c4dbgnextline();
        }
        else
        {
            _c4err("missing terminating }");
        }
    }
    goto mapflow_start;

 mapflow_finish:
    _c4dbgp("mapflow: finish");
}


//-----------------------------------------------------------------------------

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_seq_block()
{
seqblck_start:
    _c4dbgpf("handle2_seq_block: seq_id={} node_id={} level={} indent={}", m_evt_handler->m_parent->node_id, m_evt_handler->m_curr->node_id, m_evt_handler->m_curr->level, m_evt_handler->m_curr->indref);

    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_all(RSEQ));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_all(BLCK));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(RVAL|RNXT));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, 1 == (has_any(RVAL) + has_any(RNXT)));

    _maybe_skip_comment();
    csubstr rem = m_evt_handler->m_curr->line_contents.rem;
    if(!rem.len)
        goto seqblck_again;

    if(has_any(RVAL))
    {
        _c4dbgpf("seqblck[RVAL]: col={}", m_evt_handler->m_curr->pos.col);
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RNXT));
        if(m_evt_handler->m_curr->at_line_beginning())
        {
            _c4dbgpf("seqblck[RVAL]: indref={} indentation={}", m_evt_handler->m_curr->indref, m_evt_handler->m_curr->line_contents.indentation);
            if(m_evt_handler->m_curr->indentation_ge())
            {
                _c4dbgpf("seqblck[RVAL]: skip {} from indentation", m_evt_handler->m_curr->line_contents.indentation);
                _line_progressed(m_evt_handler->m_curr->line_contents.indentation);
                rem = m_evt_handler->m_curr->line_contents.rem;
                if(!rem.len)
                    goto seqblck_again;
            }
            else if(m_evt_handler->m_curr->indentation_lt())
            {
                _c4dbgp("seqblck[RVAL]: smaller indentation!");
                _handle_indentation_pop_from_block_seq();
                goto seqblck_finish;
            }
            else if(m_evt_handler->m_curr->line_contents.indentation == npos)
            {
                _c4dbgp("seqblck[RVAL]: empty line!");
                _line_progressed(m_evt_handler->m_curr->line_contents.rem.len);
                goto seqblck_again;
            }
        }
        #ifdef RYML_NO_COVERAGE__TO_BE_DELETED
        else
        {
            // accomodate annotation on the previous line. eg:
            // - &elm
            //   foo            # <-- on this line
            // - &elm
            //   &foo foo: bar  # <-- on this line
            if(rem.str[0] == ' ')
            {
                if(_handle_indentation_from_annotations())
                {
                    _c4dbgp("seqblck[RVAL]: annotations!");
                    rem = m_evt_handler->m_curr->line_contents.rem;
                    if(!rem.len)
                        goto seqblck_again;
                }
            }
        }
        #endif
        _RYML_CB_ASSERT(callbacks(), rem.len);
        _c4dbgpf("seqblck[RVAL]: '{}' node_id={}", rem.str[0], m_evt_handler->m_curr->node_id);
        const char first = rem.str[0];
        const size_t startline = m_evt_handler->m_curr->pos.line;
        // warning: the gcc optimizer on x86 builds is brittle with
        // this function:
        const size_t startindent = m_evt_handler->m_curr->line_contents.current_col();
        ScannedScalar sc;
        if(first == '\'')
        {
            _c4dbgp("seqblck[RVAL]: single-quoted scalar");
            sc = _scan_scalar_squot();
            if(!_maybe_scan_following_colon())
            {
                _c4dbgp("seqblck[RVAL]: set as val");
                _handle_annotations_before_blck_val_scalar();
                csubstr maybe_filtered = _maybe_filter_val_scalar_squot(sc); // VAL!
                m_evt_handler->set_val_scalar_squoted(maybe_filtered);
                addrem_flags(RNXT, RVAL);
            }
            else
            {
                _c4dbgp("seqblck[RVAL]: start mapblck, set scalar as key");
                addrem_flags(RNXT, RVAL);
                _handle_annotations_before_start_mapblck(startline);
                _handle_colon();
                m_evt_handler->begin_map_val_block();
                _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
                csubstr maybe_filtered = _maybe_filter_key_scalar_squot(sc); // KEY!
                m_evt_handler->set_key_scalar_squoted(maybe_filtered);
                addrem_flags(RMAP|RVAL, RSEQ|RNXT);
                _maybe_skip_whitespace_tokens();
                goto seqblck_finish;
            }
        }
        else if(first == '"')
        {
            _c4dbgp("seqblck[RVAL]: double-quoted scalar");
            sc = _scan_scalar_dquot();
            if(!_maybe_scan_following_colon())
            {
                _c4dbgp("seqblck[RVAL]: set as val");
                _handle_annotations_before_blck_val_scalar();
                csubstr maybe_filtered = _maybe_filter_val_scalar_dquot(sc); // VAL!
                m_evt_handler->set_val_scalar_dquoted(maybe_filtered);
                addrem_flags(RNXT, RVAL);
            }
            else
            {
                _c4dbgp("seqblck[RVAL]: start mapblck, set scalar as key");
                addrem_flags(RNXT, RVAL);
                _handle_annotations_before_start_mapblck(startline);
                _handle_colon();
                m_evt_handler->begin_map_val_block();
                _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
                csubstr maybe_filtered = _maybe_filter_key_scalar_dquot(sc); // KEY!
                m_evt_handler->set_key_scalar_dquoted(maybe_filtered);
                addrem_flags(RMAP|RVAL, RSEQ|RNXT);
                _maybe_skip_whitespace_tokens();
                goto seqblck_finish;
            }
        }
        // block scalars can only appear as keys when in QMRK scope
        // (ie, after ? tokens), so no need to scan following colon in
        // here.
        else if(first == '|')
        {
            _c4dbgp("seqblck[RVAL]: block-literal scalar");
            ScannedBlock sb;
            _scan_block(&sb, m_evt_handler->m_curr->indref + 1);
            _handle_annotations_before_blck_val_scalar();
            csubstr maybe_filtered = _maybe_filter_val_scalar_literal(sb);
            m_evt_handler->set_val_scalar_literal(maybe_filtered);
            addrem_flags(RNXT, RVAL);
        }
        else if(first == '>')
        {
            _c4dbgp("seqblck[RVAL]: block-folded scalar");
            ScannedBlock sb;
            _scan_block(&sb, m_evt_handler->m_curr->indref + 1);
            _handle_annotations_before_blck_val_scalar();
            csubstr maybe_filtered = _maybe_filter_val_scalar_folded(sb);
            m_evt_handler->set_val_scalar_folded(maybe_filtered);
            addrem_flags(RNXT, RVAL);
        }
        else if(_scan_scalar_plain_seq_blck(&sc))
        {
            _c4dbgp("seqblck[RVAL]: plain scalar.");
            if(!_maybe_scan_following_colon())
            {
                _c4dbgp("seqblck[RVAL]: set as val");
                _handle_annotations_before_blck_val_scalar();
                csubstr maybe_filtered = _maybe_filter_val_scalar_plain(sc, m_evt_handler->m_curr->indref);  // VAL!
                m_evt_handler->set_val_scalar_plain(maybe_filtered);
                addrem_flags(RNXT, RVAL);
            }
            else
            {
                if(startindent > m_evt_handler->m_curr->indref)
                {
                    _c4dbgp("seqblck[RVAL]: start mapblck, set scalar as key");
                    addrem_flags(RNXT, RVAL);
                    _handle_annotations_before_start_mapblck(startline);
                    _handle_colon();
                    m_evt_handler->begin_map_val_block();
                    _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
                    csubstr maybe_filtered = _maybe_filter_key_scalar_plain(sc, m_evt_handler->m_curr->indref);  // KEY!
                    m_evt_handler->set_key_scalar_plain(maybe_filtered);
                    addrem_flags(RMAP|RVAL, RSEQ|RNXT);
                    _maybe_skip_whitespace_tokens();
                    goto seqblck_finish;
                }
                else if(m_evt_handler->m_parent && m_evt_handler->m_parent->indref == startindent && has_any(RMAP|BLCK, m_evt_handler->m_parent))
                {
                    _c4dbgp("seqblck[RVAL]: empty val + end indentless seq + set key");
                    m_evt_handler->set_val_scalar_plain_empty();
                    m_evt_handler->end_seq();
                    m_evt_handler->add_sibling();
                    csubstr maybe_filtered = _maybe_filter_key_scalar_plain(sc, m_evt_handler->m_curr->indref);  // KEY!
                    m_evt_handler->set_key_scalar_plain(maybe_filtered);
                    addrem_flags(RVAL, RNXT|RKEY);
                    _maybe_skip_whitespace_tokens();
                    goto seqblck_finish;
                }
                else
                {
                    _c4err("parse error");
                }
            }
        }
        else if(first == '[')
        {
            _c4dbgp("seqblck[RVAL]: start child seqflow");
            addrem_flags(RNXT, RVAL);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->begin_seq_val_flow();
            addrem_flags(FLOW|RVAL, BLCK|RNXT);
            _line_progressed(1);
            _set_indentation(m_evt_handler->m_parent->indref + 1u);
            goto seqblck_finish;
        }
        else if(first == '{')
        {
            _c4dbgp("seqblck[RVAL]: start child mapflow");
            addrem_flags(RNXT, RVAL);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->begin_map_val_flow();
            addrem_flags(RMAP|RKEY|FLOW, BLCK|RSEQ|RVAL|RNXT);
            _line_progressed(1);
            _set_indentation(m_evt_handler->m_parent->indref + 1u);
            goto seqblck_finish;
        }
        else if(first == '-')
        {
            if(startindent == m_evt_handler->m_curr->indref)
            {
                _c4dbgp("seqblck[RVAL]: prev val was empty");
                _handle_annotations_before_blck_val_scalar();
                m_evt_handler->set_val_scalar_plain_empty();
                // keep in RVAL, but for the next sibling
                m_evt_handler->add_sibling();
            }
            else
            {
                _c4dbgp("seqblck[RVAL]: start child seqblck");
                _RYML_CB_ASSERT(this->callbacks(), startindent > m_evt_handler->m_curr->indref);
                addrem_flags(RNXT, RVAL);
                _handle_annotations_before_blck_val_scalar();
                m_evt_handler->begin_seq_val_block();
                addrem_flags(RVAL, RNXT);
                _save_indentation();
                // keep going on inside this function
            }
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
        }
        else if(first == ':')
        {
            _c4dbgp("seqblck[RVAL]: start child mapblck with empty key");
            addrem_flags(RNXT, RVAL);
            _handle_annotations_before_start_mapblck(startline);
            _handle_colon();
            m_evt_handler->begin_map_val_block();
            _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
            m_evt_handler->set_key_scalar_plain_empty();
            addrem_flags(RMAP|RVAL, RSEQ|RNXT);
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
            goto seqblck_finish;
        }
        else if(first == '&')
        {
            const csubstr anchor = _scan_anchor();
            _c4dbgpf("seqblck[RVAL]: anchor! [{}]~~~{}~~~", anchor.len, anchor);
            // we need to buffer the anchors, as there may be two
            // consecutive anchors in here
            _add_annotation(&m_pending_anchors, anchor, startindent, startline);
        }
        else if(first == '*')
        {
            csubstr ref = _scan_ref_seq();
            _c4dbgpf("seqblck[RVAL]: ref! [{}]~~~{}~~~", ref.len, ref);
            if(!_maybe_scan_following_colon())
            {
                _c4dbgp("seqblck[RVAL]: set ref as val!");
                _handle_annotations_before_blck_val_scalar();
                m_evt_handler->set_val_ref(ref);
                addrem_flags(RNXT, RVAL);
            }
            else
            {
                _c4dbgp("seqblck[RVAL]: ref is key of map");
                addrem_flags(RNXT, RVAL);
                _handle_annotations_before_start_mapblck(startline);
                m_evt_handler->begin_map_val_block();
                _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
                m_evt_handler->set_key_ref(ref);
                addrem_flags(RMAP|RVAL, RSEQ|RNXT);
                _set_indentation(startindent);
                _maybe_skip_whitespace_tokens();
                goto seqblck_finish;
            }
        }
        else if(first == '!')
        {
            csubstr tag = _scan_tag();
            _c4dbgpf("seqblck[RVAL]: val tag! [{}]~~~{}~~~", tag.len, tag);
            // we need to buffer the tags, as there may be two
            // consecutive tags in here
            _add_annotation(&m_pending_tags, tag, startindent, startline);
        }
        else if(first == '?')
        {
            _c4dbgp("seqblck[RVAL]: start child mapblck, explicit key");
            addrem_flags(RNXT, RVAL);
            m_was_inside_qmrk = true;
            m_evt_handler->begin_map_val_block();
            addrem_flags(RMAP|QMRK, RSEQ|RNXT);
            _save_indentation();
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
            goto seqblck_finish;
        }
        else
        {
            _c4err("parse error");
        }
    }
    else // RNXT
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(RNXT));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RVAL));
        //
        // handle indentation
        //
        _c4dbgpf("seqblck[RNXT]: indref={} indentation={}", m_evt_handler->m_curr->indref, m_evt_handler->m_curr->line_contents.indentation);
        if(C4_LIKELY(_at_line_begin()))
        {
            _c4dbgp("seqblck[RNXT]: at line begin");
            if(m_evt_handler->m_curr->indentation_ge())
            {
                _c4dbgpf("seqblck[RNXT]: skip {} from indref", m_evt_handler->m_curr->indref);
                _line_progressed(m_evt_handler->m_curr->indref);
                _maybe_skip_whitespace_tokens();
                rem = m_evt_handler->m_curr->line_contents.rem;
                if(!rem.len)
                    goto seqblck_again;
            }
            else if(m_evt_handler->m_curr->indentation_lt())
            {
                _c4dbgp("seqblck[RNXT]: smaller indentation!");
                _handle_indentation_pop_from_block_seq();
                if(has_all(RSEQ|BLCK))
                {
                    _c4dbgp("seqblck[RNXT]: still seqblck!");
                    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(RNXT));
                    _line_progressed(m_evt_handler->m_curr->line_contents.indentation);
                    rem = m_evt_handler->m_curr->line_contents.rem;
                    if(!rem.len)
                        goto seqblck_again;
                }
                else
                {
                    _c4dbgp("seqblck[RNXT]: no longer seqblck!");
                    goto seqblck_finish;
                }
            }
            else if(m_evt_handler->m_curr->line_contents.indentation == npos)
            {
                _c4dbgpf("seqblck[RNXT]: blank line, len={}", m_evt_handler->m_curr->line_contents.rem);
                _line_progressed(m_evt_handler->m_curr->line_contents.rem.len);
                rem = m_evt_handler->m_curr->line_contents.rem;
                if(!rem.len)
                    goto seqblck_again;
            }
        }
        else
        {
            _c4dbgp("seqblck[RNXT]: NOT at line begin");
            if(!rem.begins_with_any(" \t"))
            {
                _c4err("parse error");
            }
            else
            {
                _skipchars(" \t");
                rem = m_evt_handler->m_curr->line_contents.rem;
                if(!rem.len)
                {
                    _c4dbgp("seqblck[RNXT]: again");
                    goto seqblck_again;
                }
            }
        }
        //
        // now handle the tokens
        //
        const char first = rem.str[0];
        _c4dbgpf("seqblck[RNXT]: '{}' node_id={}", first, m_evt_handler->m_curr->node_id);
        if(first == '-')
        {
            if(m_evt_handler->m_curr->indref > 0 || m_evt_handler->m_curr->line_contents.indentation > 0 || !_is_doc_begin_token(rem))
            {
                _c4dbgp("seqblck[RNXT]: expect next val");
                addrem_flags(RVAL, RNXT);
                m_evt_handler->add_sibling();
                _line_progressed(1);
                _maybe_skip_whitespace_tokens();
            }
            else
            {
                _c4dbgp("seqblck[RNXT]: start doc");
                _start_doc_suddenly();
                _line_progressed(3);
                _maybe_skip_whitespace_tokens();
                goto seqblck_finish;
            }
        }
        else if(first == ':')
        {
            // This happens for example in `- [a: b]: c` (after
            // terminating the seq, ie, after `]`). All other cases
            // (ie colon after scalars) are caught elsewhere (ie, in
            // RVAL state).
            auto const *C4_RESTRICT prev_state = m_evt_handler->m_parent;
            if(C4_LIKELY(prev_state && (prev_state->flags & RMAP)))
            {
                _c4dbgp("seqblck[RNXT]: actually this seq was '?' key of parent map");
                m_evt_handler->end_seq();
                goto seqblck_finish;
            }
            else
            {
                _c4err("parse error");
            }
        }
        else if(first == '.')
        {
            _c4dbgp("seqblck[RNXT]: maybe doc?");
            csubstr rs = rem.sub(1);
            if(rs == ".." || rs.begins_with(".. "))
            {
                _c4dbgp("seqblck[RNXT]: end+start doc");
                _end_doc_suddenly();
                _line_progressed(3);
                _maybe_skip_whitespace_tokens();
                goto seqblck_finish;
            }
            else
            {
                _c4err("parse error");
            }
        }
        else
        {
            // may be an indentless sequence nested in a map...
            //if(m_evt_handler->m_stack.size() >= 2)
            #ifdef RYML_DBG
            char flagbuf_[128];
            for(auto const& s : m_evt_handler->m_stack)
            {
                _dbg_printf("state[{}]: ind={} node={} flags={}\n", s.level, s.indref, s.node_id, detail::_parser_flags_to_str(flagbuf_, s.flags));
            }
            #endif
            if(m_evt_handler->m_parent && has_all(RMAP|BLCK, m_evt_handler->m_parent) && m_evt_handler->m_curr->indref == m_evt_handler->m_parent->indref)
            {
                _c4dbgpf("seqblck[RNXT]: end indentless seq, go to parent={}. node={}", m_evt_handler->m_parent->node_id, m_evt_handler->m_curr->node_id);
                _RYML_CB_ASSERT(this->callbacks(), m_evt_handler->m_curr != m_evt_handler->m_parent);
                _handle_indentation_pop(m_evt_handler->m_parent);
                _RYML_CB_ASSERT(this->callbacks(), has_all(RMAP|BLCK));
                m_evt_handler->add_sibling();
                addrem_flags(RKEY, RNXT);
                goto seqblck_finish;
            }
            else //if(first != '*')
            {
                _c4err("parse error");
            }
        }
    }

 seqblck_again:
    _c4dbgt("seqblck: go again", 0);
    if(_finished_line())
    {
        _line_ended();
        _scan_line();
        if(_finished_file())
        {
            _c4dbgp("seqblck: finish!");
            _end_seq_blck();
            goto seqblck_finish;
        }
        _c4dbgnextline();
    }
    goto seqblck_start;

 seqblck_finish:
    _c4dbgp("seqblck: finish");
}


//-----------------------------------------------------------------------------

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_map_block()
{
mapblck_start:
    _c4dbgpf("handle2_map_block: map_id={} node_id={} level={} indref={}", m_evt_handler->m_parent->node_id, m_evt_handler->m_curr->node_id, m_evt_handler->m_curr->level, m_evt_handler->m_curr->indref);

    // states: RKEY|QMRK -> RKCL -> RVAL -> RNXT
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_all(RMAP));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_all(BLCK));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(RKEY|RKCL|RVAL|RNXT|QMRK));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, 1 == (has_any(RKEY) + has_any(RKCL) + has_any(RVAL) + has_any(RNXT) + has_any(QMRK)));

    _maybe_skip_comment();
    csubstr rem = m_evt_handler->m_curr->line_contents.rem;
    if(!rem.len)
        goto mapblck_again;

    if(has_any(RKEY))
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RKCL));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(QMRK));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RVAL));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RNXT));
        //
        // handle indentation
        //
        if(m_evt_handler->m_curr->at_line_beginning())
        {
            if(m_evt_handler->m_curr->indentation_eq())
            {
                _c4dbgpf("mapblck[RKEY]: skip {} from indref", m_evt_handler->m_curr->indref);
                _line_progressed(m_evt_handler->m_curr->indref);
                rem = m_evt_handler->m_curr->line_contents.rem;
                if(!rem.len)
                    goto mapblck_again;
            }
            else if(m_evt_handler->m_curr->indentation_lt())
            {
                _c4dbgp("mapblck[RKEY]: smaller indentation!");
                _handle_indentation_pop_from_block_map();
                _line_progressed(m_evt_handler->m_curr->line_contents.indentation);
                if(has_all(RMAP|BLCK))
                {
                    _c4dbgp("mapblck[RKEY]: still mapblck!");
                    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(RKEY));
                    rem = m_evt_handler->m_curr->line_contents.rem;
                    if(!rem.len)
                        goto mapblck_again;
                }
                else
                {
                    _c4dbgp("mapblck[RKEY]: no longer mapblck!");
                    goto mapblck_finish;
                }
            }
            else
            {
                _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->indentation_gt());
                _c4err("invalid indentation");
            }
        }
        //
        // now handle the tokens
        //
        const char first = rem.str[0];
        const size_t startline = m_evt_handler->m_curr->pos.line;
        const size_t startindent = m_evt_handler->m_curr->line_contents.current_col();
        _c4dbgpf("mapblck[RKEY]: '{}'", first);
        ScannedScalar sc;
        if(first == '\'')
        {
            _c4dbgp("mapblck[RKEY]: scanning single-quoted scalar");
            sc = _scan_scalar_squot();
            csubstr maybe_filtered = _maybe_filter_val_scalar_squot(sc);
            _handle_annotations_before_blck_key_scalar();
            m_evt_handler->set_key_scalar_squoted(maybe_filtered);
            addrem_flags(RVAL, RKEY);
            if(!_maybe_scan_following_colon())
                _c4err("could not find ':' colon after key");
            _maybe_skip_whitespace_tokens();
        }
        else if(first == '"')
        {
            _c4dbgp("mapblck[RKEY]: scanning double-quoted scalar");
            sc = _scan_scalar_dquot();
            csubstr maybe_filtered = _maybe_filter_val_scalar_dquot(sc);
            _handle_annotations_before_blck_key_scalar();
            m_evt_handler->set_key_scalar_dquoted(maybe_filtered);
            addrem_flags(RVAL, RKEY);
            if(!_maybe_scan_following_colon())
                _c4err("could not find ':' colon after key");
            _maybe_skip_whitespace_tokens();
        }
        // block scalars (| and >) can not be used as keys unless they
        // appear in an explicit QMRK scope (ie, after the ? token),
        else if(C4_UNLIKELY(first == '|'))
        {
            _c4err("block literal keys must be enclosed in '?'");
        }
        else if(C4_UNLIKELY(first == '>'))
        {
            _c4err("block literal keys must be enclosed in '?'");
        }
        else if(_scan_scalar_plain_map_blck(&sc))
        {
            _c4dbgp("mapblck[RKEY]: plain scalar");
            csubstr maybe_filtered = _maybe_filter_val_scalar_plain(sc, m_evt_handler->m_curr->indref);
            _handle_annotations_before_blck_key_scalar();
            m_evt_handler->set_key_scalar_plain(maybe_filtered);
            addrem_flags(RVAL, RKEY);
            if(!_maybe_scan_following_colon())
                _c4err("could not find ':' colon after key");
            _maybe_skip_whitespace_tokens();
        }
        else if(first == '?')
        {
            _c4dbgp("mapblck[RKEY]: key token!");
            addrem_flags(QMRK, RKEY);
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
            m_was_inside_qmrk = true;
            goto mapblck_again;
        }
        else if(first == ':')
        {
            _c4dbgp("mapblck[RKEY]: setting empty key");
            _handle_annotations_before_blck_key_scalar();
            m_evt_handler->set_key_scalar_plain_empty();
            addrem_flags(RVAL, RKEY);
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
        }
        else if(first == '*')
        {
            csubstr ref = _scan_ref_map();
            _c4dbgpf("mapblck[RKEY]: key ref! [{}]~~~{}~~~", ref.len, ref);
            _handle_annotations_before_blck_key_scalar();
            m_evt_handler->set_key_ref(ref);
            addrem_flags(RVAL, RKEY);
            if(!_maybe_scan_following_colon())
                _c4err("could not find ':' colon after key");
            _maybe_skip_whitespace_tokens();
        }
        else if(first == '&')
        {
            csubstr anchor = _scan_anchor();
            _c4dbgpf("mapblck[RKEY]: key anchor! [{}]~~~{}~~~", anchor.len, anchor);
            _add_annotation(&m_pending_anchors, anchor, startindent, startline);
        }
        else if(first == '!')
        {
            csubstr tag = _scan_tag();
            _c4dbgpf("mapblck[RKEY]: key tag! [{}]~~~{}~~~", tag.len, tag);
            _add_annotation(&m_pending_tags, tag, startindent, startline);
        }
        else if(first == '[')
        {
            // RYML's tree cannot store container keys, but that's
            // handled inside the tree handler. Other handlers may be
            // able to handle it.
            _c4dbgp("mapblck[RKEY]: start child seqflow (!)");
            addrem_flags(RKCL, RKEY);
            _handle_annotations_before_blck_key_scalar();
            m_evt_handler->begin_seq_key_flow();
            addrem_flags(RSEQ|FLOW|RVAL, RMAP|BLCK|RKCL);
            _line_progressed(1);
            _set_indentation(startindent);
            goto mapblck_finish;
        }
        else if(first == '{')
        {
            // RYML's tree cannot store container keys, but that's
            // handled inside the tree handler. Other handlers may be
            // able to handle it.
            _c4dbgp("mapblck[RKEY]: start child mapflow (!)");
            addrem_flags(RKCL, RKEY);
            _handle_annotations_before_blck_key_scalar();
            m_evt_handler->begin_map_key_flow();
            addrem_flags(FLOW|RKEY, BLCK|RKCL);
            _line_progressed(1);
            _set_indentation(startindent);
            goto mapblck_finish;
        }
        else if(first == '-')
        {
            _c4dbgp("mapblck[RKEY]: maybe doc?");
            if(m_evt_handler->m_curr->line_contents.indentation == 0 && _is_doc_begin_token(rem))
            {
                _c4dbgp("mapblck[RKEY]: end+start doc");
                _start_doc_suddenly();
                _line_progressed(3);
                _maybe_skip_whitespace_tokens();
                goto mapblck_finish;
            }
            else
            {
                _c4err("parse error");
            }
        }
        else if(first == '.')
        {
            _c4dbgp("mapblck[RKEY]: maybe end doc?");
            if(m_evt_handler->m_curr->line_contents.indentation == 0 && _is_doc_end_token(rem))
            {
                _c4dbgp("mapblck[RKEY]: end doc");
                _end_doc_suddenly();
                _line_progressed(3);
                _maybe_skip_whitespace_tokens();
                goto mapblck_finish;
            }
            else
            {
                _c4err("parse error");
            }
        }
       _RYML_WITH_TAB_TOKENS(
        else if(first == '\t')
        {
            _c4dbgp("mapblck[RKEY]: skip tabs");
            _maybe_skipchars('\t');
        })
        else
        {
            _c4err("parse error");
        }
    }
    else if(has_any(RKCL)) // read the key colon
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RKEY));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RVAL));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RNXT));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(QMRK));
        //
        // handle indentation
        //
        if(m_evt_handler->m_curr->at_line_beginning())
        {
            if(m_evt_handler->m_curr->indentation_eq())
            {
                _c4dbgpf("mapblck[RKCL]: skip {} from indref", m_evt_handler->m_curr->indref);
                _line_progressed(m_evt_handler->m_curr->indref);
                rem = m_evt_handler->m_curr->line_contents.rem;
                if(!rem.len)
                    goto mapblck_again;
            }
            else if(C4_UNLIKELY(m_evt_handler->m_curr->indentation_lt()))
            {
                _c4err("invalid indentation");
            }
        }
        const char first = rem.str[0];
        _c4dbgpf("mapblck[RKCL]: '{}'", first);
        if(first == ':')
        {
            _c4dbgp("mapblck[RKCL]: found the colon");
            addrem_flags(RVAL, RKCL);
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
        }
        else if(first == '?')
        {
            _c4dbgp("mapblck[RKCL]: got '?'. val was empty");
            _RYML_CB_CHECK(m_evt_handler->m_stack.m_callbacks, m_was_inside_qmrk);
            m_evt_handler->set_val_scalar_plain_empty();
            m_evt_handler->add_sibling();
            addrem_flags(QMRK, RKCL);
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
        }
        else if(first == '-')
        {
            if(m_evt_handler->m_curr->indref == 0 || m_evt_handler->m_curr->line_contents.indentation == 0 || _is_doc_begin_token(rem))
            {
                _c4dbgp("mapblck[RKCL]: end+start doc");
                _RYML_CB_CHECK(m_evt_handler->m_stack.m_callbacks, _is_doc_begin_token(rem));
                _start_doc_suddenly();
                _line_progressed(3);
                _maybe_skip_whitespace_tokens();
                goto mapblck_finish;
            }
            else
            {
                _c4err("parse error");
            }
        }
        else if(first == '.')
        {
            _c4dbgp("mapblck[RKCL]: maybe end doc?");
            csubstr rs = rem.sub(1);
            if(rs == ".." || rs.begins_with(".. "))
            {
                _c4dbgp("mapblck[RKCL]: end+start doc");
                _end_doc_suddenly();
                _line_progressed(3);
                goto mapblck_finish;
            }
            else
            {
                _c4err("parse error");
            }
        }
        else if(m_was_inside_qmrk)
        {
            _RYML_CB_CHECK(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->indentation_eq());
            _c4dbgp("mapblck[RKCL]: missing :");
            m_evt_handler->set_val_scalar_plain_empty();
            m_evt_handler->add_sibling();
            m_was_inside_qmrk = false;
            addrem_flags(RKEY, RKCL);
        }
        else
        {
            _c4err("parse error");
        }
    }
    else if(has_any(RVAL))
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RKEY));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RKCL));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RNXT));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(QMRK));
        //
        // handle indentation
        //
        if(m_evt_handler->m_curr->at_line_beginning())
        {
            _c4dbgpf("mapblck[RVAL]: indref={} indentation={}", m_evt_handler->m_curr->indref, m_evt_handler->m_curr->line_contents.indentation);
            m_evt_handler->m_curr->more_indented = false;
            if(m_evt_handler->m_curr->indref == npos)
            {
                _c4dbgpf("mapblck[RVAL]: setting indentation={}", m_evt_handler->m_parent->indref);
                _set_indentation(m_evt_handler->m_curr->line_contents.indentation);
                _line_progressed(m_evt_handler->m_curr->indref);
                rem = m_evt_handler->m_curr->line_contents.rem;
                if(!rem.len)
                    goto mapblck_again;
            }
            else if(m_evt_handler->m_curr->indentation_eq())
            {
                _c4dbgp("mapblck[RVAL]: skip indentation!");
                _line_progressed(m_evt_handler->m_curr->indref);
                rem = m_evt_handler->m_curr->line_contents.rem;
                if(!rem.len)
                    goto mapblck_again;
                // TODO: this is valid:
                //
                // ```yaml
                // a:
                // b:
                // ---
                // a:
                //  b
                // ---
                // a:
                //  b: c
                // ```
                //
                // ... but this is not:
                //
                // ```yaml
                // a:
                // v
                // ---
                // a: b: c
                // ```
                //
                // here, we probably need to set a boolean on the state
                // to disambiguate between these cases.
            }
            else if(m_evt_handler->m_curr->indentation_gt())
            {
                _c4dbgp("mapblck[RVAL]: more indented!");
                m_evt_handler->m_curr->more_indented = true;
                _line_progressed(m_evt_handler->m_curr->line_contents.indentation);
                rem = m_evt_handler->m_curr->line_contents.rem;
                if(!rem.len)
                    goto mapblck_again;
            }
            else if(m_evt_handler->m_curr->indentation_lt())
            {
                _c4dbgp("mapblck[RVAL]: smaller indentation!");
                _handle_indentation_pop_from_block_map();
                if(has_all(RMAP|BLCK))
                {
                    _c4dbgp("mapblck[RVAL]: still mapblck!");
                    _line_progressed(m_evt_handler->m_curr->line_contents.indentation);
                    if(has_any(RNXT))
                    {
                        _c4dbgp("mapblck[RVAL]: speculatively expect next keyval");
                        m_evt_handler->add_sibling();
                        addrem_flags(RKEY, RNXT);
                    }
                    goto mapblck_again;
                }
                else
                {
                    _c4dbgp("mapblck[RVAL]: no longer mapblck!");
                    goto mapblck_finish;
                }
            }
            else if(m_evt_handler->m_curr->line_contents.indentation == npos)
            {
                _c4dbgp("mapblck[RVAL]: empty line!");
                _line_progressed(m_evt_handler->m_curr->line_contents.rem.len);
                goto mapblck_again;
            }
        }
        //
        // now handle the tokens
        //
        const char first = rem.str[0];
        const size_t startline = m_evt_handler->m_curr->pos.line;
        const size_t startindent = m_evt_handler->m_curr->line_contents.current_col();
        _c4dbgpf("mapblck[RVAL]: '{}'", first);
        ScannedScalar sc;
        if(first == '\'')
        {
            _c4dbgp("mapblck[RVAL]: scanning single-quoted scalar");
            sc = _scan_scalar_squot();
            if(!_maybe_scan_following_colon())
            {
                _c4dbgp("mapblck[RVAL]: set as val");
                _handle_annotations_before_blck_val_scalar();
                csubstr maybe_filtered = _maybe_filter_val_scalar_squot(sc); // VAL!
                m_evt_handler->set_val_scalar_squoted(maybe_filtered);
                addrem_flags(RNXT, RVAL);
            }
            else
            {
                if(startindent != m_evt_handler->m_curr->indref)
                {
                    _c4dbgp("mapblck[RVAL]: start new block map, set scalar as key");
                    _handle_annotations_before_start_mapblck(startline);
                    addrem_flags(RNXT, RVAL);
                    _handle_colon();
                    m_evt_handler->begin_map_val_block();
                    _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
                    csubstr maybe_filtered = _maybe_filter_key_scalar_squot(sc); // KEY!
                    m_evt_handler->set_key_scalar_squoted(maybe_filtered);
                    _maybe_skip_whitespace_tokens();
                    // keep the child state on RVAL
                    addrem_flags(RVAL, RNXT);
                }
                else
                {
                    _c4dbgp("mapblck[RVAL]: prev val empty+this is a key");
                    m_evt_handler->set_val_scalar_plain_empty();
                    m_evt_handler->add_sibling();
                    csubstr maybe_filtered = _maybe_filter_key_scalar_squot(sc); // KEY!
                    m_evt_handler->set_key_scalar_squoted(maybe_filtered);
                    // keep going on RVAL
                    _maybe_skip_whitespace_tokens();
                }
            }
        }
        else if(first == '"')
        {
            _c4dbgp("mapblck[RVAL]: scanning double-quoted scalar");
            sc = _scan_scalar_dquot();
            if(!_maybe_scan_following_colon())
            {
                _c4dbgp("mapblck[RVAL]: set as val");
                _handle_annotations_before_blck_val_scalar();
                csubstr maybe_filtered = _maybe_filter_val_scalar_dquot(sc); // VAL!
                m_evt_handler->set_val_scalar_dquoted(maybe_filtered);
                addrem_flags(RNXT, RVAL);
            }
            else
            {
                if(startindent != m_evt_handler->m_curr->indref)
                {
                    _c4dbgp("mapblck[RVAL]: start new block map, set scalar as key");
                    _handle_annotations_before_start_mapblck(startline);
                    addrem_flags(RNXT, RVAL);
                    _handle_colon();
                    m_evt_handler->begin_map_val_block();
                    _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
                    csubstr maybe_filtered = _maybe_filter_key_scalar_dquot(sc); // KEY!
                    m_evt_handler->set_key_scalar_dquoted(maybe_filtered);
                    _maybe_skip_whitespace_tokens();
                    // keep the child state on RVAL
                    addrem_flags(RVAL, RNXT);
                }
                else
                {
                    _c4dbgp("mapblck[RVAL]: prev val empty+this is a key");
                    m_evt_handler->set_val_scalar_plain_empty();
                    m_evt_handler->add_sibling();
                    csubstr maybe_filtered = _maybe_filter_key_scalar_dquot(sc); // KEY!
                    m_evt_handler->set_key_scalar_dquoted(maybe_filtered);
                    // keep going on RVAL
                    _maybe_skip_whitespace_tokens();
                }
            }
        }
        // block scalars can only appear as keys when in QMRK scope
        // (ie, after ? tokens), so no need to scan following colon
        else if(first == '|')
        {
            _c4dbgp("mapblck[RVAL]: scanning block-literal scalar");
            ScannedBlock sb;
            _scan_block(&sb, m_evt_handler->m_curr->indref + 1);
            _handle_annotations_before_blck_val_scalar();
            csubstr maybe_filtered = _maybe_filter_val_scalar_literal(sb);
            m_evt_handler->set_val_scalar_literal(maybe_filtered);
            addrem_flags(RNXT, RVAL);
        }
        else if(first == '>')
        {
            _c4dbgp("mapblck[RVAL]: scanning block-folded scalar");
            ScannedBlock sb;
            _scan_block(&sb, m_evt_handler->m_curr->indref + 1);
            _handle_annotations_before_blck_val_scalar();
            csubstr maybe_filtered = _maybe_filter_val_scalar_folded(sb);
            m_evt_handler->set_val_scalar_folded(maybe_filtered);
            addrem_flags(RNXT, RVAL);
        }
        else if(_scan_scalar_plain_map_blck(&sc))
        {
            _c4dbgp("mapblck[RVAL]: plain scalar.");
            if(!_maybe_scan_following_colon())
            {
                _c4dbgp("mapblck[RVAL]: set as val");
                _handle_annotations_before_blck_val_scalar();
                csubstr maybe_filtered = _maybe_filter_val_scalar_plain(sc, m_evt_handler->m_curr->indref); // VAL!
                m_evt_handler->set_val_scalar_plain(maybe_filtered);
                addrem_flags(RNXT, RVAL);
            }
            else
            {
                if(startindent != m_evt_handler->m_curr->indref)
                {
                    _c4dbgpf("mapblck[RVAL]: start new block map, set scalar as key {}", m_evt_handler->m_curr->indref);
                    addrem_flags(RNXT, RVAL);
                    _handle_annotations_before_start_mapblck(startline);
                    _handle_colon();
                    m_evt_handler->begin_map_val_block();
                    _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
                    csubstr maybe_filtered = _maybe_filter_key_scalar_plain(sc, m_evt_handler->m_curr->indref); // KEY!
                    m_evt_handler->set_key_scalar_plain(maybe_filtered);
                    _maybe_skip_whitespace_tokens();
                    // keep the child state on RVAL
                    addrem_flags(RVAL, RNXT);
                }
                else
                {
                    _c4dbgp("mapblck[RVAL]: prev val empty+this is a key");
                    _handle_annotations_before_blck_val_scalar();
                    m_evt_handler->set_val_scalar_plain_empty();
                    m_evt_handler->add_sibling();
                    csubstr maybe_filtered = _maybe_filter_key_scalar_plain(sc, m_evt_handler->m_curr->indref); // KEY!
                    m_evt_handler->set_key_scalar_plain(maybe_filtered);
                    // keep going on RVAL
                    _maybe_skip_whitespace_tokens();
                }
            }
        }
        else if(first == '-')
        {
            if(rem.len == 1 || rem.str[1] == ' ' _RYML_WITH_TAB_TOKENS(|| rem.str[1] == '\t'))
            {
                _c4dbgp("mapblck[RVAL]: start val seqblck");
                addrem_flags(RNXT, RVAL);
                _handle_annotations_before_blck_val_scalar();
                m_evt_handler->begin_seq_val_block();
                addrem_flags(RSEQ|RVAL, RMAP|RNXT);
                _set_indentation(startindent);
                _line_progressed(1);
                _maybe_skip_whitespace_tokens();
                goto mapblck_finish;
            }
            else if(m_evt_handler->m_curr->indref == 0 || m_evt_handler->m_curr->line_contents.indentation == 0 || _is_doc_begin_token(rem))
            {
                _c4dbgp("mapblck[RVAL]: end+start doc");
                _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, _is_doc_begin_token(rem));
                _start_doc_suddenly();
                _line_progressed(3);
                _maybe_skip_whitespace_tokens();
                goto mapblck_finish;
            }
            else
            {
                _c4err("parse error");
            }
        }
        else if(first == '[')
        {
            _c4dbgp("mapblck[RVAL]: start val seqflow");
            addrem_flags(RNXT, RVAL);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->begin_seq_val_flow();
            addrem_flags(RSEQ|FLOW|RVAL, RMAP|BLCK|RNXT);
            _set_indentation(m_evt_handler->m_curr->indref + 1u);
            _line_progressed(1);
            goto mapblck_finish;
        }
        else if(first == '{')
        {
            _c4dbgp("mapblck[RVAL]: start val mapflow");
            addrem_flags(RNXT, RVAL);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->begin_map_val_flow();
            addrem_flags(RKEY|FLOW, BLCK|RVAL|RNXT);
            m_evt_handler->m_curr->scalar_col = m_evt_handler->m_curr->line_contents.indentation;
            _set_indentation(m_evt_handler->m_curr->indref + 1u);
            _line_progressed(1);
            goto mapblck_finish;
        }
        else if(first == '*')
        {
            csubstr ref = _scan_ref_map();
            _c4dbgpf("mapblck[RVAL]: ref! [{}]~~~{}~~~", ref.len, ref);
            if(startindent == m_evt_handler->m_curr->indref)
            {
                _c4dbgpf("mapblck[RVAL]: same indentation {}", startindent);
                m_evt_handler->set_val_ref(ref);
                addrem_flags(RNXT, RVAL);
            }
            else
            {
                _c4dbgpf("mapblck[RVAL]: larger indentation {}>{}", startindent, m_evt_handler->m_curr->indref);
                _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, startindent > m_evt_handler->m_curr->indref);
                if(_maybe_scan_following_colon())
                {
                    _c4dbgp("mapblck[RVAL]: start child map, block");
                    addrem_flags(RNXT, RVAL);
                    _handle_annotations_before_blck_val_scalar();
                    m_evt_handler->begin_map_val_block();
                    m_evt_handler->set_key_ref(ref);
                    _set_indentation(startindent);
                    // keep going in RVAL
                    addrem_flags(RVAL, RNXT);
                }
                else
                {
                    _c4dbgp("mapblck[RVAL]: was val ref");
                    _handle_annotations_before_blck_val_scalar();
                    m_evt_handler->set_val_ref(ref);
                    addrem_flags(RNXT, RVAL);
                }
            }
            _maybe_skip_whitespace_tokens();
        }
        else if(first == '&')
        {
            csubstr anchor = _scan_anchor();
            _c4dbgpf("mapblck[RVAL]: anchor! [{}]~~~{}~~~", anchor.len, anchor);
            if(startindent == m_evt_handler->m_curr->indref)
            {
                _c4dbgp("mapblck[RVAL]: anchor for next key. val is missing!");
                m_evt_handler->set_val_scalar_plain_empty();
                m_evt_handler->add_sibling();
                addrem_flags(RKEY, RVAL);
            }
            // we need to buffer the anchors, as there may be two
            // consecutive anchors in here
            _add_annotation(&m_pending_anchors, anchor, startindent, startline);
        }
        else if(first == '!')
        {
            csubstr tag = _scan_tag();
            _c4dbgpf("mapblck[RVAL]: tag! [{}]~~~{}~~~", tag.len, tag);
            if(startindent == m_evt_handler->m_curr->indref)
            {
                _c4dbgp("mapblck[RVAL]: tag for next key. val is missing!");
                _handle_annotations_before_blck_val_scalar();
                m_evt_handler->set_val_scalar_plain_empty();
                m_evt_handler->add_sibling();
                addrem_flags(RKEY, RVAL);
            }
            // we need to buffer the tags, as there may be two
            // consecutive tags in here
            _add_annotation(&m_pending_tags, tag, startindent, startline);
        }
        else if(first == '?')
        {
            if(startindent == m_evt_handler->m_curr->indref)
            {
                _c4dbgp("mapblck[RVAL]: got '?'. val was empty");
                _handle_annotations_before_blck_val_scalar();
                m_evt_handler->set_val_scalar_plain_empty();
                m_evt_handler->add_sibling();
                addrem_flags(QMRK, RVAL);
            }
            else if(startindent > m_evt_handler->m_curr->indref)
            {
                _c4dbgp("mapblck[RVAL]: start val mapblck");
                addrem_flags(RNXT, RVAL);
                _handle_annotations_before_blck_val_scalar();
                m_evt_handler->begin_map_val_block();
                addrem_flags(QMRK|BLCK, RNXT);
                _set_indentation(startindent);
            }
            else
            {
                _c4err("parse error");
            }
            m_was_inside_qmrk = true;
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
            goto mapblck_again;
        }
        else if(first == ':')
        {
            if(startindent == m_evt_handler->m_curr->indref)
            {
                _c4dbgp("mapblck[RVAL]: got ':'. val was empty, next key as well");
                m_evt_handler->set_val_scalar_plain_empty();
                m_evt_handler->add_sibling();
                m_evt_handler->set_key_scalar_plain_empty();
            }
            else if(startindent > m_evt_handler->m_curr->indref)
            {
                _c4dbgp("mapblck[RVAL]: start val mapblck");
                addrem_flags(RNXT, RVAL);
                _handle_annotations_before_start_mapblck(startline);
                _handle_colon();
                m_evt_handler->begin_map_val_block();
                _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
                m_evt_handler->set_key_scalar_plain_empty();
                // keep the child state on RVAL
                addrem_flags(RVAL, RNXT);
            }
            else
            {
                _c4err("parse error");
            }
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
            goto mapblck_again;
        }
        else if(first == '.')
        {
            _c4dbgp("mapblck[RVAL]: maybe doc?");
            csubstr rs = rem.sub(1);
            if(rs == ".." || rs.begins_with(".. "))
            {
                _c4dbgp("seqblck[RVAL]: end doc expl");
                _end_doc_suddenly();
                _line_progressed(3);
                _maybe_skip_whitespace_tokens();
                goto mapblck_finish;
            }
            else
            {
                _c4err("parse error");
            }
        }
       _RYML_WITH_TAB_TOKENS(
        else if(first == '\t')
        {
            _c4dbgp("mapblck[RVAL]: skip tabs");
            _maybe_skipchars('\t');
        })
        else
        {
            _c4err("parse error");
        }
    }
    else if(has_any(RNXT))
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RKEY));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RKCL));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RVAL));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(QMRK));
        //
        // handle indentation
        //
        if(m_evt_handler->m_curr->at_line_beginning())
        {
            _c4dbgpf("mapblck[RNXT]: indref={} indentation={}", m_evt_handler->m_curr->indref, m_evt_handler->m_curr->line_contents.indentation);
            if(m_evt_handler->m_curr->indentation_eq())
            {
                _c4dbgpf("mapblck[RNXT]: skip {} from indref", m_evt_handler->m_curr->indref);
                _line_progressed(m_evt_handler->m_curr->indref);
                _c4dbgp("mapblck[RNXT]: speculatively expect next keyval");
                m_evt_handler->add_sibling();
                addrem_flags(RKEY, RNXT);
                goto mapblck_again;
            }
            else if(m_evt_handler->m_curr->indentation_lt())
            {
                _c4dbgp("mapblck[RNXT]: smaller indentation!");
                _handle_indentation_pop_from_block_map();
                if(has_all(RMAP|BLCK))
                {
                    _line_progressed(m_evt_handler->m_curr->line_contents.indentation);
                    if(!has_any(RKCL))
                    {
                        _c4dbgp("mapblck[RNXT]: speculatively expect next keyval");
                        m_evt_handler->add_sibling();
                        addrem_flags(RKEY, RNXT);
                    }
                    goto mapblck_again;
                }
                else
                {
                    goto mapblck_finish;
                }
            }
        }
        else
        {
            _c4dbgp("mapblck[RNXT]: NOT at line begin");
            if(!rem.begins_with_any(" \t"))
            {
                _c4err("parse error");
            }
            else
            {
                _skipchars(" \t");
                rem = m_evt_handler->m_curr->line_contents.rem;
                if(!rem.len)
                {
                    _c4dbgp("seqblck[RNXT]: again");
                    goto mapblck_again;
                }
            }
        }
        //
        // handle tokens
        //
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, rem.len > 0);
        const char first = rem.str[0];
        _c4dbgpf("mapblck[RNXT]: '{}'", _c4prc(first));
        if(first == ':')
        {
            if(m_evt_handler->m_curr->more_indented)
            {
                _c4dbgp("mapblck[RNXT]: start child block map");
                C4_NOT_IMPLEMENTED();
                //m_evt_handler->actually_as_block_map();
                _line_progressed(1);
                _set_indentation(m_evt_handler->m_curr->scalar_col);
                m_evt_handler->m_curr->more_indented = false;
                goto mapblck_again;
            }
            else
            {
                _c4err("parse error");
            }
        }
        else if(first == ' ')
        {
            _c4dbgp("mapblck[RNXT]: skip spaces");
            _maybe_skip_whitespace_tokens();
        }
        else
        {
            _c4err("parse error");
        }
    }
    else if(has_any(QMRK))
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RKEY));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RKCL));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RVAL));
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RNXT));
        //
        // handle indentation
        //
        if(m_evt_handler->m_curr->at_line_beginning())
        {
            _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->line_contents.indentation != npos);
            if(m_evt_handler->m_curr->indentation_eq())
            {
                _c4dbgpf("mapblck[QMRK]: skip {} from indref", m_evt_handler->m_curr->indref);
                _line_progressed(m_evt_handler->m_curr->indref);
                rem = m_evt_handler->m_curr->line_contents.rem;
                if(!rem.len)
                    goto mapblck_again;
            }
            else if(m_evt_handler->m_curr->indentation_lt())
            {
                _c4dbgp("mapblck[QMRK]: smaller indentation!");
                _handle_indentation_pop_from_block_map();
                _line_progressed(m_evt_handler->m_curr->line_contents.indentation);
                if(has_all(RMAP|BLCK))
                {
                    _c4dbgp("mapblck[QMRK]: still mapblck!");
                    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_any(QMRK));
                    rem = m_evt_handler->m_curr->line_contents.rem;
                    if(!rem.len)
                        goto mapblck_again;
                }
                else
                {
                    _c4dbgp("mapblck[QMRK]: no longer mapblck!");
                    goto mapblck_finish;
                }
            }
            // indentation can be larger in QMRK state
            else
            {
                _c4dbgp("mapblck[QMRK]: larger indentation !");
                _line_progressed(m_evt_handler->m_curr->line_contents.indentation);
                rem = m_evt_handler->m_curr->line_contents.rem;
                if(!rem.len)
                    goto mapblck_again;
            }
        }
        //
        // now handle the tokens
        //
        const char first = rem.str[0];
        const size_t startline = m_evt_handler->m_curr->pos.line;
        const size_t startindent = m_evt_handler->m_curr->line_contents.current_col();
        _c4dbgpf("mapblck[QMRK]: '{}'", first);
        ScannedScalar sc;
        if(first == '\'')
        {
            _c4dbgp("mapblck[QMRK]: scanning single-quoted scalar");
            sc = _scan_scalar_squot();
            csubstr maybe_filtered = _maybe_filter_key_scalar_squot(sc); // KEY!
            if(!_maybe_scan_following_colon())
            {
                _c4dbgp("mapblck[QMRK]: set as key");
                _handle_annotations_before_blck_key_scalar();
                m_evt_handler->set_key_scalar_squoted(maybe_filtered);
                addrem_flags(RKCL, QMRK);
            }
            else
            {
                _c4dbgp("mapblck[QMRK]: start new block map as key (!), set scalar as key");
                addrem_flags(RKCL, QMRK);
                _handle_annotations_before_start_mapblck_as_key();
                m_evt_handler->begin_map_key_block();
                _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
                m_evt_handler->set_key_scalar_squoted(maybe_filtered);
                _maybe_skip_whitespace_tokens();
                _set_indentation(startindent);
                // keep the child state on RVAL
                addrem_flags(RVAL, RKCL|QMRK);
            }
        }
        else if(first == '"')
        {
            _c4dbgp("mapblck[QMRK]: scanning double-quoted scalar");
            sc = _scan_scalar_dquot();
            csubstr maybe_filtered = _maybe_filter_key_scalar_dquot(sc); // KEY!
            if(!_maybe_scan_following_colon())
            {
                _c4dbgp("mapblck[QMRK]: set as key");
                _handle_annotations_before_blck_key_scalar();
                m_evt_handler->set_key_scalar_dquoted(maybe_filtered);
                addrem_flags(RKCL, QMRK);
            }
            else
            {
                _c4dbgp("mapblck[QMRK]: start new block map as key (!), set scalar as key");
                addrem_flags(RKCL, QMRK);
                _handle_annotations_before_start_mapblck_as_key();
                m_evt_handler->begin_map_key_block();
                _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
                m_evt_handler->set_key_scalar_dquoted(maybe_filtered);
                _maybe_skip_whitespace_tokens();
                _set_indentation(startindent);
                // keep the child state on RVAL
                addrem_flags(RVAL, RKCL|QMRK);
            }
        }
        else if(first == '|')
        {
            _c4dbgp("mapblck[QMRK]: scanning block-literal scalar");
            ScannedBlock sb;
            _scan_block(&sb, m_evt_handler->m_curr->indref + 1);
            csubstr maybe_filtered = _maybe_filter_key_scalar_literal(sb); // KEY!
            _handle_annotations_before_blck_key_scalar();
            m_evt_handler->set_key_scalar_literal(maybe_filtered);
            addrem_flags(RKCL, QMRK);
        }
        else if(first == '>')
        {
            _c4dbgp("mapblck[QMRK]: scanning block-literal scalar");
            ScannedBlock sb;
            _scan_block(&sb, m_evt_handler->m_curr->indref + 1);
            csubstr maybe_filtered = _maybe_filter_key_scalar_folded(sb); // KEY!
            _handle_annotations_before_blck_key_scalar();
            m_evt_handler->set_key_scalar_folded(maybe_filtered);
            addrem_flags(RKCL, QMRK);
        }
        else if(_scan_scalar_plain_map_blck(&sc))
        {
            _c4dbgp("mapblck[QMRK]: plain scalar");
            csubstr maybe_filtered = _maybe_filter_key_scalar_plain(sc, m_evt_handler->m_curr->indref); // KEY!
            if(!_maybe_scan_following_colon())
            {
                _c4dbgp("mapblck[QMRK]: set as key");
                _handle_annotations_before_blck_key_scalar();
                m_evt_handler->set_key_scalar_plain(maybe_filtered);
                addrem_flags(RKCL, QMRK);
            }
            else
            {
                _c4dbgp("mapblck[QMRK]: start new block map as key (!), set scalar as key");
                addrem_flags(RKCL, QMRK);
                _handle_annotations_before_start_mapblck_as_key();
                m_evt_handler->begin_map_key_block();
                _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
                m_evt_handler->set_key_scalar_plain(maybe_filtered);
                _maybe_skip_whitespace_tokens();
                _set_indentation(startindent);
                // keep the child state on RVAL
                addrem_flags(RVAL, RKCL|QMRK);
            }
        }
        else if(first == ':')
        {
            if(startindent == m_evt_handler->m_curr->indref)
            {
                _c4dbgp("mapblck[QMRK]: empty key");
                addrem_flags(RVAL, QMRK);
                _handle_annotations_before_blck_key_scalar();
                m_evt_handler->set_key_scalar_plain_empty();
                _line_progressed(1);
                _maybe_skip_whitespace_tokens();
            }
            else
            {
                _c4dbgp("mapblck[QMRK]: start new block map as key (!), empty key");
                addrem_flags(RKCL, QMRK);
                _handle_annotations_before_start_mapblck_as_key();
                m_evt_handler->begin_map_key_block();
                _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
                m_evt_handler->set_key_scalar_plain_empty();
                _line_progressed(1);
                _maybe_skip_whitespace_tokens();
                _set_indentation(startindent);
                // keep the child state on RVAL
                addrem_flags(RVAL, RKCL|QMRK);
            }
        }
        else if(first == '*')
        {
            csubstr ref = _scan_ref_map();
            _c4dbgpf("mapblck[QMRK]: key ref! [{}]~~~{}~~~", ref.len, ref);
            if(!_maybe_scan_following_colon())
            {
                _c4dbgp("mapblck[QMRK]: set ref as key");
                _handle_annotations_before_blck_key_scalar();
                m_evt_handler->set_key_ref(ref);
                addrem_flags(RKCL, QMRK);
            }
            else
            {
                _c4dbgp("mapblck[QMRK]: start new block map as key (!), set ref as key");
                addrem_flags(RKCL, QMRK);
                _handle_annotations_before_blck_key_scalar();
                m_evt_handler->begin_map_key_block();
                m_evt_handler->set_key_ref(ref);
                _set_indentation(startindent);
                // keep the child state on RVAL
                addrem_flags(RVAL, RKCL|QMRK);
            }
            _maybe_skip_whitespace_tokens();
        }
        else if(first == '&')
        {
            csubstr anchor = _scan_anchor();
            _c4dbgpf("mapblck[QMRK]: key anchor! [{}]~~~{}~~~", anchor.len, anchor);
            _add_annotation(&m_pending_anchors, anchor, startindent, startline);
        }
        else if(first == '!')
        {
            csubstr tag = _scan_tag();
            _c4dbgpf("mapblck[QMRK]: key tag! [{}]~~~{}~~~", tag.len, tag);
            _add_annotation(&m_pending_tags, tag, startindent, startline);
        }
        else if(first == '-')
        {
            _c4dbgp("mapblck[QMRK]: maybe doc?");
            csubstr rs = rem.sub(1);
            if(rs == "--" || rs.begins_with("-- "))
            {
                _c4dbgp("mapblck[QMRK]: end+start doc");
                _start_doc_suddenly();
                _line_progressed(3);
            }
            else
            {
                _c4dbgp("mapblck[QMRK]: start child seqblck (!)");
                addrem_flags(RKCL, RKEY|QMRK);
                _handle_annotations_before_blck_key_scalar();
                m_evt_handler->begin_seq_key_block();
                addrem_flags(RVAL|RSEQ, RMAP|RKCL|QMRK);
                _set_indentation(startindent);
                _line_progressed(1);
            }
            _maybe_skip_whitespace_tokens();
            goto mapblck_finish;
        }
        else if(first == '[')
        {
            _c4dbgp("mapblck[QMRK]: start child seqflow (!)");
            addrem_flags(RKCL, RKEY|QMRK);
            m_evt_handler->begin_seq_key_flow();
            addrem_flags(RVAL|RSEQ|FLOW, RMAP|RKCL|QMRK|BLCK);
            _set_indentation(m_evt_handler->m_parent->indref);
            _line_progressed(1);
            goto mapblck_finish;
        }
        else if(first == '{')
        {
            _c4dbgp("mapblck[QMRK]: start child mapblck (!)");
            addrem_flags(RKCL, RKEY|QMRK);
            m_evt_handler->begin_map_key_flow();
            addrem_flags(RKEY|FLOW, RVAL|RKCL|QMRK|BLCK);
            _set_indentation(m_evt_handler->m_parent->indref);
            _line_progressed(1);
            goto mapblck_finish;
        }
        else if(first == '?')
        {
            _c4dbgp("mapblck[QMRK]: another QMRK '?'");
            m_evt_handler->set_key_scalar_plain_empty();
            m_evt_handler->set_val_scalar_plain_empty();
            m_evt_handler->add_sibling();
            _line_progressed(1);
        }
        else if(first == '.')
        {
            _c4dbgp("mapblck[QMRK]: maybe end doc?");
            csubstr rs = rem.sub(1);
            if(rs == ".." || rs.begins_with(".. "))
            {
                _c4dbgp("mapblck[QMRK]: end+start doc");
                _end_doc_suddenly();
                _line_progressed(3);
                goto mapblck_finish;
            }
            else
            {
                _c4err("parse error");
            }
        }
        else
        {
            _c4err("parse error");
        }
    }

 mapblck_again:
    _c4dbgt("mapblck: again", 0);
    if(_finished_line())
    {
        _line_ended();
        _scan_line();
        if(_finished_file())
        {
            _c4dbgp("mapblck: file finished!");
            _end_map_blck();
            goto mapblck_finish;
        }
        _c4dbgnextline();
    }
    goto mapblck_start;

 mapblck_finish:
    _c4dbgp("mapblck: finish");
}


//-----------------------------------------------------------------------------

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_unk_json()
{
    _c4dbgpf("handle_unk_json indref={} target={}", m_evt_handler->m_curr->indref, m_evt_handler->m_curr->node_id);

    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RNXT|RSEQ|RMAP));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_all(RTOP));

    _maybe_skip_comment();
    csubstr rem = m_evt_handler->m_curr->line_contents.rem;
    if(!rem.len)
        return;

    size_t pos = rem.first_not_of(" \t");
    if(pos)
    {
        pos = pos != npos ? pos : rem.len;
        _c4dbgpf("skipping indentation of {}", pos);
        _line_progressed(pos);
        rem = m_evt_handler->m_curr->line_contents.rem;
        if(!rem.len)
            return;
        _c4dbgpf("rem is now [{}]~~~{}~~~", rem.len, rem);
    }

    if(rem.begins_with('['))
    {
        _c4dbgp("it's a seq");
        m_evt_handler->check_trailing_doc_token();
        _maybe_begin_doc();
        m_evt_handler->begin_seq_val_flow();
        addrem_flags(RSEQ|FLOW|RVAL, RUNK|RTOP|RDOC);
        _set_indentation(m_evt_handler->m_curr->line_contents.current_col(rem));
        m_doc_empty = false;
        _line_progressed(1);
    }
    else if(rem.begins_with('{'))
    {
        _c4dbgp("it's a map");
        m_evt_handler->check_trailing_doc_token();
        _maybe_begin_doc();
        m_evt_handler->begin_map_val_flow();
        addrem_flags(RMAP|FLOW|RKEY, RVAL|RTOP|RUNK|RDOC);
        m_doc_empty = false;
        _set_indentation(m_evt_handler->m_curr->line_contents.current_col(rem));
        _line_progressed(1);
    }
    else if(_handle_bom())
    {
        _c4dbgp("byte order mark");
    }
    else
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks,  ! has_any(SSCL));
        _maybe_skip_whitespace_tokens();
        csubstr s = m_evt_handler->m_curr->line_contents.rem;
        if(!s.len)
            return;
        const size_t startindent = m_evt_handler->m_curr->line_contents.indentation; // save
        const char first = s.str[0];
        ScannedScalar sc;
        if(first == '"')
        {
            _c4dbgp("runk_json: scanning double-quoted scalar");
            m_evt_handler->check_trailing_doc_token();
            _maybe_begin_doc();
            add_flags(RDOC);
            m_doc_empty = false;
            sc = _scan_scalar_dquot();
            csubstr maybe_filtered = _maybe_filter_val_scalar_dquot(sc);
            if(!_maybe_scan_following_colon())
            {
                _c4dbgp("runk_json: set as val");
                _handle_annotations_before_blck_val_scalar();
                m_evt_handler->set_val_scalar_dquoted(maybe_filtered);
            }
            else
            {
                _c4err("parse error");
            }
        }
        else if(_scan_scalar_plain_unk(&sc))
        {
            _c4dbgp("runk_json: got a plain scalar");
            m_evt_handler->check_trailing_doc_token();
            _maybe_begin_doc();
            add_flags(RDOC);
            m_doc_empty = false;
            if(!_maybe_scan_following_colon())
            {
                _c4dbgp("runk_json: set as val");
                _handle_annotations_before_blck_val_scalar();
                csubstr maybe_filtered = _maybe_filter_val_scalar_plain(sc, startindent);
                m_evt_handler->set_val_scalar_plain(maybe_filtered);
            }
            else
            {
                _c4err("parse error");
            }
        }
        else
        {
            _c4err("parse error");
        }
    }
}


//-----------------------------------------------------------------------------

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_unk()
{
    _c4dbgpf("handle_unk indref={} target={}", m_evt_handler->m_curr->indref, m_evt_handler->m_curr->node_id);

    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(RNXT|RSEQ|RMAP));
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_all(RTOP));

    _maybe_skip_comment();
    csubstr rem = m_evt_handler->m_curr->line_contents.rem;
    if(!rem.len)
        return;

    size_t pos = rem.first_not_of(" \t");
    if(pos)
    {
        pos = pos != npos ? pos : rem.len;
        _c4dbgpf("skipping {} whitespace characters", pos);
        _line_progressed(pos);
        rem = m_evt_handler->m_curr->line_contents.rem;
        if(!rem.len)
            return;
        _c4dbgpf("rem is now [{}]~~~{}~~~", rem.len, rem);
    }

    if(m_evt_handler->m_curr->line_contents.indentation == 0u && _at_line_begin())
    {
        _c4dbgp("rtop: zero indent + at line begin");
        if(_handle_bom())
        {
            _c4dbgp("byte order mark!");
            rem = m_evt_handler->m_curr->line_contents.rem;
            if(!rem.len)
                return;
        }
        const char first = rem.str[0];
        if(first == '-')
        {
            _c4dbgp("rtop: suspecting doc");
            if(_is_doc_begin_token(rem))
            {
                _c4dbgp("rtop: begin doc");
                _maybe_end_doc();
                _begin2_doc_expl();
                _set_indentation(0);
                addrem_flags(RDOC|RUNK, NDOC);
                _line_progressed(3u);
                _maybe_skip_whitespace_tokens();
                return;
            }
        }
        else if(first == '.')
        {
            _c4dbgp("rtop: suspecting doc end");
            if(_is_doc_end_token(rem))
            {
                _c4dbgp("rtop: end doc");
                if(has_any(RDOC))
                {
                    _end2_doc_expl();
                }
                else
                {
                    _c4dbgp("rtop: ignore end doc");
                }
                addrem_flags(NDOC|RUNK, RDOC);
                _line_progressed(3u);
                _maybe_skip_whitespace_tokens();
                return;
            }
        }
        else if(first == '%')
        {
            _c4dbgpf("directive: {}", rem);
            if(C4_UNLIKELY(!m_doc_empty && has_none(NDOC)))
                _RYML_CB_ERR(m_evt_handler->m_stack.m_callbacks, "need document footer before directives");
            _handle_directive(rem);
            return;
        }
    }

    /* no else-if! */
    char first = rem.str[0];

    if(first == '[')
    {
        m_evt_handler->check_trailing_doc_token();
        _maybe_begin_doc();
        m_doc_empty = false;
        const size_t startindent = m_evt_handler->m_curr->line_contents.current_col(rem);
        if(C4_LIKELY( ! _annotations_require_key_container()))
        {
            _c4dbgp("it's a seq, flow");
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->begin_seq_val_flow();
            addrem_flags(RSEQ|FLOW|RVAL, RUNK|RTOP|RDOC);
            _set_indentation(startindent);
        }
        else
        {
            _c4dbgp("start new block map, set flow seq as key (!)");
            _handle_annotations_before_start_mapblck(m_evt_handler->m_curr->pos.line);
            m_evt_handler->begin_map_val_block();
            addrem_flags(RMAP|BLCK|RKCL, RUNK|RTOP|RDOC);
            _handle_annotations_and_indentation_after_start_mapblck(startindent, m_evt_handler->m_curr->pos.line);
            m_evt_handler->begin_seq_key_flow();
            addrem_flags(RSEQ|FLOW|RVAL, RMAP|BLCK|RKCL);
            _set_indentation(startindent);
        }
        _line_progressed(1);
    }
    else if(first == '{')
    {
        m_evt_handler->check_trailing_doc_token();
        _maybe_begin_doc();
        m_doc_empty = false;
        const size_t startindent = m_evt_handler->m_curr->line_contents.current_col(rem);
        if(C4_LIKELY( ! _annotations_require_key_container()))
        {
            _c4dbgp("it's a map, flow");
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->begin_map_val_flow();
            addrem_flags(RMAP|FLOW|RKEY, RVAL|RTOP|RUNK|RDOC);
            _set_indentation(startindent);
        }
        else
        {
            _c4dbgp("start new block map, set flow map as key (!)");
            _handle_annotations_before_start_mapblck(m_evt_handler->m_curr->pos.line);
            m_evt_handler->begin_map_val_block();
            addrem_flags(RMAP|BLCK|RKCL, RUNK|RTOP|RDOC);
            _handle_annotations_and_indentation_after_start_mapblck(startindent, m_evt_handler->m_curr->pos.line);
            m_evt_handler->begin_map_key_flow();
            addrem_flags(RMAP|FLOW|RKEY, BLCK|RKCL);
            _set_indentation(startindent);
        }
        _line_progressed(1);
    }
    else if(first == '-' && _is_blck_token(rem))
    {
        _c4dbgp("it's a seq, block");
        m_evt_handler->check_trailing_doc_token();
        _maybe_begin_doc();
        _handle_annotations_before_blck_val_scalar();
        m_evt_handler->begin_seq_val_block();
        addrem_flags(RSEQ|BLCK|RVAL, RNXT|RTOP|RUNK|RDOC);
        m_doc_empty = false;
        _set_indentation(m_evt_handler->m_curr->line_contents.current_col(rem));
        _line_progressed(1);
        _maybe_skip_whitespace_tokens();
    }
    else if(first == '?' && _is_blck_token(rem))
    {
        _c4dbgp("it's a map + this key is complex");
        m_evt_handler->check_trailing_doc_token();
        _maybe_begin_doc();
        _handle_annotations_before_blck_val_scalar();
        m_evt_handler->begin_map_val_block();
        addrem_flags(RMAP|BLCK|QMRK, RKEY|RVAL|RTOP|RUNK);
        m_doc_empty = false;
        m_was_inside_qmrk = true;
        _save_indentation();
        _line_progressed(1);
        _maybe_skip_whitespace_tokens();
    }
    else if(first == ':' && _is_blck_token(rem))
    {
        if(m_doc_empty)
        {
            _c4dbgp("it's a map with an empty key");
            const size_t startindent = m_evt_handler->m_curr->line_contents.indentation; // save
            const size_t startline = m_evt_handler->m_curr->pos.line; // save
            m_evt_handler->check_trailing_doc_token();
            _maybe_begin_doc();
            _handle_annotations_before_start_mapblck(startline);
            _handle_colon();
            m_evt_handler->begin_map_val_block();
            _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
            m_evt_handler->set_key_scalar_plain_empty();
            m_doc_empty = false;
            _set_indentation(startindent);
        }
        else
        {
            _c4dbgp("actually prev val is a key!");
            size_t prev_indentation = m_evt_handler->m_curr->indref;
            m_evt_handler->actually_val_is_first_key_of_new_map_block();
            _set_indentation(prev_indentation);
        }
        addrem_flags(RMAP|BLCK|RVAL, RTOP|RUNK|RDOC);
        _line_progressed(1);
        _maybe_skip_whitespace_tokens();
    }
    else if(first == '&')
    {
        csubstr anchor = _scan_anchor();
        _c4dbgpf("anchor! [{}]~~~{}~~~", anchor.len, anchor);
        m_evt_handler->check_trailing_doc_token();
        _maybe_begin_doc();
        const size_t indentation = m_evt_handler->m_curr->line_contents.current_col(rem);
        const size_t line = m_evt_handler->m_curr->pos.line;
        _add_annotation(&m_pending_anchors, anchor, indentation, line);
        _set_indentation(m_evt_handler->m_curr->line_contents.current_col(rem));
        m_doc_empty = false;
    }
    else if(first == '*')
    {
        csubstr ref = _scan_ref_map();
        _c4dbgpf("ref! [{}]~~~{}~~~", ref.len, ref);
        m_evt_handler->check_trailing_doc_token();
        _maybe_begin_doc();
        m_doc_empty = false;
        if(!_maybe_scan_following_colon())
        {
            _c4dbgp("runk: set val ref");
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->set_val_ref(ref);
        }
        else
        {
            _c4dbgp("runk: start new block map, set ref as key");
            const size_t startindent = m_evt_handler->m_curr->line_contents.indentation; // save
            const size_t startline = m_evt_handler->m_curr->pos.line; // save
            _handle_annotations_before_start_mapblck(startline);
            m_evt_handler->begin_map_val_block();
            _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
            m_evt_handler->set_key_ref(ref);
            _maybe_skip_whitespace_tokens();
            _set_indentation(startindent);
            addrem_flags(RMAP|BLCK|RVAL, RTOP|RUNK|RDOC);
        }
    }
    else if(first == '!')
    {
        csubstr tag = _scan_tag();
        _c4dbgpf("unk: val tag! [{}]~~~{}~~~", tag.len, tag);
        // we need to buffer the tags, as there may be two
        // consecutive tags in here
        const size_t indentation = m_evt_handler->m_curr->line_contents.current_col(rem);
        const size_t line = m_evt_handler->m_curr->pos.line;
        _add_annotation(&m_pending_tags, tag, indentation, line);
    }
    else
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks,  ! has_any(SSCL));
        _maybe_skip_whitespace_tokens();
        csubstr s = m_evt_handler->m_curr->line_contents.rem;
        if(!s.len)
            return;
        const size_t startindent = m_evt_handler->m_curr->line_contents.indentation; // save
        const size_t startline = m_evt_handler->m_curr->pos.line; // save
        first = s.str[0];
        ScannedScalar sc;
        if(first == '\'')
        {
            _c4dbgp("runk: scanning single-quoted scalar");
            m_evt_handler->check_trailing_doc_token();
            _maybe_begin_doc();
            add_flags(RDOC);
            m_doc_empty = false;
            sc = _scan_scalar_squot();
            if(!_maybe_scan_following_colon())
            {
                _c4dbgp("runk: set as val");
                _handle_annotations_before_blck_val_scalar();
                csubstr maybe_filtered = _maybe_filter_val_scalar_squot(sc);
                m_evt_handler->set_val_scalar_squoted(maybe_filtered);
            }
            else
            {
                _c4dbgp("runk: start new block map, set scalar as key");
                _handle_annotations_before_start_mapblck(startline);
                _handle_colon();
                m_evt_handler->begin_map_val_block();
                _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
                csubstr maybe_filtered = _maybe_filter_key_scalar_squot(sc);
                m_evt_handler->set_key_scalar_squoted(maybe_filtered);
                _maybe_skip_whitespace_tokens();
                _set_indentation(startindent);
                addrem_flags(RMAP|BLCK|RVAL, RTOP|RUNK|RDOC);
            }
        }
        else if(first == '"')
        {
            _c4dbgp("runk: scanning double-quoted scalar");
            m_evt_handler->check_trailing_doc_token();
            _maybe_begin_doc();
            add_flags(RDOC);
            m_doc_empty = false;
            sc = _scan_scalar_dquot();
            if(!_maybe_scan_following_colon())
            {
                _c4dbgp("runk: set as val");
                _handle_annotations_before_blck_val_scalar();
                csubstr maybe_filtered = _maybe_filter_val_scalar_dquot(sc);
                m_evt_handler->set_val_scalar_dquoted(maybe_filtered);
            }
            else
            {
                _c4dbgp("runk: start new block map, set double-quoted scalar as key");
                _handle_annotations_before_start_mapblck(startline);
                m_evt_handler->begin_map_val_block();
                _handle_colon();
                _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
                csubstr maybe_filtered = _maybe_filter_key_scalar_dquot(sc);
                m_evt_handler->set_key_scalar_dquoted(maybe_filtered);
                _maybe_skip_whitespace_tokens();
                _set_indentation(startindent);
                addrem_flags(RMAP|BLCK|RVAL, RTOP|RUNK|RDOC);
            }
        }
        else if(first == '|')
        {
            _c4dbgp("runk: scanning block-literal scalar");
            m_evt_handler->check_trailing_doc_token();
            _maybe_begin_doc();
            add_flags(RDOC);
            m_doc_empty = false;
            ScannedBlock sb;
            _scan_block(&sb, startindent);
            if(C4_LIKELY(!_maybe_scan_following_colon()))
            {
                _c4dbgp("runk: set as val");
                _handle_annotations_before_blck_val_scalar();
                csubstr maybe_filtered = _maybe_filter_val_scalar_literal(sb);
                m_evt_handler->set_val_scalar_literal(maybe_filtered);
            }
            else
            {
                _c4err("block literal keys must be enclosed in '?'");
            }
        }
        else if(first == '>')
        {
            _c4dbgp("runk: scanning block-folded scalar");
            m_evt_handler->check_trailing_doc_token();
            _maybe_begin_doc();
            add_flags(RDOC);
            m_doc_empty = false;
            ScannedBlock sb;
            _scan_block(&sb, startindent);
            if(C4_LIKELY(!_maybe_scan_following_colon()))
            {
                _c4dbgp("runk: set as val");
                _handle_annotations_before_blck_val_scalar();
                csubstr maybe_filtered = _maybe_filter_val_scalar_folded(sb);
                m_evt_handler->set_val_scalar_folded(maybe_filtered);
            }
            else
            {
                _c4err("block folded keys must be enclosed in '?'");
            }
        }
        else if(_scan_scalar_plain_unk(&sc))
        {
            _c4dbgp("runk: got a plain scalar");
            m_evt_handler->check_trailing_doc_token();
            _maybe_begin_doc();
            add_flags(RDOC);
            m_doc_empty = false;
            if(!_maybe_scan_following_colon())
            {
                _c4dbgp("runk: set as val");
                _handle_annotations_before_blck_val_scalar();
                csubstr maybe_filtered = _maybe_filter_val_scalar_plain(sc, startindent);
                m_evt_handler->set_val_scalar_plain(maybe_filtered);
            }
            else
            {
                _c4dbgp("runk: start new block map, set scalar as key");
                _handle_annotations_before_start_mapblck(startline);
                _handle_colon();
                m_evt_handler->begin_map_val_block();
                _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
                csubstr maybe_filtered = _maybe_filter_key_scalar_plain(sc, startindent);
                m_evt_handler->set_key_scalar_plain(maybe_filtered);
                _maybe_skip_whitespace_tokens();
                _set_indentation(startindent);
                addrem_flags(RMAP|BLCK|RVAL, RTOP|RUNK|RDOC);
            }
        }
    }
}


//-----------------------------------------------------------------------------

template<class EventHandler>
C4_COLD void ParseEngine<EventHandler>::_handle_usty()
{
    _c4dbgpf("handle_usty target={}", m_evt_handler->m_curr->indref, m_evt_handler->m_curr->node_id);

    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_none(BLCK|FLOW));

    #ifdef RYML_NO_COVERAGE__TO_BE_DELETED
    if(has_any(RNXT))
    {
        _c4dbgp("usty[RNXT]: finishing!");
        _end_stream();
    }
    #endif

    _maybe_skip_comment();
    csubstr rem = m_evt_handler->m_curr->line_contents.rem;
    if(!rem.len)
        return;

    size_t pos = rem.first_not_of(" \t");
    if(pos)
    {
        pos = pos != npos ? pos : rem.len;
        _c4dbgpf("skipping indentation of {}", pos);
        _line_progressed(pos);
        rem = m_evt_handler->m_curr->line_contents.rem;
        if(!rem.len)
            return;
        _c4dbgpf("rem is now [{}]~~~{}~~~", rem.len, rem);
    }

    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, rem.len > 0);
    size_t startindent = m_evt_handler->m_curr->line_contents.indentation; // save
    char first = rem.str[0];
    if(has_any(RSEQ)) // destination is a sequence
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks,  ! has_any(RMAP));
        _c4dbgpf("usty[RSEQ]: first='{}'", _c4prc(first));
        if(first == '[')
        {
            _c4dbgp("usty[RSEQ]: it's a flow seq. merging it");
            add_flags(RNXT);
            m_evt_handler->_push();
            addrem_flags(FLOW|RVAL, RNXT|USTY);
            _set_indentation(startindent);
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
        }
        else if(first == '-' && _is_blck_token(rem))
        {
            _c4dbgp("usty[RSEQ]: it's a block seq. merging it");
            add_flags(RNXT);
            m_evt_handler->_push();
            addrem_flags(BLCK|RVAL, RNXT|USTY);
            _set_indentation(startindent);
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
        }
        else
        {
            _c4err("can only parse a seq into an existing seq");
        }
    }
    else if(has_any(RMAP)) // destination is a map
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks,  ! has_any(RSEQ));
        _c4dbgpf("usty[RMAP]: first='{}'", _c4prc(first));
        if(first == '{')
        {
            _c4dbgp("usty[RMAP]: it's a flow map. merging it");
            add_flags(RNXT);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->_push();
            addrem_flags(RMAP|FLOW|RKEY, RNXT|USTY);
            _set_indentation(startindent);
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
        }
        else if(first == '?' && _is_blck_token(rem))
        {
            _c4dbgp("usty[RMAP]: it's a block map + this key is complex");
            add_flags(RNXT);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->_push();
            addrem_flags(RMAP|BLCK|QMRK, RNXT|USTY);
            m_was_inside_qmrk = true;
            _save_indentation();
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
        }
        else if(first == ':' && _is_blck_token(rem))
        {
            _c4dbgp("usty[RMAP]: it's a map with an empty key");
            add_flags(RNXT);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->_push();
            m_evt_handler->set_key_scalar_plain_empty();
            addrem_flags(RMAP|BLCK|RVAL, RNXT|USTY);
            _save_indentation();
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
        }
        else if(rem.begins_with('&'))
        {
            csubstr anchor = _scan_anchor();
            _c4dbgpf("usty[RMAP]: anchor! [{}]~~~{}~~~", anchor.len, anchor);
            const size_t indentation = m_evt_handler->m_curr->line_contents.current_col(rem);
            const size_t line = m_evt_handler->m_curr->pos.line;
            _add_annotation(&m_pending_anchors, anchor, indentation, line);
            _set_indentation(m_evt_handler->m_curr->line_contents.current_col(rem));
        }
        else if(first == '*')
        {
            csubstr ref = _scan_ref_map();
            _c4dbgpf("usty[RMAP]: ref! [{}]~~~{}~~~", ref.len, ref);
            if(!_maybe_scan_following_colon())
            {
                _c4err("cannot read a VAL to a map");
            }
            else
            {
                _c4dbgp("usty[RMAP]: start new block map, set ref as key");
                const size_t startline = m_evt_handler->m_curr->pos.line; // save
                add_flags(RNXT);
                _handle_annotations_before_start_mapblck(startline);
                m_evt_handler->_push();
                _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
                m_evt_handler->set_key_ref(ref);
                _maybe_skip_whitespace_tokens();
                _set_indentation(startindent);
                addrem_flags(RMAP|BLCK|RVAL, RNXT|USTY);
            }
        }
        else if(first == '!')
        {
            csubstr tag = _scan_tag();
            _c4dbgpf("usty[RMAP]: val tag! [{}]~~~{}~~~", tag.len, tag);
            // we need to buffer the tags, as there may be two
            // consecutive tags in here
            const size_t indentation = m_evt_handler->m_curr->line_contents.current_col(rem);
            const size_t line = m_evt_handler->m_curr->pos.line;
            _add_annotation(&m_pending_tags, tag, indentation, line);
        }
        else if(first == '[' || (first == '-' && _is_blck_token(rem)))
        {
            _c4err("cannot parse a seq into an existing map");
        }
        else
        {
            _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks,  ! has_any(SSCL));
            startindent = m_evt_handler->m_curr->line_contents.indentation; // save
            const size_t startline = m_evt_handler->m_curr->pos.line; // save
            ScannedScalar sc;
            _c4dbgpf("usty[RMAP]: maybe scalar. first='{}'", _c4prc(first));
            if(first == '\'')
            {
                _c4dbgp("usty[RMAP]: scanning single-quoted scalar");
                sc = _scan_scalar_squot();
                if(!_maybe_scan_following_colon())
                {
                    _c4err("cannot read a VAL to a map");
                }
                else
                {
                    _c4dbgp("usty[RMAP]: start new block map, set scalar as key");
                    add_flags(RNXT);
                    _handle_annotations_before_start_mapblck(startline);
                    m_evt_handler->_push();
                    _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
                    csubstr maybe_filtered = _maybe_filter_key_scalar_squot(sc);
                    m_evt_handler->set_key_scalar_squoted(maybe_filtered);
                    _set_indentation(startindent);
                    addrem_flags(RMAP|BLCK|RVAL, RNXT|USTY);
                    _maybe_skip_whitespace_tokens();
                }
            }
            else if(first == '"')
            {
                _c4dbgp("usty[RMAP]: scanning double-quoted scalar");
                sc = _scan_scalar_dquot();
                if(!_maybe_scan_following_colon())
                {
                    _c4err("cannot read a VAL to a map");
                }
                else
                {
                    _c4dbgp("usty[RMAP]: start new block map, set double-quoted scalar as key");
                    add_flags(RNXT);
                    _handle_annotations_before_start_mapblck(startline);
                    m_evt_handler->_push();
                    _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
                    csubstr maybe_filtered = _maybe_filter_key_scalar_dquot(sc);
                    m_evt_handler->set_key_scalar_dquoted(maybe_filtered);
                    _set_indentation(startindent);
                    addrem_flags(RMAP|BLCK|RVAL, RNXT|USTY);
                    _maybe_skip_whitespace_tokens();
                }
            }
            else if(first == '|')
            {
                _c4err("block literal keys must be enclosed in '?'");
            }
            else if(first == '>')
            {
                _c4err("block literal keys must be enclosed in '?'");
            }
            else if(_scan_scalar_plain_unk(&sc))
            {
                _c4dbgp("usty[RMAP]: got a plain scalar");
                if(!_maybe_scan_following_colon())
                {
                    _c4err("cannot read a VAL to a map");
                }
                else
                {
                    _c4dbgp("usty[RMAP]: start new block map, set scalar as key");
                    add_flags(RNXT);
                    _handle_annotations_before_start_mapblck(startline);
                    m_evt_handler->_push();
                    _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
                    csubstr maybe_filtered = _maybe_filter_key_scalar_plain(sc, startindent);
                    m_evt_handler->set_key_scalar_plain(maybe_filtered);
                    _set_indentation(startindent);
                    addrem_flags(RMAP|BLCK|RVAL, RNXT|USTY);
                    _maybe_skip_whitespace_tokens();
                }
            }
            else
            {
                _c4err("parse error");
            }
        }
    }
    else // destination is unknown
    {
        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks,  ! has_any(RSEQ));
        _c4dbgpf("usty[UNK]: first='{}'", _c4prc(first));
        if(first == '[')
        {
            _c4dbgp("usty[UNK]: it's a flow seq");
            add_flags(RNXT);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->begin_seq_val_flow();
            addrem_flags(RSEQ|FLOW|RVAL, RNXT|USTY);
            _set_indentation(startindent);
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
        }
        else if(first == '-' && _is_blck_token(rem))
        {
            _c4dbgp("usty[UNK]: it's a block seq");
            add_flags(RNXT);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->begin_seq_val_block();
            addrem_flags(RSEQ|BLCK|RVAL, RNXT|USTY);
            _set_indentation(startindent);
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
        }
        else if(first == '{')
        {
            _c4dbgp("usty[UNK]: it's a flow map");
            add_flags(RNXT);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->begin_map_val_flow();
            addrem_flags(RMAP|FLOW|RKEY, RNXT|USTY);
            _set_indentation(startindent);
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
        }
        else if(first == '?' && _is_blck_token(rem))
        {
            _c4dbgp("usty[UNK]: it's a map + this key is complex");
            add_flags(RNXT);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->begin_map_val_block();
            addrem_flags(RMAP|BLCK|QMRK, RNXT|USTY);
            m_was_inside_qmrk = true;
            _save_indentation();
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
        }
        else if(first == ':' && _is_blck_token(rem))
        {
            _c4dbgp("usty[UNK]: it's a map with an empty key");
            add_flags(RNXT);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->begin_map_val_block();
            m_evt_handler->set_key_scalar_plain_empty();
            addrem_flags(RMAP|BLCK|RVAL, RNXT|USTY);
            _save_indentation();
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
        }
        else if(first == '&')
        {
            csubstr anchor = _scan_anchor();
            _c4dbgpf("usty[UNK]: anchor! [{}]~~~{}~~~", anchor.len, anchor);
            const size_t indentation = m_evt_handler->m_curr->line_contents.current_col(rem);
            const size_t line = m_evt_handler->m_curr->pos.line;
            _add_annotation(&m_pending_anchors, anchor, indentation, line);
            _set_indentation(m_evt_handler->m_curr->line_contents.current_col(rem));
        }
        else if(first == '*')
        {
            csubstr ref = _scan_ref_map();
            _c4dbgpf("usty[UNK]: ref! [{}]~~~{}~~~", ref.len, ref);
            if(!_maybe_scan_following_colon())
            {
                _c4dbgp("usty[UNK]: set val ref");
                _handle_annotations_before_blck_val_scalar();
                m_evt_handler->set_val_ref(ref);
            }
            else
            {
                _c4dbgp("usty[UNK]: start new block map, set ref as key");
                const size_t startline = m_evt_handler->m_curr->pos.line; // save
                add_flags(RNXT);
                _handle_annotations_before_start_mapblck(startline);
                m_evt_handler->begin_map_val_block();
                _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
                m_evt_handler->set_key_ref(ref);
                _maybe_skip_whitespace_tokens();
                _set_indentation(startindent);
                addrem_flags(RMAP|BLCK|RVAL, RNXT|USTY);
            }
        }
        else if(first == '!')
        {
            csubstr tag = _scan_tag();
            _c4dbgpf("usty[UNK]: val tag! [{}]~~~{}~~~", tag.len, tag);
            // we need to buffer the tags, as there may be two
            // consecutive tags in here
            const size_t indentation = m_evt_handler->m_curr->line_contents.current_col(rem);
            const size_t line = m_evt_handler->m_curr->pos.line;
            _add_annotation(&m_pending_tags, tag, indentation, line);
        }
        else
        {
            _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks,  ! has_any(SSCL));
            startindent = m_evt_handler->m_curr->line_contents.indentation; // save
            const size_t startline = m_evt_handler->m_curr->pos.line; // save
            first = rem.str[0];
            ScannedScalar sc;
            _c4dbgpf("usty[UNK]: maybe scalar. first='{}'", _c4prc(first));
            if(first == '\'')
            {
                _c4dbgp("usty[UNK]: scanning single-quoted scalar");
                sc = _scan_scalar_squot();
                if(!_maybe_scan_following_colon())
                {
                    _c4dbgp("usty[UNK]: set as val");
                    _handle_annotations_before_blck_val_scalar();
                    csubstr maybe_filtered = _maybe_filter_val_scalar_squot(sc);
                    m_evt_handler->set_val_scalar_squoted(maybe_filtered);
                    _end_stream();
                }
                else
                {
                    _c4dbgp("usty[UNK]: start new block map, set scalar as key");
                    add_flags(RNXT);
                    _handle_annotations_before_start_mapblck(startline);
                    m_evt_handler->begin_map_val_block();
                    _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
                    csubstr maybe_filtered = _maybe_filter_key_scalar_squot(sc);
                    m_evt_handler->set_key_scalar_squoted(maybe_filtered);
                    _set_indentation(startindent);
                    addrem_flags(RMAP|BLCK|RVAL, RNXT|USTY);
                    _maybe_skip_whitespace_tokens();
                }
            }
            else if(first == '"')
            {
                _c4dbgp("usty[UNK]: scanning double-quoted scalar");
                sc = _scan_scalar_dquot();
                if(!_maybe_scan_following_colon())
                {
                    _c4dbgp("usty[UNK]: set as val");
                    _handle_annotations_before_blck_val_scalar();
                    csubstr maybe_filtered = _maybe_filter_val_scalar_dquot(sc);
                    m_evt_handler->set_val_scalar_dquoted(maybe_filtered);
                    _end_stream();
                }
                else
                {
                    _c4dbgp("usty[UNK]: start new block map, set double-quoted scalar as key");
                    add_flags(RNXT);
                    _handle_annotations_before_start_mapblck(startline);
                    m_evt_handler->begin_map_val_block();
                    _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
                    csubstr maybe_filtered = _maybe_filter_key_scalar_dquot(sc);
                    m_evt_handler->set_key_scalar_dquoted(maybe_filtered);
                    _set_indentation(startindent);
                    addrem_flags(RMAP|BLCK|RVAL, RNXT|USTY);
                    _maybe_skip_whitespace_tokens();
                }
            }
            else if(first == '|')
            {
                _c4dbgp("usty[UNK]: scanning block-literal scalar");
                ScannedBlock sb;
                _scan_block(&sb, startindent);
                _c4dbgp("usty[UNK]: set as val");
                _handle_annotations_before_blck_val_scalar();
                csubstr maybe_filtered = _maybe_filter_val_scalar_literal(sb);
                m_evt_handler->set_val_scalar_literal(maybe_filtered);
                _end_stream();
            }
            else if(first == '>')
            {
                _c4dbgp("usty[UNK]: scanning block-folded scalar");
                ScannedBlock sb;
                _scan_block(&sb, startindent);
                _c4dbgp("usty[UNK]: set as val");
                _handle_annotations_before_blck_val_scalar();
                csubstr maybe_filtered = _maybe_filter_val_scalar_folded(sb);
                m_evt_handler->set_val_scalar_folded(maybe_filtered);
                _end_stream();
            }
            else if(_scan_scalar_plain_unk(&sc))
            {
                _c4dbgp("usty[UNK]: got a plain scalar");
                if(!_maybe_scan_following_colon())
                {
                    _c4dbgp("usty[UNK]: set as val");
                    _handle_annotations_before_blck_val_scalar();
                    csubstr maybe_filtered = _maybe_filter_val_scalar_plain(sc, startindent);
                    m_evt_handler->set_val_scalar_plain(maybe_filtered);
                    _end_stream();
                }
                else
                {
                    _c4dbgp("usty[UNK]: start new block map, set scalar as key");
                    add_flags(RNXT);
                    _handle_annotations_before_start_mapblck(startline);
                    m_evt_handler->begin_map_val_block();
                    _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
                    csubstr maybe_filtered = _maybe_filter_key_scalar_plain(sc, startindent);
                    m_evt_handler->set_key_scalar_plain(maybe_filtered);
                    _set_indentation(startindent);
                    addrem_flags(RMAP|BLCK|RVAL, RNXT|USTY);
                    _maybe_skip_whitespace_tokens();
                }
            }
            else
            {
                _c4err("parse error");
            }
        }
    }
}


//-----------------------------------------------------------------------------

template<class EventHandler>
void ParseEngine<EventHandler>::parse_json_in_place_ev(csubstr filename, substr src)
{
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_stack.size() >= 1);
    m_file = filename;
    m_buf = src;
    _reset();
    m_evt_handler->start_parse(filename.str, &_s_relocate_arena, this);
    m_evt_handler->begin_stream();
    while( ! _finished_file())
    {
        _scan_line();
        while( ! _finished_line())
        {
            _c4dbgnextline();
            _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks,  ! m_evt_handler->m_curr->line_contents.rem.empty());
            if(has_any(RSEQ))
            {
                _handle_seq_json();
            }
            else if(has_any(RMAP))
            {
                _handle_map_json();
            }
            else if(has_any(RUNK))
            {
                _handle_unk_json();
            }
            else
            {
                _c4err("internal error");
            }
        }
        if(_finished_file())
            break; // it may have finished because of multiline blocks
        _line_ended();
    }
    _end_stream();
    m_evt_handler->finish_parse();
}


//-----------------------------------------------------------------------------

template<class EventHandler>
void ParseEngine<EventHandler>::parse_in_place_ev(csubstr filename, substr src)
{
    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_stack.size() >= 1);
    m_file = filename;
    m_buf = src;
    _reset();
    m_evt_handler->start_parse(filename.str, &_s_relocate_arena, this);
    m_evt_handler->begin_stream();
    while( ! _finished_file())
    {
        _scan_line();
        while( ! _finished_line())
        {
            _c4dbgnextline();
            _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks,  ! m_evt_handler->m_curr->line_contents.rem.empty());
            if(has_any(FLOW))
            {
                if(has_none(RSEQIMAP))
                {
                    if(has_any(RSEQ))
                    {
                        _handle_seq_flow();
                    }
                    else
                    {
                        _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_all(RMAP));
                        _handle_map_flow();
                    }
                }
                else
                {
                    _handle_seq_imap();
                }
            }
            else if(has_any(BLCK))
            {
                if(has_any(RSEQ))
                {
                    _handle_seq_block();
                }
                else
                {
                    _RYML_CB_ASSERT(m_evt_handler->m_stack.m_callbacks, has_all(RMAP));
                    _handle_map_block();
                }
            }
            else if(has_any(RUNK))
            {
                _handle_unk();
            }
            else if(has_any(USTY))
            {
                _handle_usty();
            }
            else
            {
                _c4err("internal error");
            }
        }
        if(_finished_file())
            break; // it may have finished because of multiline blocks
        _line_ended();
    }
    _end_stream();
    m_evt_handler->finish_parse();
}
/** @endcond */

} // namespace yml
} // namespace c4

// NOLINTEND(hicpp-signed-bitwise,cppcoreguidelines-avoid-goto,hicpp-avoid-goto,hicpp-multiway-paths-covered)

#undef _c4dbgnextline

#if defined(_MSC_VER)
#   pragma warning(pop)
#elif defined(__clang__)
#   pragma clang diagnostic pop
#elif defined(__GNUC__)
#   pragma GCC diagnostic pop
#endif

#endif // _C4_YML_PARSE_ENGINE_DEF_HPP_
