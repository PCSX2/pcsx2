#ifndef C4_DUMP_HPP_
#define C4_DUMP_HPP_

#include <c4/substr.hpp>

/** @file dump.hpp This file provides functions to dump several
 * arguments as strings to a user-provided function sink, for example
 * to implement a type-safe printf()-like function (where the sink
 * would just be a plain call to putchars()). The function sink can be
 * passed either by dynamic dispatching or by static dispatching (as a
 * template argument). There are analogs to @ref c4::cat() (@ref
 * c4::cat_dump() and @ref c4::cat_dump_resume()), @ref c4::catsep()
 * (@ref catsetp_dump() and @ref catsep_dump_resume()) and @ref
 * c4::format() (@ref c4::format_dump() and @ref
 * c4::format_dump_resume()). The analogs have two types: immediate
 * and resuming. An analog of immediate type cannot be retried when
 * the work buffer is too small; this means that successful dumps in
 * the first (successful) arguments will be dumped again in the
 * subsequent attempt to call. An analog of resuming type will only
 * ever dump as-yet-undumped arguments, through the use of @ref
 * DumpResults return type. */

namespace c4 {

C4_SUPPRESS_WARNING_GCC_CLANG_WITH_PUSH("-Wold-style-cast")


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/** @defgroup dump_building_blocks Basic building blocks for dumping.
 *
 * The basic building block: given an argument and a
 * buffer, serialize the argument to the buffer using @ref
 * c4::to_chars(), and dump the buffer to the provided sink
 * function. When the argument is a string, no serialization is
 * performed, and the argument is dumped directly to the sink.
 *
 * @{ */


/** Type of the function to be used as the sink. This function
 * receives as its argument the string with characters to send to the
 * sink.
 *
 * @warning the string passed to the sink may have zero length. If the
 * user sink uses memcpy(), the call to memcpy() should be defended
 * with a check for zero length (calling memcpy with zero length is
 * undefined behavior).
 * */
using SinkPfn = void (*)(csubstr str);


/** a traits class to use in SFINAE with @ref c4::dump() to select if
 * a type is treated as string type (which is dumped directly to the
 * sink, using to_csubstr()), or if the type is treated as a value,
 * which is first serialized to a buffer using to_chars(), and then
 * the serialization serialized as */
template<class T> struct dump_directly : public std::false_type {};
template<> struct dump_directly<csubstr> : public std::true_type {};
template<> struct dump_directly< substr> : public std::true_type {};
template<> struct dump_directly<const char*> : public std::true_type {};
template<> struct dump_directly<      char*> : public std::true_type {};
template<size_t N> struct dump_directly<const char (&)[N]> : public std::true_type {};
template<size_t N> struct dump_directly<      char (&)[N]> : public std::true_type {};
template<size_t N> struct dump_directly<const char[N]> : public std::true_type {};
template<size_t N> struct dump_directly<      char[N]> : public std::true_type {};


/** Dump a string-type object to the (statically dispatched) sink. The
 * string is dumped directly, without any intermediate serialization.
 *
 * @return the number of bytes needed to serialize the string-type
 * object, which is always 0 because there is no serialization
 *
 * @note the argument is considered a value when @ref
 * dump_directly<Arg> is a false type, which is the default. To enable
 * the argument to be treated as a string type, which is dumped
 * directly to the sink without intermediate serialization, define
 * dump_directly<T> to a true type.
 *
 * @warning the string passed to the sink may have zero length. If the
 * user sink uses memcpy(), the call to memcpy() should be defended
 * with a check for zero length (calling memcpy with zero length is
 * undefined behavior).
 *
 * @see dump_directly<T>
 */
template<SinkPfn sinkfn, class Arg>
inline auto dump(substr buf, Arg const& a)
    -> typename std::enable_if<dump_directly<Arg>::value, size_t>::type
{
    C4_ASSERT(!buf.overlaps(a));
    C4_UNUSED(buf);
    // dump directly, no need to serialize to the buffer
    sinkfn(to_csubstr(a));
    return 0; // no space was used in the buffer
}
/** Dump a string-type object to the (dynamically dispatched)
 * sink. The string is dumped directly, without any intermediate
 * serialization to the buffer.
 *
 * @return the number of bytes needed to serialize the string-type
 * object, which is always 0 because there is no serialization
 *
 * @note the argument is considered a value when @ref
 * dump_directly<Arg> is a false type, which is the default. To enable
 * the argument to be treated as a string type, which is dumped
 * directly to the sink without intermediate serialization, define
 * dump_directly<T> to a true type.
 *
 * @warning the string passed to the sink may have zero length. If the
 * user sink uses memcpy(), the call to memcpy() should be defended
 * with a check for zero length (calling memcpy with zero length is
 * undefined behavior).
 *
 * @see dump_directly<T>
 * */
template<class SinkFn, class Arg>
inline auto dump(SinkFn &&sinkfn, substr buf, Arg const& a)
    -> typename std::enable_if<dump_directly<Arg>::value, size_t>::type
{
    C4_UNUSED(buf);
    C4_ASSERT(!buf.overlaps(a));
    // dump directly, no need to serialize to the buffer
    std::forward<SinkFn>(sinkfn)(to_csubstr(a));
    return 0; // no space was used in the buffer
}


/** Dump a value to the sink. Given an argument @p a and a buffer @p
 * buf, serialize the argument to the buffer using @ref to_chars(),
 * and then dump the buffer to the (statically dispatched) sink
 * function passed as the template argument. If the buffer is too
 * small to serialize the argument, the sink function is not called.
 *
 * @note the argument is considered a value when @ref
 * dump_directly<Arg> is a false type, which is the default. To enable
 * the argument to be treated as a string type, which is dumped
 * directly to the sink without intermediate serialization, define
 * dump_directly<T> to a true type.
 *
 * @see dump_directly<T>
 *
 * @return the number of characters required to serialize the
 * argument. */
template<SinkPfn sinkfn, class Arg>
inline auto dump(substr buf, Arg const& a)
    -> typename std::enable_if<!dump_directly<Arg>::value, size_t>::type
{
    // serialize to the buffer
    const size_t sz = to_chars(buf, a);
    // dump the buffer to the sink
    if(C4_LIKELY(sz <= buf.len))
    {
        // NOTE: don't do this:
        //sinkfn(buf.first(sz));
        // ... but do this instead:
        sinkfn({buf.str, sz});
        // ... this is needed because Release builds for armv5 and
        // armv6 were failing for the first call, with the wrong
        // buffer being passed into the function (!)
    }
    return sz;
}
/** Dump a value to the sink. Given an argument @p a and a buffer @p
 * buf, serialize the argument to the buffer using @ref
 * c4::to_chars(), and then dump the buffer to the (dynamically
 * dispatched) sink function, passed as @p sinkfn. If the buffer is too
 * small to serialize the argument, the sink function is not called.
 *
 * @note the argument is considered a value when @ref
 * dump_directly<Arg> is a false type, which is the default. To enable
 * the argument to be treated as a string type, which is dumped
 * directly to the sink without intermediate serialization, define
 * dump_directly<T> to a true type.
 *
 * @see @ref dump_directly<T>
 *
 * @return the number of characters required to serialize the
 * argument. */
template<class SinkFn, class Arg>
inline auto dump(SinkFn &&sinkfn, substr buf, Arg const& a)
    -> typename std::enable_if<!dump_directly<Arg>::value, size_t>::type
{
    // serialize to the buffer
    const size_t sz = to_chars(buf, a);
    // dump the buffer to the sink
    if(C4_LIKELY(sz <= buf.len))
    {
        // NOTE: don't do this:
        //std::forward<SinkFn>(sinkfn)(buf.first(sz));
        // ... but do this instead:
        std::forward<SinkFn>(sinkfn)({buf.str, sz});
        // ... this is needed because Release builds for armv5 and
        // armv6 were failing for the first call, with the wrong
        // buffer being passed into the function (!)
    }
    return sz;
}


/** An opaque type used by resumeable dump functions like @ref
 * cat_dump_resume(), @ref catsep_dump_resume() or @ref
 * format_dump_resume(). */
struct DumpResults
{
    enum : size_t { noarg = (size_t)-1 };
    size_t bufsize = 0;
    size_t lastok = noarg;
    bool success_until(size_t expected) const { return lastok == noarg ? false : lastok >= expected; }
    bool write_arg(size_t arg) const { return lastok == noarg || arg > lastok; }
    size_t argfail() const { return lastok + 1; }
};

/** @} */


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


/** @defgroup cat_dump Dump several arguments to a sink,
 * concatenated. This is the analog to @ref c4::cat(), with the
 * significant difference that each argument is immediately sent to
 * the sink (resulting in multiple calls to the sink function, once
 * per argument), whereas equivalent usage of c4::cat() would first
 * serialize all the arguments to the buffer, and then call the sink
 * once at the end. As a consequence, the size needed for the buffer
 * is only the maximum of the size needed for the arguments, whereas
 * with c4::cat(), the size needed for the buffer would be the sum of
 * the size needed for the arguments. When the size of dump
 *
 * @{ */

/// @cond dev
// terminates the variadic recursion
template<class SinkFn>
size_t cat_dump(SinkFn &&, substr) // NOLINT
{
    return 0;
}

// terminates the variadic recursion
template<SinkPfn sinkfn>
size_t cat_dump(substr) // NOLINT
{
    return 0;
}
/// @endcond


/** Dump several arguments to the (dynamically dispatched) sink
 * function, as if through c4::cat(). For each argument, @ref dump()
 * is called with the buffer and sink. If any of the arguments is too
 * large for the buffer, no subsequent argument is sent to the sink,
 * (but all the arguments are still processed to compute the size
 * required for the buffer). This function can be safely called with an
 * empty buffer.
 *
 * @return the size required for the buffer, which is the maximum size
 * across all arguments
 *
 * @note subsequent calls with the same set of arguments will dump
 * again the first successful arguments. If each argument must only be
 * sent once to the sink (for example with printf-like behavior), use
 * instead @ref cat_dump_resume(). */
template<class SinkFn, class Arg, class... Args>
size_t cat_dump(SinkFn &&sinkfn, substr buf, Arg const& a, Args const& ...more)
{
    const size_t size_for_a = dump(std::forward<SinkFn>(sinkfn), buf, a);
    if(C4_UNLIKELY(size_for_a > buf.len))
        buf.len = 0; // ensure no more calls to the sink
    const size_t size_for_more = cat_dump(std::forward<SinkFn>(sinkfn), buf, more...);
    return size_for_more > size_for_a ? size_for_more : size_for_a;
}


/** Dump several arguments to the (statically dispatched) sink
 * function, as if through c4::cat(). For each argument, @ref dump()
 * is called with the buffer and sink. If any of the arguments is too
 * large for the buffer, no subsequent argument is sent to the sink,
 * (but all the arguments are still processed to compute the size
 * required for the buffer). This function can be safely called with an
 * empty buffer.
 *
 * @return the size required for the buffer, which is the maximum size
 * across all arguments
 *
 * @note subsequent calls with the same set of arguments will dump
 * again the first successful arguments. If each argument must only be
 * sent once to the sink (for example with printf-like behavior), use
 * instead @ref cat_dump_resume(). */
template<SinkPfn sinkfn, class Arg, class... Args>
size_t cat_dump(substr buf, Arg const& a, Args const& ...more)
{
    const size_t size_for_a = dump<sinkfn>(buf, a);
    if(C4_UNLIKELY(size_for_a > buf.len))
        buf.len = 0; // ensure no more calls to the sink
    const size_t size_for_more = cat_dump<sinkfn>(buf, more...);
    return size_for_more > size_for_a ? size_for_more : size_for_a;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/// @cond dev
namespace detail {

// terminates the variadic recursion
template<SinkPfn sinkfn>
C4_ALWAYS_INLINE DumpResults cat_dump_resume(size_t, DumpResults results, substr)
{
    return results;
}

// terminates the variadic recursion
template<class SinkFn>
C4_ALWAYS_INLINE DumpResults cat_dump_resume(size_t, SinkFn &&, DumpResults results, substr) // NOLINT
{
    return results;
}

template<SinkPfn sinkfn, class Arg, class... Args>
DumpResults cat_dump_resume(size_t currarg, DumpResults results, substr buf, Arg const& C4_RESTRICT a, Args const& ...more)
{
    if(C4_LIKELY(results.write_arg(currarg)))
    {
        size_t sz = dump<sinkfn>(buf, a);  // yield to the specialized function
        if(currarg == results.lastok + 1 && sz <= buf.len)
            results.lastok = currarg;
        results.bufsize = sz > results.bufsize ? sz : results.bufsize;
    }
    return detail::cat_dump_resume<sinkfn>(currarg + 1u, results, buf, more...);
}

template<class SinkFn, class Arg, class... Args>
DumpResults cat_dump_resume(size_t currarg, SinkFn &&sinkfn, DumpResults results, substr buf, Arg const& C4_RESTRICT a, Args const& ...more)
{
    if(C4_LIKELY(results.write_arg(currarg)))
    {
        size_t sz = dump(std::forward<SinkFn>(sinkfn), buf, a);  // yield to the specialized function
        if(currarg == results.lastok + 1 && sz <= buf.len)
            results.lastok = currarg;
        results.bufsize = sz > results.bufsize ? sz : results.bufsize;
    }
    return detail::cat_dump_resume(currarg + 1u, std::forward<SinkFn>(sinkfn), results, buf, more...);
}
} // namespace detail
/// @endcond


template<SinkPfn sinkfn, class Arg, class... Args>
C4_ALWAYS_INLINE DumpResults cat_dump_resume(substr buf, Arg const& C4_RESTRICT a, Args const& ...more)
{
    return detail::cat_dump_resume<sinkfn>(0u, DumpResults{}, buf, a, more...);
}

template<class SinkFn, class Arg, class... Args>
C4_ALWAYS_INLINE DumpResults cat_dump_resume(SinkFn &&sinkfn, substr buf, Arg const& C4_RESTRICT a, Args const& ...more)
{
    return detail::cat_dump_resume(0u, std::forward<SinkFn>(sinkfn), DumpResults{}, buf, a, more...);
}


template<SinkPfn sinkfn, class Arg, class... Args>
C4_ALWAYS_INLINE DumpResults cat_dump_resume(DumpResults results, substr buf, Arg const& C4_RESTRICT a, Args const& ...more)
{
    if(results.bufsize > buf.len)
        return results;
    return detail::cat_dump_resume<sinkfn>(0u, results, buf, a, more...);
}

template<class SinkFn, class Arg, class... Args>
C4_ALWAYS_INLINE DumpResults cat_dump_resume(SinkFn &&sinkfn, DumpResults results, substr buf, Arg const& C4_RESTRICT a, Args const& ...more)
{
    if(results.bufsize > buf.len)
        return results;
    return detail::cat_dump_resume(0u, std::forward<SinkFn>(sinkfn), results, buf, a, more...);
}

/** @} */


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/// @cond dev
// terminate the recursion
template<class SinkFn, class Sep>
size_t catsep_dump(SinkFn &&, substr, Sep const& C4_RESTRICT) // NOLINT
{
    return 0;
}

// terminate the recursion
template<SinkPfn sinkfn, class Sep>
size_t catsep_dump(substr, Sep const& C4_RESTRICT) // NOLINT
{
    return 0;
}
/// @endcond

/** take the function pointer as a function argument */
template<class SinkFn, class Sep, class Arg, class... Args>
size_t catsep_dump(SinkFn &&sinkfn, substr buf, Sep const& sep, Arg const& a, Args const& ...more)
{
    size_t sz = dump(std::forward<SinkFn>(sinkfn), buf, a);
    if(C4_UNLIKELY(sz > buf.len))
        buf.len = 0; // ensure no more calls
    if C4_IF_CONSTEXPR (sizeof...(more) > 0)
    {
        size_t szsep = dump(std::forward<SinkFn>(sinkfn), buf, sep);
        if(C4_UNLIKELY(szsep > buf.len))
            buf.len = 0; // ensure no more calls
        sz = sz > szsep ? sz : szsep;
    }
    size_t size_for_more = catsep_dump(std::forward<SinkFn>(sinkfn), buf, sep, more...);
    return size_for_more > sz ? size_for_more : sz;
}

/** take the function pointer as a template argument */
template<SinkPfn sinkfn, class Sep, class Arg, class... Args>
size_t catsep_dump(substr buf, Sep const& sep, Arg const& a, Args const& ...more)
{
    size_t sz = dump<sinkfn>(buf, a);
    if(C4_UNLIKELY(sz > buf.len))
        buf.len = 0; // ensure no more calls
    if C4_IF_CONSTEXPR (sizeof...(more) > 0)
    {
        size_t szsep = dump<sinkfn>(buf, sep);
        if(C4_UNLIKELY(szsep > buf.len))
            buf.len = 0; // ensure no more calls
        sz = sz > szsep ? sz : szsep;
    }
    size_t size_for_more = catsep_dump<sinkfn>(buf, sep, more...);
    return size_for_more > sz ? size_for_more : sz;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/// @cond dev
namespace detail {
template<SinkPfn sinkfn, class Arg>
void catsep_dump_resume_(size_t currarg, DumpResults *C4_RESTRICT results, substr *buf, Arg const& a)
{
    if(C4_LIKELY(results->write_arg(currarg)))
    {
        size_t sz = dump<sinkfn>(*buf, a);
        results->bufsize = sz > results->bufsize ? sz : results->bufsize;
        if(C4_LIKELY(sz <= buf->len))
            results->lastok = currarg;
        else
            buf->len = 0;
    }
}

template<class SinkFn, class Arg>
void catsep_dump_resume_(size_t currarg, SinkFn &&sinkfn, DumpResults *C4_RESTRICT results, substr *C4_RESTRICT buf, Arg const& C4_RESTRICT a)
{
    if(C4_LIKELY(results->write_arg(currarg)))
    {
        size_t sz = dump(std::forward<SinkFn>(sinkfn), *buf, a);
        results->bufsize = sz > results->bufsize ? sz : results->bufsize;
        if(C4_LIKELY(sz <= buf->len))
            results->lastok = currarg;
        else
            buf->len = 0;
    }
}

template<SinkPfn sinkfn, class Sep, class Arg>
C4_ALWAYS_INLINE void catsep_dump_resume(size_t currarg, DumpResults *C4_RESTRICT results, substr *C4_RESTRICT buf, Sep const&, Arg const& a)
{
    detail::catsep_dump_resume_<sinkfn>(currarg, results, buf, a);
}

template<class SinkFn, class Sep, class Arg>
C4_ALWAYS_INLINE void catsep_dump_resume(size_t currarg, SinkFn &&sinkfn, DumpResults *C4_RESTRICT results, substr *C4_RESTRICT buf, Sep const&, Arg const& a)
{
    detail::catsep_dump_resume_(currarg, std::forward<SinkFn>(sinkfn), results, buf, a);
}

template<SinkPfn sinkfn, class Sep, class Arg, class... Args>
C4_ALWAYS_INLINE void catsep_dump_resume(size_t currarg, DumpResults *C4_RESTRICT results, substr *C4_RESTRICT buf, Sep const& sep, Arg const& a, Args const& ...more)
{
    detail::catsep_dump_resume_<sinkfn>(currarg     , results, buf, a);
    detail::catsep_dump_resume_<sinkfn>(currarg + 1u, results, buf, sep);
    detail::catsep_dump_resume <sinkfn>(currarg + 2u, results, buf, sep, more...);
}

template<class SinkFn, class Sep, class Arg, class... Args>
C4_ALWAYS_INLINE void catsep_dump_resume(size_t currarg, SinkFn &&sinkfn, DumpResults *C4_RESTRICT results, substr *C4_RESTRICT buf, Sep const& sep, Arg const& a, Args const& ...more)
{
    detail::catsep_dump_resume_(currarg     , std::forward<SinkFn>(sinkfn), results, buf, a);
    detail::catsep_dump_resume_(currarg + 1u, std::forward<SinkFn>(sinkfn), results, buf, sep);
    detail::catsep_dump_resume (currarg + 2u, std::forward<SinkFn>(sinkfn), results, buf, sep, more...);
}
} // namespace detail
/// @endcond


template<SinkPfn sinkfn, class Sep, class... Args>
C4_ALWAYS_INLINE DumpResults catsep_dump_resume(substr buf, Sep const& sep, Args const& ...args)
{
    DumpResults results;
    detail::catsep_dump_resume<sinkfn>(0u, &results, &buf, sep, args...);
    return results;
}

template<class SinkFn, class Sep, class... Args>
C4_ALWAYS_INLINE DumpResults catsep_dump_resume(SinkFn &&sinkfn, substr buf, Sep const& sep, Args const& ...args)
{
    DumpResults results;
    detail::catsep_dump_resume(0u, std::forward<SinkFn>(sinkfn), &results, &buf, sep, args...);
    return results;
}


template<SinkPfn sinkfn, class Sep, class... Args>
C4_ALWAYS_INLINE DumpResults catsep_dump_resume(DumpResults results, substr buf, Sep const& sep, Args const& ...args)
{
    detail::catsep_dump_resume<sinkfn>(0u, &results, &buf, sep, args...);
    return results;
}

template<class SinkFn, class Sep, class... Args>
C4_ALWAYS_INLINE DumpResults catsep_dump_resume(SinkFn &&sinkfn, DumpResults results, substr buf, Sep const& sep, Args const& ...args)
{
    detail::catsep_dump_resume(0u, std::forward<SinkFn>(sinkfn), &results, &buf, sep, args...);
    return results;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/// @cond dev
namespace detail {
// terminate the recursion
C4_ALWAYS_INLINE size_t _format_dump_compute_size()
{
    return 0u;
}
template<class T>
C4_ALWAYS_INLINE auto _format_dump_compute_size(T const&)
    -> typename std::enable_if<dump_directly<T>::value, size_t>::type
{
    return 0u; // no buffer needed
}
template<class T>
C4_ALWAYS_INLINE auto _format_dump_compute_size(T const& v)
    -> typename std::enable_if<!dump_directly<T>::value, size_t>::type
{
    return to_chars({}, v);
}
template<class Arg, class... Args>
size_t _format_dump_compute_size(Arg const& a, Args const& ...more)
{
    const size_t sz = _format_dump_compute_size(a); // don't call to_chars() directly
    const size_t rest = _format_dump_compute_size(more...);
    return sz > rest ? sz : rest;
}
} // namespace detail

// terminate the recursion
template<class SinkFn>
C4_ALWAYS_INLINE size_t format_dump(SinkFn &&sinkfn, substr, csubstr fmt)
{
    // we can dump without using buf, so no need to check it
    std::forward<SinkFn>(sinkfn)(fmt);
    return 0u;
}
// terminate the recursion
/** take the function pointer as a template argument */
template<SinkPfn sinkfn>
C4_ALWAYS_INLINE size_t format_dump(substr, csubstr fmt)
{
    // we can dump without using buf, so no need to check it
    sinkfn(fmt);
    return 0u;
}
/// @endcond


/** take the function pointer as a function argument */
template<class SinkFn, class Arg, class... Args>
C4_NO_INLINE size_t format_dump(SinkFn &&sinkfn, substr buf, csubstr fmt, Arg const& a, Args const& ...more)
{
    // we can dump without using buf
    // but we'll only dump if the buffer is ok
    size_t pos = fmt.find("{}"); // @todo use _find_fmt()
    if(C4_UNLIKELY(pos == csubstr::npos))
    {
        std::forward<SinkFn>(sinkfn)(fmt);
        return 0u;
    }
    std::forward<SinkFn>(sinkfn)(fmt.first(pos)); // we can dump without using buf
    fmt = fmt.sub(pos + 2); // skip {} do this before assigning to pos again
    pos = dump(std::forward<SinkFn>(sinkfn), buf, a); // reuse pos to get needed_size
    // dump no more if the buffer was exhausted
    size_t size_for_more;
    if(C4_LIKELY(pos <= buf.len))
        size_for_more = format_dump(std::forward<SinkFn>(sinkfn), buf, fmt, more...);
    else
        size_for_more = detail::_format_dump_compute_size(more...);
    return size_for_more > pos ? size_for_more : pos;
}

/** take the function pointer as a template argument */
template<SinkPfn sinkfn, class Arg, class... Args>
C4_NO_INLINE size_t format_dump(substr buf, csubstr fmt, Arg const& C4_RESTRICT a, Args const& ...more)
{
    // we can dump without using buf
    // but we'll only dump if the buffer is ok
    size_t pos = fmt.find("{}"); // @todo use _find_fmt()
    if(C4_UNLIKELY(pos == csubstr::npos))
    {
        sinkfn(fmt);
        return 0u;
    }
    sinkfn(fmt.first(pos)); // we can dump without using buf
    fmt = fmt.sub(pos + 2); // skip {} do this before assigning to pos again
    pos = dump<sinkfn>(buf, a); // reuse pos to get needed_size
    // dump no more if the buffer was exhausted
    size_t size_for_more;
    if(C4_LIKELY(pos <= buf.len))
        size_for_more = format_dump<sinkfn>(buf, fmt, more...);
    else
        size_for_more = detail::_format_dump_compute_size(more...);
    return size_for_more > pos ? size_for_more : pos;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/// @cond dev
namespace detail {
// terminate the recursion
template<SinkPfn sinkfn>
DumpResults format_dump_resume(size_t currarg, DumpResults results, substr, csubstr fmt)
{
    if(C4_LIKELY(results.write_arg(currarg)))
    {
        // we can dump without using buf
        sinkfn(fmt);
        results.lastok = currarg;
    }
    return results;
}

// terminate the recursion
template<class SinkFn>
DumpResults format_dump_resume(size_t currarg, SinkFn &&sinkfn, DumpResults results, substr, csubstr fmt)
{
    if(C4_LIKELY(results.write_arg(currarg)))
    {
        // we can dump without using buf
        std::forward<SinkFn>(sinkfn)(fmt);
        results.lastok = currarg;
    }
    return results;
}

template<SinkPfn sinkfn, class Arg, class... Args>
DumpResults format_dump_resume(size_t currarg, DumpResults results, substr buf, csubstr fmt, Arg const& a, Args const& ...more)
{
    // we need to process the format even if we're not
    // going to print the first arguments because we're resuming
    const size_t pos = fmt.find("{}"); // @todo use _find_fmt()
    if(C4_LIKELY(pos != csubstr::npos))
    {
        if(C4_LIKELY(results.write_arg(currarg)))
        {
            sinkfn(fmt.first(pos));
            results.lastok = currarg;
        }
        if(C4_LIKELY(results.write_arg(currarg + 1u)))
        {
            const size_t len = dump<sinkfn>(buf, a);
            results.bufsize = len > results.bufsize ? len : results.bufsize;
            if(C4_LIKELY(len <= buf.len))
            {
                results.lastok = currarg + 1u;
            }
            else
            {
                const size_t rest = _format_dump_compute_size(more...);
                results.bufsize = rest > results.bufsize ? rest : results.bufsize;
                return results;
            }
        }
    }
    else
    {
        if(C4_LIKELY(results.write_arg(currarg)))
        {
            sinkfn(fmt);
            results.lastok = currarg;
        }
        return results;
    }
    // NOTE: sparc64 had trouble with reassignment to fmt, and
    // was passing the original fmt to the recursion:
    //fmt = fmt.sub(pos + 2); // DONT!
    return detail::format_dump_resume<sinkfn>(currarg + 2u, results, buf, fmt.sub(pos + 2), more...);
}


template<class SinkFn, class Arg, class... Args>
DumpResults format_dump_resume(size_t currarg, SinkFn &&sinkfn, DumpResults results, substr buf, csubstr fmt, Arg const& a, Args const& ...more)
{
    // we need to process the format even if we're not
    // going to print the first arguments because we're resuming
    const size_t pos = fmt.find("{}"); // @todo use _find_fmt()
    if(C4_LIKELY(pos != csubstr::npos))
    {
        if(C4_LIKELY(results.write_arg(currarg)))
        {
            std::forward<SinkFn>(sinkfn)(fmt.first(pos));
            results.lastok = currarg;
        }
        if(C4_LIKELY(results.write_arg(currarg + 1u)))
        {
            const size_t len = dump(std::forward<SinkFn>(sinkfn), buf, a);
            results.bufsize = len > results.bufsize ? len : results.bufsize;
            if(C4_LIKELY(len <= buf.len))
            {
                results.lastok = currarg + 1u;
            }
            else
            {
                const size_t rest = _format_dump_compute_size(more...);
                results.bufsize = rest > results.bufsize ? rest : results.bufsize;
                return results;
            }
        }
    }
    else
    {
        if(C4_LIKELY(results.write_arg(currarg)))
        {
            std::forward<SinkFn>(sinkfn)(fmt);
            results.lastok = currarg;
        }
        return results;
    }
    // NOTE: sparc64 had trouble with reassignment to fmt, and
    // was passing the original fmt to the recursion:
    //fmt = fmt.sub(pos + 2); // DONT!
    return detail::format_dump_resume(currarg + 2u, std::forward<SinkFn>(sinkfn), results, buf, fmt.sub(pos + 2), more...);
}
} // namespace detail
/// @endcond


template<SinkPfn sinkfn, class... Args>
C4_ALWAYS_INLINE DumpResults format_dump_resume(substr buf, csubstr fmt, Args const& ...args)
{
    return detail::format_dump_resume<sinkfn>(0u, DumpResults{}, buf, fmt, args...);
}

template<class SinkFn, class... Args>
C4_ALWAYS_INLINE DumpResults format_dump_resume(SinkFn &&sinkfn, substr buf, csubstr fmt, Args const& ...args)
{
    return detail::format_dump_resume(0u, std::forward<SinkFn>(sinkfn), DumpResults{}, buf, fmt, args...);
}


template<SinkPfn sinkfn, class... Args>
C4_ALWAYS_INLINE DumpResults format_dump_resume(DumpResults results, substr buf, csubstr fmt, Args const& ...args)
{
    return detail::format_dump_resume<sinkfn>(0u, results, buf, fmt, args...);
}

template<class SinkFn, class... Args>
C4_ALWAYS_INLINE DumpResults format_dump_resume(SinkFn &&sinkfn, DumpResults results, substr buf, csubstr fmt, Args const& ...args)
{
    return detail::format_dump_resume(0u, std::forward<SinkFn>(sinkfn), results, buf, fmt, args...);
}

C4_SUPPRESS_WARNING_GCC_CLANG_POP

} // namespace c4


#endif /* C4_DUMP_HPP_ */
