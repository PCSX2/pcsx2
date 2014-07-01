///////////////////////////////////////////////////////////////////////////////
// Name:        wx/stack.h
// Purpose:     STL stack clone
// Author:      Lindsay Mathieson, Vadim Zeitlin
// Created:     30.07.2001
// Copyright:   (c) 2001 Lindsay Mathieson <lindsay@mathieson.org> (WX_DECLARE_STACK)
//                  2011 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_STACK_H_
#define _WX_STACK_H_

#include "wx/vector.h"

#if wxUSE_STD_CONTAINERS

#include <stack>
#define wxStack std::stack

#else // !wxUSE_STD_CONTAINERS

// Notice that unlike std::stack, wxStack currently always uses wxVector and
// can't be used with any other underlying container type.
//
// Another difference is that comparison operators between stacks are not
// implemented (but they should be, see 23.2.3.3 of ISO/IEC 14882:1998).

template <typename T>
class wxStack
{
public:
    typedef wxVector<T> container_type;
    typedef typename container_type::size_type size_type;
    typedef typename container_type::value_type value_type;

    wxStack() { }
    explicit wxStack(const container_type& cont) : m_cont(cont) { }

    // Default copy ctor, assignment operator and dtor are ok.


    bool empty() const { return m_cont.empty(); }
    size_type size() const { return m_cont.size(); }

    value_type& top() { return m_cont.back(); }
    const value_type& top() const { return m_cont.back(); }

    void push(const value_type& val) { m_cont.push_back(val); }
    void pop() { m_cont.pop_back(); }

private:
    container_type m_cont;
};

#endif // wxUSE_STD_CONTAINERS/!wxUSE_STD_CONTAINERS


// Deprecated macro-based class for compatibility only, don't use any more.
#define WX_DECLARE_STACK(obj, cls) \
class cls : public wxVector<obj> \
{\
public:\
    void push(const obj& o)\
    {\
        push_back(o); \
    };\
\
    void pop()\
    {\
        pop_back(); \
    };\
\
    obj& top()\
    {\
        return at(size() - 1);\
    };\
    const obj& top() const\
    {\
        return at(size() - 1); \
    };\
}

#endif // _WX_STACK_H_

