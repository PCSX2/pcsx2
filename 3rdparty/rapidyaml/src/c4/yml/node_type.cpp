#include "c4/yml/node_type.hpp"

namespace c4 {
namespace yml {

const char* NodeType::type_str(NodeType_e ty) noexcept
{
    switch(ty & _TYMASK)
    {
    case KEYVAL:
        return "KEYVAL";
    case KEY:
        return "KEY";
    case VAL:
        return "VAL";
    case MAP:
        return "MAP";
    case SEQ:
        return "SEQ";
    case KEYMAP:
        return "KEYMAP";
    case KEYSEQ:
        return "KEYSEQ";
    case DOCSEQ:
        return "DOCSEQ";
    case DOCMAP:
        return "DOCMAP";
    case DOCVAL:
        return "DOCVAL";
    case DOC:
        return "DOC";
    case STREAM:
        return "STREAM";
    case NOTYPE:
        return "NOTYPE";
    default:
        if((ty & KEYVAL) == KEYVAL)
            return "KEYVAL***";
        if((ty & KEYMAP) == KEYMAP)
            return "KEYMAP***";
        if((ty & KEYSEQ) == KEYSEQ)
            return "KEYSEQ***";
        if((ty & DOCSEQ) == DOCSEQ)
            return "DOCSEQ***";
        if((ty & DOCMAP) == DOCMAP)
            return "DOCMAP***";
        if((ty & DOCVAL) == DOCVAL)
            return "DOCVAL***";
        if(ty & KEY)
            return "KEY***";
        if(ty & VAL)
            return "VAL***";
        if(ty & MAP)
            return "MAP***";
        if(ty & SEQ)
            return "SEQ***";
        if(ty & DOC)
            return "DOC***";
        return "(unk)";
    }
}

csubstr NodeType::type_str(substr buf, NodeType_e flags) noexcept
{
    size_t pos = 0;
    bool gotone = false;

    #define _prflag(fl, txt)                                    \
    do {                                                        \
        if((flags & (fl)) == (fl))                              \
        {                                                       \
            if(gotone)                                          \
            {                                                   \
                if(pos + 1 < buf.len)                           \
                    buf[pos] = '|';                             \
                ++pos;                                          \
            }                                                   \
            csubstr fltxt = txt;                                \
            if(pos + fltxt.len <= buf.len)                      \
                memcpy(buf.str + pos, fltxt.str, fltxt.len);    \
            pos += fltxt.len;                                   \
            gotone = true;                                      \
            flags = (flags & ~(fl)); /*remove the flag*/        \
        }                                                       \
    } while(0)

    _prflag(STREAM, "STREAM");
    _prflag(DOC, "DOC");
    // key properties
    _prflag(KEY, "KEY");
    _prflag(KEYNIL, "KNIL");
    _prflag(KEYTAG, "KTAG");
    _prflag(KEYANCH, "KANCH");
    _prflag(KEYREF, "KREF");
    _prflag(KEY_LITERAL, "KLITERAL");
    _prflag(KEY_FOLDED, "KFOLDED");
    _prflag(KEY_SQUO, "KSQUO");
    _prflag(KEY_DQUO, "KDQUO");
    _prflag(KEY_PLAIN, "KPLAIN");
    _prflag(KEY_UNFILT, "KUNFILT");
    // val properties
    _prflag(VAL, "VAL");
    _prflag(VALNIL, "VNIL");
    _prflag(VALTAG, "VTAG");
    _prflag(VALANCH, "VANCH");
    _prflag(VALREF, "VREF");
    _prflag(VAL_UNFILT, "VUNFILT");
    _prflag(VAL_LITERAL, "VLITERAL");
    _prflag(VAL_FOLDED, "VFOLDED");
    _prflag(VAL_SQUO, "VSQUO");
    _prflag(VAL_DQUO, "VDQUO");
    _prflag(VAL_PLAIN, "VPLAIN");
    _prflag(VAL_UNFILT, "VUNFILT");
    // container properties
    _prflag(MAP, "MAP");
    _prflag(SEQ, "SEQ");
    _prflag(FLOW_SL, "FLOWSL");
    _prflag(FLOW_ML, "FLOWML");
    _prflag(BLOCK, "BLCK");
    if(pos == 0)
        _prflag(NOTYPE, "NOTYPE");

    #undef _prflag

    if(pos < buf.len)
    {
        buf[pos] = '\0';
        return buf.first(pos);
    }
    else
    {
        csubstr failed;
        failed.len = pos + 1;
        failed.str = nullptr;
        return failed;
    }
}


//-----------------------------------------------------------------------------

// see https://www.yaml.info/learn/quote.html#noplain
bool scalar_style_query_squo(csubstr s) noexcept
{
    return ! s.first_of_any("\n ", "\n\t");
}

// see https://www.yaml.info/learn/quote.html#noplain
bool scalar_style_query_plain(csubstr s) noexcept
{
    if(s.begins_with("-."))
    {
        if(s == "-.inf" || s == "-.INF")
            return true;
        else if(s.sub(2).is_number())
            return true;
    }
    else if(s.begins_with_any("0123456789.-+") && s.is_number())
    {
        return true;
    }
    return s != ':'
        && ( ! s.begins_with_any("-:?*&,'\"{}[]|>%#@`\r")) // @ and ` are reserved characters
        && ( ! s.ends_with_any(":#"))
             // make this check in the last place, as it has linear
             // complexity, while the previous ones are
             // constant-time
        && (s.first_of("\n#:[]{},") == npos);
}

NodeType_e scalar_style_choose(csubstr s) noexcept
{
    if(s.len)
    {
        if(s.begins_with_any(" \n\t")
           ||
           s.ends_with_any(" \n\t"))
        {
            return SCALAR_DQUO;
        }
        else if( ! scalar_style_query_plain(s))
        {
            return scalar_style_query_squo(s) ? SCALAR_SQUO : SCALAR_DQUO;
        }
        // nothing remarkable - use plain
        return SCALAR_PLAIN;
    }
    return s.str ? SCALAR_SQUO : SCALAR_PLAIN;
}

NodeType_e scalar_style_json_choose(csubstr s) noexcept
{
    // do not quote special cases
    bool plain = (
        (s == "true" || s == "false" || s == "null")
        ||
        (
            // do not quote numbers
            s.is_number()
            &&
            (
                // quote integral numbers if they have a leading 0
                // https://github.com/biojppm/rapidyaml/issues/291
                (!(s.len > 1 && s.begins_with('0')))
                // do not quote reals with leading 0
                // https://github.com/biojppm/rapidyaml/issues/313
                || (s.find('.') != csubstr::npos)
            )
        )
    );
    return plain ? SCALAR_PLAIN : SCALAR_DQUO;
}

} // namespace yml
} // namespace c4
