///////////////////////////////////////////////////////////////////////////////
// Name:        wx/anystr.h
// Purpose:     wxAnyStrPtr class declaration
// Author:      Vadim Zeitlin
// Created:     2009-03-23
// Copyright:   (c) 2008 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_ANYSTR_H_
#define _WX_ANYSTR_H_

#include "wx/string.h"

// ----------------------------------------------------------------------------
// wxAnyStrPtr
//
// Notice that this is an internal and intentionally not documented class. It
// is only used by wxWidgets itself to ensure compatibility with previous
// versions and shouldn't be used by user code. When you see a function
// returning it you should just know that you can treat it as a string pointer.
// ----------------------------------------------------------------------------

// This is a helper class convertible to either narrow or wide string pointer.
// It is similar to wxCStrData but, unlike it, can be NULL which is required to
// represent the return value of wxDateTime::ParseXXX() methods for example.
//
// NB: this class is fully inline and so doesn't need to be DLL-exported
class wxAnyStrPtr
{
public:
    // ctors: this class must be created from the associated string or using
    // its default ctor for an invalid NULL-like object; notice that it is
    // immutable after creation.

    // ctor for invalid pointer
    wxAnyStrPtr()
        : m_str(NULL)
    {
    }

    // ctor for valid pointer into the given string (whose lifetime must be
    // greater than ours and which should remain constant while we're used)
    wxAnyStrPtr(const wxString& str, const wxString::const_iterator& iter)
        : m_str(&str),
          m_iter(iter)
    {
    }

    // default copy ctor is ok and so is default dtor, in particular we do not
    // free the string


    // various operators meant to make this class look like a superposition of
    // char* and wchar_t*

    // this one is needed to allow boolean expressions involving these objects,
    // e.g. "if ( FuncReturningAnyStrPtr() && ... )" (unfortunately using
    // unspecified_bool_type here wouldn't help with ambiguity between all the
    // different conversions to pointers)
    operator bool() const { return m_str != NULL; }

    // at least VC6 and VC7 also need this one or they complain about ambiguity
    // for !anystr expressions
    bool operator!() const { return !((bool)*this); }


    // and these are the conversions operator which allow to assign the result
    // of FuncReturningAnyStrPtr() to either char* or wxChar* (i.e. wchar_t*)
    operator const char *() const
    {
        if ( !m_str )
            return NULL;

        // check if the string is convertible to char at all
        //
        // notice that this pointer points into wxString internal buffer
        // containing its char* representation and so it can be kept for as
        // long as wxString is not modified -- which is long enough for our
        // needs
        const char *p = m_str->c_str().AsChar();
        if ( *p )
        {
            // find the offset of the character corresponding to this iterator
            // position in bytes: we don't have any direct way to do it so we
            // need to redo the conversion again for the part of the string
            // before the iterator to find its length in bytes in current
            // locale
            //
            // NB: conversion won't fail as it succeeded for the entire string
            p += strlen(wxString(m_str->begin(), m_iter).mb_str());
        }
        //else: conversion failed, return "" as we can't do anything else

        return p;
    }

    operator const wchar_t *() const
    {
        if ( !m_str )
            return NULL;

        // no complications with wide strings (as long as we discount
        // surrogates as we do for now)
        //
        // just remember that this works as long as wxString keeps an internal
        // buffer with its wide wide char representation, just as with AsChar()
        // above
        return m_str->c_str().AsWChar() + (m_iter - m_str->begin());
    }

    // Because the objects of this class are only used as return type for
    // functions which can return NULL we can skip providing dereferencing
    // operators: the code using this class must test it for NULL first and if
    // it does anything else with it it has to assign it to either char* or
    // wchar_t* itself, before dereferencing.
    //
    // IOW this
    //
    //      if ( *FuncReturningAnyStrPtr() )
    //
    // is invalid because it could crash. And this
    //
    //      const char *p = FuncReturningAnyStrPtr();
    //      if ( p && *p )
    //
    // already works fine.

private:
    // the original string and the position in it we correspond to, if the
    // string is NULL this object is NULL pointer-like
    const wxString * const m_str;
    const wxString::const_iterator m_iter;

    wxDECLARE_NO_ASSIGN_CLASS(wxAnyStrPtr);
};

#endif // _WX_ANYSTR_H_

