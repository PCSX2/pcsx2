#include "c4/utf.hpp"
#include "c4/charconv.hpp"

namespace c4 {

C4_SUPPRESS_WARNING_GCC_CLANG_WITH_PUSH("-Wold-style-cast")

size_t decode_code_point(uint8_t *C4_RESTRICT buf, size_t buflen, const uint32_t code)
{
    C4_ASSERT(buf);
    C4_ASSERT(buflen >= 4);
    C4_UNUSED(buflen);
    if (code <= UINT32_C(0x7f))
    {
        buf[0] = (uint8_t)code;
        return 1u;
    }
    else if(code <= UINT32_C(0x7ff))
    {
        buf[0] = (uint8_t)(UINT32_C(0xc0) | (code >> 6u));            /* 110xxxxx */
        buf[1] = (uint8_t)(UINT32_C(0x80) | (code & UINT32_C(0x3f))); /* 10xxxxxx */
        return 2u;
    }
    else if(code <= UINT32_C(0xffff))
    {
        buf[0] = (uint8_t)(UINT32_C(0xe0) | ((code >> 12u)));                  /* 1110xxxx */
        buf[1] = (uint8_t)(UINT32_C(0x80) | ((code >>  6u) & UINT32_C(0x3f))); /* 10xxxxxx */
        buf[2] = (uint8_t)(UINT32_C(0x80) | ((code       ) & UINT32_C(0x3f))); /* 10xxxxxx */
        return 3u;
    }
    else if(code <= UINT32_C(0x10ffff))
    {
        buf[0] = (uint8_t)(UINT32_C(0xf0) | ((code >> 18u)));                  /* 11110xxx */
        buf[1] = (uint8_t)(UINT32_C(0x80) | ((code >> 12u) & UINT32_C(0x3f))); /* 10xxxxxx */
        buf[2] = (uint8_t)(UINT32_C(0x80) | ((code >>  6u) & UINT32_C(0x3f))); /* 10xxxxxx */
        buf[3] = (uint8_t)(UINT32_C(0x80) | ((code       ) & UINT32_C(0x3f))); /* 10xxxxxx */
        return 4u;
    }
    return 0;
}

substr decode_code_point(substr out, csubstr code_point)
{
    C4_ASSERT(out.len >= 4);
    C4_ASSERT(!code_point.begins_with("U+"));
    C4_ASSERT(!code_point.begins_with("\\x"));
    C4_ASSERT(!code_point.begins_with("\\u"));
    C4_ASSERT(!code_point.begins_with("\\U"));
    C4_ASSERT(!code_point.begins_with('0'));
    C4_ASSERT(code_point.len <= 8);
    C4_ASSERT(code_point.len > 0);
    uint32_t code_point_val;
    C4_CHECK(read_hex(code_point, &code_point_val));
    size_t ret = decode_code_point((uint8_t*)out.str, out.len, code_point_val);
    C4_ASSERT(ret <= 4);
    return out.first(ret);
}

size_t first_non_bom(csubstr s)
{
    #define c4check2_(s, c0, c1)         ((s).len >= 2) && (((s).str[0] == (c0)) && ((s).str[1] == (c1)))
    #define c4check3_(s, c0, c1, c2)     ((s).len >= 3) && (((s).str[0] == (c0)) && ((s).str[1] == (c1)) && ((s).str[2] == (c2)))
    #define c4check4_(s, c0, c1, c2, c3) ((s).len >= 4) && (((s).str[0] == (c0)) && ((s).str[1] == (c1)) && ((s).str[2] == (c2)) && ((s).str[3] == (c3)))
    // see https://en.wikipedia.org/wiki/Byte_order_mark#Byte-order_marks_by_encoding
    if(s.len < 2u)
        return false;
    else if(c4check3_(s, '\xef', '\xbb', '\xbf')) // UTF-8
        return 3u;
    else if(c4check4_(s, '\x00', '\x00', '\xfe', '\xff')) // UTF-32BE
        return 4u;
    else if(c4check4_(s, '\xff', '\xfe', '\x00', '\x00')) // UTF-32LE
        return 4u;
    else if(c4check2_(s, '\xfe', '\xff')) // UTF-16BE
        return 2u;
    else if(c4check2_(s, '\xff', '\xfe')) // UTF-16BE
        return 2u;
    else if(c4check3_(s, '\x2b', '\x2f', '\x76')) // UTF-7
        return 3u;
    else if(c4check3_(s, '\xf7', '\x64', '\x4c')) // UTF-1
        return 3u;
    else if(c4check4_(s, '\xdd', '\x73', '\x66', '\x73')) // UTF-EBCDIC
        return 4u;
    else if(c4check3_(s, '\x0e', '\xfe', '\xff')) // SCSU
        return 3u;
    else if(c4check3_(s, '\xfb', '\xee', '\x28')) // BOCU-1
        return 3u;
    else if(c4check4_(s, '\x84', '\x31', '\x95', '\x33')) // GB18030
        return 4u;
    return 0u;
    #undef c4check2_
    #undef c4check3_
    #undef c4check4_
}

substr get_bom(substr s)
{
    return s.first(first_non_bom(s));
}
csubstr get_bom(csubstr s)
{
    return s.first(first_non_bom(s));
}
substr skip_bom(substr s)
{
    return s.sub(first_non_bom(s));
}
csubstr skip_bom(csubstr s)
{
    return s.sub(first_non_bom(s));
}

C4_SUPPRESS_WARNING_GCC_CLANG_POP

} // namespace c4
