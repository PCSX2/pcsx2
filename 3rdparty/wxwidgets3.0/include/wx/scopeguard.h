///////////////////////////////////////////////////////////////////////////////
// Name:        wx/scopeguard.h
// Purpose:     declares wxwxScopeGuard and related macros
// Author:      Vadim Zeitlin
// Modified by:
// Created:     03.07.2003
// Copyright:   (c) 2003 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

/*
    Acknowledgements: this header is heavily based on (well, almost the exact
    copy of) ScopeGuard.h by Andrei Alexandrescu and Petru Marginean published
    in December 2000 issue of C/C++ Users Journal.
    http://www.cuj.com/documents/cujcexp1812alexandr/
 */

#ifndef _WX_SCOPEGUARD_H_
#define _WX_SCOPEGUARD_H_

#include "wx/defs.h"

#include "wx/except.h"

// ----------------------------------------------------------------------------
// helpers
// ----------------------------------------------------------------------------

#ifdef __WATCOMC__

// WATCOM-FIXME: C++ of Open Watcom 1.3 doesn't like OnScopeExit() created
// through template so it must be workarounded with dedicated inlined macro.
// For compatibility with Watcom compilers wxPrivate::OnScopeExit must be
// replaced with wxPrivateOnScopeExit but in user code (for everyone who
// doesn't care about OW compatibility) wxPrivate::OnScopeExit still works.

#define wxPrivateOnScopeExit(guard)          \
    {                                        \
        if ( !(guard).WasDismissed() )       \
        {                                    \
            wxTRY                            \
            {                                \
                (guard).Execute();           \
            }                                \
            wxCATCH_ALL(;)                   \
        }                                    \
    }

#define wxPrivateUse(n) wxUnusedVar(n)

#else

namespace wxPrivate
{
    // in the original implementation this was a member template function of
    // ScopeGuardImplBase but gcc 2.8 which is still used for OS/2 doesn't
    // support member templates and so we must make it global
    template <class ScopeGuardImpl>
    void OnScopeExit(ScopeGuardImpl& guard)
    {
        if ( !guard.WasDismissed() )
        {
            // we're called from ScopeGuardImpl dtor and so we must not throw
            wxTRY
            {
                guard.Execute();
            }
            wxCATCH_ALL(;) // do nothing, just eat the exception
        }
    }

    // just to avoid the warning about unused variables
    template <class T>
    void Use(const T& WXUNUSED(t))
    {
    }
} // namespace wxPrivate

#define wxPrivateOnScopeExit(n) wxPrivate::OnScopeExit(n)
#define wxPrivateUse(n) wxPrivate::Use(n)

#endif

// ============================================================================
// wxScopeGuard for functions and functors
// ============================================================================

// ----------------------------------------------------------------------------
// wxScopeGuardImplBase: used by wxScopeGuardImpl[0..N] below
// ----------------------------------------------------------------------------

class wxScopeGuardImplBase
{
public:
    wxScopeGuardImplBase() : m_wasDismissed(false) { }

    wxScopeGuardImplBase(const wxScopeGuardImplBase& other)
        : m_wasDismissed(other.m_wasDismissed)
    {
        other.Dismiss();
    }

    void Dismiss() const { m_wasDismissed = true; }

    // for OnScopeExit() only (we can't make it friend, unfortunately)!
    bool WasDismissed() const { return m_wasDismissed; }

protected:
    ~wxScopeGuardImplBase() { }

    // must be mutable for copy ctor to work
    mutable bool m_wasDismissed;

private:
    wxScopeGuardImplBase& operator=(const wxScopeGuardImplBase&);
};

// wxScopeGuard is just a reference, see the explanation in CUJ article
typedef const wxScopeGuardImplBase& wxScopeGuard;

// ----------------------------------------------------------------------------
// wxScopeGuardImpl0: scope guard for actions without parameters
// ----------------------------------------------------------------------------

template <class F>
class wxScopeGuardImpl0 : public wxScopeGuardImplBase
{
public:
    static wxScopeGuardImpl0<F> MakeGuard(F fun)
    {
        return wxScopeGuardImpl0<F>(fun);
    }

    ~wxScopeGuardImpl0() { wxPrivateOnScopeExit(*this); }

    void Execute() { m_fun(); }

protected:
    wxScopeGuardImpl0(F fun) : m_fun(fun) { }

    F m_fun;

    wxScopeGuardImpl0& operator=(const wxScopeGuardImpl0&);
};

template <class F>
inline wxScopeGuardImpl0<F> wxMakeGuard(F fun)
{
    return wxScopeGuardImpl0<F>::MakeGuard(fun);
}

// ----------------------------------------------------------------------------
// wxScopeGuardImpl1: scope guard for actions with 1 parameter
// ----------------------------------------------------------------------------

template <class F, class P1>
class wxScopeGuardImpl1 : public wxScopeGuardImplBase
{
public:
    static wxScopeGuardImpl1<F, P1> MakeGuard(F fun, P1 p1)
    {
        return wxScopeGuardImpl1<F, P1>(fun, p1);
    }

    ~wxScopeGuardImpl1() { wxPrivateOnScopeExit(* this); }

    void Execute() { m_fun(m_p1); }

protected:
    wxScopeGuardImpl1(F fun, P1 p1) : m_fun(fun), m_p1(p1) { }

    F m_fun;
    const P1 m_p1;

    wxScopeGuardImpl1& operator=(const wxScopeGuardImpl1&);
};

template <class F, class P1>
inline wxScopeGuardImpl1<F, P1> wxMakeGuard(F fun, P1 p1)
{
    return wxScopeGuardImpl1<F, P1>::MakeGuard(fun, p1);
}

// ----------------------------------------------------------------------------
// wxScopeGuardImpl2: scope guard for actions with 2 parameters
// ----------------------------------------------------------------------------

template <class F, class P1, class P2>
class wxScopeGuardImpl2 : public wxScopeGuardImplBase
{
public:
    static wxScopeGuardImpl2<F, P1, P2> MakeGuard(F fun, P1 p1, P2 p2)
    {
        return wxScopeGuardImpl2<F, P1, P2>(fun, p1, p2);
    }

    ~wxScopeGuardImpl2() { wxPrivateOnScopeExit(*this); }

    void Execute() { m_fun(m_p1, m_p2); }

protected:
    wxScopeGuardImpl2(F fun, P1 p1, P2 p2) : m_fun(fun), m_p1(p1), m_p2(p2) { }

    F m_fun;
    const P1 m_p1;
    const P2 m_p2;

    wxScopeGuardImpl2& operator=(const wxScopeGuardImpl2&);
};

template <class F, class P1, class P2>
inline wxScopeGuardImpl2<F, P1, P2> wxMakeGuard(F fun, P1 p1, P2 p2)
{
    return wxScopeGuardImpl2<F, P1, P2>::MakeGuard(fun, p1, p2);
}

// ----------------------------------------------------------------------------
// wxScopeGuardImpl3: scope guard for actions with 3 parameters
// ----------------------------------------------------------------------------

template <class F, class P1, class P2, class P3>
class wxScopeGuardImpl3 : public wxScopeGuardImplBase
{
public:
    static wxScopeGuardImpl3<F, P1, P2, P3> MakeGuard(F fun, P1 p1, P2 p2, P3 p3)
    {
        return wxScopeGuardImpl3<F, P1, P2, P3>(fun, p1, p2, p3);
    }

    ~wxScopeGuardImpl3() { wxPrivateOnScopeExit(*this); }

    void Execute() { m_fun(m_p1, m_p2, m_p3); }

protected:
    wxScopeGuardImpl3(F fun, P1 p1, P2 p2, P3 p3)
        : m_fun(fun), m_p1(p1), m_p2(p2), m_p3(p3) { }

    F m_fun;
    const P1 m_p1;
    const P2 m_p2;
    const P3 m_p3;

    wxScopeGuardImpl3& operator=(const wxScopeGuardImpl3&);
};

template <class F, class P1, class P2, class P3>
inline wxScopeGuardImpl3<F, P1, P2, P3> wxMakeGuard(F fun, P1 p1, P2 p2, P3 p3)
{
    return wxScopeGuardImpl3<F, P1, P2, P3>::MakeGuard(fun, p1, p2, p3);
}

// ============================================================================
// wxScopeGuards for object methods
// ============================================================================

// ----------------------------------------------------------------------------
// wxObjScopeGuardImpl0
// ----------------------------------------------------------------------------

template <class Obj, class MemFun>
class wxObjScopeGuardImpl0 : public wxScopeGuardImplBase
{
public:
    static wxObjScopeGuardImpl0<Obj, MemFun>
        MakeObjGuard(Obj& obj, MemFun memFun)
    {
        return wxObjScopeGuardImpl0<Obj, MemFun>(obj, memFun);
    }

    ~wxObjScopeGuardImpl0() { wxPrivateOnScopeExit(*this); }

    void Execute() { (m_obj.*m_memfun)(); }

protected:
    wxObjScopeGuardImpl0(Obj& obj, MemFun memFun)
        : m_obj(obj), m_memfun(memFun) { }

    Obj& m_obj;
    MemFun m_memfun;
};

template <class Obj, class MemFun>
inline wxObjScopeGuardImpl0<Obj, MemFun> wxMakeObjGuard(Obj& obj, MemFun memFun)
{
    return wxObjScopeGuardImpl0<Obj, MemFun>::MakeObjGuard(obj, memFun);
}

template <class Obj, class MemFun, class P1>
class wxObjScopeGuardImpl1 : public wxScopeGuardImplBase
{
public:
    static wxObjScopeGuardImpl1<Obj, MemFun, P1>
        MakeObjGuard(Obj& obj, MemFun memFun, P1 p1)
    {
        return wxObjScopeGuardImpl1<Obj, MemFun, P1>(obj, memFun, p1);
    }

    ~wxObjScopeGuardImpl1() { wxPrivateOnScopeExit(*this); }

    void Execute() { (m_obj.*m_memfun)(m_p1); }

protected:
    wxObjScopeGuardImpl1(Obj& obj, MemFun memFun, P1 p1)
        : m_obj(obj), m_memfun(memFun), m_p1(p1) { }

    Obj& m_obj;
    MemFun m_memfun;
    const P1 m_p1;
};

template <class Obj, class MemFun, class P1>
inline wxObjScopeGuardImpl1<Obj, MemFun, P1>
wxMakeObjGuard(Obj& obj, MemFun memFun, P1 p1)
{
    return wxObjScopeGuardImpl1<Obj, MemFun, P1>::MakeObjGuard(obj, memFun, p1);
}

template <class Obj, class MemFun, class P1, class P2>
class wxObjScopeGuardImpl2 : public wxScopeGuardImplBase
{
public:
    static wxObjScopeGuardImpl2<Obj, MemFun, P1, P2>
        MakeObjGuard(Obj& obj, MemFun memFun, P1 p1, P2 p2)
    {
        return wxObjScopeGuardImpl2<Obj, MemFun, P1, P2>(obj, memFun, p1, p2);
    }

    ~wxObjScopeGuardImpl2() { wxPrivateOnScopeExit(*this); }

    void Execute() { (m_obj.*m_memfun)(m_p1, m_p2); }

protected:
    wxObjScopeGuardImpl2(Obj& obj, MemFun memFun, P1 p1, P2 p2)
        : m_obj(obj), m_memfun(memFun), m_p1(p1), m_p2(p2) { }

    Obj& m_obj;
    MemFun m_memfun;
    const P1 m_p1;
    const P2 m_p2;
};

template <class Obj, class MemFun, class P1, class P2>
inline wxObjScopeGuardImpl2<Obj, MemFun, P1, P2>
wxMakeObjGuard(Obj& obj, MemFun memFun, P1 p1, P2 p2)
{
    return wxObjScopeGuardImpl2<Obj, MemFun, P1, P2>::
                                            MakeObjGuard(obj, memFun, p1, p2);
}

template <class Obj, class MemFun, class P1, class P2, class P3>
class wxObjScopeGuardImpl3 : public wxScopeGuardImplBase
{
public:
    static wxObjScopeGuardImpl3<Obj, MemFun, P1, P2, P3>
        MakeObjGuard(Obj& obj, MemFun memFun, P1 p1, P2 p2, P3 p3)
    {
        return wxObjScopeGuardImpl3<Obj, MemFun, P1, P2, P3>(obj, memFun, p1, p2, p3);
    }

    ~wxObjScopeGuardImpl3() { wxPrivateOnScopeExit(*this); }

    void Execute() { (m_obj.*m_memfun)(m_p1, m_p2, m_p3); }

protected:
    wxObjScopeGuardImpl3(Obj& obj, MemFun memFun, P1 p1, P2 p2, P3 p3)
        : m_obj(obj), m_memfun(memFun), m_p1(p1), m_p2(p2), m_p3(p3) { }

    Obj& m_obj;
    MemFun m_memfun;
    const P1 m_p1;
    const P2 m_p2;
    const P3 m_p3;
};

template <class Obj, class MemFun, class P1, class P2, class P3>
inline wxObjScopeGuardImpl3<Obj, MemFun, P1, P2, P3>
wxMakeObjGuard(Obj& obj, MemFun memFun, P1 p1, P2 p2, P3 p3)
{
    return wxObjScopeGuardImpl3<Obj, MemFun, P1, P2, P3>::
                                        MakeObjGuard(obj, memFun, p1, p2, p3);
}

// ----------------------------------------------------------------------------
// wxVariableSetter: use the same technique as for wxScopeGuard to allow
//                   setting a variable to some value on block exit
// ----------------------------------------------------------------------------

namespace wxPrivate
{

// empty class just to be able to define a reference to it
class VariableSetterBase : public wxScopeGuardImplBase { };

typedef const VariableSetterBase& VariableSetter;

template <typename T, typename U>
class VariableSetterImpl : public VariableSetterBase
{
public:
    VariableSetterImpl(T& var, U value)
        : m_var(var),
          m_value(value)
    {
    }

    ~VariableSetterImpl() { wxPrivateOnScopeExit(*this); }

    void Execute() { m_var = m_value; }

private:
    T& m_var;
    const U m_value;

    // suppress the warning about assignment operator not being generated
    VariableSetterImpl<T, U>& operator=(const VariableSetterImpl<T, U>&);
};

template <typename T>
class VariableNullerImpl : public VariableSetterBase
{
public:
    VariableNullerImpl(T& var)
        : m_var(var)
    {
    }

    ~VariableNullerImpl() { wxPrivateOnScopeExit(*this); }

    void Execute() { m_var = NULL; }

private:
    T& m_var;

    VariableNullerImpl<T>& operator=(const VariableNullerImpl<T>&);
};

} // namespace wxPrivate

template <typename T, typename U>
inline
wxPrivate::VariableSetterImpl<T, U> wxMakeVarSetter(T& var, U value)
{
      return wxPrivate::VariableSetterImpl<T, U>(var, value);
}

// calling wxMakeVarSetter(ptr, NULL) doesn't work because U is deduced to be
// "int" and subsequent assignment of "U" to "T *" fails, so provide a special
// function for this special case
template <typename T>
inline
wxPrivate::VariableNullerImpl<T> wxMakeVarNuller(T& var)
{
    return wxPrivate::VariableNullerImpl<T>(var);
}

// ============================================================================
// macros for declaring unnamed scoped guards (which can't be dismissed)
// ============================================================================

// NB: the original code has a single (and much nicer) ON_BLOCK_EXIT macro
//     but this results in compiler warnings about unused variables and I
//     didn't find a way to work around this other than by having different
//     macros with different names or using a less natural syntax for passing
//     the arguments (e.g. as Boost preprocessor sequences, which would mean
//     having to write wxON_BLOCK_EXIT(fwrite, (buf)(size)(n)(fp)) instead of
//     wxON_BLOCK_EXIT4(fwrite, buf, size, n, fp)).

#define wxGuardName    wxMAKE_UNIQUE_NAME(wxScopeGuard)

#define wxON_BLOCK_EXIT0_IMPL(n, f) \
    wxScopeGuard n = wxMakeGuard(f); \
    wxPrivateUse(n)
#define wxON_BLOCK_EXIT0(f) \
    wxON_BLOCK_EXIT0_IMPL(wxGuardName, f)

#define wxON_BLOCK_EXIT_OBJ0_IMPL(n, o, m) \
    wxScopeGuard n = wxMakeObjGuard(o, m); \
    wxPrivateUse(n)
#define wxON_BLOCK_EXIT_OBJ0(o, m) \
    wxON_BLOCK_EXIT_OBJ0_IMPL(wxGuardName, o, &m)

#define wxON_BLOCK_EXIT_THIS0(m) \
    wxON_BLOCK_EXIT_OBJ0(*this, m)


#define wxON_BLOCK_EXIT1_IMPL(n, f, p1) \
    wxScopeGuard n = wxMakeGuard(f, p1); \
    wxPrivateUse(n)
#define wxON_BLOCK_EXIT1(f, p1) \
    wxON_BLOCK_EXIT1_IMPL(wxGuardName, f, p1)

#define wxON_BLOCK_EXIT_OBJ1_IMPL(n, o, m, p1) \
    wxScopeGuard n = wxMakeObjGuard(o, m, p1); \
    wxPrivateUse(n)
#define wxON_BLOCK_EXIT_OBJ1(o, m, p1) \
    wxON_BLOCK_EXIT_OBJ1_IMPL(wxGuardName, o, &m, p1)

#define wxON_BLOCK_EXIT_THIS1(m, p1) \
    wxON_BLOCK_EXIT_OBJ1(*this, m, p1)


#define wxON_BLOCK_EXIT2_IMPL(n, f, p1, p2) \
    wxScopeGuard n = wxMakeGuard(f, p1, p2); \
    wxPrivateUse(n)
#define wxON_BLOCK_EXIT2(f, p1, p2) \
    wxON_BLOCK_EXIT2_IMPL(wxGuardName, f, p1, p2)

#define wxON_BLOCK_EXIT_OBJ2_IMPL(n, o, m, p1, p2) \
    wxScopeGuard n = wxMakeObjGuard(o, m, p1, p2); \
    wxPrivateUse(n)
#define wxON_BLOCK_EXIT_OBJ2(o, m, p1, p2) \
    wxON_BLOCK_EXIT_OBJ2_IMPL(wxGuardName, o, &m, p1, p2)

#define wxON_BLOCK_EXIT_THIS2(m, p1, p2) \
    wxON_BLOCK_EXIT_OBJ2(*this, m, p1, p2)


#define wxON_BLOCK_EXIT3_IMPL(n, f, p1, p2, p3) \
    wxScopeGuard n = wxMakeGuard(f, p1, p2, p3); \
    wxPrivateUse(n)
#define wxON_BLOCK_EXIT3(f, p1, p2, p3) \
    wxON_BLOCK_EXIT3_IMPL(wxGuardName, f, p1, p2, p3)

#define wxON_BLOCK_EXIT_OBJ3_IMPL(n, o, m, p1, p2, p3) \
    wxScopeGuard n = wxMakeObjGuard(o, m, p1, p2, p3); \
    wxPrivateUse(n)
#define wxON_BLOCK_EXIT_OBJ3(o, m, p1, p2, p3) \
    wxON_BLOCK_EXIT_OBJ3_IMPL(wxGuardName, o, &m, p1, p2, p3)

#define wxON_BLOCK_EXIT_THIS3(m, p1, p2, p3) \
    wxON_BLOCK_EXIT_OBJ3(*this, m, p1, p2, p3)


#define wxSetterName wxMAKE_UNIQUE_NAME(wxVarSetter)

#define wxON_BLOCK_EXIT_SET_IMPL(n, var, value) \
    wxPrivate::VariableSetter n = wxMakeVarSetter(var, value); \
    wxPrivateUse(n)

#define wxON_BLOCK_EXIT_SET(var, value) \
    wxON_BLOCK_EXIT_SET_IMPL(wxSetterName, var, value)

#define wxON_BLOCK_EXIT_NULL_IMPL(n, var) \
    wxPrivate::VariableSetter n = wxMakeVarNuller(var); \
    wxPrivateUse(n)

#define wxON_BLOCK_EXIT_NULL(ptr) \
    wxON_BLOCK_EXIT_NULL_IMPL(wxSetterName, ptr)

#endif // _WX_SCOPEGUARD_H_
