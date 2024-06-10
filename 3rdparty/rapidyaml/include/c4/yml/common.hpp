#ifndef _C4_YML_COMMON_HPP_
#define _C4_YML_COMMON_HPP_

/** @file common.hpp Common utilities and infrastructure used by ryml. */

#include <cstddef>
#include <c4/substr.hpp>
#include <c4/yml/export.hpp>


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
#   define RYML_NOEXCEPT
#else
#   define RYML_ASSERT(cond)
#   define RYML_ASSERT_MSG(cond, msg)
#   define _RYML_CB_ASSERT(cb, cond)
#   define RYML_NOEXCEPT noexcept
#endif

#define RYML_DEPRECATED(msg) C4_DEPRECATED(msg)

#define RYML_CHECK(cond)                                                \
    do {                                                                \
        if(C4_UNLIKELY(!(cond)))                                        \
        {                                                               \
            RYML_DEBUG_BREAK()                                          \
            c4::yml::error("check failed: " #cond, c4::yml::Location(__FILE__, __LINE__, 0)); \
            C4_UNREACHABLE_AFTER_ERR();                                 \
        }                                                               \
    } while(0)

#define RYML_CHECK_MSG(cond, msg)                                       \
    do                                                                  \
    {                                                                   \
        if(C4_UNLIKELY(!(cond)))                                        \
        {                                                               \
            RYML_DEBUG_BREAK()                                          \
            c4::yml::error(msg ": check failed: " #cond, c4::yml::Location(__FILE__, __LINE__, 0)); \
            C4_UNREACHABLE_AFTER_ERR();                                 \
        }                                                               \
    } while(0)

#if defined(RYML_DBG) && !defined(NDEBUG) && !defined(C4_NO_DEBUG_BREAK)
#   define RYML_DEBUG_BREAK()                               \
    {                                                       \
        if(c4::get_error_flags() & c4::ON_ERROR_DEBUGBREAK) \
        {                                                   \
            C4_DEBUG_BREAK();                               \
        }                                                   \
    }
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

enum : size_t {
    /** a null position */
    npos = size_t(-1),
    /** an index to none */
    NONE = size_t(-1)
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

    LineCol() : offset(), line(), col() {}
    //! construct from line and column
    LineCol(size_t l, size_t c) : offset(0), line(l), col(c) {}
    //! construct from offset, line and column
    LineCol(size_t o, size_t l, size_t c) : offset(o), line(l), col(c) {}
};


//! a source file position
struct RYML_EXPORT Location : public LineCol
{
    csubstr name;

    operator bool () const { return !name.empty() || line != 0 || offset != 0; }

    Location() : LineCol(), name() {}
    Location(                         size_t l, size_t c) : LineCol{   l, c}, name( ) {}
    Location(    csubstr n,           size_t l, size_t c) : LineCol{   l, c}, name(n) {}
    Location(    csubstr n, size_t b, size_t l, size_t c) : LineCol{b, l, c}, name(n) {}
    Location(const char *n,           size_t l, size_t c) : LineCol{   l, c}, name(to_csubstr(n)) {}
    Location(const char *n, size_t b, size_t l, size_t c) : LineCol{b, l, c}, name(to_csubstr(n)) {}
};


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
    Callbacks();

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
do                                                                      \
{                                                                       \
    const char msg[] = msg_literal;                                     \
    RYML_DEBUG_BREAK()                                                  \
    c4::yml::error((cb),                                                \
                   msg, sizeof(msg),                                    \
                   c4::yml::Location(__FILE__, 0, __LINE__, 0));        \
    C4_UNREACHABLE_AFTER_ERR();                                         \
} while(0)
#define _RYML_CB_CHECK(cb, cond)                                        \
    do                                                                  \
    {                                                                   \
        if(!(cond))                                                     \
        {                                                               \
            const char msg[] = "check failed: " #cond;                  \
            RYML_DEBUG_BREAK()                                          \
            c4::yml::error((cb),                                        \
                           msg, sizeof(msg),                            \
                           c4::yml::Location(__FILE__, 0, __LINE__, 0)); \
            C4_UNREACHABLE_AFTER_ERR();                                 \
        }                                                               \
    } while(0)
#define _RYML_CB_ALLOC_HINT(cb, T, num, hint) (T*) (cb).m_allocate((num) * sizeof(T), (hint), (cb).m_user_data)
#define _RYML_CB_ALLOC(cb, T, num) _RYML_CB_ALLOC_HINT((cb), (T), (num), nullptr)
#define _RYML_CB_FREE(cb, buf, T, num)                              \
    do {                                                            \
        (cb).m_free((buf), (num) * sizeof(T), (cb).m_user_data);    \
        (buf) = nullptr;                                            \
    } while(0)


namespace detail {
template<int8_t signedval, uint8_t unsignedval>
struct _charconstant_t
    : public std::conditional<std::is_signed<char>::value,
                              std::integral_constant<int8_t, signedval>,
                              std::integral_constant<uint8_t, unsignedval>>::type
{};
#define _RYML_CHCONST(signedval, unsignedval) ::c4::yml::detail::_charconstant_t<INT8_C(signedval), UINT8_C(unsignedval)>::value
} // namespace detail


namespace detail {
struct _SubstrWriter
{
    substr buf;
    size_t pos;
    _SubstrWriter(substr buf_, size_t pos_=0) : buf(buf_), pos(pos_) {}
    void append(csubstr s)
    {
        C4_ASSERT(!s.overlaps(buf));
        if(s.len && pos + s.len <= buf.len)
        {
            C4_ASSERT(s.str);
            memcpy(buf.str + pos, s.str, s.len);
        }
        pos += s.len;
    }
    void append(char c)
    {
        if(pos < buf.len)
            buf.str[pos] = c;
        ++pos;
    }
    void append_n(char c, size_t numtimes)
    {
        if(numtimes && pos + numtimes < buf.len)
            memset(buf.str + pos, c, numtimes);
        pos += numtimes;
    }
    size_t slack() const { return pos <= buf.len ? buf.len - pos : 0; }
    size_t excess() const { return pos > buf.len ? pos - buf.len : 0; }
    //! get the part written so far
    csubstr curr() const { return pos <= buf.len ? buf.first(pos) : buf; }
    //! get the part that is still free to write to (the remainder)
    substr rem() { return pos < buf.len ? buf.sub(pos) : buf.last(0); }

    size_t advance(size_t more) { pos += more; return pos; }
};
} // namespace detail

/// @endcond

C4_SUPPRESS_WARNING_GCC_CLANG_POP

} // namespace yml
} // namespace c4

#endif /* _C4_YML_COMMON_HPP_ */
