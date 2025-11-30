#ifndef _C4_YML_DETAIL_DBGPRINT_HPP_
#define _C4_YML_DETAIL_DBGPRINT_HPP_

#ifndef _C4_YML_COMMON_HPP_
#include "../common.hpp"
#endif

#ifdef RYML_DBG
#include <cstdio>
#endif


//-----------------------------------------------------------------------------
// debug prints

#ifndef RYML_DBG
#   define _c4dbgt(fmt, ...)
#   define _c4dbgpf(fmt, ...)
#   define _c4dbgpf_(fmt, ...)
#   define _c4dbgp(msg)
#   define _c4dbgp_(msg)
#   define _c4dbgq(msg)
#   define _c4presc(...)
#   define _c4prscalar(msg, scalar, keep_newlines)
#else
#   define _c4dbgt(fmt, ...)   do { if(_dbg_enabled()) {                \
                               this->_dbg ("{}:{}: "   fmt     , __FILE__, __LINE__, __VA_ARGS__); } } while(0)
#   define _c4dbgpf(fmt, ...)  _dbg_printf("{}:{}: "   fmt "\n", __FILE__, __LINE__, __VA_ARGS__)
#   define _c4dbgpf_(fmt, ...) _dbg_printf("{}:{}: "   fmt     , __FILE__, __LINE__, __VA_ARGS__)
#   define _c4dbgp(msg)        _dbg_printf("{}:{}: "   msg "\n", __FILE__, __LINE__             )
#   define _c4dbgp_(msg)       _dbg_printf("{}:{}: "   msg     , __FILE__, __LINE__             )
#   define _c4dbgq(msg)        _dbg_printf(msg "\n")
#   define _c4presc(...)       do { if(_dbg_enabled()) __c4presc(__VA_ARGS__); } while(0)
#   define _c4prscalar(msg, scalar, keep_newlines)                  \
    do {                                                            \
        _c4dbgpf_("{}: [{}]~~~", msg, scalar.len);                  \
        if(_dbg_enabled()) {                                        \
            __c4presc((scalar).str, (scalar).len, (keep_newlines)); \
        }                                                           \
        _c4dbgq("~~~");                                             \
    } while(0)
#endif // RYML_DBG


//-----------------------------------------------------------------------------

#ifdef RYML_DBG

#include <c4/dump.hpp>
namespace c4 {
inline bool& _dbg_enabled() { static bool enabled = true; return enabled; }
inline void _dbg_set_enabled(bool yes) { _dbg_enabled() = yes; }
inline void _dbg_dumper(csubstr s)
{
    if(s.str)
        fwrite(s.str, 1, s.len, stdout);
}
inline substr _dbg_buf() noexcept
{
    static char writebuf[2048];
    return substr{writebuf, sizeof(writebuf)}; // g++-5 has trouble with return writebuf;
}
template<class ...Args>
C4_NO_INLINE void _dbg_printf(c4::csubstr fmt, Args const& ...args)
{
    if(_dbg_enabled())
    {
        substr buf = _dbg_buf();
        const size_t needed_size = c4::format_dump(&_dbg_dumper, buf, fmt, args...);
        C4_CHECK(needed_size <= buf.len);
    }
}
inline C4_NO_INLINE void __c4presc(const char *s, size_t len, bool keep_newlines=false)
{
    RYML_ASSERT(s || !len);
    size_t prev = 0;
    for(size_t i = 0; i < len; ++i)
    {
        switch(s[i])
        {
        case '\n'  : _dbg_printf("{}{}{}", csubstr(s+prev, i-prev), csubstr("\\n"), csubstr(keep_newlines ? "\n":"")); prev = i+1; break;
        case '\t'  : _dbg_printf("{}{}", csubstr(s+prev, i-prev), csubstr("\\t")); prev = i+1; break;
        case '\0'  : _dbg_printf("{}{}", csubstr(s+prev, i-prev), csubstr("\\0")); prev = i+1; break;
        case '\r'  : _dbg_printf("{}{}", csubstr(s+prev, i-prev), csubstr("\\r")); prev = i+1; break;
        case '\f'  : _dbg_printf("{}{}", csubstr(s+prev, i-prev), csubstr("\\f")); prev = i+1; break;
        case '\b'  : _dbg_printf("{}{}", csubstr(s+prev, i-prev), csubstr("\\b")); prev = i+1; break;
        case '\v'  : _dbg_printf("{}{}", csubstr(s+prev, i-prev), csubstr("\\v")); prev = i+1; break;
        case '\a'  : _dbg_printf("{}{}", csubstr(s+prev, i-prev), csubstr("\\a")); prev = i+1; break;
        case '\x1b': _dbg_printf("{}{}", csubstr(s+prev, i-prev), csubstr("\\x1b")); prev = i+1; break;
        case -0x3e/*0xc2u*/:
            if(i+1 < len)
            {
                if(s[i+1] == -0x60/*0xa0u*/)
                {
                    _dbg_printf("{}{}", csubstr(s+prev, i-prev), csubstr("\\_")); prev = i+1;
                }
                else if(s[i+1] == -0x7b/*0x85u*/)
                {
                    _dbg_printf("{}{}", csubstr(s+prev, i-prev), csubstr("\\N")); prev = i+1;
                }
            }
            break;
        case -0x1e/*0xe2u*/:
            if(i+2 < len && s[i+1] == -0x80/*0x80u*/)
            {
                if(s[i+2] == -0x58/*0xa8u*/)
                {
                    _dbg_printf("{}{}", csubstr(s+prev, i-prev), csubstr("\\L")); prev = i+1;
                }
                else if(s[i+2] == -0x57/*0xa9u*/)
                {
                    _dbg_printf("{}{}", csubstr(s+prev, i-prev), csubstr("\\P")); prev = i+1;
                }
            }
            break;
        }
    }
    if(len > prev)
        _dbg_printf("{}", csubstr(s+prev, len-prev));
}
inline void __c4presc(csubstr s, bool keep_newlines=false)
{
    __c4presc(s.str, s.len, keep_newlines);
}
} // namespace c4

#endif // RYML_DBG

#endif /* _C4_YML_DETAIL_DBGPRINT_HPP_ */
