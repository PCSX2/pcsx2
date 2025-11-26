#include "c4/yml/tag.hpp"
#include "c4/yml/detail/dbgprint.hpp"


namespace c4 {
namespace yml {

bool is_custom_tag(csubstr tag)
{
    if((tag.len > 2) && (tag.str[0] == '!'))
    {
        size_t pos = tag.find('!', 1);
        return pos != npos && pos > 1 && tag.str[1] != '<';
    }
    return false;
}

csubstr normalize_tag(csubstr tag)
{
    YamlTag_e t = to_tag(tag);
    if(t != TAG_NONE)
        return from_tag(t);
    if(tag.begins_with("!<"))
        tag = tag.sub(1);
    if(tag.begins_with("<!"))
        return tag;
    return tag;
}

csubstr normalize_tag_long(csubstr tag)
{
    YamlTag_e t = to_tag(tag);
    if(t != TAG_NONE)
        return from_tag_long(t);
    if(tag.begins_with("!<"))
        tag = tag.sub(1);
    if(tag.begins_with("<!"))
        return tag;
    return tag;
}

csubstr normalize_tag_long(csubstr tag, substr output)
{
    csubstr result = normalize_tag_long(tag);
    if(result.begins_with("!!"))
    {
        tag = tag.sub(2);
        const csubstr pfx = "<tag:yaml.org,2002:";
        const size_t len = pfx.len + tag.len + 1;
        if(len <= output.len)
        {
            memcpy(output.str          , pfx.str, pfx.len);
            memcpy(output.str + pfx.len, tag.str, tag.len);
            output[pfx.len + tag.len] = '>';
            result = output.first(len);
        }
        else
        {
            result.str = nullptr;
            result.len = len;
        }
    }
    return result;
}

YamlTag_e to_tag(csubstr tag)
{
    if(tag.begins_with("!<"))
        tag = tag.sub(1);
    if(tag.begins_with("!!"))
        tag = tag.sub(2);
    else if(tag.begins_with('!'))
        return TAG_NONE;
    else if(tag.begins_with("tag:yaml.org,2002:"))
    {
        RYML_ASSERT(csubstr("tag:yaml.org,2002:").len == 18);
        tag = tag.sub(18);
    }
    else if(tag.begins_with("<tag:yaml.org,2002:"))
    {
        RYML_ASSERT(csubstr("<tag:yaml.org,2002:").len == 19);
        tag = tag.sub(19);
        if(!tag.len)
            return TAG_NONE;
        tag = tag.offs(0, 1);
    }

    if(tag == "map")
        return TAG_MAP;
    else if(tag == "omap")
        return TAG_OMAP;
    else if(tag == "pairs")
        return TAG_PAIRS;
    else if(tag == "set")
        return TAG_SET;
    else if(tag == "seq")
        return TAG_SEQ;
    else if(tag == "binary")
        return TAG_BINARY;
    else if(tag == "bool")
        return TAG_BOOL;
    else if(tag == "float")
        return TAG_FLOAT;
    else if(tag == "int")
        return TAG_INT;
    else if(tag == "merge")
        return TAG_MERGE;
    else if(tag == "null")
        return TAG_NULL;
    else if(tag == "str")
        return TAG_STR;
    else if(tag == "timestamp")
        return TAG_TIMESTAMP;
    else if(tag == "value")
        return TAG_VALUE;
    else if(tag == "yaml")
        return TAG_YAML;

    return TAG_NONE;
}

csubstr from_tag_long(YamlTag_e tag)
{
    switch(tag)
    {
    case TAG_MAP:
        return {"<tag:yaml.org,2002:map>"};
    case TAG_OMAP:
        return {"<tag:yaml.org,2002:omap>"};
    case TAG_PAIRS:
        return {"<tag:yaml.org,2002:pairs>"};
    case TAG_SET:
        return {"<tag:yaml.org,2002:set>"};
    case TAG_SEQ:
        return {"<tag:yaml.org,2002:seq>"};
    case TAG_BINARY:
        return {"<tag:yaml.org,2002:binary>"};
    case TAG_BOOL:
        return {"<tag:yaml.org,2002:bool>"};
    case TAG_FLOAT:
        return {"<tag:yaml.org,2002:float>"};
    case TAG_INT:
        return {"<tag:yaml.org,2002:int>"};
    case TAG_MERGE:
        return {"<tag:yaml.org,2002:merge>"};
    case TAG_NULL:
        return {"<tag:yaml.org,2002:null>"};
    case TAG_STR:
        return {"<tag:yaml.org,2002:str>"};
    case TAG_TIMESTAMP:
        return {"<tag:yaml.org,2002:timestamp>"};
    case TAG_VALUE:
        return {"<tag:yaml.org,2002:value>"};
    case TAG_YAML:
        return {"<tag:yaml.org,2002:yaml>"};
    case TAG_NONE:
    default:
        return {""};
    }
}

csubstr from_tag(YamlTag_e tag)
{
    switch(tag)
    {
    case TAG_MAP:
        return {"!!map"};
    case TAG_OMAP:
        return {"!!omap"};
    case TAG_PAIRS:
        return {"!!pairs"};
    case TAG_SET:
        return {"!!set"};
    case TAG_SEQ:
        return {"!!seq"};
    case TAG_BINARY:
        return {"!!binary"};
    case TAG_BOOL:
        return {"!!bool"};
    case TAG_FLOAT:
        return {"!!float"};
    case TAG_INT:
        return {"!!int"};
    case TAG_MERGE:
        return {"!!merge"};
    case TAG_NULL:
        return {"!!null"};
    case TAG_STR:
        return {"!!str"};
    case TAG_TIMESTAMP:
        return {"!!timestamp"};
    case TAG_VALUE:
        return {"!!value"};
    case TAG_YAML:
        return {"!!yaml"};
    case TAG_NONE:
    default:
        return {""};
    }
}


bool TagDirective::create_from_str(csubstr directive_)
{
    csubstr directive = directive_;
    directive = directive.sub(4);
    if(!directive.begins_with(' '))
        return false;
    directive = directive.triml(' ');
    size_t pos = directive.find(' ');
    if(pos == npos)
        return false;
    handle = directive.first(pos);
    directive = directive.sub(handle.len).triml(' ');
    pos = directive.find(' ');
    if(pos != npos)
        directive = directive.first(pos);
    prefix = directive;
    next_node_id = NONE;
    _c4dbgpf("%TAG: handle={} prefix={}", handle, prefix);
    return true;
}

size_t TagDirective::transform(csubstr tag, substr output, Callbacks const& callbacks, bool with_brackets) const
{
    _c4dbgpf("%TAG: handle={} prefix={} next_node={}. tag={}", handle, prefix, next_node_id, tag);
    _RYML_CB_ASSERT(callbacks, tag.len >= handle.len);
    csubstr rest = tag.sub(handle.len);
    _c4dbgpf("%TAG: rest={}", rest);
    if(rest.begins_with('<'))
    {
        _c4dbgpf("%TAG: begins with <. rest={}", rest);
        if(C4_UNLIKELY(!rest.ends_with('>')))
            _RYML_CB_ERR(callbacks, "malformed tag");
        rest = rest.offs(1, 1);
        if(rest.begins_with(prefix))
        {
            _c4dbgpf("%TAG: already transformed! actual={}", rest.sub(prefix.len));
            return 0; // return 0 to signal that the tag is local and cannot be resolved
        }
    }
    size_t len = prefix.len + rest.len;
    if(with_brackets)
        len += 2;
    size_t numpc = rest.count('%');
    if(numpc == 0)
    {
        if(len <= output.len)
        {
            if(with_brackets)
            {
                output.str[0] = '<';
                memcpy(1u + output.str, prefix.str, prefix.len);
                memcpy(1u + output.str + prefix.len, rest.str, rest.len);
                output.str[1u + prefix.len + rest.len] = '>';
            }
            else
            {
                memcpy(output.str, prefix.str, prefix.len);
                memcpy(output.str + prefix.len, rest.str, rest.len);
            }
        }
    }
    else
    {
        // need to decode URI % sequences
        size_t pos = rest.find('%');
        _RYML_CB_ASSERT(callbacks, pos != npos);
        do {
            size_t next = rest.first_not_of("0123456789abcdefABCDEF", pos+1);
            if(next == npos)
                next = rest.len;
            _RYML_CB_CHECK(callbacks, pos+1 < next);
            _RYML_CB_CHECK(callbacks, pos+1 + 2 <= next);
            size_t delta = next - (pos+1);
            len -= delta;
            pos = rest.find('%', pos+1);
        } while(pos != npos);
        if(len <= output.len)
        {
            size_t prev = 0, wpos = 0;
            auto appendstr = [&](csubstr s) { memcpy(output.str + wpos, s.str, s.len); wpos += s.len; };
            auto appendchar = [&](char c) { output.str[wpos++] = c; };
            if(with_brackets)
                appendchar('<');
            appendstr(prefix);
            pos = rest.find('%');
            _RYML_CB_ASSERT(callbacks, pos != npos);
            do {
                size_t next = rest.first_not_of("0123456789abcdefABCDEF", pos+1);
                if(next == npos)
                    next = rest.len;
                _RYML_CB_CHECK(callbacks, pos+1 < next);
                _RYML_CB_CHECK(callbacks, pos+1 + 2 <= next);
                uint8_t val;
                if(C4_UNLIKELY(!read_hex(rest.range(pos+1, next), &val) || val > 127))
                    _RYML_CB_ERR(callbacks, "invalid URI character");
                appendstr(rest.range(prev, pos));
                appendchar(static_cast<char>(val));
                prev = next;
                pos = rest.find('%', pos+1);
            } while(pos != npos);
            _RYML_CB_ASSERT(callbacks, pos == npos);
            _RYML_CB_ASSERT(callbacks, prev > 0);
            _RYML_CB_ASSERT(callbacks, rest.len >= prev);
            appendstr(rest.sub(prev));
            if(with_brackets)
                appendchar('>');
            _RYML_CB_ASSERT(callbacks, wpos == len);
        }
    }
    return len;
}

} // namespace yml
} // namespace c4
