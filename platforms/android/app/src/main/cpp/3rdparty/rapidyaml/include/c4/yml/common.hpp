#ifndef _C4_YML_COMMON_HPP_
#define _C4_YML_COMMON_HPP_

/** @file common.hpp Common utilities and infrastructure used by ryml. */

#include <cstddef>
#include <c4/substr.hpp>
#include <c4/charconv.hpp>
#include <c4/dump.hpp>
#include <c4/yml/export.hpp>

#if defined(C4_MSVC) || defined(C4_MINGW) || defined(_WIN32) || defined(C4_WIN)
#include <malloc.h>
#else
#include <alloca.h>
#endif



//-----------------------------------------------------------------------------

#ifndef RYML_ERRMSG_SIZE
/// size for the error message buffer
#define RYML_ERRMSG_SIZE (1024)
#endif

#ifndef RYML_LOGBUF_SIZE
/// size for the buffer used to format individual values to string
/// while preparing an error message. This is only used for formatting
/// individual values in the message; final messages will be larger
/// than this value (see @ref RYML_ERRMSG_SIZE). This is also used for
/// the detailed debug log messages when RYML_DBG is defined.
#define RYML_LOGBUF_SIZE (256)
#endif

#ifndef RYML_LOGBUF_SIZE_MAX
/// size for the fallback larger log buffer. When @ref
/// RYML_LOGBUF_SIZE is not large enough to convert a value to string,
/// then temporary stack memory is allocated up to
/// RYML_LOGBUF_SIZE_MAX. This limit is in place to prevent a stack
/// overflow. If the printed value requires more than
/// RYML_LOGBUF_SIZE_MAX, the value is silently skipped.
#define RYML_LOGBUF_SIZE_MAX (1024)
#endif

#ifndef RYML_LOCATIONS_SMALL_THRESHOLD
/// threshold at which a location search will revert from linear to
/// binary search.
#define RYML_LOCATIONS_SMALL_THRESHOLD (30)
#endif


//-----------------------------------------------------------------------------
// Specify groups to have a predefined topic order in doxygen:

/** @defgroup doc_quickstart Quickstart
 *
 * Example code for every feature.
 */

/** @defgroup doc_parse Parse utilities
 * @see sample::sample_parse_in_place
 * @see sample::sample_parse_in_arena
 * @see sample::sample_parse_file
 * @see sample::sample_parse_reuse_tree
 * @see sample::sample_parse_reuse_parser
 * @see sample::sample_parse_reuse_tree_and_parser
 * @see sample::sample_location_tracking
 */

/** @defgroup doc_emit Emit utilities
 *
 * Utilities to emit YAML and JSON, either to a memory buffer or to a
 * file or ostream-like class.
 *
 * @see sample::sample_emit_to_container
 * @see sample::sample_emit_to_stream
 * @see sample::sample_emit_to_file
 * @see sample::sample_emit_nested_node
 * @see sample::sample_emit_style
 */

/** @defgroup doc_node_type Node types
 */

/** @defgroup doc_tree Tree utilities
 * @see sample::sample_quick_overview
 * @see sample::sample_iterate_trees
 * @see sample::sample_create_trees
 * @see sample::sample_tree_arena
 *
 * @see sample::sample_static_trees
 * @see sample::sample_location_tracking
 *
 * @see sample::sample_docs
 * @see sample::sample_anchors_and_aliases
 * @see sample::sample_tags
 */

/** @defgroup doc_node_classes Node classes
 *
 * High-level node classes.
 *
 * @see sample::sample_quick_overview
 * @see sample::sample_iterate_trees
 * @see sample::sample_create_trees
 * @see sample::sample_tree_arena
 */

/** @defgroup doc_callbacks Callbacks for errors and allocation
 *
 * Functions called by ryml to allocate/free memory and to report
 * errors.
 *
 * @see sample::sample_error_handler
 * @see sample::sample_global_allocator
 * @see sample::sample_per_tree_allocator
 */

/** @defgroup doc_serialization Serialization/deserialization
 *
 * Contains information on how to serialize and deserialize
 * fundamental types, user scalar types, user container types and
 * interop with std scalar/container types.
 *
 */

/** @defgroup doc_ref_utils Anchor/Reference utilities
 *
 * @see sample::sample_anchors_and_aliases
 * */

/** @defgroup doc_tag_utils Tag utilities
 * @see sample::sample_tags
 */

/** @defgroup doc_preprocessors Preprocessors
 *
 * Functions for preprocessing YAML prior to parsing.
 */


//-----------------------------------------------------------------------------

// document macros for doxygen
#ifdef __DOXYGEN__ // defined in Doxyfile::PREDEFINED

/** define this macro with a boolean value to enable/disable
 * assertions to check preconditions and assumptions throughout the
 * codebase; this causes a slowdown of the code, and larger code
 * size. By default, this macro is defined unless NDEBUG is defined
 * (see C4_USE_ASSERT); as a result, by default this macro is truthy
 * only in debug builds. */
#   define RYML_USE_ASSERT

/** (Undefined by default) Define this macro to disable ryml's default
 * implementation of the callback functions; see @ref c4::yml::Callbacks  */
#   define RYML_NO_DEFAULT_CALLBACKS

/** (Undefined by default) When this macro is defined (and
 * @ref RYML_NO_DEFAULT_CALLBACKS is not defined), the default error
 * handler will throw C++ exceptions of type `std::runtime_error`. */
#   define RYML_DEFAULT_CALLBACK_USES_EXCEPTIONS

/** Conditionally expands to `noexcept` when @ref RYML_USE_ASSERT is 0 and
 * is empty otherwise. The user is unable to override this macro. */
#   define RYML_NOEXCEPT

#endif


//-----------------------------------------------------------------------------


/** @cond dev*/
#ifndef RYML_USE_ASSERT
#   define RYML_USE_ASSERT C4_USE_ASSERT
#endif

#if RYML_USE_ASSERT
#   define RYML_ASSERT(cond) RYML_CHECK(cond)
#   define RYML_ASSERT_MSG(cond, msg) RYML_CHECK_MSG(cond, msg)
#   define _RYML_CB_ASSERT(cb, cond) _RYML_CB_CHECK((cb), (cond))
#   define _RYML_CB_ASSERT_(cb, cond, loc) _RYML_CB_CHECK((cb), (cond), (loc))
#   define RYML_NOEXCEPT
#else
#   define RYML_ASSERT(cond)
#   define RYML_ASSERT_MSG(cond, msg)
#   define _RYML_CB_ASSERT(cb, cond)
#   define _RYML_CB_ASSERT_(cb, cond, loc)
#   define RYML_NOEXCEPT noexcept
#endif

#define RYML_DEPRECATED(msg) C4_DEPRECATED(msg)

#define RYML_CHECK(cond)                                                \
    do {                                                                \
        if(C4_UNLIKELY(!(cond)))                                        \
        {                                                               \
            RYML_DEBUG_BREAK();                                         \
            c4::yml::error("check failed: " #cond, c4::yml::Location(__FILE__, __LINE__, 0)); \
            C4_UNREACHABLE_AFTER_ERR();                                 \
        }                                                               \
    } while(0)

#define RYML_CHECK_MSG(cond, msg)                                       \
    do                                                                  \
    {                                                                   \
        if(C4_UNLIKELY(!(cond)))                                        \
        {                                                               \
            RYML_DEBUG_BREAK();                                         \
            c4::yml::error(msg ": check failed: " #cond, c4::yml::Location(__FILE__, __LINE__, 0)); \
            C4_UNREACHABLE_AFTER_ERR();                                 \
        }                                                               \
    } while(0)

#if defined(RYML_DBG) && !defined(NDEBUG) && !defined(C4_NO_DEBUG_BREAK)
#   define RYML_DEBUG_BREAK()                               \
    do {                                                    \
        if(c4::get_error_flags() & c4::ON_ERROR_DEBUGBREAK) \
        {                                                   \
            C4_DEBUG_BREAK();                               \
        }                                                   \
    } while(0)
#else
#   define RYML_DEBUG_BREAK()
#endif

/** @endcond */


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

namespace c4 {
namespace yml {

C4_SUPPRESS_WARNING_GCC_CLANG_WITH_PUSH("-Wold-style-cast")


#ifndef RYML_ID_TYPE
/** The type of a node id in the YAML tree. In the future, the default
 * will likely change to int32_t, which was observed to be faster.
 * @see id_type */
#define RYML_ID_TYPE size_t
#endif


/** The type of a node id in the YAML tree; to override the default
 * type, define the macro @ref RYML_ID_TYPE to a suitable integer
 * type. */
using id_type = RYML_ID_TYPE;
static_assert(std::is_integral<id_type>::value, "id_type must be an integer type");


C4_SUPPRESS_WARNING_GCC_WITH_PUSH("-Wuseless-cast")
enum : id_type {
    /** an index to none */
    NONE = id_type(-1),
};
C4_SUPPRESS_WARNING_GCC_CLANG_POP


enum : size_t {
    /** a null string position */
    npos = size_t(-1)
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

//! holds a position into a source buffer
struct RYML_EXPORT LineCol
{
    //! number of bytes from the beginning of the source buffer
    size_t offset;
    //! line
    size_t line;
    //! column
    size_t col;

    LineCol() = default;
    //! construct from line and column
    LineCol(size_t l, size_t c) : offset(0), line(l), col(c) {}
    //! construct from offset, line and column
    LineCol(size_t o, size_t l, size_t c) : offset(o), line(l), col(c) {}
};
static_assert(std::is_trivially_copyable<LineCol>::value, "LineCol not trivially copyable");
static_assert(std::is_trivially_default_constructible<LineCol>::value, "LineCol not trivially default constructible");
static_assert(std::is_standard_layout<LineCol>::value, "Location not trivial");


//! a source file position
struct RYML_EXPORT Location
{
    //! number of bytes from the beginning of the source buffer
    size_t offset;
    //! line
    size_t line;
    //! column
    size_t col;
    //! file name
    csubstr name;

    operator bool () const { return !name.empty() || line != 0 || offset != 0 || col != 0; }
    operator LineCol const& () const { return reinterpret_cast<LineCol const&>(*this); } // NOLINT

    Location() = default;
    Location(                         size_t l, size_t c) : offset( ), line(l), col(c), name( ) {}
    Location(               size_t b, size_t l, size_t c) : offset(b), line(l), col(c), name( ) {}
    Location(    csubstr n,           size_t l, size_t c) : offset( ), line(l), col(c), name(n) {}
    Location(    csubstr n, size_t b, size_t l, size_t c) : offset(b), line(l), col(c), name(n) {}
    Location(const char *n,           size_t l, size_t c) : offset( ), line(l), col(c), name(to_csubstr(n)) {}
    Location(const char *n, size_t b, size_t l, size_t c) : offset(b), line(l), col(c), name(to_csubstr(n)) {}
};
static_assert(std::is_standard_layout<Location>::value, "Location not trivial");


//-----------------------------------------------------------------------------

/** @addtogroup doc_callbacks
 *
 * @{ */

struct Callbacks;


/** set the global callbacks for the library; after a call to this
 * function, these callbacks will be used by newly created objects
 * (unless they are copying older objects with different
 * callbacks). If @ref RYML_NO_DEFAULT_CALLBACKS is defined, it is
 * mandatory to call this function prior to using any other library
 * facility.
 *
 * @warning This function is NOT thread-safe.
 *
 * @warning the error callback must never return: see @ref pfn_error
 * for more details */
RYML_EXPORT void set_callbacks(Callbacks const& c);

/** get the global callbacks
 * @warning This function is not thread-safe. */
RYML_EXPORT Callbacks const& get_callbacks();

/** set the global callbacks back to their defaults ()
 * @warning This function is not thread-safe. */
RYML_EXPORT void reset_callbacks();


/** the type of the function used to report errors
 *
 * @warning When given by the user, this function MUST interrupt
 * execution, typically by either throwing an exception, or using
 * `std::longjmp()` ([see
 * documentation](https://en.cppreference.com/w/cpp/utility/program/setjmp))
 * or by calling `std::abort()`. If the function returned, the parser
 * would enter into an infinite loop, or the program may crash. */
using pfn_error = void (*) (const char* msg, size_t msg_len, Location location, void *user_data);


/** the type of the function used to allocate memory; ryml will only
 * allocate memory through this callback. */
using pfn_allocate = void* (*)(size_t len, void* hint, void *user_data);


/** the type of the function used to free memory; ryml will only free
 * memory through this callback. */
using pfn_free = void (*)(void* mem, size_t size, void *user_data);


/** a c-style callbacks class. Can be used globally by the library
 * and/or locally by @ref Tree and @ref Parser objects. */
struct RYML_EXPORT Callbacks
{
    void *       m_user_data;
    pfn_allocate m_allocate;
    pfn_free     m_free;
    pfn_error    m_error;

    /** Construct an object with the default callbacks. If
     * @ref RYML_NO_DEFAULT_CALLBACKS is defined, the object will have null
     * members.*/
    Callbacks() noexcept;

    /** Construct an object with the given callbacks.
     *
     * @param user_data Data to be forwarded in every call to a callback.
     *
     * @param alloc A pointer to an allocate function. Unless
     *        @ref RYML_NO_DEFAULT_CALLBACKS is defined, when this
     *        parameter is null, will fall back to ryml's default
     *        alloc implementation.
     *
     * @param free A pointer to a free function. Unless
     *        @ref RYML_NO_DEFAULT_CALLBACKS is defined, when this
     *        parameter is null, will fall back to ryml's default free
     *        implementation.
     *
     * @param error A pointer to an error function, which must never
     *        return (see @ref pfn_error). Unless
     *        @ref RYML_NO_DEFAULT_CALLBACKS is defined, when this
     *        parameter is null, will fall back to ryml's default
     *        error implementation.
     */
    Callbacks(void *user_data, pfn_allocate alloc, pfn_free free, pfn_error error);

    bool operator!= (Callbacks const& that) const { return !operator==(that); }
    bool operator== (Callbacks const& that) const
    {
        return (m_user_data == that.m_user_data &&
                m_allocate == that.m_allocate &&
                m_free == that.m_free &&
                m_error == that.m_error);
    }
};


/** @} */


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

typedef enum {
    NOBOM,
    UTF8,
    UTF16LE,
    UTF16BE,
    UTF32LE,
    UTF32BE,
} Encoding_e;


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/// @cond dev

// BEWARE! MSVC requires that [[noreturn]] appears before RYML_EXPORT
[[noreturn]] RYML_EXPORT void error(Callbacks const& cb, const char *msg, size_t msg_len, Location loc);
[[noreturn]] RYML_EXPORT void error(const char *msg, size_t msg_len, Location loc);

[[noreturn]] inline void error(const char *msg, size_t msg_len)
{
    error(msg, msg_len, Location{});
}
template<size_t N>
[[noreturn]] inline void error(const char (&msg)[N], Location loc)
{
    error(msg, N-1, loc);
}
template<size_t N>
[[noreturn]] inline void error(const char (&msg)[N])
{
    error(msg, N-1, Location{});
}

#define _RYML_CB_ERR(cb, msg_literal)                                   \
    _RYML_CB_ERR_(cb, msg_literal, c4::yml::Location(__FILE__, 0, __LINE__, 0))
#define _RYML_CB_CHECK(cb, cond)                                        \
    _RYML_CB_CHECK_(cb, cond, c4::yml::Location(__FILE__, 0, __LINE__, 0))
#define _RYML_CB_ERR_(cb, msg_literal, loc)                             \
do                                                                      \
{                                                                       \
    const char msg[] = msg_literal;                                     \
    RYML_DEBUG_BREAK();                                                 \
    c4::yml::error((cb), msg, sizeof(msg)-1, loc);                      \
    C4_UNREACHABLE_AFTER_ERR();                                         \
} while(0)
#define _RYML_CB_CHECK_(cb, cond, loc)                                  \
    do                                                                  \
    {                                                                   \
        if(C4_UNLIKELY(!(cond)))                                        \
        {                                                               \
            const char msg[] = "check failed: " #cond;                  \
            RYML_DEBUG_BREAK();                                         \
            c4::yml::error((cb), msg, sizeof(msg)-1, loc);              \
            C4_UNREACHABLE_AFTER_ERR();                                 \
        }                                                               \
    } while(0)
#define _RYML_CB_ALLOC_HINT(cb, T, num, hint) (T*) (cb).m_allocate((num) * sizeof(T), (hint), (cb).m_user_data)
#define _RYML_CB_ALLOC(cb, T, num) _RYML_CB_ALLOC_HINT((cb), T, (num), nullptr)
#define _RYML_CB_FREE(cb, buf, T, num)                              \
    do {                                                            \
        (cb).m_free((buf), (num) * sizeof(T), (cb).m_user_data);    \
        (buf) = nullptr;                                            \
    } while(0)


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

typedef enum {
    BLOCK_LITERAL, //!< keep newlines (|)
    BLOCK_FOLD     //!< replace newline with single space (>)
} BlockStyle_e;

typedef enum {
    CHOMP_CLIP,    //!< single newline at end (default)
    CHOMP_STRIP,   //!< no newline at end     (-)
    CHOMP_KEEP     //!< all newlines from end (+)
} BlockChomp_e;


/** Abstracts the fact that a scalar filter result may not fit in the
 * intended memory. */
struct FilterResult
{
    C4_ALWAYS_INLINE bool valid() const noexcept { return str.str != nullptr; }
    C4_ALWAYS_INLINE size_t required_len() const noexcept { return str.len; }
    C4_ALWAYS_INLINE csubstr get() const { RYML_ASSERT(valid()); return str; }
    csubstr str;
};
/** Abstracts the fact that a scalar filter result may not fit in the
 * intended memory. */
struct FilterResultExtending
{
    C4_ALWAYS_INLINE bool valid() const noexcept { return str.str != nullptr; }
    C4_ALWAYS_INLINE size_t required_len() const noexcept { return reqlen; }
    C4_ALWAYS_INLINE csubstr get() const { RYML_ASSERT(valid()); return str; }
    csubstr str;
    size_t reqlen;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


namespace detail {
// is there a better way to do this?
template<int8_t signedval, uint8_t unsignedval>
struct _charconstant_t
    : public std::conditional<std::is_signed<char>::value,
                              std::integral_constant<int8_t, static_cast<int8_t>(unsignedval)>,
                              std::integral_constant<uint8_t, unsignedval>>::type
{};
#define _RYML_CHCONST(signedval, unsignedval) ::c4::yml::detail::_charconstant_t<INT8_C(signedval), UINT8_C(unsignedval)>::value
} // namespace detail


namespace detail {
struct _SubstrWriter
{
    substr buf;
    size_t pos;
    _SubstrWriter(substr buf_, size_t pos_=0) : buf(buf_), pos(pos_) { C4_ASSERT(buf.str); }
    void append(csubstr s)
    {
        C4_ASSERT(!s.overlaps(buf));
        C4_ASSERT(s.str || !s.len);
        if(s.len && pos + s.len <= buf.len)
        {
            C4_ASSERT(s.str);
            memcpy(buf.str + pos, s.str, s.len);
        }
        pos += s.len;
    }
    void append(char c)
    {
        C4_ASSERT(buf.str);
        if(pos < buf.len)
            buf.str[pos] = c;
        ++pos;
    }
    void append_n(char c, size_t numtimes)
    {
        C4_ASSERT(buf.str);
        if(numtimes && pos + numtimes < buf.len)
            memset(buf.str + pos, c, numtimes);
        pos += numtimes;
    }
    size_t slack() const { return pos <= buf.len ? buf.len - pos : 0; }
    size_t excess() const { return pos > buf.len ? pos - buf.len : 0; }
    //! get the part written so far
    csubstr curr() const { return pos <= buf.len ? buf.first(pos) : buf; }
    //! get the part that is still free to write to (the remainder)
    substr rem() const { return pos < buf.len ? buf.sub(pos) : buf.last(0); }

    size_t advance(size_t more) { pos += more; return pos; }
};
} // namespace detail


namespace detail {
// dumpfn is a function abstracting prints to terminal (or to string).
template<class DumpFn, class ...Args>
C4_NO_INLINE void _dump(DumpFn &&dumpfn, csubstr fmt, Args&& ...args)
{
    DumpResults results;
    // try writing everything:
    {
        // buffer for converting individual arguments. it is defined
        // in a child scope to free it in case the buffer is too small
        // for any of the arguments.
        char writebuf[RYML_LOGBUF_SIZE];
        results = format_dump_resume(std::forward<DumpFn>(dumpfn), writebuf, fmt, std::forward<Args>(args)...);
    }
    // if any of the arguments failed to fit the buffer, allocate a
    // larger buffer (up to a limit) and resume writing.
    //
    // results.bufsize is set to the size of the largest element
    // serialized. Eg int(1) will require 1 byte.
    if(C4_UNLIKELY(results.bufsize > RYML_LOGBUF_SIZE))
    {
        const size_t bufsize = results.bufsize <= RYML_LOGBUF_SIZE_MAX ? results.bufsize : RYML_LOGBUF_SIZE_MAX;
        #ifdef C4_MSVC
        substr largerbuf = {static_cast<char*>(_alloca(bufsize)), bufsize};
        #else
        substr largerbuf = {static_cast<char*>(alloca(bufsize)), bufsize};
        #endif
        results = format_dump_resume(std::forward<DumpFn>(dumpfn), results, largerbuf, fmt, std::forward<Args>(args)...);
    }
}
template<class ...Args>
C4_NORETURN C4_NO_INLINE void _report_err(Callbacks const& C4_RESTRICT callbacks, csubstr fmt, Args const& C4_RESTRICT ...args)
{
    char errmsg[RYML_ERRMSG_SIZE] = {0};
    detail::_SubstrWriter writer(errmsg);
    auto dumpfn = [&writer](csubstr s){ writer.append(s); };
    _dump(dumpfn, fmt, args...);
    writer.append('\n');
    const size_t len = writer.pos < RYML_ERRMSG_SIZE ? writer.pos : RYML_ERRMSG_SIZE;
    callbacks.m_error(errmsg, len, {}, callbacks.m_user_data);
    C4_UNREACHABLE_AFTER_ERR();
}
} // namespace detail


inline csubstr _c4prc(const char &C4_RESTRICT c) // pass by reference!
{
    switch(c)
    {
    case '\n': return csubstr("\\n");
    case '\t': return csubstr("\\t");
    case '\0': return csubstr("\\0");
    case '\r': return csubstr("\\r");
    case '\f': return csubstr("\\f");
    case '\b': return csubstr("\\b");
    case '\v': return csubstr("\\v");
    case '\a': return csubstr("\\a");
    default: return csubstr(&c, 1);
    }
}

/// @endcond

C4_SUPPRESS_WARNING_GCC_POP

} // namespace yml
} // namespace c4

#endif /* _C4_YML_COMMON_HPP_ */
