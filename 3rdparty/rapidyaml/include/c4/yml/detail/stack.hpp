#ifndef _C4_YML_DETAIL_STACK_HPP_
#define _C4_YML_DETAIL_STACK_HPP_

#ifndef _C4_YML_COMMON_HPP_
#include "../common.hpp"
#endif

#ifdef RYML_DBG
#   include <type_traits>
#endif

#include <string.h>

namespace c4 {
namespace yml {

C4_SUPPRESS_WARNING_GCC_CLANG_WITH_PUSH("-Wold-style-cast")

namespace detail {

/** A lightweight contiguous stack with Small Storage
 * Optimization. This is required because std::vector can throw
 * exceptions, and we don't want to enforce any particular error
 * mechanism. */
template<class T, id_type N=16>
class stack
{
    static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");
    static_assert(std::is_trivially_destructible<T>::value, "T must be trivially destructible");

public:

    enum : id_type { sso_size = N };

public:

    T              m_buf[size_t(N)];
    T *C4_RESTRICT m_stack;
    id_type        m_size;
    id_type        m_capacity;
    Callbacks      m_callbacks;

public:

    constexpr static bool is_contiguous() { return true; }

    stack(Callbacks const& cb)
        : m_buf()
        , m_stack(m_buf)
        , m_size(0)
        , m_capacity(N)
        , m_callbacks(cb) {}
    stack() : stack(get_callbacks()) {}
    ~stack()
    {
        _free();
    }

    stack(stack const& that) RYML_NOEXCEPT : stack(that.m_callbacks)
    {
        resize(that.m_size);
        _cp(&that);
    }

    stack(stack &&that) noexcept : stack(that.m_callbacks)
    {
        _mv(&that);
    }

    stack& operator= (stack const& that) RYML_NOEXCEPT
    {
        if(&that != this)
        {
            _cb(that.m_callbacks);
            resize(that.m_size);
            _cp(&that);
        }
        return *this;
    }

    stack& operator= (stack &&that) noexcept
    {
        _cb(that.m_callbacks);
        _mv(&that);
        return *this;
    }

public:

    id_type size() const { return m_size; }
    id_type empty() const { return m_size == 0; }
    id_type capacity() const { return m_capacity; }

    void clear()
    {
        m_size = 0;
    }

    void resize(id_type sz)
    {
        reserve(sz);
        m_size = sz;
    }

    void reserve(id_type sz);

    void push(T const& C4_RESTRICT n)
    {
        _RYML_CB_ASSERT(m_callbacks, (const char*)&n + sizeof(T) < (const char*)m_stack || &n > m_stack + m_capacity);
        if(m_size == m_capacity)
        {
            id_type cap = m_capacity == 0 ? N : 2 * m_capacity;
            reserve(cap);
        }
        m_stack[m_size] = n;
        ++m_size;
    }

    void push_top()
    {
        _RYML_CB_ASSERT(m_callbacks, m_size > 0);
        if(m_size == m_capacity)
        {
            id_type cap = m_capacity == 0 ? N : 2 * m_capacity;
            reserve(cap);
        }
        m_stack[m_size] = m_stack[m_size - 1];
        ++m_size;
    }

    T const& C4_RESTRICT pop()
    {
        _RYML_CB_ASSERT(m_callbacks, m_size > 0);
        --m_size;
        return m_stack[m_size];
    }

    C4_ALWAYS_INLINE T const& C4_RESTRICT top() const { _RYML_CB_ASSERT(m_callbacks, m_size > 0); return m_stack[m_size - 1]; }
    C4_ALWAYS_INLINE T      & C4_RESTRICT top()       { _RYML_CB_ASSERT(m_callbacks, m_size > 0); return m_stack[m_size - 1]; }

    C4_ALWAYS_INLINE T const& C4_RESTRICT bottom() const { _RYML_CB_ASSERT(m_callbacks, m_size > 0); return m_stack[0]; }
    C4_ALWAYS_INLINE T      & C4_RESTRICT bottom()       { _RYML_CB_ASSERT(m_callbacks, m_size > 0); return m_stack[0]; }

    C4_ALWAYS_INLINE T const& C4_RESTRICT top(id_type i) const { _RYML_CB_ASSERT(m_callbacks, i < m_size); return m_stack[m_size - 1 - i]; }
    C4_ALWAYS_INLINE T      & C4_RESTRICT top(id_type i)       { _RYML_CB_ASSERT(m_callbacks, i < m_size); return m_stack[m_size - 1 - i]; }

    C4_ALWAYS_INLINE T const& C4_RESTRICT bottom(id_type i) const { _RYML_CB_ASSERT(m_callbacks, i < m_size); return m_stack[i]; }
    C4_ALWAYS_INLINE T      & C4_RESTRICT bottom(id_type i)       { _RYML_CB_ASSERT(m_callbacks, i < m_size); return m_stack[i]; }

    C4_ALWAYS_INLINE T const& C4_RESTRICT operator[](id_type i) const { _RYML_CB_ASSERT(m_callbacks, i < m_size); return m_stack[i]; }
    C4_ALWAYS_INLINE T      & C4_RESTRICT operator[](id_type i)       { _RYML_CB_ASSERT(m_callbacks, i < m_size); return m_stack[i]; }

public:

    using       iterator = T       *;
    using const_iterator = T const *;

    iterator begin() { return m_stack; }
    iterator end  () { return m_stack + m_size; }

    const_iterator begin() const { return (const_iterator)m_stack; }
    const_iterator end  () const { return (const_iterator)m_stack + m_size; }

public:

    void _free();
    void _cp(stack const* C4_RESTRICT that);
    void _mv(stack * that);
    void _cb(Callbacks const& cb);

};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

template<class T, id_type N>
void stack<T, N>::reserve(id_type sz)
{
    if(sz <= m_size)
        return;
    if(sz <= N)
    {
        m_stack = m_buf;
        m_capacity = N;
        return;
    }
    T *buf = (T*) m_callbacks.m_allocate((size_t)sz * sizeof(T), m_stack, m_callbacks.m_user_data);
    _RYML_CB_ASSERT(m_callbacks, ((uintptr_t)buf % alignof(T)) == 0u);
    memcpy(buf, m_stack, (size_t)m_size * sizeof(T));
    if(m_stack != m_buf)
    {
        m_callbacks.m_free(m_stack, (size_t)m_capacity * sizeof(T), m_callbacks.m_user_data);
    }
    m_stack = buf;
    m_capacity = sz;
}


//-----------------------------------------------------------------------------

template<class T, id_type N>
void stack<T, N>::_free()
{
    _RYML_CB_ASSERT(m_callbacks, m_stack != nullptr); // this structure cannot be memset() to zero
    if(m_stack != m_buf)
    {
        m_callbacks.m_free(m_stack, (size_t)m_capacity * sizeof(T), m_callbacks.m_user_data);
        m_stack = m_buf;
        m_size = N;
        m_capacity = N;
    }
    else
    {
        _RYML_CB_ASSERT(m_callbacks, m_capacity == N);
    }
}


//-----------------------------------------------------------------------------

template<class T, id_type N>
void stack<T, N>::_cp(stack const* C4_RESTRICT that)
{
    if(that->m_stack != that->m_buf)
    {
        _RYML_CB_ASSERT(m_callbacks, that->m_capacity > N);
        _RYML_CB_ASSERT(m_callbacks, that->m_size <= that->m_capacity);
    }
    else
    {
        _RYML_CB_ASSERT(m_callbacks, that->m_capacity <= N);
        _RYML_CB_ASSERT(m_callbacks, that->m_size <= that->m_capacity);
    }
    memcpy(m_stack, that->m_stack, that->m_size * sizeof(T));
    m_size = that->m_size;
    m_capacity = that->m_size < N ? N : that->m_size;
    m_callbacks = that->m_callbacks;
}


//-----------------------------------------------------------------------------

template<class T, id_type N>
void stack<T, N>::_mv(stack * that)
{
    if(that->m_stack != that->m_buf)
    {
        _RYML_CB_ASSERT(m_callbacks, that->m_capacity > N);
        _RYML_CB_ASSERT(m_callbacks, that->m_size <= that->m_capacity);
        m_stack = that->m_stack;
    }
    else
    {
        _RYML_CB_ASSERT(m_callbacks, that->m_capacity <= N);
        _RYML_CB_ASSERT(m_callbacks, that->m_size <= that->m_capacity);
        memcpy(m_buf, that->m_buf, that->m_size * sizeof(T));
        m_stack = m_buf;
    }
    m_size = that->m_size;
    m_capacity = that->m_capacity;
    m_callbacks = that->m_callbacks;
    // make sure no deallocation happens on destruction
    _RYML_CB_ASSERT(m_callbacks, that->m_stack != m_buf);
    that->m_stack = that->m_buf;
    that->m_capacity = N;
    that->m_size = 0;
}


//-----------------------------------------------------------------------------

template<class T, id_type N>
void stack<T, N>::_cb(Callbacks const& cb)
{
    if(cb != m_callbacks)
    {
        _free();
        m_callbacks = cb;
    }
}

} // namespace detail

C4_SUPPRESS_WARNING_GCC_CLANG_POP

} // namespace yml
} // namespace c4

#endif /* _C4_YML_DETAIL_STACK_HPP_ */
