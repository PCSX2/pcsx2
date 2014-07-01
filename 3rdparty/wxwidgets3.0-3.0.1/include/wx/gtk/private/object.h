///////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/private/object.h
// Purpose:     wxGtkObject class declaration
// Author:      Vadim Zeitlin
// Created:     2008-08-27
// Copyright:   (c) 2008 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_PRIVATE_OBJECT_H_
#define _WX_GTK_PRIVATE_OBJECT_H_

// ----------------------------------------------------------------------------
// Convenience class for calling g_object_unref() automatically
// ----------------------------------------------------------------------------

template <typename T>
class wxGtkObject
{
public:
    explicit wxGtkObject(T *p) : m_ptr(p) { }
    ~wxGtkObject() { g_object_unref(m_ptr); }

    operator T *() const { return m_ptr; }

private:
    T * const m_ptr;

    // copying could be implemented by using g_object_ref() but for now there
    // is no need for it so don't implement it
    wxDECLARE_NO_COPY_CLASS(wxGtkObject);
};

#endif // _WX_GTK_PRIVATE_OBJECT_H_

