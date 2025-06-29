#ifndef _C4_YML_PARSER_STATE_HPP_
#define _C4_YML_PARSER_STATE_HPP_

#ifndef _C4_YML_COMMON_HPP_
#include "c4/yml/common.hpp"
#endif

// NOLINTBEGIN(hicpp-signed-bitwise)

namespace c4 {
namespace yml {

/** data type for @ref ParserState_e */
using ParserFlag_t = int;

/** Enumeration of the state flags for the parser */
typedef enum : ParserFlag_t {
    RTOP = 0x01 <<  0,   ///< reading at top level
    RUNK = 0x01 <<  1,   ///< reading unknown state (when starting): must determine whether scalar, map or seq
    RMAP = 0x01 <<  2,   ///< reading a map
    RSEQ = 0x01 <<  3,   ///< reading a seq
    FLOW = 0x01 <<  4,   ///< reading is inside explicit flow chars: [] or {}
    BLCK = 0x01 <<  5,   ///< reading in block mode
    QMRK = 0x01 <<  6,   ///< reading an explicit key (`? key`)
    RKEY = 0x01 <<  7,   ///< reading a scalar as key
    RVAL = 0x01 <<  9,   ///< reading a scalar as val
    RKCL = 0x01 <<  8,   ///< reading the key colon (ie the : after the key in the map)
    RNXT = 0x01 << 10,   ///< read next val or keyval
    SSCL = 0x01 << 11,   ///< there's a stored scalar
    QSCL = 0x01 << 12,   ///< stored scalar was quoted
    RSET = 0x01 << 13,   ///< the (implicit) map being read is a !!set. @see https://yaml.org/type/set.html
    RDOC = 0x01 << 14,   ///< reading a document
    NDOC = 0x01 << 15,   ///< no document mode. a document has ended and another has not started yet.
    USTY = 0x01 << 16,   ///< reading in unknown style mode - must determine FLOW or BLCK
    //! reading an implicit map nested in an explicit seq.
    //! eg, {key: [key2: value2, key3: value3]}
    //! is parsed as {key: [{key2: value2}, {key3: value3}]}
    RSEQIMAP = 0x01 << 17,
} ParserState_e;

#ifdef RYML_DBG
/** @cond dev */
namespace detail {
csubstr _parser_flags_to_str(substr buf, ParserFlag_t flags);
} // namespace
/** @endcond */
#endif


/** Helper to control the line contents while parsing a buffer */
struct LineContents
{
    substr  rem;         ///< the stripped line remainder; initially starts at the first non-space character
    size_t  indentation; ///< the number of spaces on the beginning of the line
    substr  full;        ///< the full line, including newlines on the right
    substr  stripped;    ///< the stripped line, excluding newlines on the right

    LineContents() = default;

    void reset_with_next_line(substr buf, size_t offset)
    {
        RYML_ASSERT(offset <= buf.len);
        size_t e = offset;
        // get the current line stripped of newline chars
        while(e < buf.len && (buf.str[e] != '\n' && buf.str[e] != '\r'))
            ++e;
        RYML_ASSERT(e >= offset);
        const substr stripped_ = buf.range(offset, e);
        #if defined(__GNUC__) && __GNUC__ == 11
        C4_DONT_OPTIMIZE(stripped_);
        #endif
        // advance pos to include the first line ending
        if(e < buf.len && buf.str[e] == '\r')
            ++e;
        if(e < buf.len && buf.str[e] == '\n')
            ++e;
        const substr full_ = buf.range(offset, e);
        reset(full_, stripped_);
    }

    void reset(substr full_, substr stripped_)
    {
        rem = stripped_;
        indentation = stripped_.first_not_of(' ');  // find the first column where the character is not a space
        full = full_;
        stripped = stripped_;
    }

    C4_ALWAYS_INLINE size_t current_col() const RYML_NOEXCEPT
    {
        // WARNING: gcc x86 release builds were wrong (eg returning 0
        // when the result should be 4 ) when this function was like
        // this:
        //
        //return current_col(rem);
        //
        // (see below for the full definition of the called overload
        // of current_col())
        //
        // ... so we explicitly inline the code in here:
        RYML_ASSERT(rem.str >= full.str);
        size_t col = static_cast<size_t>(rem.str - full.str);
        return col;
        //
        // this was happening only on builds specifically with (gcc
        // AND x86 AND release); no other builds were having the
        // problem: not in debug, not in x64, not in other
        // architectures, not in clang, not in visual studio. WTF!?
        //
        // Enabling debug prints with RYML_DBG made the problem go
        // away, so these could not be used to debug the
        // problem. Adding prints inside the called current_col() also
        // made the problem go away! WTF!???
        //
        // a prize will be offered to anybody able to explain why this
        // was happening.
    }

    C4_ALWAYS_INLINE size_t current_col(csubstr s) const RYML_NOEXCEPT
    {
        RYML_ASSERT(s.str >= full.str);
        RYML_ASSERT(full.is_super(s));
        size_t col = static_cast<size_t>(s.str - full.str);
        return col;
    }
};
static_assert(std::is_standard_layout<LineContents>::value, "LineContents not standard");


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

struct ParserState
{
    LineContents line_contents;
    Location     pos;
    ParserFlag_t flags;
    size_t       indref;  ///< the reference indentation in the current block scope
    id_type      level;
    id_type      node_id; ///< don't hold a pointer to the node as it will be relocated during tree resizes
    size_t       scalar_col; // the column where the scalar (or its quotes) begin
    bool         more_indented;
    bool         has_children;

    ParserState() = default;

    void start_parse(const char *file, id_type node_id_)
    {
        level = 0;
        pos.name = to_csubstr(file);
        pos.offset = 0;
        pos.line = 1;
        pos.col = 1;
        node_id = node_id_;
        more_indented = false;
        scalar_col = 0;
        indref = 0;
        has_children = false;
    }

    void reset_after_push()
    {
        node_id = NONE;
        indref = npos;
        more_indented = false;
        ++level;
        has_children = false;
    }

    C4_ALWAYS_INLINE void reset_before_pop(ParserState const& to_pop)
    {
        pos = to_pop.pos;
        line_contents = to_pop.line_contents;
    }

public:

    C4_ALWAYS_INLINE bool at_line_beginning() const noexcept
    {
        return line_contents.rem.str == line_contents.full.str;
    }
    C4_ALWAYS_INLINE bool indentation_eq() const noexcept
    {
        RYML_ASSERT(indref != npos);
        return line_contents.indentation != npos && line_contents.indentation == indref;
    }
    C4_ALWAYS_INLINE bool indentation_ge() const noexcept
    {
        RYML_ASSERT(indref != npos);
        return line_contents.indentation != npos && line_contents.indentation >= indref;
    }
    C4_ALWAYS_INLINE bool indentation_gt() const noexcept
    {
        RYML_ASSERT(indref != npos);
        return line_contents.indentation != npos && line_contents.indentation > indref;
    }
    C4_ALWAYS_INLINE bool indentation_lt() const noexcept
    {
        RYML_ASSERT(indref != npos);
        return line_contents.indentation != npos && line_contents.indentation < indref;
    }
};
static_assert(std::is_standard_layout<ParserState>::value, "ParserState not standard");


} // namespace yml
} // namespace c4

// NOLINTEND(hicpp-signed-bitwise)

#endif /* _C4_YML_PARSER_STATE_HPP_ */
