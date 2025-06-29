#include "c4/yml/common.hpp"

#ifndef RYML_NO_DEFAULT_CALLBACKS
#   include <stdlib.h>
#   include <stdio.h>
#   ifdef RYML_DEFAULT_CALLBACK_USES_EXCEPTIONS
#       include <stdexcept>
#   endif
#endif // RYML_NO_DEFAULT_CALLBACKS


namespace c4 {
namespace yml {

C4_SUPPRESS_WARNING_GCC_CLANG_WITH_PUSH("-Wold-style-cast")
C4_SUPPRESS_WARNING_MSVC_WITH_PUSH(4702/*unreachable code*/) // on the call to the unreachable macro

namespace {
Callbacks s_default_callbacks;
} // anon namespace

#ifndef RYML_NO_DEFAULT_CALLBACKS
void report_error_impl(const char* msg, size_t length, Location loc, FILE *f)
{
    if(!f)
        f = stderr;
    if(loc)
    {
        if(!loc.name.empty())
        {
            // this is more portable than using fprintf("%.*s:") which
            // is not available in some embedded platforms
            fwrite(loc.name.str, 1, loc.name.len, f); // NOLINT
            fputc(':', f); // NOLINT
        }
        fprintf(f, "%zu:", loc.line); // NOLINT
        if(loc.col)
            fprintf(f, "%zu:", loc.col); // NOLINT
        if(loc.offset)
            fprintf(f, " (%zuB):", loc.offset); // NOLINT
        fputc(' ', f); // NOLINT
    }
    RYML_ASSERT(!csubstr(msg, length).ends_with('\0'));
    fwrite(msg, 1, length, f); // NOLINT
    fputc('\n', f); // NOLINT
    fflush(f); // NOLINT
}

[[noreturn]] void error_impl(const char* msg, size_t length, Location loc, void * /*user_data*/)
{
    RYML_ASSERT(!csubstr(msg, length).ends_with('\0'));
    report_error_impl(msg, length, loc, nullptr);
#ifdef RYML_DEFAULT_CALLBACK_USES_EXCEPTIONS
    throw std::runtime_error(std::string(msg, length));
#else
    ::abort();
#endif
}

void* allocate_impl(size_t length, void * /*hint*/, void * /*user_data*/)
{
    void *mem = ::malloc(length);
    if(mem == nullptr)
    {
        const char msg[] = "could not allocate memory";
        error_impl(msg, sizeof(msg)-1, {}, nullptr);
    }
    return mem;
}

void free_impl(void *mem, size_t /*length*/, void * /*user_data*/)
{
    ::free(mem);
}
#endif // RYML_NO_DEFAULT_CALLBACKS



Callbacks::Callbacks() noexcept
    :
    m_user_data(nullptr),
    #ifndef RYML_NO_DEFAULT_CALLBACKS
    m_allocate(allocate_impl),
    m_free(free_impl),
    m_error(error_impl)
    #else
    m_allocate(nullptr),
    m_free(nullptr),
    m_error(nullptr)
    #endif
{
}

Callbacks::Callbacks(void *user_data, pfn_allocate alloc_, pfn_free free_, pfn_error error_)
    :
    m_user_data(user_data),
    #ifndef RYML_NO_DEFAULT_CALLBACKS
    m_allocate(alloc_ ? alloc_ : allocate_impl),
    m_free(free_ ? free_ : free_impl),
    m_error((error_ ? error_ : error_impl))
    #else
    m_allocate(alloc_),
    m_free(free_),
    m_error(error_)
    #endif
{
    RYML_CHECK(m_allocate);
    RYML_CHECK(m_free);
    RYML_CHECK(m_error);
}


void set_callbacks(Callbacks const& c)
{
    s_default_callbacks = c;
}

Callbacks const& get_callbacks()
{
    return s_default_callbacks;
}

void reset_callbacks()
{
    set_callbacks(Callbacks());
}

// the [[noreturn]] attribute needs to be here as well (UB otherwise)
// https://en.cppreference.com/w/cpp/language/attributes/noreturn
[[noreturn]] void error(Callbacks const& cb, const char *msg, size_t msg_len, Location loc)
{
    cb.m_error(msg, msg_len, loc, cb.m_user_data);
    abort(); // call abort in case the error callback didn't interrupt execution
    C4_UNREACHABLE();
}

// the [[noreturn]] attribute needs to be here as well (UB otherwise)
// see https://en.cppreference.com/w/cpp/language/attributes/noreturn
[[noreturn]] void error(const char *msg, size_t msg_len, Location loc)
{
    error(s_default_callbacks, msg, msg_len, loc);
    C4_UNREACHABLE();
}

C4_SUPPRESS_WARNING_MSVC_POP
C4_SUPPRESS_WARNING_GCC_CLANG_POP

} // namespace yml
} // namespace c4
